//4348.005
//Project 2
//Group 25 - members and contributions:
//  Wasif Reaz (wxr190002):
//      add_free_block()
//      get_free_block()
//      init_fs()
//      report
//      documentation
//  Andrew Sen (ats190003):
//      main()
//      generecized fill_an_inode_and_write()
//      write_superblock()
//      write_dir_entry()
//      constants for inode flags
//      Makefile
//      setup GitHub repo, git, .gitignore

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>


#define BLOCK_SIZE 1024
#define INODE_SIZE 64

//***i-node flags***//
    #define IALLOC  0b1000000000000000  //file is allocated

    //file types - pick one
    #define IPLAINF 0b0                 //plain
    #define IDIRF   0b0100000000000000  //directory
    #define ICHARF  0b0010000000000000  //char special file
    #define IBLOCKF 0b0110000000000000  //block special file

    //file sizes - pick one
    #define ISMALL  0b0
    #define IMED    0b0000100000000000
    #define ILONG   0b0001000000000000
    #define ILLONG  0b0001100000000000

    #define ISUID   0b0000010000000000  //setuid on execute
    #define ISGID   0b0000001000000000  //setgid on execute

    //is this advisable? might remove if not actually convenient
    #define IOP(x)  ((x) << 6)          //owner permissions; x is a number 0-7
    #define IGP(x)  ((x) << 3)          //group permissions; x is a number 0-7
    #define IWP(x)  (x)                 //world permissions; x is a number 0-7

#define MAX_SIZE_SMALL (9 * BLOCK_SIZE)
#define MAX_SIZE_MED   (9 * (BLOCK_SIZE / sizeof(int)) * BLOCK_SIZE)
#define MAX_SIZE_LONG  (9 * pow(BLOCK_SIZE / sizeof(int), 2) * BLOCK_SIZE)
#define MAX_SIZE_LLONG (9 * pow(BLOCK_SIZE / sizeof(int), 3) * BLOCK_SIZE)


typedef struct
{
    int isize;
    int fsize;
    int nfree;
    unsigned int free[200];
    char flock;
    char ilock;
    char fmod;
    unsigned int time;
} superblock_type;

typedef struct
{
    unsigned short flags;
    unsigned short nlinks;
    unsigned int uid;
    unsigned int gid;
    unsigned int size0;
    unsigned int size1;
    unsigned int addr[9];
    unsigned int actime;
    unsigned int modtime;
} inode_type;

typedef struct
{
    unsigned int inode;
    char filename[28];
} dir_type; // 32 Bytes long


//function declarations
int open_fs(char*);
void inode_writer(int, inode_type);
inode_type inode_reader(int, inode_type);
void fill_an_inode_and_write(inode_type*, int, short);
int add_free_block(int);
int get_free_block();
int write_dir_entry(int, dir_type);
void write_superblock();
void initfs(int, int);
int get_next_inum();
void free_inode(int);
void allocate_free_blocks(inode_type*, int, int);
void deallocate_blocks(inode_type*);
void rm(char*);
int main();


//globals
superblock_type superBlock;
inode_type root;
int fd = -1;


