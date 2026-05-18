#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "fatfs/ff.h"

wchar_t* formatDateFromFdate(WORD fdate) {
	int year = ((fdate >> 9) & 0x7F) + 1980;
	int month = (fdate >> 5) & 0xF;
	int day = fdate & 0x1F;

	static wchar_t formattedDate[11];  // "YYYY-MM-DD\0"
	swprintf(formattedDate, 11, L"%04d-%02d-%02d", year, month, day);
	return formattedDate;
}

wchar_t* formatTimeFromFtime(WORD ftime) {
	int hour = (ftime >> 11) & 0x1F;
	int minute = (ftime >> 5) & 0x3F;
	int second = (ftime & 0x1F) * 2;

	static wchar_t formattedTime[6];  // "HH:MM\0"
	swprintf(formattedTime, 6, L"%02d:%02d", hour, minute);
	return formattedTime;
}

wchar_t* getAttributes(BYTE fattrib) {
	static wchar_t attributes[100];
	swprintf(attributes, 100, L"%s%s%s%s%s%s",
		(fattrib & AM_RDO) ? L"Read-only " : L"",
		(fattrib & AM_HID) ? L"Hidden " : L"",
		(fattrib & AM_SYS) ? L"System " : L"",
		(fattrib & AM_ARC) ? L"Archive " : L"",
		(fattrib & AM_DIR) ? L"Directory" : L"",
		(!(fattrib & (AM_RDO | AM_HID | AM_SYS | AM_ARC | AM_DIR))) ? L"Normal" : L"");
	return attributes;
}

static
FRESULT list_dir(const wchar_t* path)
{
	FRESULT res;
	DIR dir;
	FILINFO fno;
	int nfile, ndir;

	wprintf(L"%-10s %-10s%-25s%-15s%s\n", L"Date", L"Time", L"Attributes", L"Size", L"Name");

	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		nfile = ndir = 0;
		for (;;) {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) break;  /* Error or end of dir */
			if (fno.fattrib & AM_DIR) {					   /* Directory */
				wprintf(L"%-10s %-10s%-25s%-15s%s\n", formatDateFromFdate(fno.fdate), formatTimeFromFtime(fno.ftime), getAttributes(fno.fattrib), L"-", fno.fname);
				ndir++;
			}
			else {										   /* File */
				wprintf(L"%-10s %-10s%-25s%-15llu%s\n", formatDateFromFdate(fno.fdate), formatTimeFromFtime(fno.ftime), getAttributes(fno.fattrib), fno.fsize, fno.fname);
				nfile++;
			}
		}
		f_closedir(&dir);
		wprintf(L"%d dirs, %d files.\n", ndir, nfile);
	}
	else {
		wprintf(L"Failed to open \"%s\". (%u)\n", path, res);
	}
	return res;
}

bool
fatio_list(const wchar_t* path)
{
	FRESULT rc = list_dir(path);
	if (rc == FR_OK || rc == FR_EXIST)
		return true;
	wprintf(L"list %s failed %d\n", path, rc);
	return false;
}
