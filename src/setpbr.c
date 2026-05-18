#include <stdio.h>
#include <fatio.h>
#include <wchar.h>
#include <stdint.h>
#include <winioctl.h>

#include "setpbr.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "grub/disk.h"
#include "grub/partition.h"
#include "grub/fs.h"
#include "grub/fat.h"
#include "grub/exfat.h"
#include "grub/ntfs.h"
#include <grub/file.h>

grub_disk_addr_t
grub_br_get_fs_reserved_sectors(grub_disk_t disk)
{
	grub_disk_addr_t reserved = 0;
	grub_uint8_t vbr[GRUB_DISK_SECTOR_SIZE];
	const char *fs_name = grub_fs_get_name(disk);
	grub_errno = GRUB_ERR_NONE;
	if (!fs_name)
		goto out;
	if (grub_disk_read(disk, 0, 0, GRUB_DISK_SECTOR_SIZE, vbr))
		goto out;
	if (grub_strncmp(fs_name, "fat", 3) == 0)
	{
		struct grub_fat_bpb *bpb = (void *)vbr;
		reserved = bpb->num_reserved_sectors;
	}
	else if (grub_strcmp(fs_name, "exfat") == 0)
	{
		struct grub_exfat_bpb *bpb = (void *)vbr;
		reserved = bpb->num_reserved_sectors;
	}
	else if (grub_strcmp(fs_name, "ntfs") == 0)
	{
		grub_file_t file = 0;
		reserved = 16;
		file = grub_zalloc(sizeof(struct grub_file));
		if (!file)
			goto out;
		file->disk = disk;
		if (grub_ntfs_fs.fs_open(file, "/$Boot"))
			goto out;
		reserved = file->size >> GRUB_DISK_SECTOR_BITS;
		grub_ntfs_fs.fs_close(file);
		grub_free(file);
	}
	else if (grub_strcmp(fs_name, "ext") == 0)
		reserved = 2;
out:
	grub_errno = GRUB_ERR_NONE;
	return reserved;
}

/* Copyright (C) 2020  JPZ4085
 * Based on fat32.c Copyright (C) 2009  Henrik Carlqvist */
static uint32_t exfat_boot_checksum(const void *sector, size_t size)
{
	size_t i;
	uint32_t sum = 0;
	uint32_t block = (uint32_t)size * 11;

	for (i = 0; i < block; i++)
		/* skip volume_state and allocated_percent fields */
		if (i != 0x6a && i != 0x6b && i != 0x70)
			sum = ((sum << 31) | (sum >> 1)) + ((const uint8_t *)sector)[i];
	return sum;
}

void grub_br_write_exfat_checksum(grub_disk_t disk)
{
	uint32_t checksum = 0;
	unsigned i;

	grub_uint8_t *vbr_buf = NULL;
	vbr_buf = grub_calloc(11, GRUB_DISK_SECTOR_SIZE);
	if (!vbr_buf)
		return;
	if (grub_disk_read(disk, 0, 0, 11 << GRUB_DISK_SECTOR_BITS, vbr_buf))
		goto fail;

	checksum = exfat_boot_checksum(vbr_buf, GRUB_DISK_SECTOR_SIZE);
	/* Write new main VBR checksum */
	for (i = 0; i < (GRUB_DISK_SECTOR_SIZE / sizeof(checksum)); i++)
	{
		if (grub_disk_write(disk, 11, i * sizeof(checksum), sizeof(checksum), &checksum))
			break;
	}
	/* Write new backup VBR */
	grub_disk_write(disk, 12, 0, 11 << GRUB_DISK_SECTOR_BITS, vbr_buf);
	/* Write new backup VBR checksum */
	for (i = 0; i < (GRUB_DISK_SECTOR_SIZE / sizeof(checksum)); i++)
	{
		if (grub_disk_write(disk, 23, i * sizeof(checksum), sizeof(checksum), &checksum))
			break;
	}
fail:
	grub_free(vbr_buf);
}

