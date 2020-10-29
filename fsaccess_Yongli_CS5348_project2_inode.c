/***********************************************************************
 Modified by : Yongli Shan
 Collaborator/Team partner: Jinglun Zhang
 Date: 10/29/20

 Modifications include
 int savesuper_block(); //a function to update superblock if it's modified 
void clear_block(int index);// Clear a block not in use anymore
void clear_inode(int index);// Clear an inode not in use anymore
int cpin(char* externalFile, char* v6File);//copy externalFile to V6 internal file
int cpout(char* internal_path, char* external_path);//copy internal V6 file to external file and save
int find_position_of_dir_entry_for_this_file(char* path);//find the position in the file system to write in new dir
int get_inode_number_with_full_path(char* path);//get internal inode position of a file or a path
int get_inode_number(char* filename, int inode_number);//recursive function to browse directory layers and find file
unsigned int allocblock(); // allocate block to write data
int allocinode(); // allocate inode to make new fiel or directory
 
 This program allows user to do two things: 
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large 
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. Quit: save all work and exit the program.
   
 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - q
     
 File name is limited to 14 characters.
 ***********************************************************************/

#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include <time.h>
#include<string.h>
#include<stdlib.h>

#define FREE_SIZE 150  
#define I_SIZE 200
#define BLOCK_SIZE 1024    
#define ADDR_SIZE 11
#define INPUT_SIZE 256
#define INODE_SIZE 64 // inode has been doubled


// Superblock Structure

typedef struct {
  unsigned short isize;
  unsigned short fsize;
  unsigned short nfree;
  unsigned int free[FREE_SIZE];
  unsigned short ninode;
  unsigned short inode[I_SIZE];
  char flock;
  char ilock;
  unsigned short fmod;
  unsigned short time[2];
} superblock_type;

superblock_type superBlock;

// I-Node Structure

typedef struct {
unsigned short flags;
unsigned short nlinks;
unsigned short uid;
unsigned short gid;
unsigned int size;
unsigned int addr[ADDR_SIZE];
unsigned short actime[2];
unsigned short modtime[2];
} inode_type; 

inode_type inode; //the fisr inode

typedef struct {
  unsigned short inode;
  unsigned char filename[14];
} dir_type;

dir_type root;

int fileDescriptor;		//file descriptor 
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777; // User, Group, & World have all access privileges 
const unsigned short DIR_SIZE = 16;


int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );
void create_root();
int savesuper_block();
void clear_block(int index);
void clear_inode(int index);
int cpin(char* externalFile, char* v6File);
int cpout(char* internal_path, char* external_path);
int find_position_of_dir_entry_for_this_file(char* path);
int get_inode_number_with_full_path(char* path);
int get_inode_number(char* filename, int inode_number);
unsigned int allocblock();
int allocinode();


int preInitialization(){

  char *n1, *n2;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  
  filepath = strtok(NULL, " ");
  n1 = strtok(NULL, " ");
  n2 = strtok(NULL, " ");         
      
  if(access(filepath, F_OK) != -1) {
      
      if((fileDescriptor = open(filepath, O_RDWR, 0600)) == -1){
      
         printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
         return 1;
      }
      lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
      read(fileDescriptor, &superBlock, BLOCK_SIZE);
      printf("filesystem already exists and the same will be used.\n");
      printf("Fsize: %d.\n", superBlock.fsize);
      printf("Number of inodes: %d\n", superBlock.ninode);
      printf("Finish restoring previous file system.\n");
      return 0;
  
  } else {
  
        	if (!n1 || !n2)
              printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
            
       		else {
          		numBlocks = atoi(n1);
          		numInodes = atoi(n2);
          		
          		if( initfs(filepath,numBlocks, numInodes )){
          		  printf("The file system is initialized\n");	
                return 0;
          		} else {
            		printf("Error initializing file system. Exiting... \n");
            		return 1;
          		}
       		}
  }
}

