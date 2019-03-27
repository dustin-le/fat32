#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

FILE *fp; // Leave file pointer outside to keep it available outside of open_file function.

char BS_OEMName[8]; // Name string. Indicates what system formatted the volume.
int16_t BPB_BytsPerSec; // Count of bytes per sector. 
int8_t BPB_SecPerClus; // Number of sectors per allocation unit.
int16_t BPB_RsvdSecCnt; // Number of reserved sectors in the Reserved region of the volume starting at the first sector of the volume.
int8_t BPB_NumFATs; // The count of FAT data structures on the volume.
int16_t BPB_RootEntCnt; // Must be set to 0 for FAT32 volumes.
char BS_VolLab[11]; // Volume label. Matches the 11-byte volume label recorded in root directory.
int32_t BPB_FATSz32; // 32-bit count of sectors occupied by ONE FAT.
int32_t BPB_RootClus; // Set to the cluster number of the first cluster of the root directory, usually 2.

int32_t RootDirSectors = 0; // ((BPB_RootEntCnt * 32) + (BPB_BytsPerSec – 1)) / BPB_BytsPerSec
int32_t FirstDataSector = 0; // BPB_ResvdSecCnt + (BPB_NumFATs * FATSz) + RootDirSectors
int32_t FirstSectorofCluster = 0; // ((N – 2) * BPB_SecPerClus) + FirstDataSector, where N is any valid data cluster number.

int root; // Address of root directory.
int check; // Checks if currently in root directory (0 for no, 1 for yes).

struct __attribute__((__packed__)) DirectoryEntry {
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t Unused1[8];
	uint16_t DIR_FirstClusterHigh; // Always 0
	uint8_t Unused2[4];
	uint16_t DIR_FirstClusterLow;
	uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

// Finds the starting address of a block of data given the sector number corresponding to that data block.
int LBAToOffset(int32_t sector)
{
	return (( sector - 2 ) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);
}

// Given a logical block address, look up into the first FAT and return the logical block address of the block in the file. If there is no further blocks then return -1.
int16_t NextLB(uint32_t sector)
{
	uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
	int16_t val;
	fseek(fp, FATAddress, SEEK_SET);
	fread(&val, 2, 1, fp);
	return val;
}	

void open_file(char* filename)
{
	fp = fopen(filename, "r");

	if (fp == NULL)
	{
		printf("Error: File system image not found.\n\n");
	}	
	
	else
	{
		fseek(fp, 3, SEEK_SET); // Skip BS_jmpBoot
		fread(&BS_OEMName, 8, 1, fp);

		fread(&BPB_BytsPerSec, 2, 1, fp);

		fread(&BPB_SecPerClus, 1, 1, fp);

		fread(&BPB_RsvdSecCnt, 2, 1, fp);

		fseek(fp, 16, SEEK_SET); // Skip BPB_RsvdSecCrit
		fread(&BPB_NumFATs, 1, 1, fp);

		fread(&BPB_RootEntCnt, 2, 1, fp);

		fseek(fp, 36, SEEK_SET); // Skip to BPB_FATSz32
		fread(&BPB_FATSz32, 4, 1, fp);

		fseek(fp, 44, SEEK_SET); // Skip BPB_ExtFlags and BPB_FSVer
		fread(&BPB_RootClus, 4, 1, fp);
		
		fseek(fp, 71, SEEK_SET); // Skip to BS_VolLab
		fread(&BS_VolLab, 11, 1, fp);

		root = (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);  //0x100400
		
		check = 1;
		fseek(fp, root, SEEK_SET);
		fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
	}
}

// ls implementation. Prints out file information read from open_file function. 
void ls()
{
	int i;
	for (i = 0; i < 16; i++)
	{
		//Ignores files with attributes that are not 0x01, 0x10, or 0x20, as those files are hidden.
		if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
		{
			char name[12];
			memset(&name, 0, 12);

			strncpy(name, dir[i].DIR_Name, 11);
			printf("%s %d %x\n", name, dir[i].DIR_FirstClusterLow, dir[i].DIR_Attr);
		}
	}	
}

void cd(char* directory)
{
	/*
	int i;
	for (i = 0; i < 16; i++)
	{
			if (!strcmp(dir[i].DIR_Name, directory))
			{
				printf("Match!\n");
			}	
	}	
	*/
	fseek(fp, LBAToOffset(dir[8].DIR_FirstClusterLow), SEEK_SET);
	fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
}	

void stat(char* name)
{
	int i;
	for (i = 0; i < 16; i++)
	{
		if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
		{
			if (!strcmp(name, dir[i].DIR_Name))
			{
				printf("%s: %x || %d\n", name, dir[i].DIR_Attr, dir[i].DIR_FirstClusterLow);
				break;
			}	
		}	
	}	
}	

int main()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

		if (token[0] != NULL)
		{
			if (!strcmp(token[0], "open"))
			{
				if (token[1] == NULL)
				{
					printf("Specify a file to open.\n\n");
				}	

				else
				{
					open_file(token[1]);
				}	
			}

			if (!strcmp(token[0], "exit") || !strcmp(token[0], "quit"))
			{
				exit(0);
			}	

			if (!strcmp(token[0], "info"))
			{
				printf("\t\t HEX  ||  DEC\n");
				printf("BPB_BytsPerSec: %4x  || %4d\n", BPB_BytsPerSec, BPB_BytsPerSec);
				printf("BPB_SecPerClus: %4x  || %4d\n", BPB_SecPerClus, BPB_SecPerClus);
				printf("BPB_RsvdSecCnt: %4x  || %4d\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
				printf("BPB_NumFATs: %7x  || %4d\n", BPB_NumFATs, BPB_NumFATs);
				printf("BPB_FATSz32: %7x  || %4d\n\n", BPB_FATSz32, BPB_FATSz32);
			}	

			if (!strcmp(token[0], "ls"))
			{
				ls();
			}

			if (!strcmp(token[0], "cd"))
			{
				if (token[1] != NULL)
				{
					cd(token[1]);
				}	
			}	

			if (!strcmp(token[0], "stat"))
			{
				if (token[1] != NULL)
				{
					stat(token[1]);
				}	
			}	

			if (!strcmp(token[0], "test"))
			{
				int i;
				for (i = 0; i < 16; i++)
				{
							if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
							{
								char name[12];
								memset(&name, 0, 12);

								strncpy(name, dir[i].DIR_Name, 11);
								printf("%s: %x\n", name, dir[i].DIR_Attr);
							}	
				}	

				/*
				int i, test;
				for (i = 0; i < 16; i++)
				{
					if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
					{
							char name[12];
							memset(&name, 0, 12);

							strncpy(name, dir[i].DIR_Name, 11);
							test = NextLB(dir[i].DIR_FirstClusterLow);
							printf("%s: %d\n", name, test);
					}	
				}	
				*/
				/*
				int i;
				for (i = 0; i < 16; i++)
				{
					if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
					{
						printf("%s\n", dir[i].DIR_Name);
					}
				}	
				*/
			}	
		}
		free( working_root );
  }
  return 0;
}
