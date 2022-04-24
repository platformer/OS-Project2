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
int allocate_free_blocks(inode_type*, int, int);
int deallocate_blocks(inode_type*);
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
    int num_inums = superBlock.isize / sizeof(inode_type);

    for (inum = 1; inum <= num_inums; inum++)
    {
        inode = inode_reader(inum, inode);
        if (inode.flags & IALLOC == 0)
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
    if (inode.flags & IALLOC == IALLOC)
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
int allocate_free_blocks(inode_type *inode, int inum, int num_blocks)
{
    if (inode->flags & ILLONG == ISMALL)
    {
        int i;
        for (i = 0; i < 9 && i < num_blocks; i++)
        {
            inode->addr[i] = get_free_block();
        }

        inode_writer(inum, *inode);
    }
    else if (inode->flags & ILLONG == IMED)
    {
        int i;
        int j = 0;

        for (i = 0; i < 9; i++)
        {
            inode->addr[i] = get_free_block();
            lseek(fd, inode->addr[i] * BLOCK_SIZE, SEEK_SET);

            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                if (i * (BLOCK_SIZE / sizeof(int))
                    + j >= num_blocks)
                {
                    inode_writer(inum, *inode);
                    return 1;
                }

                int DBid = get_free_block();
                write(fd, &DBid, sizeof(int));\
            }
        }
    }
    else if (inode->flags & ILLONG == ILONG)
    {
        int i;
        int j = 0;
        int k = 0;

        for (i = 0; i < 9; i++)
        {
            inode->addr[i] = get_free_block();

            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                int SIBid = get_free_block();
                write(fd, &SIBid, sizeof(int));
                lseek(fd, SIBid * BLOCK_SIZE, SEEK_SET);

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    if (i * pow(BLOCK_SIZE / sizeof(int), 2)
                        + j * (BLOCK_SIZE / sizeof(int))
                        + k >= num_blocks)
                    {
                        inode_writer(inum, *inode);
                        return 1;
                    }

                    int DBid = get_free_block();
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
                lseek(fd, inode->addr[i] * BLOCK_SIZE + j * sizeof(int), SEEK_SET);
                int DIBid = get_free_block();
                write(fd, &DIBid, sizeof(int));

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    lseek(fd, DIBid * BLOCK_SIZE + k * sizeof(int), SEEK_SET);
                    int SIBid = get_free_block();
                    write(fd, &SIBid, sizeof(int));
                    lseek(fd, SIBid * BLOCK_SIZE, sizeof(int));

                    for (l = 0; l < BLOCK_SIZE / sizeof(int); l++)
                    {
                        if (i * pow(BLOCK_SIZE / sizeof(int), 3)
                            + j * pow(BLOCK_SIZE / sizeof(int), 2)
                            + k * (BLOCK_SIZE / sizeof(int))
                            + l >= num_blocks)
                        {
                            inode_writer(inum, *inode);
                            return 1;
                        }

                        int DBid = get_free_block();
                        write(fd, &DBid, sizeof(int));
                    }
                }
            }
        }
    }

    return -1;
}