// opens the specified file and returns a file descriptor
// returns -1 on failure, 1 on success
int open_fs(char *file_name)
{
    fd = open(file_name, O_RDWR | O_CREAT, 0600);

    if (fd == -1)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

// Function to write inode
void inode_writer(int inum, inode_type inode)
{
    lseek(fd, 2 * BLOCK_SIZE + (inum - 1) * INODE_SIZE, SEEK_SET);
    write(fd, &inode, sizeof(inode));
}

// Function to read inodes
inode_type inode_reader(int inum, inode_type inode)
{
    lseek(fd, 2 * BLOCK_SIZE + (inum - 1) * INODE_SIZE, SEEK_SET);
    read(fd, &inode, sizeof(inode));
    return inode;
}

// Function to write inode number after filling some fields
// arguments:
//      inode: points to the inode to store data in
//      inum: inumber to associate inode with
//      flags: flags to be assigned to inode
void fill_an_inode_and_write(inode_type *inode, int inum, short flags)
{
    inode->flags = flags;
    inode->actime = time(NULL);
    inode->modtime = time(NULL);
    inode->size0 = inode->size1 = 0;    
    inode_writer(inum, *inode);
}

// Gets the earliest available inum, starting from 1, based on
// whether an inode's IALLOC flag is unset
// on success, returns valid inum
// on failure (no inums left), returns -1
int get_next_inum()
{
    int inum;
    inode_type inode;
    int num_inums = superBlock.isize * BLOCK_SIZE / sizeof(inode_type);

    for (inum = 1; inum <= num_inums; inum++)
    {
        inode = inode_reader(inum, inode);

        if ((inode.flags & IALLOC) == 0)
        {
            return inum;
        }
    }
    
    return -1;
}

// Frees an inode by unsetting inode's IALLOC flag
// arguments:
//      inum: number of inode to free
void free_inode(int inum)
{
    inode_type inode;
    inode = inode_reader(inum, inode);
    if ((inode.flags & IALLOC) == IALLOC)
    {
        inode.flags -= IALLOC;
    }
    inode_writer(inum, inode);
}

// Adds the designated block number to the free array
// writes current free array to a block if free array is full
// arguments:
//      blocknum: number of block to add to free array
// returns -1 on failure, 1 on success
int add_free_block(int blocknum)
{
    if (blocknum <= superBlock.isize + 1 || blocknum >= superBlock.fsize)
    {
        return -1;
    }
    if (superBlock.nfree == 200)
    {
        lseek(fd, BLOCK_SIZE * blocknum, SEEK_SET);
        write(fd, &superBlock.nfree, sizeof(int));
        write(fd, superBlock.free, 200 * sizeof(int));
        superBlock.nfree = 0;
    }
    superBlock.free[superBlock.nfree] = blocknum;
    superBlock.nfree++;
    superBlock.time = time(NULL);
    return 1;
}

// Gets the next avaialable block number from the free array
// Reads in the next chain of free blocks if free array is empty
// returns -1 on failure, 1 on success
int get_free_block()
{
    superBlock.nfree--;
    int blocknum = superBlock.free[superBlock.nfree];
    if (blocknum == 0)
    {
        return -1;
    }
    else if (superBlock.nfree == 0)
    {
        lseek(fd, BLOCK_SIZE * blocknum, SEEK_SET);
        read(fd, &superBlock.nfree, sizeof(int));
        read(fd, superBlock.free, 200 * sizeof(int));
    }
    superBlock.time = time(NULL);
    return blocknum;
}

// Function to allocate a designated number of free blocks for a given inode
// arguments:
//      inode: pointer to an existing inode
//             inode should already have relevant fields initialized,
//              including size0 and size1
//      inum: number of inode
//      num_blocks: number of data blocks required by the file
void allocate_free_blocks(inode_type *inode, int inum, int num_blocks)
{
    if ((inode->flags & ILLONG) == ISMALL)
    {
        int i;
        for (i = 0; i < 9 && i < num_blocks; i++)
        {
            inode->addr[i] = get_free_block();
        }

        inode_writer(inum, *inode);
        return;
    }
    else if ((inode->flags & ILLONG) == IMED)
    {
        int i;
        int j = 0;

        for (i = 0; i < 9; i++)
        {
            inode->addr[i] = get_free_block();
            
            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                if (i * (BLOCK_SIZE / sizeof(int))
                    + j >= num_blocks)
                {
                    inode_writer(inum, *inode);
                    return;
                }

                int DBid = get_free_block();
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                write(fd, &DBid, sizeof(int));\
            }
        }
    }
    else if ((inode->flags & ILLONG) == ILONG)
    {
        int i;
        int j = 0;
        int k = 0;

        for (i = 0; i < 9; i++)
        {
            inode->addr[i] = get_free_block();

            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                int SIBid = get_free_block();
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                write(fd, &SIBid, sizeof(int));

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    if (i * pow(BLOCK_SIZE / sizeof(int), 2)
                        + j * (BLOCK_SIZE / sizeof(int))
                        + k >= num_blocks)
                    {
                        inode_writer(inum, *inode);
                        return;
                    }

                    int DBid = get_free_block();
                    lseek(fd, SIBid * BLOCK_SIZE + k * sizeof(int), SEEK_SET);
                    write(fd, &DBid, sizeof(int));
                }
            }
        }
    }
    else
    {
        int i;
        int j = 0;
        int k = 0;
        int l = 0;

        for (i = 0; i < 9; i++)
        {
            inode->addr[i] = get_free_block();

            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                int DIBid = get_free_block();
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                write(fd, &DIBid, sizeof(int));

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    int SIBid = get_free_block();
                    lseek(fd, DIBid * BLOCK_SIZE + k * sizeof(int), SEEK_SET);
                    write(fd, &SIBid, sizeof(int));

                    for (l = 0; l < BLOCK_SIZE / sizeof(int); l++)
                    {
                        if (i * pow(BLOCK_SIZE / sizeof(int), 3)
                            + j * pow(BLOCK_SIZE / sizeof(int), 2)
                            + k * (BLOCK_SIZE / sizeof(int))
                            + l >= num_blocks)
                        {
                            inode_writer(inum, *inode);
                            return;
                        }

                        int DBid = get_free_block();
                        lseek(fd, SIBid * BLOCK_SIZE + l * sizeof(int), sizeof(int));
                        write(fd, &DBid, sizeof(int));
                    }
                }
            }
        }
    }

    return;
}

