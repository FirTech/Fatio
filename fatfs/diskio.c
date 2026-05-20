/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <time.h>

#include "ff.h"
#include "diskio.h"

#include <fatio.h>
#include <locale.h>


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	(void)pdrv;
	if (g_ctx.disk == NULL)
		return STA_NOINIT;
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	(void)pdrv;
	if (g_ctx.disk == NULL)
		return STA_NOINIT;
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	grub_size_t size = count << GRUB_DISK_SECTOR_BITS;

	(void)pdrv;
	if (g_ctx.disk == NULL)
		return RES_NOTRDY;
	if (sector > g_ctx.total_sectors)
		return RES_ERROR;

	if (grub_disk_read(g_ctx.disk, sector, 0, size, buff) == GRUB_ERR_NONE)
		return RES_OK;
	return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	grub_size_t size = count << GRUB_DISK_SECTOR_BITS;

	(void)pdrv;
	if (g_ctx.disk == NULL)
		return RES_NOTRDY;
	if (sector > g_ctx.total_sectors)
		return RES_ERROR;

	if (grub_disk_write(g_ctx.disk, sector, 0, size, buff) == GRUB_ERR_NONE)
		return RES_OK;
	grub_print_error();
	return RES_ERROR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	(void)pdrv;
	if (g_ctx.disk == NULL)
		return RES_NOTRDY;
	switch (cmd)
	{
	case CTRL_SYNC:
		return RES_OK;
	case GET_SECTOR_COUNT:
		*(LBA_t*)buff = g_ctx.total_sectors;
		return RES_OK;
	case GET_SECTOR_SIZE:
		*(WORD*)buff = GRUB_DISK_SECTOR_SIZE;
		return RES_OK;
	case GET_BLOCK_SIZE:
		*(DWORD*)buff = 8;
		return RES_OK;
	case CTRL_TRIM:
		return RES_OK;
	}

	return RES_PARERR;
}

DWORD get_fattime(void)
{
	SYSTEMTIME tm;

	GetLocalTime(&tm);

	return (tm.wYear - 1980) << 25 | tm.wMonth << 21 | tm.wDay << 16 | tm.wHour << 11 | tm.wMinute << 5 | tm.wSecond >> 1;
}

