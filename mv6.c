#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>


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
    #define IOP(x)  (x << 6)            //owner permissions; x is a number 0-7
    #define IGP(x)  (x << 3)            //group permissions; x is a number 0-7
    #define IWP(x)  x                   //world permissions; x is a number 0-7


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
void fill_an_inode_and_write(inode_type*, int, int);
int add_free_block(int);
int get_free_block();
//int write_dir_entry(int, dir_type);
void initfs(int, int);
int main();


//globals
superblock_type superBlock;
inode_type root;
int fd;


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

// Function to write inode number after filling some fileds
void fill_an_inode_and_write(inode_type *inode, int inum, int flags)
{
    inode->flags = flags;
    inode->actime = time(NULL);
    inode->modtime = time(NULL);
    inode->size0 = inode->size1 = 0;

    //simple addr loop, make this more complicated later
    inode->addr[0] = get_free_block();
    int i;
    for (i = 1; i < 9; i++)
    {
        inode->addr[i] = -1;
    }
    
    inode_writer(inum, *inode);
}

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
        write(fd, &superBlock.free, 200 * sizeof(int));
        superBlock.nfree = 0;
    }
    superBlock.free[superBlock.nfree] = blocknum;
    superBlock.nfree++;
    superBlock.time = time(NULL);
    return 1;
}

int get_free_block()
{
    superBlock.nfree--;
    if (superBlock.free[superBlock.nfree] == 0)
    {
        return -1;
    }
    else if (superBlock.nfree == 0)
    {
        lseek(fd, BLOCK_SIZE * superBlock.free[superBlock.nfree], SEEK_SET);
        read(fd, &superBlock.nfree, sizeof(int));
        read(fd, &superBlock.free, 200 * sizeof(int));
    }
    superBlock.time = time(NULL);
    return 1;
}

/* int write_dir_entry(int dir_inum, dir_type entry)
{
    inode_type dir;
    inode_reader(dir_inum, dir);
    if (dir.flags & IDIRF == IDIRF)
    {
        //dir_inum doesn't refer to a directory
        return -1;
    }
    lseek(fd, 2 * BLOCK_SIZE + (dir_inum - 1) * INODE_SIZE, SEEK_SET);
    read(fd, &entry, sizeof(dir_type));
    return 1;
} */

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
    fill_an_inode_and_write(&root, 1, IALLOC | IDIRF);
}

int main()
{
    char cmd[10];
    char fname[256];

    while (1){
        scanf("%s", cmd);

        if (!strcmp(cmd, "q"))
        {
            lseek(fd, BLOCK_SIZE, SEEK_SET);
            write(fd, &superBlock, sizeof(superblock_type));
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
                while ((getchar()) != '\n');
            }
            else if (new_isize >= new_fsize)
            {
                printf("ERROR: fsize is too small\n");
            }
            else
            {
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
        else
        {
            printf("ERROR: %s is not a command\n", cmd);
        }
    }
}
