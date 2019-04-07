# FAT32 File System

## Overview
User space shell that interprets a FAT32 file system image.

## Functionality
* Opens FAT32 image.
* Navigate the FAT32 image.
* Closes FAT32 image.
* Prints out boot information such as:
	* BPB\_BytsPerSec
	* BPB\_SecPerClus
	* BPB\_RsvdSecCnt
	* BPB\_NumFats
	* BPB\_FATSz32
* Print the attributes and starting cluster number of files and directories.
* Retrieves a file from the FAT32 image and places it in your current working directory.
* Retrieves a file from the current working directory and places it in your FAT32 image.

## What I Learned
* How the boot sector is structured and the importance of the information.
* How individual files are structured in the FAT32 file system.
* What happens when you "delete" a file.
* How directories are structured and the methods in maneuvering them.
* How to extract files from and inject files into the FAT32 image file.
