#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "fatfs/ff.h"

bool
fatio_dump(const wchar_t* in_name, const wchar_t* out_name)
{
	bool rc = FALSE;
	FRESULT res;
	UINT br, bw;
	UINT64 ofs = 0;
	FIL in;
	FILE* file = 0;
	res = f_open(&in, in_name, FA_READ);
	if (res)
	{
		grub_printf("src open failed %d\n", res);
		return false;
	}
	if (_wfopen_s(&file, out_name, L"wb") != 0)
	{
		grub_printf("dst open failed\n");
		return false;
	}
	br = BUFFER_SIZE;
	wprintf(L"copy %s -> %s\n", in_name, out_name);
	for (;;)
	{
		// read file
		res = f_read(&in, g_ctx.buffer, BUFFER_SIZE, &br);
		if (res != FR_OK)
		{
			grub_printf("read failed %d\n", res);
			break;
		}
		loader(((double)ofs / (double)in.obj.objsize) * 100);
		ofs += br;
		if (br == 0)
		{
			rc = true;
			break;
		}
		bw = fwrite(g_ctx.buffer, 1, br, file);
		if (bw < br)
		{
			grub_printf("write failed %d\n", res);
			break; /* error or disk full */
		}
	}
	grub_printf("\n");
	fclose(file);
	f_close(&in);

	return rc;
}