# Copilot Instructions

This repository contains a toy file system implementation for educational purposes, part of the FSO (File System Organization) course at DI-FCT/UNL.

## Project Overview

The project implements a simplified UNIX-like file system with the following components:

- **fso-sh**: An interactive shell to browse and test the file system
- **fs**: Core file system operations (mount, create, mkdir, link, unlink, ls, debug)
- **disk**: Disk emulation layer with block-based I/O operations
- **bitmap**: Bitmap utilities for tracking free/used blocks

## Build Instructions

Use `make` to build the project:

```bash
make        # Build the fso-sh executable
make clean  # Clean build artifacts and regenerate dependencies
```

## Code Style

- Follow existing C code conventions used in the project
- Use POSIX-compliant C
- Include appropriate header guards in `.h` files
- Use descriptive function and variable names

## File System Structure

The file system has the following disk layout:
- Block 0: Super block (contains FS metadata)
- Block 1+: Bitmap for free/used block tracking
- After bitmap: Inode blocks (root directory is always inode 0)
- After inodes: Data blocks

## Key Constants

- `DISK_BLOCK_SIZE`: 1024 bytes
- `MAXFILENAME`: 62 characters
- `DIRBLOCK_PER_INODE`: 11 direct blocks per inode

## Testing

Test the file system using the provided disk images:
- `small.dsk`: Small test disk image
- `medium.dsk`: Medium-sized test disk image

Run the shell with: `./fso-sh <diskfile>`
