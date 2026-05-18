#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "fatfs/ff.h"

bool
fatio_mkdir(const wchar_t* path)
{
	FRESULT rc = f_mkdir(path);

	if (rc == FR_OK || rc == FR_EXIST)
		return true;
	wprintf(L"mkdir %s failed %d\n", path, rc);
	return false;
}
