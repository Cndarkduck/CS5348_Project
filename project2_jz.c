/***********************************************************************
 
 
 
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
#include<string.h>
#include<stdlib.h>

#define FREE_SIZE 152  
#define I_SIZE 200
#define BLOCK_SIZE 1024    
#define ADDR_SIZE 11
#define INPUT_SIZE 256


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

inode_type inode;

typedef struct {
  unsigned short inode;
  unsigned char filename[14];
} dir_type;

dir_type root;

int fileDescriptor ;		//file descriptor 
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777; // User, Group, & World have all access privileges 
const unsigned short INODE_SIZE = 64; // inode has been doubled


int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );
void create_root();

int main() {
 
  char input[INPUT_SIZE];
  char *splitter;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  printf("Size of super block = %d , size of i-node = %d\n",sizeof(superBlock),sizeof(inode));
  printf("Enter command:\n");
  
  while(1) {
  
    scanf(" %[^\n]s", input);
    splitter = strtok(input," ");
    
    if(strcmp(splitter, "initfs") == 0){
    
        preInitialization();
        splitter = NULL;
                       
    } 
	else if(strcmp(splitter, "cpin") == 0){
		char *externalfile = strtok(NULL, " ");
		char *xv6file = strtok(NULL, " ");
		if(cpin(externalfile, xv6file) == -1){
			printf("Failing to copy external file %s to xv6 file %s.\n", externalfile, xv6file);
		}
		splitter = NULL;
	}
	else if(strcmp(splitter, "cpout") == 0){
		char *xv6file = strtok(NULL, " ");
		char *externalfile = strtok(NULL, " ");
		if(cpout(xv6file, externalfile) == -1)
			printf("Failing to copy xv6 file %s to external file %s.\n", xv6file, externalfile);
		splitter = NULL;
	}
	else if (strcmp(splitter, "q") == 0) {
   
       lseek(fileDescriptor, BLOCK_SIZE, 0);
       write(fileDescriptor, &superBlock, BLOCK_SIZE);
       return 0;
     
    }
  }
}

int preInitialization(){

  char *n1, *n2;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  
  filepath = strtok(NULL, " ");
  n1 = strtok(NULL, " ");
  n2 = strtok(NULL, " ");
         
      
  if(access(filepath, F_OK) != -1) {
      
      if(fileDescriptor = open(filepath, O_RDWR, 0600) == -1){
      
         printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
         return 1;
      }
      printf("filesystem already exists and the same will be used.\n");
  
  } else {
  
        	if (!n1 || !n2)
              printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
            
       		else {
          		numBlocks = atoi(n1);
          		numInodes = atoi(n2);
          		
          		if( initfs(filepath,numBlocks, numInodes )){
          		  printf("The file system is initialized\n");	
          		} else {
            		printf("Error initializing file system. Exiting... \n");
            		return 1;
          		}
       		}
  }
}

int initfs(char* path, unsigned short blocks,unsigned short inodes) {

   unsigned int buffer[BLOCK_SIZE/4];
   int bytes_written;
   
   unsigned short i = 0;
   superBlock.fsize = blocks;
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
	if(inodes < 200)
		superBlock.ninode = inodes;
	else
		superBlock.ninode = 200;
   for (i = 0; i < superBlock.ninode; i++)
	    superBlock.inode[i] = i + 1;		//Initializing the inode array to inode numbers
   
   superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
   superBlock.ilock = 'b';					
   superBlock.fmod = 0;
   superBlock.time[0] = 0;
   superBlock.time[1] = 1970;
   
   lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
   write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system
   
   // writing zeroes to all inodes in ilist
   for (i = 0; i < BLOCK_SIZE/4; i++) 
   	  buffer[i] = 0;
        
   for (i = 0; i < superBlock.isize; i++)
   	  write(fileDescriptor, buffer, BLOCK_SIZE);
   
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
    free_list_data[0] = FREE_SIZE;
    
    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) {
       if ( i < FREE_SIZE ) {
         free_list_data[i + 1] = superBlock.free[i];
       } else {
         free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
       }
    }
    
    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, free_list_data, BLOCK_SIZE ); // Writing free list to header block
    
    superBlock.nfree = 0;
    
  } else {
	lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
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
  root.filename[1] = '\0';
  
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
  
  lseek(fileDescriptor, root_data_block, 0);
  write(fileDescriptor, &root, 16);
  
  root.filename[0] = '.';
  root.filename[1] = '.';
  root.filename[2] = '\0';
  
  write(fileDescriptor, &root, 16);
}

int cpin (char * externalfile, char* v6_file){
	int fde;
	if(fde = open(externalfile, O_RDONLY) == -1){
		printf("\n fail opening external file named %s\n", externalfile);
		return -1;
	}
	//Get the i_number
	unsigned short inum = allocateIno();
	printf("I_node number %d was allocated of new file.", inum);
	if(inum == 0){
		printf("No inode was left for allocation. Can not create new file in the system.\n");
		return -1;
	}
	//Add i_number and newly created file name to root directory
	addToRoot(inum, v6_file);
	//increase the nlinks of rootinode by 1
	inode.nlinks++;
	//construct a new inode of the file
	inode_type newInode;
	//Implement the inode for the newly created file
	newInode.flags = inode_alloc_flag | dir_access_rights;
	newInode.nlinks = 1;
	newInode.uid = 0;
	newInode.gid = 0;
	int fileRead;
	char fileBuffer[BLOCK_SIZE];
	int addrIndex = 0;
	printf("Start copying external file to file system.\n");
	while(1){
		if(fileRead = read(fde, fileBuffer, BLOCK_SIZE) != 0){
			printf("read %d bytes.\n", fileRead);
			//allocate a data block for the file
			int dataIndex = allocateData();
			printf("Get dataindex %d\n", dataIndex);
			//fill the addr[addrIndex] using new data block#
			newInode.addr[addrIndex] = dataIndex;
			printf("filling address %d\n", addrIndex);
			//fill the data block just be allocated with 
			lseek(fileDescriptor, BLOCK_SIZE * dataIndex, SEEK_SET);
			printf("Go to data block\n");
			write(fileDescriptor, fileBuffer, BLOCK_SIZE);
			printf("Write to data block\n");
			//calculate the size of the file when reaching the end of the file
		}
		if(fileRead < BLOCK_SIZE){
			int totalSize = addrIndex * BLOCK_SIZE + fileRead;
			newInode.size = totalSize;
			printf("Finish copying external file to the file %s.\n", v6_file);
			break;
		}
		addrIndex++;
	}
	newInode.actime[0] = 0;
	newInode.modtime[0] = 0;
	newInode.modtime[1] = 0;
	//write newInode to the position indicated by inum
	lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
	int writeInode = inum - 1;
	lseek(fileDescriptor, writeInode * 64, SEEK_CUR);
	write(fileDescriptor, &newInode, 64);
	printf("File %s was successfully created.\n", v6_file);
	return 1;
}
/*method to allocate data block for new file*/
int allocateData(){
	//when getting the cell has index 0 in free array
	int i;
	if(superBlock.nfree == 1){
		//get the block number
		unsigned int next = superBlock.free[0];
		//set the fd to the file pointed by block number
		lseek(fileDescriptor, next * BLOCK_SIZE, SEEK_SET);
		//the first two bytes of fd is value for new nfree
		unsigned short newFree;
		read(fileDescriptor, &newFree, 2);
		superBlock.nfree = newFree;
		//construct an array to contain new free list
		unsigned int temp[newFree];
		//read next nfree elements to temp buffer
		read(fileDescriptor, temp, newFree * 4);
		//replace free array with content in temp buffer
		for(i = 0; i < newFree; i++){
			superBlock.free[i] = temp[i];
		}
	}
	//decrease the nfree by 1 and return the block# pointed by free[nfree]
	superBlock.nfree--;
	return superBlock.free[superBlock.nfree];
}
/*method to allocate inode for new file, return the # of inode being allocated*/
 int allocateIno (){
	if(superBlock.ninode == 0)
		return -1;
	superBlock.ninode--;
	return superBlock.inode[superBlock.ninode];
}