int initfs(char* path, unsigned short blocks,unsigned short inodes) {

   unsigned int buffer[BLOCK_SIZE] ={0};
   unsigned short i = 0;
   //for (i = 0; i < BLOCK_SIZE/4; i++) buffer[i] = 0;//unsigned int takes 4 bytes 
   //int bytes_written;
   
   superBlock.fsize = blocks; // not used 
   unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;
   
   if((inodes%inodes_per_block) == 0)
   superBlock.isize = inodes/inodes_per_block;
   else
   superBlock.isize = (inodes/inodes_per_block) + 1;
   
   if((fileDescriptor = open(path,O_RDWR|O_CREAT,0700))== -1)
       {
           printf("\n open() failed with the following error [%s]\n",strerror(errno));
           return 0;
       }
       
   for (i = 0; i < FREE_SIZE; i++)
      superBlock.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.
    
   superBlock.nfree = 0;
   superBlock.ninode = I_SIZE;
   
   for (i = 0; i < I_SIZE; i++)
	    superBlock.inode[i] = i + 1;		//Initializing the inode array to inode numbers
   
   superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
   superBlock.ilock = 'b';					
   superBlock.fmod = 0;
   superBlock.time[0] = 0;
   superBlock.time[1] = 1970;
   
   lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
   write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system
   
   // writing zeroes to all inodes in ilist
        
   for (i = 0; i < superBlock.isize; i++){
     lseek(fileDescriptor, BLOCK_SIZE*(2 + i), SEEK_SET);//TODO mistake corrected??
     write(fileDescriptor, &buffer, BLOCK_SIZE);

   }   	  
   
   int data_blocks = blocks - 2 - superBlock.isize;
   int data_blocks_for_free_list = data_blocks - 1;
   
   // Create root directory
   create_root();
 
   for ( i = 2 + superBlock.isize + 1; i < data_blocks_for_free_list; i++ ) {
      add_block_to_free_list(i , buffer);
   }
   
   return 1;
}

// Add Data blocks to free list
void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){

  if ( superBlock.nfree == FREE_SIZE ) {

    int free_list_data[BLOCK_SIZE / 4], i;
    free_list_data[0] = FREE_SIZE;//first entry is the free block number
    
    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) {
       if ( i < FREE_SIZE ) {
         free_list_data[i + 1] = superBlock.free[i];//direct entries of free list to the free block address
       } else {
         free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
       }
    }
    
    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, &free_list_data, BLOCK_SIZE ); // Writing free list to header block
    
    superBlock.nfree = 0;
    
  } else {

	  lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, &empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
  }

  superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
  ++superBlock.nfree;
}

// Create root directory
void create_root() {

  int root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
  int i;
  
  root.inode = 1;   // root directory's inode number is 1.
  root.filename[0] = '.';
  root.filename[1] = '.';//TODO: line needed?
  root.filename[2] = '\0';
  
  inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory 
  inode.nlinks = 0; 
  inode.uid = 0;
  inode.gid = 0;
  inode.size = INODE_SIZE;
  inode.addr[0] = root_data_block;
  
  for( i = 1; i < ADDR_SIZE; i++ ) {
    inode.addr[i] = 0;
  }
  
  inode.actime[0] = 0;
  inode.modtime[0] = 0;
  inode.modtime[1] = 0;
  
  lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
  write(fileDescriptor, &inode, INODE_SIZE);   // 
  
  lseek(fileDescriptor, root_data_block*BLOCK_SIZE, 0);//TODO, mistake corrected??
  write(fileDescriptor, &root, 16);
  
  root.filename[0] = '.';
  //root.filename[1] = '.';
  root.filename[1] = '\0';
  
  write(fileDescriptor, &root, 16);
 
}

///////////////////
int savesuper_block() {
    superBlock.time[0] = (unsigned int)time(NULL);
    lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, &superBlock, sizeof(superblock_type));
    return 0;
}

