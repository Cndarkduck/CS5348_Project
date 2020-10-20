
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <zconf.h>
#include <string.h>
#include <fcntl.h>

struct super_block {
    unsigned int isize;
    unsigned int fsize;
    unsigned int nfree;
    unsigned int free[100];
    unsigned int ninode;
    unsigned int inode[100];
    char flock;
    char ilock;
    char fmod;
    unsigned int time_t;
    char unused[201];
};

struct i_node {
    unsigned short flags;
    char nlinks;
    char uid;
    char gid;
    unsigned int size;
    unsigned int addr[8];
    unsigned int actime[2];
    unsigned int modtime[2];
    char unused[15];

};

struct directory_entry_struct{
    int id_inode;
    char file_name[12];
};

#define sizeofblk 1024
#define sizeofinode 64

int fd = -1;
struct super_block super_block = {0, 0, 0, {0}, 0, {0}, 0, 0, 0, 0, {0}};
unsigned int numOfBlock = 0;
unsigned int numOfinode = 0;

int savesuper_block();
int writeblk(unsigned int blkid);
int setfreeblocks(unsigned int startpos, unsigned int endpos);
unsigned int readblk();
int setrootdir();
int fillinodes( unsigned int total_inodes);
unsigned short readinode();
int cpin(char* externalFile, char* v6File);
int cpout(char* internal_path, char* external_path);
int mkdir(char* path);
void clear_block(int index);
int rm(char* path);
int find_position_of_dir_entry_for_this_file(char* path);
int find_next_avaliable_directory_entry_position(int dir_inode_no);
int get_inode_number_with_path(char* path);
int get_inode_number(char* filename, int inode_number);
int initfs(char* path, unsigned int total_blocks,unsigned int total_inodes);
int welcome();












int savesuper_block() {
    super_block.time_t = (unsigned int)time(NULL);
    lseek(fd, sizeofblk, SEEK_SET);
    write(fd, &super_block, sizeof(struct super_block));
    return 0;
}

int writeblk(unsigned int blkid) {
    if(blkid >= super_block.fsize) {
        return -1;
    }

    if(super_block.nfree == 100) {
        int offset = blkid * sizeofblk;

        lseek(fd, offset, SEEK_SET);
        write(fd, &super_block.nfree, 4);
        offset += 4;

        lseek(fd, offset, SEEK_SET);
        write(fd, &super_block.free, 4 * super_block.nfree);

        printf("Block#%d: nfree=%u, free[1]=%d,free[99]=%d\n", blkid,super_block.nfree,super_block.free[1],super_block.free[99]);

        super_block.free[0] = blkid;
        super_block.nfree = 1;
    }
    else {
        super_block.free[super_block.nfree] = blkid;
        if (blkid == super_block.fsize - 1) {
            printf("Block#%d: nfree=%u, free[1]=%d,free[%d]=%d\n", blkid,super_block.nfree,super_block.free[1],super_block.nfree,super_block.free[super_block.nfree]);
        }
        super_block.nfree++;
    }



    savesuper_block();

    return 0;
}

int setfreeblocks(unsigned int startpos, unsigned int endpos) {
    super_block.free[0] = 0;
    super_block.nfree = 1;

    savesuper_block();

    unsigned int pointer;

    for(pointer = startpos; pointer <= endpos; pointer++) {
        writeblk(pointer);
    }

    return 0;
}


unsigned int readblk() {
    super_block.nfree--;

    unsigned int blkid;

    if(super_block.nfree == 0) {
        blkid = super_block.free[0];

        int offset = blkid * sizeofblk;

        lseek(fd, offset, SEEK_SET);
        read(fd, &super_block.nfree, 4);
        offset += 4;

        lseek(fd, offset, SEEK_SET);
        read(fd, &super_block.free, 4 * super_block.nfree);
    }
    else {
        blkid = super_block.free[super_block.nfree];
    }

    savesuper_block();

    char empty[sizeofblk] = {0};

    lseek(fd, blkid * sizeofblk, SEEK_SET);
    write(fd, &empty, sizeofblk);
    return blkid;
}

