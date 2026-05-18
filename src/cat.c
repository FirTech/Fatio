#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "fatfs/ff.h"

bool
fatio_cat(const wchar_t* path)
{
	FIL fil;
	TCHAR line[4096];

	FRESULT rc = f_open(&fil, path, FA_READ);
	if (rc == FR_OK || rc == FR_EXIST)
	{
		while (f_gets(line, sizeof line, &fil)) {
			wprintf(L"%s", line);
		}
		f_close(&fil);
		return true;
	}
	wprintf(L"cat %s failed %d\n", path, rc);
	return false;
}