//////////////////
int allocinode() {
    superBlock.ninode--;

    unsigned int inodeid = 0;

    if(superBlock.ninode == -1) {
        superBlock.ninode = 0;
        unsigned int i = 1;
        inode_type freeinode;
        //Search for an unallocated(released) inode and return its index
        while (i <= I_SIZE) {
            lseek(fileDescriptor, 2 * BLOCK_SIZE + (i-1) * INODE_SIZE, SEEK_SET);
            read(fileDescriptor, &freeinode, INODE_SIZE);
            if ((freeinode.flags & 0100000) == 0) { //unallocated
                inodeid = i;
                i++;//inonde number starts from 1
                break;                
            }            
        }
        //Search inode area and push unallocated(released) inode into the ninode 
        //and inode list in superblock, junk collection
        while (superBlock.ninode <= I_SIZE || i <= I_SIZE) {
            lseek(fileDescriptor, 2 * BLOCK_SIZE + (i-1) * INODE_SIZE, SEEK_SET);
            read(fileDescriptor, &freeinode, INODE_SIZE);
            if ((freeinode.flags & 0100000) == 0) { //unallocated
                superBlock.ninode++;
            }
            i++;
        }
    }
    else {
        inodeid = superBlock.inode[superBlock.ninode];
    }

    savesuper_block();

    if(inodeid ==0){
        printf("All inodes are used! Restart the system!");
    }

    return inodeid;
}

//////////////////
unsigned int allocblock() {
    superBlock.nfree--;

    unsigned int blkid;

    if(superBlock.nfree == 0) {
        blkid = superBlock.free[0];

        int offset = blkid * BLOCK_SIZE;

        lseek(fileDescriptor, offset, SEEK_SET);
        read(fileDescriptor, &superBlock.nfree, 4);
        offset += 4;

        lseek(fileDescriptor, offset, SEEK_SET);
        read(fileDescriptor, &superBlock.free, 4 * superBlock.nfree);
    }


    else {
        blkid = superBlock.free[superBlock.nfree];
    }

    savesuper_block();

    clear_block(blkid);

    return blkid;
}
///////////////////
int get_inode_number(char* filename, int inode_number) {

    dir_type dir_entry;
    int offset;
    lseek(fileDescriptor, 2 * BLOCK_SIZE + (inode_number -1) * INODE_SIZE + 12, SEEK_SET);
    read(fileDescriptor, &offset, 4);//offset points to root directory block
    //printf("Parent directory inode number is %d, and points to block #%d \n",inode_number, offset);

    int i;
    for ( i = 0; i < BLOCK_SIZE / DIR_SIZE; i++)
    {
        lseek(fileDescriptor, offset * BLOCK_SIZE + i * 16, SEEK_SET);
        read(fileDescriptor, &dir_entry, 16);
        //printf("checkpoint, filename in dir entry %d is %s \n", i, dir_entry.filename);
        if (strcmp(dir_entry.filename, filename) == 0) {
            int inode_num = dir_entry.inode;
            //printf("Internal file %s already exists, inode #%d \n",filename, inode_num);
            return inode_num;
        }
    }
    printf("Internal file %s doesn't not exist in parent inode #%d \n", filename, inode_number);
    return 0;
}

int get_inode_number_with_full_path(char* inpath){

    const char s[2] = "/";
    char *token;

    char path[14];
    strcpy(path, inpath);

    token = strtok(path,s);
    int i = 1;
    while (token != NULL)
    {
        if(get_inode_number(token,i) == 0)
            return 0;

        i= get_inode_number(token,i);
        token = strtok(NULL,s);
    }
    return i;
}