int setrootdir() {
    int offset = 2 * sizeofblk ;
    struct directory_entry_struct directory_entry;
    struct i_node inode1 = {0,0,0,0,0,{0},(unsigned int)time(NULL),(unsigned int)time(NULL),{0}};

    inode1.flags |=1 <<15;
    inode1.flags |=1 <<14;
    inode1.flags |=0 <<13;



    directory_entry.id_inode = 1;
    strcpy(directory_entry.file_name, ".");
    unsigned int dir_file_block = readblk();
    lseek(fd,dir_file_block*sizeofblk,SEEK_SET);
    write(fd,&directory_entry,16);
    strcpy(directory_entry.file_name, "..");
    lseek(fd,dir_file_block*sizeofblk + 16 ,SEEK_SET);
    write(fd,&directory_entry,16);

    inode1.addr[0]=dir_file_block;


    lseek(fd, offset+9, SEEK_SET);
    write(fd, &inode1.addr[0], 4);
    return 0;
}

int fillinodes( unsigned int total_inodes) {

    unsigned int iNum;
    int maxNum = total_inodes;

    for (iNum = 2; iNum <= maxNum; iNum++)
    {

        super_block.inode[super_block.ninode] = iNum;  // write
        super_block.ninode++;

        if (super_block.ninode == 100)
        {
            break;
        }
    }
    return 0;
}