// Function to deallocate a designated number of blocks for a given inode
// This function DOES NOT change the size flags or indirect-block scheme of the inode
// Only update size0 and size1 AFTER calling this function,
//      because this function uses the size to calculate where to start deallocating
// arguments:
//      inode: pointer to an existing inode
//      inum: number of inode
int deallocate_blocks(inode_type *inode)
{
    long size = (long)inode->size0 << (sizeof(int) * 8);
    size += inode->size1;
    int cur_blocks = (int)ceil(size / (double) BLOCK_SIZE);

    if (inode->flags & ILLONG == ISMALL)
    {
        int i;
        for (i = 0; i < 9 && i < cur_blocks; i++)
        {
            add_free_block(inode->addr[i]);
        }
    }
    else if (inode->flags & ILLONG == IMED)
    {
        int i;
        int j = 0;

        for (i = 0; i < 9; i++)
        {
            lseek(fd, inode->addr[i] * BLOCK_SIZE, SEEK_SET);

            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                if (i * (BLOCK_SIZE / sizeof(int))
                    + j == cur_blocks)
                {
                    return;
                }

                int block_num;
                read(fd, &block_num, sizeof(int));
                add_free_block(block_num);
            }

            add_free_block(inode->addr[i]);
        }
    }
    else if (inode->flags & ILLONG == ILONG)
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
                lseek(fd, SIBid * BLOCK_SIZE, SEEK_SET);

                for (k = 0; k < BLOCK_SIZE / sizeof(int); k++)
                {
                    if (i * pow(BLOCK_SIZE / sizeof(int), 2)
                        + j * (BLOCK_SIZE / sizeof(int))
                        + k == cur_blocks)
                    {
                        return;
                    }

                    int block_num;
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
                    lseek(fd, SIBid * BLOCK_SIZE, SEEK_SET);

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

    return 1;
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
    if (dir.flags & IBLOCKF != IDIRF)
    {
        //dir_inum doesn't refer to a directory
        return -1;
    }
    // assuming root is only directort
    // and that root will be small
    lseek(fd, dir.addr[dir.size1 / BLOCK_SIZE] * BLOCK_SIZE + (dir.size1 % BLOCK_SIZE), SEEK_SET);
    write(fd, &entry, sizeof(dir_type));
    dir.size1 += sizeof(dir_type);

    dir.actime = time(NULL);
    dir.modtime = time(NULL);
    inode_writer(dir_inum, dir);
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

    fill_an_inode_and_write(&root, 1, IALLOC | IDIRF | ISMALL);
    allocate_free_blocks(&root, 1, 1);
    dir_type entry;
    entry.inode = 1;
    strcpy(entry.filename, ".");
    write_dir_entry(1, entry);
    strcpy(entry.filename, "..");
    write_dir_entry(1, entry);
}

void rm(char *filename){
    int i;
    for (i = 0; i < 9; i++){
        lseek(fd, root.addr[i] * BLOCK_SIZE, SEEK_SET);

        int j;
        for (j = 0; j < BLOCK_SIZE / sizeof(dir_type); j++){
            if (i * BLOCK_SIZE + j * sizeof(dir_type) >= root.size1){
                printf("ERROR: Could not find %s\n", filename);
                return;
            }

            dir_type file;
            read(fd, &file, sizeof(dir_type));

            if (!strcmp(&filename[1], file.filename)){
                root.size1 -= sizeof(dir_type);
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

                if (open_fs(new_fname) < 0)
                {
                    printf("ERROR: Failed to open %s. Reverting...", new_fname);
                }
                else
                {
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

                if (open_fs(new_fname) < 0)
                {
                    printf("ERROR: Failed to open %s. Reverting...", new_fname);
                }
                else
                {
                    strcpy(fname, new_fname);
                    lseek(fd, BLOCK_SIZE, SEEK_SET);
                    read(fd, &superBlock, sizeof(superblock_type));
                }
            }
        }
        else if (!strcmp(cmd, "cpin"))
        {
            char ext_fname[256];
            char int_fname[28];

            if (scanf("%s %s", ext_fname, int_fname) < 2)
            {
                printf("ERROR: 1 or more arguments were the wrong type\n");
            }
            else if (int_fname[0] != '/'){
                printf("ERROR: missing '/'\n");
            }
            else
            {
                // cpin code
            }
        }
        else if (!strcmp(cmd, "cpout"))
        {
            char int_fname[28];
            char ext_fname[256];

            if (scanf("%s %s", int_fname, ext_fname) < 2)
            {
                printf("ERROR: 1 or more arguments were the wrong type\n");
            }
            else if (int_fname[0] != '/'){
                printf("ERROR: missing '/'\n");
            }
            else
            {
                // cpout code
            }
        }
        else if (!strcmp(cmd, "rm"))
        {
            char rm_fname[28];

            if (scanf("%s", rm_fname) < 1)
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
}