// Function to deallocate a designated number of blocks for a given inode
// This function DOES NOT change the size flags or indirect-block scheme of the inode
// Only update size0 and size1 AFTER calling this function,
//      because this function uses the size to calculate where to start deallocating
// arguments:
//      inode: pointer to an existing inode
//      inum: number of inode
void deallocate_blocks(inode_type *inode)
{
    unsigned long size = (unsigned long)inode->size0 << (sizeof(int) * 8);
    size += inode->size1;
    int cur_blocks = (int)ceil(size / (double) BLOCK_SIZE);

    if ((inode->flags & ILLONG) == ISMALL)
    {
        int i;
        for (i = 0; i < 9 && i < cur_blocks; i++)
        {
            add_free_block(inode->addr[i]);
        }
    }
    else if ((inode->flags & ILLONG) == IMED)
    {
        int i;
        int j = 0;

        for (i = 0; i < 9; i++)
        {
            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                if (i * (BLOCK_SIZE / sizeof(int))
                    + j == cur_blocks)
                {
                    return;
                }

                int block_num;
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                read(fd, &block_num, sizeof(int));
                add_free_block(block_num);
            }

            add_free_block(inode->addr[i]);
        }
    }
    else if ((inode->flags & ILLONG) == ILONG)
    {
        int i;
        int j = 0;
        int k = 0;

        for (i = 0; i < 9; i++)
        {
            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                int SIBid;
                read(fd, &SIBid, sizeof(int));

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    if (i * pow(BLOCK_SIZE / sizeof(int), 2)
                        + j * (BLOCK_SIZE / sizeof(int))
                        + k == cur_blocks)
                    {
                        return;
                    }

                    int block_num;
                    lseek(fd, SIBid * BLOCK_SIZE + k * sizeof(int), SEEK_SET);
                    read(fd, &block_num, sizeof(int));
                    add_free_block(block_num);
                }

                add_free_block(SIBid);
            }

            add_free_block(inode->addr[i]);
        }
    }
    else
    {
        int i;
        int j = 0;
        int k = 0;
        int l = 0;

        for (i = 0; i < 9; i++)
        {
            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                int DIBid;
                read(fd, &DIBid, sizeof(int));

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    lseek(fd, DIBid * BLOCK_SIZE + k * sizeof(int), SEEK_SET);
                    int SIBid;
                    read(fd, &SIBid, sizeof(int));

                    for (l = 0; l < BLOCK_SIZE / sizeof(int); l++)
                    {
                        if (i * pow(BLOCK_SIZE / sizeof(int), 3)
                            + j * pow(BLOCK_SIZE / sizeof(int), 2)
                            + k * (BLOCK_SIZE / sizeof(int))
                            + l == cur_blocks)
                        {
                            return;
                        }

                        int block_num;
                        lseek(fd, SIBid * BLOCK_SIZE + l * sizeof(int), SEEK_SET);
                        read(fd, &block_num, sizeof(int));
                        add_free_block(block_num);
                    }

                    add_free_block(SIBid);
                }

                add_free_block(DIBid);
            }

            add_free_block(inode->addr[i]);
        }
    }
}

