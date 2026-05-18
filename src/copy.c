#include <stdio.h>
#include <fatio.h>
#include <wchar.h>
#include <io.h>
#include <dirent.h>
#include <time.h>
#include <math.h>

#include "fatfs/ff.h"

// Global variables for tracking copy progress
static UINT64 g_total_size = 0;		// Total size of all files
static UINT64 g_copied_size = 0;	// Size of copied data
static int g_total_progress = -1; // Current progress percentage
static UINT32 g_total_files = 0;	// Total number of files

// Format file size with appropriate units
static const wchar_t *format_size(UINT64 size)
{
	static wchar_t buf[32];
	const wchar_t *units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
	int unit = 0;
	double size_d = (double)size;

	while (size_d >= 1024.0 && unit < 4)
	{
		size_d /= 1024.0;
		unit++;
	}

	if (unit == 0)
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"%.0f %s", size_d, units[unit]);
	else
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"%.2f %s", size_d, units[unit]);

	return buf;
}

// Calculate total size and count of files in directory
static UINT64 calculate_total_size(const wchar_t *path)
{
	UINT64 total = 0;
	_WDIR *dir = wopendir(path);
	struct _stat stbuf;
	struct wdirent *ent;
	wchar_t new_path[MAX_PATH];

	if (!dir)
		return 0;

	while ((ent = wreaddir(dir)) != NULL)
	{
		if (wcscmp(ent->d_name, L".") == 0 || wcscmp(ent->d_name, L"..") == 0)
			continue;

		swprintf(new_path, sizeof(new_path) / sizeof(new_path[0]), L"%s\\%s", path, ent->d_name);

		if (_wstat(new_path, &stbuf) == -1)
			continue;

		if (S_ISDIR(stbuf.st_mode))
		{
			total += calculate_total_size(new_path);
		}
		else if (S_ISREG(stbuf.st_mode))
		{
			total += stbuf.st_size;
			g_total_files++;
		}
	}
	wclosedir(dir);
	return total;
}

// Update and display total progress
static void update_total_progress(void)
{
	if (g_total_size > 0)
	{
		int new_progress = (int)(((double)g_copied_size / (double)g_total_size) * 100);
		if (new_progress != g_total_progress)
		{
			g_total_progress = new_progress;
			loader(g_total_progress);
		}
	}
}

bool copy_file(const wchar_t *in_name, const wchar_t *out_name, bool update)
{
	bool rc = FALSE;
	FRESULT res;
	UINT br, bw;
	UINT64 ofs = 0;
	FIL out;
	FILE *file = 0;
	struct _stat stbuf;
	FILINFO out_info;

	// Open input file
	if (_wfopen_s(&file, in_name, L"rb") != 0)
	{
		grub_printf("src open failed\n");
		return false;
	}

	// Set larger buffer for input file
	setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);

	// Get input file size
	if (_wstat(in_name, &stbuf) == -1)
	{
		grub_printf("Failed to get file size\n");
		fclose(file);
		return false;
	}
	long long file_size = stbuf.st_size;

	// Check if file can be skipped
	if (update && f_stat(out_name, &out_info) == FR_OK)
	{
		time_t fatTimestamp = ((out_info.fdate >> 9) + 1980 - 1970) * 31536000L + ((out_info.fdate >> 5) & 15) * 2592000L + (out_info.fdate & 31) * 86400L + (out_info.ftime >> 11) * 3600L + ((out_info.ftime >> 5) & 63) * 60L + (out_info.ftime & 31) * 2L;

		struct tm *file_time_utc = gmtime(&stbuf.st_mtime);
		struct tm *fat_time_utc = gmtime(&fatTimestamp);

		time_t file_time_utc_ts = mktime(file_time_utc);
		time_t fat_time_utc_ts = mktime(fat_time_utc);

		if (out_info.fsize == file_size && fabs(file_time_utc_ts - fat_time_utc_ts) <= 2)
		{
			g_copied_size += file_size; // Update copied size
			update_total_progress();		// Update progress display
			fclose(file);
			return true;
		}
	}

	// Open output file
	res = f_open(&out, out_name, FA_WRITE | FA_CREATE_ALWAYS);
	if (res)
	{
		grub_printf("dst open failed %d\n", res);
		fclose(file);
		return false;
	}

	// Pre-allocate space for large files (>10MB)
	if (file_size > 10 * 1024 * 1024)
	{
		f_expand(&out, file_size, 1);
	}

	while ((br = fread(g_ctx.buffer, 1, BUFFER_SIZE, file)) > 0)
	{
		ofs += br;
		if (f_write(&out, g_ctx.buffer, br, &bw) != FR_OK || bw < br)
		{
			grub_printf("write failed\n");
			rc = FALSE;
			break;
		}

		g_copied_size += bw;		 // Update copied size
		update_total_progress(); // Update progress display
	}

	// Ensure all data is written
	f_sync(&out);
	f_close(&out);
	fclose(file);

	return br == 0;
}