int addToRoot(int inodeno, char* fileN){
	int fileFlag;
	int i, j;
	dir_type direct;
	inode_type rootInode;
	lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, &rootInode, INODE_SIZE);
	for(i = 0; i < ADDR_SIZE; i++){
		lseek(fileDescriptor, BLOCK_SIZE * rootInode.addr[i], SEEK_SET);
		for(j = 0; j < BLOCK_SIZE / INODE_SIZE; j++){
			read(fileDescriptor, &direct, 16);
			if(strcmp(fileN, direct.filename) == 0){
				printf("File with same name already exists. Choose a new file name\n");
				return -1;
			}
		}
	}
	printf("There is no file with same name in the file system. Continue...\n");
	dir_type currentDir;
	for(i = 0; i < ADDR_SIZE; i++){
		lseek(fileDescriptor, BLOCK_SIZE * rootInode.addr[i], 0); //Go the data block pointed by root inode which contains dir entries
		for( j = 0; j < (BLOCK_SIZE / sizeof(dir_type)); j++){
			read(fileDescriptor, &currentDir, 16);
			if(currentDir.inode == 0){//Available entry founnd
				currentDir.inode = inodeno;
				strcpy(currentDir.filename, fileN);
				lseek(fileDescriptor, -16, SEEK_CUR);
				write(fileDescriptor, &currentDir, 16);
				break;
			}				
			else
				lseek(fileDescriptor, 16, SEEK_CUR);
		}
	}
	printf("Successfully add file to root directory\n");
	return 1;
}
int cpout(char * v6file, char * externalfile){
	int i, j;
	dir_type direct;
	int iNum;
	int flag = 0;
	//search through the root directory to find the file
	for(i = 0; i < ADDR_SIZE; i++){
			if(flag = 1) break;
			lseek(fileDescriptor, BLOCK_SIZE * inode.addr[i], SEEK_SET);
		for(j = 0; j < BLOCK_SIZE / 16; j++){
			read(fileDescriptor, &direct, 16);
			if(strcmp(v6file, direct.filename) == 0){
				printf("Found file %s with i_number %d.\n", v6file, direct.inode);
				iNum = direct.inode;
				flag = 1;
				break;
			}
		}
	}
	if(flag == 0){
		printf("File %s was not found in the file system.\n", v6file);
		return -1;
	}
	int externalfd;
	if(externalfd = creat(externalfile, 0666) == -1){
		printf("Can not create external file %s. \n", externalfile);
		return -1;
	}
	//direct to the iNumth inode to get the addresses of the data_block for the xv6 file
	inode_type currentInode; //inode of the file
	lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
	lseek(fileDescriptor, (iNum - 1)* 64, SEEK_CUR);
	read(fileDescriptor, &currentInode, 64);
	char fileBuffer [BLOCK_SIZE];
	for(i = 0; i < 11; i++){
		int num_bytes;
		int bytes_write;
		lseek(fileDescriptor, BLOCK_SIZE * (currentInode.addr[i]), SEEK_SET);
		if(num_bytes = read(fileDescriptor, fileBuffer, BLOCK_SIZE) > 0)
		if(bytes_write = write(externalfd, fileBuffer, num_bytes) <= 0){
			return -1;
		}
	}
	printf("File %s successfully copied to file %s. \n", v6file, externalfile);
	return 1;
}