//////////////////////////
int find_position_of_dir_entry_for_this_file(char *inpath){
    char new_name[14];
    const char s[2] = "/";
    char *token;

    char path[14];
    strcpy(path, inpath);

    token = strtok(path,s);
    int i = 1;
    int j;
    while (token != NULL)
    {
        j = i;
        if(i==0)
            printf("Invalid path!\n");        
        i= get_inode_number(token,i);  // i dir i-node number, j parent dir inode number.        
        strcpy(new_name, token);              // new_name is the dir name.
        token = strtok(NULL,s);
    }

    dir_type dir_entry;
    int offset;
    lseek(fileDescriptor, 2*BLOCK_SIZE + (j-1)*INODE_SIZE + 12, SEEK_SET);//+ point to addr[0] of parent directory
    read(fileDescriptor, &offset, 4);//read the data block location of the parent and find the file
    for (int k = 0; k< BLOCK_SIZE/DIR_SIZE; k++)
    {
        lseek(fileDescriptor, offset*BLOCK_SIZE+ k*16, SEEK_SET);//*16 is the directory information
        read(fileDescriptor, &dir_entry, 16);
        if (strcmp(dir_entry.filename, new_name) == 0 )
        {
            printf("The file: %s is found at inode# %d\n",new_name,i);
            clear_inode(i);
            return offset*BLOCK_SIZE+ k*16;            
        }
    }

   return offset*BLOCK_SIZE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void clear_inode(int index) //
{
    int indexi = index - 1; //inode 0 is not real
    char emptyb[BLOCK_SIZE] = {0};
    inode_type current_inode;
    lseek(fileDescriptor, 2*BLOCK_SIZE + indexi*INODE_SIZE, SEEK_SET);
    read(fileDescriptor, &current_inode, INODE_SIZE);
    int i = 0;    
    while(current_inode.addr[i]!=0){
      clear_block(current_inode.addr[i]);
      i++;
    }
    char emptyi[INODE_SIZE] = {0};
    lseek(fileDescriptor, 2*BLOCK_SIZE + indexi*INODE_SIZE, SEEK_SET);
    write(fileDescriptor, &emptyi, INODE_SIZE);

    printf("Inode #%d was purged for reuse.\n",index);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void clear_block(int index)
{
    char empty[BLOCK_SIZE] = {0};
    lseek(fileDescriptor, index*BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, &empty, BLOCK_SIZE);

    //printf("Block #%d was purged for reuse.\n", index);
}



//////////////////////////////////////////////////////////////
int cpin(char *externalFile, char *v6File) {

    char new_name[14];    
    const char s[2] = "/";
    char *token;

    char v6[14];
    strcpy(v6, v6File);
    
    token = strtok(v6,s);

    while (token != NULL)
    {
        strcpy(new_name, token);
        token = strtok(NULL,s);
    }

    //if file not found, create it, and copy external file 
    
    inode_type v6FileInode;
    int newFileInodeNum = allocinode();
    dir_type newdir;

    strcpy(newdir.filename, new_name);
    lseek(fileDescriptor, find_position_of_dir_entry_for_this_file(v6File),SEEK_SET);
       
    newdir.inode = newFileInodeNum;
    printf("Allocating inode #%d for file.\n",newFileInodeNum);

    write(fileDescriptor, &newdir,16); 
    
    int exFileFd = open(externalFile, O_RDONLY);
    
    char buffer[BLOCK_SIZE] = {0};
        
    //if (read(exFileFd, &buffer, BLOCK_SIZE) == 0){
    //  printf("check point1, buffer is empty ");
    //}
    int indexOfAddr = 0;
    int k = 0;
    lseek(exFileFd,k*BLOCK_SIZE,SEEK_SET);
    while (read(exFileFd, &buffer, BLOCK_SIZE) != 0){
        //fwrite(buffer, sizeof(buffer), 1, stdout);
        int currentBlk = allocblock();
        lseek(fileDescriptor, currentBlk * BLOCK_SIZE, SEEK_SET);
        write(fileDescriptor, &buffer, sizeof(buffer));
        printf("Copying to allocated block#%d \n", currentBlk);
        
        //read(fileDescriptor, &buffer, BLOCK_SIZE);
        //printf("Transferred buffer:%s \n", buffer);
        
        if (indexOfAddr < ADDR_SIZE) {
            lseek(fileDescriptor, 2 * BLOCK_SIZE + (newFileInodeNum-1) * INODE_SIZE + 12 + indexOfAddr * 4, SEEK_SET);
            write(fileDescriptor, &currentBlk, 4);            
            indexOfAddr++;
            k++;
        } 
        else{
          printf("File is too large and is not able to handle!\n");
          return 0;
        }        
        lseek(exFileFd, k*BLOCK_SIZE,SEEK_SET);    
    }
    printf( "%s is copied into %s successfully.\n",externalFile,v6File);
    return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int cpout(char* internal_path, char* external_path){

    //printf("%s", internal_path);
    //printf("%s", external_path);
    int fd1 = open(external_path,O_RDWR | O_CREAT, 0666);
    int inode_no = get_inode_number_with_full_path(internal_path);
    printf("The inode of %s is inode #%d\n", internal_path, inode_no);
    int count =0;

    if(inode_no == 0){
      printf("Internal file %s doesn't exist!\n", internal_path);
      return 0;
    }                

    inode_type current_inode;
    lseek(fileDescriptor, 2*BLOCK_SIZE+ (inode_no-1)*INODE_SIZE, SEEK_SET);
    read(fileDescriptor, &current_inode, INODE_SIZE); // inode is internal_path's inode
    char buffer[BLOCK_SIZE] = {0};
    int i = 0;
    for (i = 0; i < ADDR_SIZE; i++)
    {
        //printf("checkpoint: block address read: %d", inode.addr[i]);
        if (current_inode.addr[i] == 0)
        {
          printf("Copying file out, the last block #%d\n", current_inode.addr[i-1]);
            break;
        }
        lseek(fileDescriptor, current_inode.addr[i] * BLOCK_SIZE, SEEK_SET);
        read(fileDescriptor, &buffer, BLOCK_SIZE);
        int buffersize = strlen(buffer);
        printf("Writing %d number of bytes to external file.\n", buffersize);
        lseek(fd1, i*BLOCK_SIZE, SEEK_SET);
        write(fd1, &buffer, buffersize);
    }
    
    close(fd1);

    printf( " %s is copied  to %s successfully.\n",internal_path,external_path);
    return 0;
}
/////////////////////////////////////////////////////

int main() {
 
  char input[INPUT_SIZE];
  char *splitter;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  //printf("Size of unsigned short = %ld, size of unsigned int = %ld, size of char = %ld\n", sizeof(unsigned short), sizeof(unsigned int), sizeof(char));
  printf("Size of superblock_type = %ld, size of inode_type = %ld, size of dir_type = %ld\n",sizeof(superblock_type), sizeof(inode_type), sizeof(dir_type));
  //printf("Size of super block = %ld , size of i-node = %ld\n",sizeof(superBlock),sizeof(inode));
  printf("Enter command:\n");
  printf(">> ");
  
  while(1) {
  
    scanf(" %[^\n]s", input);
    splitter = strtok(input," ");
    
    if(strcmp(splitter, "initfs") == 0){
    
        preInitialization();
        printf(">> ");
        splitter = NULL;
                       
    } 
    
    else if (strcmp(splitter, "q") == 0) {
   
       lseek(fileDescriptor, BLOCK_SIZE, 0);
       write(fileDescriptor, &superBlock, BLOCK_SIZE);
       return 0;
     
    } 

    else if(strcmp(splitter, "cpin")==0) {
       
      char *externalfile;
      char *v6file;
      externalfile = strtok(NULL, " ");
      v6file = strtok(NULL, " ");
      cpin(externalfile, v6file);
      printf(">> ");
      splitter = NULL;
        }

    else if(strcmp(splitter, "cpout")==0)
        {
      //printf("abc");
      char *externalfile;
      char *v6file;
      v6file = strtok(NULL, " ");
      externalfile = strtok(NULL, " ");
      cpout(v6file, externalfile);
      printf(">> ");
      splitter = NULL;
        }
  }
}