bool copy_folder(const wchar_t *in_name, const wchar_t *out_name, bool update)
{
	_WDIR *dir = wopendir(in_name);
	struct _stat stbuf;
	struct wdirent *ent;
	wchar_t new_path[MAX_PATH];
	wchar_t out_path[MAX_PATH];

	if (!dir)
	{
		wprintf(L"open %s failed\n", in_name);
		return false;
	}

	FRESULT out_stat = f_stat(out_name, NULL);
	if (out_stat == FR_NO_PATH || out_stat == FR_NO_FILE)
		f_mkdir(out_name);

	while ((ent = wreaddir(dir)) != NULL)
	{
		if (wcscmp(ent->d_name, L".") == 0 || wcscmp(ent->d_name, L"..") == 0)
			continue;

		swprintf(new_path, sizeof(new_path) / sizeof(new_path[0]), L"%s\\%s", in_name, ent->d_name);
		swprintf(out_path, sizeof(out_path) / sizeof(out_path[0]), L"%s\\%s", out_name, ent->d_name);

		if (_wstat(new_path, &stbuf) == -1)
			continue;
		if (S_ISDIR(stbuf.st_mode))
			copy_folder(new_path, out_path, update);
		else if (S_ISREG(stbuf.st_mode))
			copy_file(new_path, out_path, update);
	}
	wclosedir(dir);
	return true;
}

wchar_t *get_file_name(const wchar_t *path)
{
	const wchar_t *fileName = wcsrchr(path, L'/');
	if (fileName == NULL)
		fileName = wcsrchr(path, L'\\');
	return _wcsdup((fileName != NULL) ? fileName + 1 : path);
}

bool fatio_copy(const wchar_t *in_name, const wchar_t *out_name, bool update)
{
	struct _stat instbuf;
	wchar_t out_path[MAX_PATH];
	bool result;

	// Reset global variables
	g_total_size = 0;
	g_copied_size = 0;
	g_total_progress = -1;
	g_total_files = 0;

	if (_wstat(in_name, &instbuf) == -1)
	{
		wprintf(L"open %s failed\n", in_name);
		return false;
	}

	// Calculate total size and file count
	if (S_ISDIR(instbuf.st_mode))
	{
		g_total_size = calculate_total_size(in_name);
		wprintf(L"Copy %s -> %s\n", in_name, out_name);
		wprintf(L"Total files: %u\n", g_total_files);
		wprintf(L"Total size: %s\n\n", format_size(g_total_size));
	}
	else if (S_ISREG(instbuf.st_mode))
	{
		g_total_size = instbuf.st_size;
		wprintf(L"Copy %s -> %s\n", in_name, out_name);
		wprintf(L"Size: %s\n\n", format_size(g_total_size));
	}

	// Execute copy operation
	if (S_ISDIR(instbuf.st_mode))
	{
		result = copy_folder(in_name, out_name, update);
	}
	else if (S_ISREG(instbuf.st_mode))
	{
		if (wcscmp(out_name, L"\\") == 0)
			swprintf(out_path, sizeof(out_path) / sizeof(out_path[0]), L"\\%ls", get_file_name(in_name));
		else
			swprintf(out_path, sizeof(out_path) / sizeof(out_path[0]), L"%ls\\%ls", out_name, get_file_name(in_name));
		result = copy_file(in_name, out_path, update);
	}
	else
	{
		result = false;
	}

	if (g_total_progress >= 0)
		grub_printf("\n");

	return result;
}