// writes an enttry into the specified directory
// arguments:
//      dir_inum: inum of directory to write into
//      entry: entry to be written
// returns -1 on failure, 1 on success
int write_dir_entry(int dir_inum, dir_type entry)
{
    inode_type dir;
    dir = inode_reader(dir_inum, dir);
    if ((dir.flags & IBLOCKF) != IDIRF)
    {
        //dir_inum doesn't refer to a directory
        return -1;
    }

    // assuming root is only directory
    // and that root will be small
    int new_block_count = (dir.size1 + sizeof(dir_type)) / BLOCK_SIZE;
    if (new_block_count > dir.size1 / BLOCK_SIZE){
        if (new_block_count >= 9){
            printf("ERROR: cannot add more entries to root");
            return -1;
        }
        dir.addr[new_block_count] = get_free_block();
    }

    int i;
    for (i = 0; i < 9; i++){
        int j;
        for (j = 0; j < BLOCK_SIZE / sizeof(dir_type); j++){
            dir_type file;
            lseek(fd, root.addr[i] * BLOCK_SIZE + j * sizeof(dir_type), SEEK_SET);
            read(fd, &file, sizeof(dir_type));

            if (file.inode == 0 || i * BLOCK_SIZE + j * sizeof(dir_type) == dir.size1){
                lseek(fd, root.addr[i] * BLOCK_SIZE + j * sizeof(dir_type), SEEK_SET);
                write(fd, &entry, sizeof(dir_type));

                dir.size1 += sizeof(dir_type);

                dir.actime = time(NULL);
                dir.modtime = time(NULL);
                inode_writer(dir_inum, dir);

                if (dir_inum == 1){
                    root = inode_reader(1, root);
                }
                return 1;
            }
        }
    }

    return 1;
}

// writes the superblock to block 1
void write_superblock()
{
    if (fd != -1)
    {
        lseek(fd, BLOCK_SIZE, SEEK_SET);
        write(fd, &superBlock, sizeof(superblock_type));
    }
}

// initializes the file system
// arguments:
//      n1: total number of blocks
//      n2: number of inode blocks
void init_fs(int n1, int n2)
{
    superBlock.fsize = n1;
    superBlock.isize = n2;
    superBlock.nfree = 0;

    add_free_block(0);
    int i;
    for (i = superBlock.isize + 2; i < superBlock.fsize; i++)
    {
        add_free_block(i);
    }

    root.addr[0] = get_free_block();
    fill_an_inode_and_write(&root, 1, IALLOC | IDIRF | ISMALL);
    dir_type entry;
    entry.inode = 1;
    strcpy(entry.filename, ".");
    write_dir_entry(1, entry);
    strcpy(entry.filename, "..");
    write_dir_entry(1, entry);
}

