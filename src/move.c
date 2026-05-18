#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "fatfs/ff.h"

bool
fatio_move(const wchar_t* in_name, const wchar_t* out_name)
{
	FRESULT rc = f_rename(in_name, out_name);
	if (rc == FR_OK || rc == FR_EXIST)
		return true;
	wprintf(L"move %s to %s failed %d\n", in_name, out_name, rc);
	return false;
}