unsigned short readinode() {
    super_block.ninode--;

    unsigned int inodeid;

    if(super_block.ninode == -1) {
        unsigned int i = 0;
        struct i_node freeinode;
        while (i < numOfinode) {
            lseek(fd, 2 * sizeofblk + i * sizeofinode, SEEK_SET);
            read(fd, &freeinode, sizeofinode);
            if ((freeinode.flags & 0100000) == 0) {
                inodeid = i;
                break;
            }
            i++;
        }
        while (i < numOfinode || super_block.ninode < 100) {
            lseek(fd, 2 * sizeofblk + i * sizeofinode, SEEK_SET);
            read(fd, &freeinode, sizeofinode);
            if ((freeinode.flags & 0100000) == 0) {
                super_block.ninode++;
                lseek(fd, 1 * sizeofblk + 4 + 4 + 4 + 400 + 4 + super_block.ninode * 4, SEEK_SET);
                write(fd, &i, 4);
            }
            i++;
        }
    }
    else {
        inodeid = super_block.inode[super_block.ninode];
    }

    savesuper_block();


    return inodeid;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int cpin(char *externalFile, char *v6File) {
    char new_name[14];

    const char s[2] = "/";
    char *token;

    token = strtok(v6File,s);
    int i = 1;
    int j;
    while (token != NULL)
    {

        j = i;
        if(i==-1)
            printf("Invalid path !");
        i= get_inode_number(token,i);
        strcpy(new_name, token);
        token = strtok(NULL,s);
    }
    char *v6 = v6File;
    struct i_node v6FileInode;
    unsigned short newFileInodeNum = readinode();
    struct directory_entry_struct newdir;
    newdir.id_inode = newFileInodeNum;
    strcpy(newdir.file_name , v6);
    lseek(fd,find_position_of_dir_entry_for_this_file(v6File),SEEK_SET);
    write(fd,&newdir,16);

    int exFileFd = open(externalFile, O_RDWR | O_TRUNC | O_CREAT);
    int indexOfAddr = 0;
    int firstIndex = 0, secondIndex = 0, thirdIndex = 0;
    int firstBlock = 0, secondBlock = 0, thirdBlock = 0;
    char buffer[sizeofblk] = {0};
    lseek(exFileFd,0,SEEK_SET);
    while (read(exFileFd, &buffer, sizeofblk) != 0){
        int currentBlk = readblk();
        lseek(fd, currentBlk * sizeofblk, SEEK_SET);
        write(fd, buffer, sizeof(buffer));
        if (indexOfAddr < 7) {
            lseek(fd, 2 * sizeofblk + newFileInodeNum * 64 + 9 + indexOfAddr * 4, SEEK_SET);
            write(fd, &currentBlk, 4);
            indexOfAddr++;
        } else {
            if (firstIndex == 0 && secondIndex == 0 && thirdIndex == 0){
                firstBlock = readblk();
                secondIndex = readblk();
                thirdBlock = readblk();
                lseek(fd, 2 * sizeofblk + newFileInodeNum * 64 + 9 + 7 * 4, SEEK_SET);
                write(fd, &firstBlock, 4);
            }
            lseek(fd, firstBlock * sizeofblk + firstIndex * 4, SEEK_SET);
            write(fd, &secondBlock, 4);
            lseek(fd, secondBlock * sizeofblk + secondIndex * 4, SEEK_SET);
            write(fd, &thirdBlock, 4);
            lseek(fd, thirdBlock * sizeofblk + thirdIndex * 4, SEEK_SET);
            write(fd, &currentBlk, 4);
            if (thirdIndex < 255) {
                thirdIndex++;
                thirdBlock = readblk();
            } else {
                thirdIndex = 0;
                thirdBlock = readblk();
                secondIndex ++;
                secondIndex = readblk();
            }
            if (secondIndex == 256) {
                secondIndex = 0;
                secondIndex = readblk();
                firstIndex ++;
                firstBlock = readblk();
            }
        }
        int i = 0;
        i++;
        lseek(exFileFd,i*sizeofblk,SEEK_SET);
    }

    printf( "%s is copied into  %s successfully./n",externalFile,v6File);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int cpout(char* internal_path, char* external_path){

    //printf("%s", internal_path);
    //printf("%s", external_path);
    int fd1 = open(external_path,O_RDWR | O_TRUNC | O_CREAT);
    int inode_no = get_inode_number_with_path(internal_path);
    int count =0;

    if(inode_no == -1)
        return 0;

    struct i_node inode;
    lseek(fd, 2*sizeofblk+ (inode_no-1)*sizeofinode, SEEK_SET);
    write(fd, &inode, sizeofinode); // inode is internal_path's inode


    char buffer[1024];

    char flag[1024] = {0};
    int i = 0;
    for (i = 0; i < 7 ; i++)
    {
        if (inode.addr[i] == 0)
        {
            break;
        }
        lseek(fd, inode.addr[i] * sizeofblk, SEEK_SET);
        read(fd, &buffer, sizeofblk);
        lseek(fd1, i*sizeofblk, SEEK_SET);
        write(fd1, &buffer, sizeofblk);

    }
    int firstBlock=0 , secondBlock=0 , thirdBlock=0 , currBlock=0;
    int firstIndex=0 , secondIndex=0 , thirdIndex=0;
    while(1)
    {
        lseek(fd, inode.addr[7] * sizeofblk + firstIndex * 4, SEEK_SET);
        read(fd, &firstBlock,4);
        lseek(fd, firstBlock * sizeofblk + secondIndex * 4, SEEK_SET);
        read(fd, &secondBlock,4);
        lseek(fd, secondBlock * sizeofblk + thirdIndex * 4, SEEK_SET);
        read(fd, &thirdBlock,4);
        currBlock = thirdBlock;

        lseek(fd, currBlock * sizeofblk, SEEK_SET);
        read(fd, &buffer, sizeofblk);

        if(buffer == flag){
            break;
        }

        lseek(fd1, count*sizeofblk, SEEK_SET);
        write(fd1, &buffer, sizeofblk);
        count++;

        if (thirdIndex < 255)
        {
            thirdIndex++;
        }
        else
        {
            thirdIndex = 0;
            secondIndex ++;
        }
        if (secondIndex == 256)
        {
            secondIndex = 0;
            firstIndex ++;
        }
    }

    printf( " %s is copied  to %s successfully./n",internal_path,external_path);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int mkdir(char* path){
    char new_name[12];

    const char s[2] = "/";
    char *token;

    token = strtok(path,s);
    int i = 1;
    int j;
    while (token != NULL)
    {

        j = i;
        if(i==-1)
            printf("Invalid path !");
        i= get_inode_number(token,i);
        strcpy(new_name, token);
        token = strtok(NULL,s);
    }

    if(i!= -1)
    {
        printf("This directory already exists");
        return 0;
    }
    printf("The i is %d\n", i);

    int i_node_num = readinode();
    printf("The new free inode number is %d\n",i_node_num);
    struct i_node current_inode;


    struct directory_entry_struct dir;

    dir.id_inode = i_node_num;
    strcpy(dir.file_name,".");
    unsigned int dir_file_block = readblk();
    printf("The new block number is %d\n",dir_file_block);
    lseek(fd,dir_file_block*sizeofblk,SEEK_SET);
    write(fd,&dir,16);
    dir.id_inode = j;
    printf("parent inode num is %d\n",j);
    strcpy(dir.file_name, "..");
    lseek(fd,dir_file_block*sizeofblk + 16 ,SEEK_SET);
    write(fd,&dir,16);

    current_inode.addr[0]=dir_file_block;


    lseek(fd, (i_node_num-1)*64 +2*sizeofblk+9, SEEK_SET);
    write(fd, &current_inode.addr[0], 4);


    printf("The parent inode number is %d\n",j);

    int offset = find_next_avaliable_directory_entry_position(j);
    dir.id_inode = i_node_num;
    strcpy(dir.file_name,new_name);
    printf("The position of available directory is %d\n",offset);
    lseek(fd,offset,SEEK_SET);
    write(fd,&dir,16);
    int m = get_inode_number_with_path(path);
    printf("The new directory's inode number is %d\n ",m );

    printf("Directory %s is created successfully!\n",path);
    return 0;

}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void clear_block(int index)
{
    char empty[sizeofblk] = {0};
    lseek(fd, index*sizeofblk, SEEK_SET);
    write(fd, &empty, sizeofblk);
}

int rm(char* path)
{
    int i = get_inode_number_with_path(path);

    if(i == -1)
    {
        printf("The path doesn't exist");
        return 0;
    }


    struct i_node inode;

    for(i = 0; i < 7;i++)
    {
        int inode_addr;
        lseek(fd, 2*sizeofblk + (i-1)*sizeofinode + 9 + 4*i, SEEK_SET);
        write(fd, &inode_addr, 4);
        if(inode_addr == 0)
            break;
        clear_block(inode_addr);  // Written by range
        writeblk(inode_addr);
    }
    int firstBlock=0 , secondBlock=0 , thirdBlock=0 , currBlock=0;
    int firstIndex=0 , secondIndex=0 , thirdIndex=0;
    while(1)
    {
        lseek(fd, inode.addr[7] * sizeofblk + firstIndex * 4, SEEK_SET);
        read(fd, &firstBlock,4);
        lseek(fd, firstBlock * sizeofblk + secondIndex * 4, SEEK_SET);
        read(fd, &secondBlock,4);
        lseek(fd, secondBlock * sizeofblk + thirdIndex * 4, SEEK_SET);
        read(fd, &thirdBlock,4);

        if (thirdBlock == 0)
            break;
        currBlock = thirdBlock;
        char flag[1024] = {0};


        lseek(fd, currBlock * sizeofblk, SEEK_SET);
        write(fd, &flag, sizeofblk);

        if (thirdIndex < 255)
        {
            thirdIndex++;
        }
        else
        {
            thirdIndex = 0;
            secondIndex ++;
        }
        if (secondIndex == 256)
        {
            secondIndex = 0;
            firstIndex ++;
        }
    }


    struct i_node empty = {0};
    lseek(fd, 2*sizeofblk + (i-1)*sizeofinode, SEEK_SET);
    write(fd, &empty, sizeofinode);
    super_block.inode[super_block.ninode] = i;
    super_block.ninode = super_block.ninode+1;
    savesuper_block();

    struct directory_entry_struct dir_entry = {0};
    int offset = find_position_of_dir_entry_for_this_file(path);

    lseek(fd, offset, SEEK_SET);
    write(fd, &dir_entry, 16);

    printf("Directory %s is removed successfully!/n",path);
    return 0;
}

int find_position_of_dir_entry_for_this_file(char* path){
    char new_name[12];
    const char s[2] = "/";
    char *token;

    token = strtok(path,s);
    int i = 1;
    int j;
    while (token != NULL)
    {
        if(i==-1)
            printf("Invalid path !");
        j =i;
        i= get_inode_number(token,i);  // i dir i-node number, j parent dir inode number.
        strcpy(new_name, token);              // new_name is the dir name.
        token = strtok(NULL,s);
    }

    struct directory_entry_struct dir_entry;
    printf("the parent inode num is %d\n",j);
    int offset;
    lseek(fd, 2*sizeofblk + (j-1)*sizeofinode + 9, SEEK_SET);
    write(fd, &offset, 4);
    for (i = 0; i< 64; i++)
    {
        lseek(fd, offset*sizeofblk+ i*16, SEEK_SET);
        read(fd, &dir_entry, 16);
        if (strcmp(dir_entry.file_name, new_name) == 0 )
        {
            return offset*sizeofblk+ i*16;
        }
    }
    return 0;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int find_next_avaliable_directory_entry_position(int dir_inode_no)
{

    int offset;
    int position;
    struct i_node inode;

    struct directory_entry_struct dir_entry;
    int addr0;
    lseek(fd, 2*sizeofblk + (dir_inode_no-1)*sizeofinode + 9, SEEK_SET);
    read(fd, &addr0, 4);
    printf("The addr we found is %d\n",addr0);
    int i;



    for (i = 0; i <64 ; i++)
    {
        lseek(fd, addr0 * sizeofblk + i * 16, SEEK_SET);
        read(fd, &dir_entry, 16);
        printf("The parent's directory inode numbers %d\n",dir_entry.id_inode);

        if (dir_entry.id_inode <= 0) {
            offset = 16 * i;
            position = addr0 * sizeofblk + offset;
            return position;
        }

    }

    return 0;
}



int get_inode_number_with_path(char* path){


    const char s[2] = "/";
    char *token;

    token = strtok(path,s);
    int i = 1;
    while (token != NULL)
    {
        if(get_inode_number(token,i ) == -1)
            return -1;

        i= get_inode_number(token,i);
        token = strtok(NULL,s);
    }


    return i;
}

int get_inode_number(char* filename, int inode_number) {

    struct directory_entry_struct dir_entry;
    int offset;
    lseek(fd, 2 * sizeofblk + (inode_number - 1) * sizeofinode + 9, SEEK_SET);
    read(fd, &offset, 4);

    int i;
    for ( i = 0; i < 63; i++)
    {

        lseek(fd, offset * sizeofblk + i * 16, SEEK_SET);
        read(fd, &dir_entry, 16);
        if (strcmp(dir_entry.file_name, filename) == 0) {
            int inode_num = dir_entry.id_inode;
            return inode_num;
        }
    }
    return -1;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int initfs(char* path, unsigned int total_blocks,unsigned int total_inodes) {

    printf("initializing filesystem...\n");

    printf("\n");

    printf("datablock size = %d bytes\n", sizeofblk);

    printf("\n");

    printf("inode size = %d bytes\n", sizeofinode);

    printf("\n");

    fd = open(path, O_RDWR | O_TRUNC | O_CREAT);
    if(fd == -1) {
        printf("no loaded file\n");
        return -1;
    }

    numOfBlock = total_blocks;
    numOfinode = total_inodes;

    if(numOfBlock == 0 || numOfinode == 0 || numOfinode > numOfBlock * 16) {
        printf("incorrect parameters\n");
        return -1;
    }


    char emptyBlock[sizeofblk] = {0};
    int i;
    for (i = 0; i < numOfBlock; i++) {
        lseek(fd, i * sizeofblk, SEEK_SET);
        write(fd, emptyBlock, sizeofblk);
    }

    super_block.isize = numOfinode / 16;
    super_block.isize += (super_block.isize * 16) >= numOfinode ? 0 : 1;
    super_block.fsize = numOfBlock;
    super_block.nfree = 0;
    super_block.ninode = 0;
    super_block.flock = 0;
    super_block.ilock = 0;
    super_block.fmod = 0;
    super_block.time_t = (unsigned int)time(NULL);

    savesuper_block();

    setfreeblocks(super_block.isize + 2, super_block.fsize - 1);

    setrootdir(1);

    fillinodes(total_inodes);

    return 1;
}



int main(int args, char* argv[]) {
    char *parser, cmd[256];
    char *file_path;
    char *num1, *num2;
    unsigned int bno =0, inode_no=0;
    unsigned short num_bytes;
    //welcome();
    printf("\n\nInitialize file system by using initfs <<name of your v6filesystem>> << total blocks>> << total inodes>>\n");
    while(1)
    {
        printf("\nEnter command\n");
        scanf(" %[^\n]s", cmd);
        parser = strtok(cmd," ");
        if(strcmp(parser, "initfs")==0)
        {
            file_path = strtok(NULL, " ");
            num1 = strtok(NULL, " ");
            num2 = strtok(NULL, " ");
            if(access(file_path,X_OK) != -1)
            {
                if( fd = open(file_path, O_RDWR) == -1)
                {
                    printf("File system exists, open failed");
                    return 1;
                }
                access(file_path, R_OK |W_OK | X_OK);
                printf("Filesystem already exists.\n");
                printf("Same file system to be used\n");
            }
            else
            {
                if (!num1 || !num2)
                    printf("Enter all arguments in form initfs v6filesystem 5000 400.\n");
                else
                {
                    bno = atoi(num1);
                    inode_no = atoi(num2);
                    if(initfs(file_path,bno, inode_no))
                    {
                        printf("File System has been Initialized \n");
                    }
                    else
                    {
                        printf("Error: File system initialization error.\n");
                    }
                }
            }
            parser = NULL;
            printf("\n");
        }
        else if(strcmp(parser, "cpin")==0)
        {
            char *externalfile;
            char *v6file;
            externalfile = strtok(NULL, " ");
            v6file = strtok(NULL, " ");
            cpin(externalfile, v6file);
        }
        else if(strcmp(parser, "cpout")==0)
        {
            //printf("abc");
            char *externalfile;
            char *v6file;
            v6file = strtok(NULL, " ");
            externalfile = strtok(NULL, " ");
            cpout(v6file, externalfile);
        }
        else if(strcmp(parser, "mkdir")==0)
        {
            //printf("abc");
            char *v6dir = "0";
            v6dir = strtok(NULL, " ");
            mkdir(v6dir);
        }
        else if(strcmp(parser, "rm")==0)
        {
            char *v6file = "0";
            v6file = strtok(NULL, " ");
            rm(v6file);
        }
        else if(strcmp(parser, "q")==0)
        {
            printf("\nEXITING FILE SYSTEM NOW....\n");
            lseek(fd,sizeofblk,0);
            if((num_bytes =write(fd,&super_block,sizeofblk)) < sizeofblk)
            {
                printf("\nerror in writing the super block\n");
            }
            return 0;
        }
        else
        {
            printf("\nInvalid command\n ");
            printf("\n");
        }
    }
}