// Function to copy a real file into virtual file system
void cpin(const char *int_fname , const char *ext_fname){
//printf("cpin start\n");
	char ReadBuffer[BLOCK_SIZE];
	int sfd;
	unsigned long sfileSize = 0;
	unsigned long TotalBytesRead = 0;
	int CurrentBytesRead = 0;

	FILE *fp = open(int_fname, "r");
	fseek(fp, 0, SEEK_END);
	sfileSize = ftell(fp);
	fclose(fp);

	//printf("\nfile size is %ld\n", sfileSize);

	//open external file
	if ((sfd = open(int_fname, O_RDONLY)) == -1)
	{
		printf("\nCan't open source file %s\n", int_fname);
		return -1;
	}

	//printf("\nint_fname file open success %s\n", int_fname);

	unsigned int inumber = get_next_inum();
	//printf("inode no %d\n", inumber);
	if (inumber < 2) // 1 for root
	{
		printf("Error : ran out of inodes \n");
		return;
	}

	char* partialPath = strtok(ext_fname, "/");

	char* FinalFileName = (char *)malloc(sizeof(char) * 12);

	while (partialPath)
	{
		if (strlen(partialPath) < 11)
			strcpy(FinalFileName, partialPath);
		else
			strncpy(FinalFileName, partialPath, 11);

		partialPath = strtok(NULL, "/");
	}

	//printf("file name is %s\n", FinalFileName);

	//new file - modified V6 file system
	new_dir.inode = inumber;
	if (strlen(FinalFileName) < 12)
	{
		memcpy(new_dir.filename, FinalFileName, strlen(FinalFileName));
	}
	else
	{
		memcpy(new_dir.filename, ext_fname, 11);
	}

	//New file - inode struct
	inoderef.flags = plainfile | allocatedinode | 000777;
	inoderef.nlinks = 1;
	inoderef.uid = '0';
	inoderef.gid = '0';
	inoderef.size1 = 0;


	if (sfileSize <= 27648)
	{
		//small file
		int index = 0;
		while (1)
		{
			memset(ReadBuffer, 0x00, BLOCK_SIZE);
			if ((CurrentBytesRead = read(sfd, ReadBuffer, BLOCK_SIZE)) != 0 && index < 27)
			{
				int NewBlockNum = allocatedatablock();
				//printf("Blk_no is %d\n", NewBlockNum);
				blockwriterchar(ReadBuffer, NewBlockNum);
				inoderef.addr[index] = NewBlockNum;
				TotalBytesRead += CurrentBytesRead;
				index++;
				//printf("Small file - current read %d, totalFileSize now %ld\n", CurrentBytesRead, TotalBytesRead);
			}
			else
			{
				inoderef.size1 = TotalBytesRead;
				//printf("Small file copied \n \n");
				//printf("External file Size %lu, Copied file size %lu\n", sfileSize, TotalBytesRead);
				break;
			}
		}
	}
	else
	{
		//large file
		inoderef.flags = largefile | plainfile | allocatedinode | 000777;
		int index = 0;
		for (; index < 26; index++)
		{
			memset(ReadBuffer, 0x00, BLOCK_SIZE);
			CurrentBytesRead = read(sfd, ReadBuffer, BLOCK_SIZE);
			int NewBlockNum = allocatedatablock();
			blockwriterchar(ReadBuffer, NewBlockNum);
			inoderef.addr[index] = NewBlockNum;
			TotalBytesRead += CurrentBytesRead;
			//printf("large file - current read %d, totalFileSize now %ld\n", CurrentBytesRead, TotalBytesRead);
		}


		//triple indirect block allocation
		inoderef.addr[index] = allocatedatablock();
		//printf("allocate data block for 26th addr %d\n", inoderef.addr[index]);

		int x, y, z = 0;
		unsigned int array1[256];
		unsigned int array2[256];
		unsigned int array3[256];

		for (x = 0; x < 256; x++)
		{
			array1[x] = 0;
			array2[x] = 0;
			array3[x] = 0;
		}

		int finish = 0;

		for (x = 0; x < 256; x++)
		{
			if ((CurrentBytesRead = read(sfd, ReadBuffer, BLOCK_SIZE)) != 0)
			{
				TotalBytesRead += CurrentBytesRead;
				//printf("large file - just read - current read %d, totalFileSize now %ld\n", CurrentBytesRead, TotalBytesRead);
				array1[x] = allocatedatablock();
				//printf("allocate data block for array1 %d - %d\n", x, array1[x]);
				for (y = 0; y < 256; y++)
				{
					array2[y] = allocatedatablock();
					//printf("allocate data block for array2 %d - %d\n", y, array2[y]);
					for (z = 0; z < 256; z++)
					{
						array3[z] = allocatedatablock();
						//printf("allocate data block for array3 %d - %d\n", z, array3[z]);
						blockwriterchar(ReadBuffer, array3[z]);
						memset(ReadBuffer, 0x00, BLOCK_SIZE);
						if ((CurrentBytesRead = read(sfd, ReadBuffer, BLOCK_SIZE)) != 0)
						{
							TotalBytesRead += CurrentBytesRead;
							//Read success
							//printf("large file - just read - current read %d, totalFileSize now %ld\n", CurrentBytesRead, TotalBytesRead);
						}
						else
						{
							inoderef.size1 = TotalBytesRead;
							finish = 1;
							break;
						}
					}
					write_superblock(array3, array2[y]);
					if (finish)
						break;
				}
				write_superblock(array2, array1[x]);
				if (finish)
					break;
			}
			else
			{
				inoderef.size1 = TotalBytesRead;
				finish = 1;
				//printf("Large file copied\n");
				//printf("External file Size %lu, Copied file size %lu\n\n", sfileSize, TotalBytesRead);
				break;
			}

		}
		write_superblock(array1, inoderef.addr[26]);

	}

	inoderef.actime[0] = 0;
	inoderef.modtime[0] = 0;
	inoderef.modtime[1] = 0;

	write_to_inode(inoderef, (inumber - 1));
	lseek(fd, 2 * BLOCK_SIZE, 0);
	read(fd, &inoderef, INODE_SIZE);
	inoderef.nlinks++;
	write_to_directory(inoderef, new_dir);
	printf("\nFile copied\n\n");
	printf("External file Size %lu, Copied file size %lu\n\n", sfileSize, TotalBytesRead);
    
	return 0;   
}

