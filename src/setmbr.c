#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "setmbr.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "grub/partition.h"

static int probe_partmap_hook(struct grub_disk* disk, const grub_partition_t partition, void* data)
{
	(void)disk;
	(void)partition;
	(void)data;
	return 1;
}

grub_partition_map_t
grub_partmap_probe(grub_disk_t disk)
{
	grub_partition_map_t partmap = 0;
	FOR_PARTITION_MAPS(partmap)
	{
		if (partmap->iterate(disk, probe_partmap_hook, NULL) == GRUB_ERR_NONE)
			break;
		grub_errno = GRUB_ERR_NONE;
	}
	return partmap;
}

bool fatio_setmbr(unsigned disk_id, const wchar_t *in_name, bool keep)
{
	int rc = 0;

	// set disk name
	char *name = NULL;
	name = grub_xasprintf("hd%u", disk_id);
	if (name == NULL)
		return false;

	// open disk
	grub_disk_t disk = grub_disk_open(name);
	if (!disk)
	{
		grub_printf("open disk failed\n");
		return false;
	}

	if (_wcsicmp(in_name, L"--empty") == 0)
	{
		size_t buffer_size = sizeof(empty_mbr);
		BYTE *buffer = grub_malloc(buffer_size);
		memcpy(buffer, empty_mbr, buffer_size);
		rc = grub_disk_write(disk, 0, 0, buffer_size, buffer);
	}
	else if (_wcsicmp(in_name, L"--nt5") == 0)
	{
		size_t buffer_size = sizeof(nt5_mbr);
		BYTE *buffer = grub_malloc(buffer_size);
		memcpy(buffer, nt5_mbr, buffer_size);
		rc = grub_disk_write(disk, 0, 0, buffer_size, buffer);
	}
	else if (_wcsicmp(in_name, L"--nt6") == 0)
	{
		size_t buffer_size = sizeof(nt6_mbr);
		BYTE *buffer = grub_malloc(buffer_size);
		memcpy(buffer, nt6_mbr, buffer_size);
		rc = grub_disk_write(disk, 0, 0, buffer_size, buffer);
	}
	else if (_wcsicmp(in_name, L"--grub4dos") == 0)
	{
			size_t buffer_size = sizeof(grldr_mbr);
			BYTE* buffer = grub_malloc(buffer_size);
			memcpy(buffer, grldr_mbr, buffer_size);
			rc = grub_disk_write(disk, 0, 0, 440, buffer);
			rc = grub_disk_write(disk, 1, 0, sizeof(grldr_mbr) - GRUB_DISK_SECTOR_SIZE, grldr_mbr + GRUB_DISK_SECTOR_SIZE);
	}
	else if (_wcsicmp(in_name, L"--syslinux") == 0)
	{
		grub_partition_map_t partmap = NULL;
		partmap = grub_partmap_probe(disk);
		if (!partmap)
		{
			grub_printf("unsupported partition table\n");
			rc = -1;
		}
		else if (grub_strcmp(partmap->name, "msdos") == 0)
		{
			rc = grub_disk_write(disk, 0, 0, 440, syslinux_mbr);
		}
		else if (grub_strcmp(partmap->name, "gpt") == 0)
		{
			rc = grub_disk_write(disk, 0, 0, 440, syslinux_gpt_mbr);
		}
	}
	else if (_wcsicmp(in_name, L"--ultraiso") == 0)
	{
		size_t buffer_size = sizeof(ultraiso_hdd);
		BYTE *buffer = grub_malloc(buffer_size);
		memcpy(buffer, ultraiso_hdd, buffer_size);
		rc = grub_disk_write(disk, 0, 0, buffer_size, buffer);
	}
	else if (_wcsicmp(in_name, L"--rufus") == 0)
	{
		size_t buffer_size = sizeof(rufus_mbr);
		BYTE *buffer = grub_malloc(buffer_size);
		memcpy(buffer, rufus_mbr, buffer_size);
		rc = grub_disk_write(disk, 0, 0, buffer_size, buffer);
	}
	else
	{
		FILE *file = 0;
		struct _stat stbuf;

		// open input file
		if (_wfopen_s(&file, in_name, L"rb") != 0)
		{
			grub_printf("src open failed\n");
			return false;
		}

		// get input file size
		if (_wstat(in_name, &stbuf) == -1)
		{
			grub_printf("Failed to get file size\n");
			return false;
		}
		size_t file_size = stbuf.st_size;

		// read mbr file
		BYTE *buffer = grub_malloc(file_size);
		fread(buffer, 1, file_size, file);
		fclose(file);

		if (keep)
		{
			grub_disk_read(disk, 0, 0x1b8, 0x1fe - 0x1b8, buffer + 0x1b8);
		}

		rc = grub_disk_write(disk, 0, 0, file_size, buffer);
	}

	grub_disk_close(disk);
	if (rc == 0)
		return true;
	wprintf(L"MBR write failed %d\n", rc);
	return false;
}