bool fatio_setpbr(unsigned disk_id, unsigned part_id, const wchar_t *in_name)
{
	int rc = 0;
	HANDLE hVolume = INVALID_HANDLE_VALUE;
	DWORD dwReturn = 0;
	BOOL bRet = FALSE;

	// Lock volume before writing
	if (!fatio_set_disk(disk_id, part_id))
	{
		grub_printf("Failed to lock volume\n");
		return false;
	}

	grub_disk_t disk = g_ctx.disk;
	const char* fs_name = grub_fs_get_name(disk);

	if (_wcsicmp(in_name, L"--nt5") == 0)
	{
		if (grub_strcmp(fs_name, "fat12") == 0 || grub_strcmp(fs_name, "fat16") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *ntldr_fat16 = ntldr_pbr;
			rc1 = grub_disk_write(disk, 0, 0, 3, ntldr_fat16); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x3e, 0x200 - 0x3e, ntldr_fat16 + 0x3e);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "fat32") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *ntldr_fat32 = ntldr_pbr + 0x200;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 2)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, ntldr_fat32); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x5a, 0x400 - 0x5a, ntldr_fat32 + 0x5a);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "ntfs") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *ntldr_ntfs = ntldr_pbr + 0x600;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 7)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, ntldr_ntfs); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x54, 0xe00 - 0x54, ntldr_ntfs + 0x54);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else
		{
			rc = -1;
			grub_printf("unsupported filesystem %s\n", fs_name);
		}
	}
	else if (_wcsicmp(in_name, L"--nt6") == 0)
	{
		if (grub_strcmp(fs_name, "fat12") == 0 || grub_strcmp(fs_name, "fat16") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *bootmgr_fat16 = bootmgr_pbr;
			rc1 = grub_disk_write(disk, 0, 0, 3, bootmgr_fat16); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x3e, 0x200 - 0x3e, bootmgr_fat16 + 0x3e);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "fat32") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *bootmgr_fat32 = bootmgr_pbr + 0x200;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 2)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, bootmgr_fat32); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x5a, 0x400 - 0x5a, bootmgr_fat32 + 0x5a);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "exfat") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *bootmgr_exfat = bootmgr_pbr + 0x600;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 3)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, bootmgr_exfat); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x78, 0x600 - 0x78, bootmgr_exfat + 0x78);
			grub_br_write_exfat_checksum(disk);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "ntfs") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0;
			grub_uint8_t *bootmgr_ntfs = bootmgr_pbr + 0xc00;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 10)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, bootmgr_ntfs); // jmp_boot[3]
			rc2 = grub_disk_write(disk, 0, 0x54, 0x1400 - 0x54, bootmgr_ntfs + 0x54);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE)
				rc = -1;
		}
		else
		{
			rc = -1;
			grub_printf("unsupported filesystem %s\n", fs_name);
		}
	}
	else if (_wcsicmp(in_name, L"--grub4dos") == 0)
	{
		grub_uint8_t grldr[5] = {'G', 'R', 'L', 'D', 'R'};

		if (grub_strcmp(fs_name, "fat12") == 0 || grub_strcmp(fs_name, "fat16") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0, rc3 = 0;
			grub_uint8_t *grldr_fat16 = grldr_pbr + 0x200;
			rc1 = grub_disk_write(disk, 0, 0, 3, grldr_fat16); // jmp_boot[3]
			rc3 = grub_disk_write(disk, 0, 0x3e, 0x200 - 0x3e, grldr_fat16 + 0x3e);
			rc3 = grub_disk_write(disk, 0, 0x1e3, 5, grldr);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE || rc3 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "fat32") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0, rc3 = 0;
			grub_uint8_t *grldr_fat32 = grldr_pbr;
			rc1 = grub_disk_write(disk, 0, 0, 3, grldr_fat32); // jmp_boot[3]
			rc3 = grub_disk_write(disk, 0, 0x5a, 0x200 - 0x5a, grldr_fat32 + 0x5a);
			rc3 = grub_disk_write(disk, 0, 0x1e3, 5, grldr);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE || rc3 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "exfat") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0, rc3 = 0;
			grub_uint8_t *grldr_exfat = grldr_pbr + 0x800;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 2)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, grldr_exfat); // jmp_boot[3]
			rc3 = grub_disk_write(disk, 0, 0x78, 0x400 - 0x78, grldr_exfat + 0x78);
			rc3 = grub_disk_write(disk, 0, 0x1e3, 5, grldr);
			grub_br_write_exfat_checksum(disk);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE || rc3 != GRUB_ERR_NONE)
				rc = -1;
		}
		else if (grub_strcmp(fs_name, "ntfs") == 0)
		{
			grub_err_t rc1 = 0, rc2 = 0, rc3 = 0;
			grub_uint8_t *grldr_ntfs = grldr_pbr + 0xc00;
			grub_disk_addr_t reserved = grub_br_get_fs_reserved_sectors(disk);
			if (reserved < 4)
				return grub_error(GRUB_ERR_BAD_DEVICE, "unsupported reserved sectors");
			rc1 = grub_disk_write(disk, 0, 0, 3, grldr_ntfs); // jmp_boot[3]
			rc3 = grub_disk_write(disk, 0, 0x54, 0x800 - 0x54, grldr_ntfs + 0x54);
			rc3 = grub_disk_write(disk, 0, 0x1e3, 5, grldr);
			if (rc1 != GRUB_ERR_NONE || rc2 != GRUB_ERR_NONE || rc3 != GRUB_ERR_NONE)
				rc = -1;
		}
		else
		{
			rc = -1;
			grub_printf("unsupported filesystem %s\n", fs_name);
		}
	}

	fatio_unset_disk();
	if (rc == 0)
		return true;
	wprintf(L"PBR write failed %d\n", rc);
	return false;
}