// removal of a directory entry is signified by making inum field 0
void cpout(const char *int_fname , const char *ext_fname){
    char *newPartialPath = (char*)malloc(strlen(int_fname));
        strcpy(newPartialPath, int_fname);
        char* sourcePartialPath = strtok(newPartialPath, "/");
        char* FileName = (char *)malloc(sizeof(char) * 256);

        while (sourcePartialPath)
        {
            if (strlen(sourcePartialPath) < 255)
                strcpy(FileName , sourcePartialPath);
            else
                strncpy(FileName , sourcePartialPath, 255);

            sourcePartialPath = strtok(NULL, "/");
        }



        int dfd;
        //open or create external file(target file) for read and write
        if ((dfd = open(ext_fname, O_RDWR | O_CREAT, 0600)) == -1)
        {
            printf("\nError opening file: %s\n", ext_fname);
            return;
        }

        //CheckParentExists(int_fname)

        inode_Struct TempStruct;
        TempStruct = read_from_inode(1);

        int InodeToCheck = 1;
        int found = 0;
        int end = 0;
        char ReadBuffer[BLOCK_SIZE];

        int AddressArrayindex = 0;
        for (; AddressArrayindex < 27; AddressArrayindex++)
        {
            unsigned int dataBlock = TempStruct.addr[AddressArrayindex];
            //printf("dataBlock  %d\n", dataBlock );
            lseek(fd, dataBlock * BLOCK_SIZE, 0);

            int index = 0;
            for (; index < 16; index++)
            {
                dirStruct TempReadDir;

                read(fd, &TempReadDir, sizeof(dirStruct));
                //printf("filename check %s, %s\n", TempReadDir.filename, int_fname);
                if (strcmp(TempReadDir.filename, FileName) == 0)
                {
                    InodeToCheck = TempReadDir.inode;
                    found = 1;
                    break;
                }
                else if (strcmp(TempReadDir.filename, "") == 0)
                {
                    end = 0;
                    break;
                }
            }
            if (found || end)
                break;
        }

        //printf("inode to check %d\n", InodeToCheck);

        TempStruct = read_from_inode(InodeToCheck);

        //printf("Read from Inode %d", TempStruct.addr[0]);

        unsigned int SSize = TempStruct.size1;
        unsigned int TotalReadBytes = 0;
        unsigned int ReadBytes = 0;
        int LargeFile = 0;
        int finish = 0;

        unsigned short compare = largefile | plainfile | allocatedinode | 000777;

        if (TempStruct.flags == compare)
        {
            LargeFile = 1;
        }

        int sfd = 0;
        if ((sfd = open(ext_fname, O_RDWR | O_CREAT, 0600)) == -1)
        {
            printf("\n open() call for the path [%s] failed with error [%s]\n", dest, strerror(errno));
            return;
        }

        //while (TotalReadBytes < SSize)
        {
            int i = 0;
            for (; i < 26; i++)
            {
                int written = 0;

                if (TempStruct.addr[i] < 2)
                {
                    finish = 1;
                    break;
                }

                lseek(fd, TempStruct.addr[i] * BLOCK_SIZE, 0);
                ReadBytes = read(fd, ReadBuffer, BLOCK_SIZE);


                if ((written = write(sfd, ReadBuffer, BLOCK_SIZE)) < 1)
                {
                    printf("written %d", written);
                }
                TotalReadBytes += ReadBytes;
                if (ReadBytes < 1 /*|| TotalReadBytes >= SSize*/)
                {
                    finish = 1;
                    break;
                }
            }

            if (LargeFile)
            {
                int x, y, z = 0;
                unsigned int array1[256];
                unsigned int array2[256];
                unsigned int array3[256];

                for (x = 0; x < 256; x++)
                {
                    array1[x] = 0;
                    array2[x] = 0;
                    array3[x] = 0;
                }

                lseek(fd, TempStruct.addr[26] * BLOCK_SIZE, 0);
                read(fd, array1, BLOCK_SIZE);

                for (x = 0; x < 256; x++)
                {
                    if (array1[x] < 2)
                    {
                        finish = 1;
                        break;
                    }

                    lseek(fd, array1[x] * BLOCK_SIZE, 0);
                    read(fd, array2, BLOCK_SIZE);

                    for (y = 0; y < 256; y++)
                    {
                        if (array2[y] < 2)
                        {
                            finish = 1;
                            break;
                        }
                        lseek(fd, array2[y] * BLOCK_SIZE, 0);
                        read(fd, array3, BLOCK_SIZE);

                        for (z = 0; z < 256; z++)
                        {
                            if (array3[z] < 2)
                            {
                                finish = 1;
                                break;
                            }
                            lseek(fd, array3[z] * BLOCK_SIZE, 0);
                            ReadBytes = read(fd, ReadBuffer, BLOCK_SIZE);

                            int written = 0;
                            if ((written = write(sfd, ReadBuffer, BLOCK_SIZE)) < 1)
                            {
                                printf("written %d", written);
                            }

                            if (ReadBytes < 1)
                            {
                                finish = 1;
                                break;
                            }
                        }
                        if (finish)
                            break;
                    }
                    if (finish)
                        break;
                }
            }
            printf("\nFile copied\n\n");
            close(sfd);
        }   
}
// arguments:
//     filename: name of virtual file to remove
void rm(char *filename){
    int i;
    int skipped = 0;
    for (i = 0; i < 9; i++){
        lseek(fd, root.addr[i] * BLOCK_SIZE, SEEK_SET);

        int j;
        for (j = 0; j < BLOCK_SIZE / sizeof(dir_type); j++){
            if (i * BLOCK_SIZE + (j - skipped) * sizeof(dir_type) >= root.size1){
                printf("ERROR: Could not find %s\n", filename);
                return;
            }

            dir_type file;
            read(fd, &file, sizeof(dir_type));

            if (file.inode == 0){
                skipped++;
                continue;
            }

            if (!strcmp(&filename[1], file.filename)){
                root.size1 -= sizeof(dir_type);
                inode_writer(1, root);
                file.inode = 0;
                lseek(fd, root.addr[i] * BLOCK_SIZE + j * sizeof(dir_type), SEEK_SET);
                write(fd, &file, sizeof(dir_type));

                inode_type inode;
                inode = inode_reader(file.inode, inode);
                deallocate_blocks(&inode);
                free_inode(file.inode);
                return;
            }
        }
    }

    printf("ERROR: Could not find %s\n", filename);
}

