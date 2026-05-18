#include <stdio.h>
#include <fatio.h>
#include <dl.h>
#include <wchar.h>
#include <winioctl.h>

#include <grub/disk.h>
#include <grub/fs.h>
#include <grub/err.h>
#include <grub/partition.h>

#include "fatfs/ff.h"

struct fatio_ctx g_ctx;

void
fatio_remove_trailing_backslash(wchar_t* path)
{
	size_t len = wcslen(path);
	if (len < 1 || path[len - 1] != L'\\')
		return;
	path[len - 1] = L'\0';
}

static bool
lock_volume(unsigned disk_id)
{
	WCHAR path[MAX_PATH];
	grub_uint64_t lba = grub_partition_get_start(g_ctx.disk->partition);

	//grub_printf("disk %u, lba = %llu\n", disk_id, lba);

	HANDLE volume = FindFirstVolumeW(path, MAX_PATH);

	while (volume != NULL && volume != INVALID_HANDLE_VALUE)
	{
		DWORD dw = 0;
		STORAGE_DEVICE_NUMBER sdn = { 0 };
		PARTITION_INFORMATION_EX pie = { 0 };

		fatio_remove_trailing_backslash(path);

		HANDLE hv = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hv == INVALID_HANDLE_VALUE)
			goto next;

		if (!DeviceIoControl(hv, IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL, 0, &sdn, (DWORD)(sizeof(STORAGE_DEVICE_NUMBER)), &dw, NULL))
			goto next;

		switch (sdn.DeviceType)
		{
		case FILE_DEVICE_DISK:
		case FILE_DEVICE_DISK_FILE_SYSTEM:
		case FILE_DEVICE_FILE_SYSTEM:
			if (sdn.DeviceNumber != disk_id)
				goto next;
			break;
		default:
			goto next;
		}

		dw = sizeof(PARTITION_INFORMATION_EX);
		if (!DeviceIoControl(hv, IOCTL_DISK_GET_PARTITION_INFO_EX,
			NULL, 0, &pie, dw, &dw, NULL))
			goto next;
		if (lba != (grub_uint64_t)(pie.StartingOffset.QuadPart >> GRUB_DISK_SECTOR_BITS))
			goto next;
		//wprintf(L"Locking: %s, %lu, %lu, %llu\n", path, sdn.DeviceNumber, sdn.PartitionNumber, pie.StartingOffset.QuadPart >> GRUB_DISK_SECTOR_BITS);
		dw = 0;
		if (!DeviceIoControl(hv, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dw, NULL))
		{
			grub_printf("Failed to lock volume\n");
			CloseHandle(hv);
			return false;
		}
		g_ctx.volume = hv;
		break;
	next:
		if (hv != INVALID_HANDLE_VALUE)
			CloseHandle(hv);
		if (FindNextVolumeW(volume, path, MAX_PATH) == 0)
			break;
	}
	return true;
}

bool
fatio_set_disk(unsigned disk_id, unsigned part_id)
{
	char* name = NULL;
	g_ctx.volume = INVALID_HANDLE_VALUE;
	name = grub_xasprintf("hd%u,%u", disk_id, part_id);
	if (name == NULL)
		return false;
	g_ctx.disk = grub_disk_open(name);
	grub_free(name);
	if (g_ctx.disk == NULL)
		return false;
	if (g_ctx.disk->dev->id != GRUB_DISK_DEVICE_WINDISK_ID || !g_ctx.disk->partition)
	{
		grub_disk_close(g_ctx.disk);
		g_ctx.disk = NULL;
		return false;
	}
	g_ctx.total_sectors = grub_disk_native_sectors(g_ctx.disk);
	g_ctx.buffer = grub_malloc(BUFFER_SIZE);
	if (g_ctx.buffer == NULL)
	{
		grub_disk_close(g_ctx.disk);
		g_ctx.disk = NULL;
		return false;
	}
	if (lock_volume(disk_id))
		return true;
	grub_free(g_ctx.buffer);
	g_ctx.buffer = NULL;
	grub_disk_close(g_ctx.disk);
	g_ctx.disk = NULL;
	return false;
}

void
fatio_unset_disk(void)
{
	if (g_ctx.disk)
		grub_disk_close(g_ctx.disk);
	g_ctx.disk = NULL;
	g_ctx.total_sectors = 0;
	if (g_ctx.buffer)
		grub_free(g_ctx.buffer);
	g_ctx.buffer = NULL;
	if (g_ctx.volume != NULL && g_ctx.volume != INVALID_HANDLE_VALUE)
		CloseHandle(g_ctx.volume);
	GetLogicalDrives();
}
