#include <stdio.h>
#include <fatio.h>
#include <wchar.h>
#include <loopback.h>

#include <grub/fs.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/charset.h>

#include "fatfs/ff.h"

static wchar_t*
get_u16_path(const char* file)
{
	wchar_t* buf = NULL;
	const char* p = grub_strchr(file, ')');
	p++;
	grub_size_t len = grub_strlen(p) + 1;
	buf = grub_calloc(len, sizeof(wchar_t));
	grub_utf8_to_utf16(buf, len, (const grub_uint8_t*)p, -1, NULL);
	return buf;
}

static bool
grub_copy(const char* in_name, const wchar_t* out_name)
{
	bool rc = FALSE;
	FRESULT res;
	grub_ssize_t br;
	UINT bw;
	UINT64 ofs = 0;
	FIL out;
	grub_file_t file = 0;
	file = grub_file_open(in_name, GRUB_FILE_TYPE_CAT | GRUB_FILE_TYPE_NO_DECOMPRESS);
	if (file == NULL)
	{
		grub_printf("%s open failed\n", in_name);
		return false;
	}
	res = f_open(&out, out_name, FA_WRITE | FA_CREATE_ALWAYS);
	if (res)
	{
		grub_printf("dst open failed %d\n", res);
		return false;
	}
	br = BUFFER_SIZE;
	for (;;)
	{
		// read file
		br = grub_file_read(file, g_ctx.buffer, BUFFER_SIZE);
		if (br < 0)
		{
			grub_printf("read failed\n");
			break;
		}
		loader(((double)ofs / (double)file->size) * 100);
		ofs += br;
		if (br == 0)
		{
			rc = true;
			break;
		}
		res = f_write(&out, g_ctx.buffer, br, &bw);
		if (res || bw < (UINT)br)
		{
			grub_printf("write failed %d\n", res);
			break; /* error or disk full */
		}
	}
	grub_printf("\n");
	grub_file_close(file);
	f_close(&out);

	return rc;
}

static bool
is_hidden(const char* filename)
{
	if (strcmp(filename, ".") == 0)
		return true;
	if (strcmp(filename, "..") == 0)
		return true;
	return false;
}

struct ctx_extract_file
{
	grub_fs_t fs;
	grub_disk_t disk;
	char* pwd;
};

static void
extract_dir_real(struct ctx_extract_file* ctx);

static int
callback_extract_file(const char* filename,
	const struct grub_dirhook_info* info, void* data)
{
	struct ctx_extract_file* ctx = data;
	if (is_hidden(filename))
		return 0;
	if (info->symlink)
		return 0;
	if (info->dir)
	{
		struct ctx_extract_file* new_ctx = grub_malloc(sizeof(struct ctx_extract_file));
		new_ctx->pwd = grub_xasprintf("%s/%s/", ctx->pwd, filename);
		new_ctx->fs = ctx->fs;
		new_ctx->disk = ctx->disk;
		grub_printf("[+] %s\n", new_ctx->pwd);
		wchar_t* u16 = get_u16_path(new_ctx->pwd);
		fatio_mkdir(u16);
		grub_free(u16);
		extract_dir_real(new_ctx);
		grub_free(new_ctx->pwd);
		grub_free(new_ctx);
	}
	else
	{
		char* path = grub_xasprintf("%s/%s", ctx->pwd, filename);
		grub_printf("--- %s\n", path);
		wchar_t* u16 = get_u16_path(path);
		grub_copy(path, u16);
		grub_free(u16);
		grub_free(path);
	}
	grub_errno = GRUB_ERR_NONE;
	return 0;
}

static void
extract_dir_real(struct ctx_extract_file* ctx)
{
	grub_errno = GRUB_ERR_NONE;
	char* dir = grub_strchr(ctx->pwd, ')');
	ctx->fs->fs_dir(ctx->disk, ++dir, callback_extract_file, ctx);
	grub_errno = GRUB_ERR_NONE;
}

bool
fatio_extract(const wchar_t* src)
{
	grub_disk_t disk = NULL;
	grub_fs_t fs = NULL;
	if (!grub_loopback_set(src))
		return false;
	disk = grub_disk_open("loop");
	if (!disk)
	{
		grub_printf("Failed to open loop disk\n");
		grub_loopback_unset();
		return false;
	}
	fs = grub_fs_probe(disk);
	if (!fs)
	{
		grub_printf("Failed to probe fs\n");
		grub_disk_close(disk);
		grub_loopback_unset();
		return false;
	}

	grub_printf("fs: %s\n", fs->name);

	struct ctx_extract_file ctx =
	{
		.fs = fs,
		.disk = disk,
		.pwd = grub_strdup("(loop)/"),
	};
	extract_dir_real(&ctx);
	grub_free(ctx.pwd);

	grub_disk_close(disk);
	grub_loopback_unset();
	return true;
}