// main function
// includes terminal command logic
int main()
{
    char cmd[10];
    char fname[256];

    while (1){
        scanf("%s", cmd);

        if (!strcmp(cmd, "q"))
        {
            write_superblock();
            exit(0);
        }
        else if (!strcmp(cmd, "initfs"))
        {
            char new_fname[256];
            unsigned new_fsize;
            unsigned new_isize;

            if (scanf("%s %u %u", new_fname, &new_fsize, &new_isize) < 3)
            {
                printf("ERROR: 1 or more arguments were the wrong type\n");
                
            }
            else if (new_isize >= new_fsize)
            {
                printf("ERROR: fsize is too small\n");
            }
            else
            {
                write_superblock();

                int oldfd = fd;

                if (open_fs(new_fname) < 0)
                {
                    printf("ERROR: Failed to open %s. Reverting...\n", new_fname);
                    fd = oldfd;
                }
                else
                {
                    close(oldfd);
                    strcpy(fname, new_fname);
                    init_fs(new_fsize, new_isize);
                }
            }
        }
        else if (!strcmp(cmd, "open"))
        {
            char new_fname[256];

            if (scanf("%s", new_fname) < 1)
            {
                printf("ERROR: Could not read name of file system\n");
            }
            else
            {
                write_superblock();

                int oldfd = fd;
                fd = open(new_fname, O_RDWR, 0600);

                if (fd < 0)
                {
                    printf("ERROR: Failed to open %s. Reverting...\n", new_fname);
                    fd = oldfd;
                }
                else
                {
                    close(oldfd);
                    strcpy(fname, new_fname);
                    lseek(fd, BLOCK_SIZE, SEEK_SET);
                    read(fd, &superBlock, sizeof(superblock_type));
                    root = inode_reader(1, root);
                }
            }
        }
        else if (!strcmp(cmd, "cpin"))
        {
            char ext_fname[256];
            char int_fname[29];

            if (fd == -1){
                printf("ERROR: no file system has been opened\n");
            }
            else if (scanf("%s %s", ext_fname, int_fname) < 2)
            {
                printf("ERROR: 1 or more arguments were the wrong type\n");
            }
            else if (int_fname[0] != '/'){
                printf("ERROR: missing '/'\n");
            }
            else
            {
                cpin (int_fname, ext_fname);
            }
        }

        else if (!strcmp(cmd, "cpout"))
        {
            char int_fname[29];
            char ext_fname[256];

            if (fd == -1){
                printf("ERROR: no file system has been opened\n");
            }
            else if (scanf("%s %s", int_fname, ext_fname) < 2)
            {
                printf("ERROR: 1 or more arguments were the wrong type\n");
            }
            else if (int_fname[0] != '/'){
                printf("ERROR: missing '/'\n");
            }
            else
            {
                cpout (int_fname, ext_fname);
            }
        }
        else if (!strcmp(cmd, "rm"))
        {
            char rm_fname[29];

            if (fd == -1){
                printf("ERROR: no file system has been opened\n");
            }
            else if (scanf("%s", rm_fname) < 1)
            {
                printf("ERROR: Could not read name of file\n");
            }
            else if (rm_fname[0] != '/'){
                printf("ERROR: missing '/'\n");
            }
            else
            {
                rm(rm_fname);
            }
        }
        else
        {
            printf("ERROR: %s is not a command\n", cmd);
        }

        while ((getchar()) != '\n');
    }
