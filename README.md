# fatio

A program for reading and writing FAT32/EXFAT file systems.

## command

### list

List supported partitions.

```shell
fatio.exe list
```

### mkfs

Create an FAT/exFAT volume, Supported format options: FAT, FAT32, EXFAT.

```shell
fatio.exe mkfs Disk Part format [CLUSTER_SIZE]
# Example:
# fatio.exe mkfs 1 2 FAT
# fatio.exe mkfs 1 2 FAT 512
```

### label

Set/remove the label of a volume.

```shell
fatio.exe label Disk Part [String]
# Example:
# fatio.exe label 1 2 mydisk
```

### mkdir

Create a new directory.

```shell
fatio.exe mkdir Disk Part Dir
# Example:
# fatio.exe mkdir 1 2 \dir
```

### ls

List files in the specified directory.

```shell
fatio.exe ls Disk Part Dir
# Example:
# fatio.exe ls 1 2 \
```

### cat

Print files content from FAT partition.

```shell
fatio.exe cat Disk Part Dest_File
# Examples:
# fatio.exe cat 1 2 \text.txt
# fatio.exe cat 1 2 \files\text.txt
```

### copy

Copy files from FAT/EXFAT file systems.

```shell
fatio.exe copy Disk Part Src_File Dest_File [-y]
# Examples:
# fatio.exe copy 1 2 D:\text.txt text.txt
# fatio.exe copy 1 2 D:\text.txt \dir\text.txt
# fatio.exe copy 1 2 D:\files \
# Update mode, copy only when the source file is inconsistent
# fatio.exe copy 1 2 D:\files \ -y
```

### remove

Remove the file from FAT partition.

```shell
fatio.exe remove Disk Part Dest_File
# Examples:
# fatio.exe remove 1 2 \text.txt
# fatio.exe remove 1 2 \dir
```

### move

move the file from FAT partition.

```shell
fatio.exe move Disk Part Src_File Dest_File
# Examples:
# fatio.exe move 1 2 \text.txt \abc.txt
```

### extract

Extract the archive file to FAT partition.

```shell
fatio.exe extract Disk Part File
# Example:
# fatio.exe extract 1 2 D:\windows.iso
```

### dump

Dump the file from FAT partition.

```shell
fatio.exe dump Disk Part Src_File Dest_File
# Examples:
# fatio.exe dump 1 2 text.txt D:\text.txt
# fatio.exe dump 1 2 \dir\text.txt D:\text.txt
```

### chmod

Change file attributes for files on a FAT partition.

| Attributes | Description |
| ---------- | ----------- |
| A          | Archive     |
| R          | Read Only   |
| S          | System      |
| H          | Hidden      |

```shell
fatio.exe chmod Disk Part File [+/-A] [+/-H] [+/-R] [+/-S]
# Examples:
# fatio.exe chmod 1 2 text.txt +A +H +R
```

### setmbr

Set disk MBR, support types: empty, nt5, nt6, grub4dos, ultraiso, rufus, syslinux.

```shell
fatio.exe setmbr Disk [--MBR_TYPE] [DEST_FILE] [-n]
# Examples:
# fatio.exe setmbr 1 --nt6
# fatio.exe setmbr 1 --grub4dos
# fatio.exe setmbr 1 D:\mbr.bin
# Do NOT keep original disk signature and partition table
# fatio.exe setmbr 1 D:\mbr.bin -n
```

### setpbr

Set disk PBR, support types: nt5, nt6, grub4dos.

```shell
fatio.exe setpbr Disk Part [--PBR_TYPE]
# Examples:
# fatio.exe setmbr 1 1 --nt6
# fatio.exe setmbr 1 2 --grub4dos
```

### setpartid

Set part id type (e.g. `0x07` represents NFT/exFAT, `0x0C` represents FAT32).

```shell
fatio.exe setpartid Disk Part ID
# Examples:
# fatio.exe setpartid 1 1 0C
# fatio.exe setpartid 1 2 EF
```

Common partition type identifiers:

| part type | part format |
| :-------: | :---------: |
|   0x07    | NTFS/exFAT  |
|   0x0C    | FAT32 (LBA) |
|   0x83    |    Linux    |
|   0x82    | Linux Swap  |
|   0x05    |  扩展分区   |

### setactive

Set partition active.

```shell
fatio.exe setactive Disk Part
# Examples:
# fatio.exe setactive 1 1
# fatio.exe setactive 1 2
```

### swap

Swap partition order.

```shell
fatio.exe swap Disk Part1 Part2
# Examples:
# fatio.exe swap 1 1 2
```

## build

This project can be built from the command line without opening the Visual Studio IDE, but it still requires the Visual C++ build toolchain.

Requirements:

- Visual Studio 2022 Build Tools (or Visual Studio 2022)
- MSVC v143 C++ x86/x64 build tools
- Windows 10/11 SDK
- NuGet packages restored under `packages\`

Build scripts:

- `build.bat`
- `build_x86.bat`
- `build_x64.bat`

Examples:

```bat
build.bat
build.bat Rebuild x64 Release
build.bat Build Win32 Debug
build_x86.bat
build_x64.bat
```

Arguments:

- Build type: `Build`, `Clean`, `Rebuild`
- Platform: `Win32`, `x64`, `all`, `No32bit`
- Configuration: `Debug`, `Release`, `LLVMDebug`, `LLVMRelease`, `all`

Output layout:

- Final binaries: `target\<Platform>\<Configuration>\`
- Intermediate files: `target\<Platform>\<Configuration>\obj\`

## License

General Public License v3.0
