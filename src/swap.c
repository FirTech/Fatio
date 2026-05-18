#include <stdio.h>
#include <fatio.h>
#include <grub/msdos_partition.h>

bool
fatio_swap_partitions(unsigned disk_id, unsigned partno1, unsigned partno2)
{
    int rc = 0;

    // Check if the partition numbers are the same
    if (partno1 == partno2) {
        grub_printf("Error: Partition numbers cannot be the same\n");
        return false;
    }

    // set disk name
    char* name = grub_xasprintf("hd%u", disk_id);
    if (name == NULL)
        return false;

    // open disk
    grub_disk_t disk = grub_disk_open(name);
    if (!disk) {
        grub_printf("open disk failed\n");
        grub_free(name);
        return false;
    }

    // read mbr
    grub_uint8_t mbr[GRUB_DISK_SECTOR_SIZE];
    if (grub_disk_read(disk, 0, 0, sizeof(mbr), mbr)) {
        grub_printf("Failed to read MBR\n");
        grub_disk_close(disk);
        grub_free(name);
        return false;
    }

    // check mbr signature
    struct grub_msdos_partition_mbr* mbr_part = (struct grub_msdos_partition_mbr*)mbr;
    if (mbr_part->signature != grub_cpu_to_le16_compile_time(0xAA55)) {
        grub_printf("Invalid MBR signature\n");
        grub_disk_close(disk);
        grub_free(name);
        return false;
    }

    // check part number£¨1-4£©
    if (partno1 < 1 || partno1 > 4 || partno2 < 1 || partno2 > 4) {
        grub_printf("Invalid partition number\n");
        grub_disk_close(disk);
        grub_free(name);
        return false;
    }

    // Convert to array index£¨0-3£©
    unsigned idx1 = partno1 - 1;
    unsigned idx2 = partno2 - 1;

    // swap partition
    struct grub_msdos_partition_entry temp = mbr_part->entries[idx1];
    mbr_part->entries[idx1] = mbr_part->entries[idx2];
    mbr_part->entries[idx2] = temp;

    // write back the modified MBR
    rc = grub_disk_write(disk, 0, 0, sizeof(mbr), mbr);
    if (rc != 0) {
        grub_printf("Failed to write MBR\n");
        grub_disk_close(disk);
        grub_free(name);
        return false;
    }

    // refresh disk cache
    grub_disk_cache_invalidate_all();

    // Force refresh of system cache under Windows
    char diskPath[MAX_PATH];
    snprintf(diskPath, sizeof(diskPath), "\\\\.\\PhysicalDrive%u", disk_id);
    HANDLE hDisk = CreateFileA(diskPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hDisk);
        CloseHandle(hDisk);
    }

    grub_disk_close(disk);
    grub_free(name);
    return true;
}