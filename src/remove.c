#include <stdio.h>
#include <fatio.h>
#include <wchar.h>

#include "fatfs/ff.h"

static
FRESULT remove_files(const wchar_t* path)
{
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    wchar_t new_path[256];

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            if (fno.fattrib & AM_DIR)                      /* It is a directory */
            {
                swprintf(new_path, sizeof(new_path) / sizeof(new_path[0]), L"%s\\%s", path, fno.fname);
                res = remove_files(new_path);                    /* Enter the directory */
                if (res != FR_OK) break;
                f_unlink(new_path);
            }
            else                                           /* It is a file. */
            {
                swprintf(new_path, sizeof(new_path) / sizeof(new_path[0]), L"%s\\%s", path, fno.fname);
                f_chmod(new_path, 0, AM_RDO);
                res = f_unlink(new_path);
            }
        }
        f_closedir(&dir);
        f_chmod(new_path, 0, AM_RDO);
        f_unlink(path);
    }

    return res;
}


bool
fatio_remove(const wchar_t* path)
{
    FRESULT rc;
    DIR dir;

    rc = f_opendir(&dir, path);
    if (rc == FR_OK)
    {
        rc = remove_files(path);
        if (rc == FR_OK) 
            return true;
    }
    else
    {
        f_chmod(path, 0, AM_RDO);
        rc = f_unlink(path);
        if (rc == FR_OK || rc == FR_EXIST)
            return true;
    }
    wprintf(L"remove %s failed %d\n", path, rc);
    return false;
}
