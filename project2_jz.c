/*
 Author: Jinglun Zhang
 UTD ID: 2021543942
 Collaborator/Team partner: Yongli Shan
 UTD ID: 2021541803
 Prof.S Venkatesan
 Project-2
 
 Modifications include
 int savesuper_block(); //A function to update superblock if it's modified 
 int printfile();//A function to print the file names in the root directory
 int cpin(char *externalfile, char *v6_file);//copy externalFile to V6 internal file
 int allocateData(); //allocate block to write data
 int allocateIno(); // allocate inode to make new fiel or directory
 int addToRoot(int inodeno, char* fileN); //Add dir_type with inodeno and fileN to root directory
 int cpout(char *v6file, char *externalfile);//copy internal V6 file to external file and save
 Also change codes provided by instructor.
 
 This program allows user to do two things: 
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large 
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. Quit: save all work and exit the program.
   3. cpin: Copy external file to internal v6 file.
   4. cpout: Copyt internal v6 file to external file.
   
 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - q
	 - cpin (external file path) (internal file path)
	 - cpout (internal file path) (external file path)
     
 File name is limited to 14 characters.
 Compilation: gcc project2_jz.c -o project2.out
 Run using: ./project2.out
 */

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
  printf(">> ");
  
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
			printf(">> ");
		}
		splitter = NULL;
	}
	else if(strcmp(splitter, "cpout") == 0){
		char *xv6file = strtok(NULL, " ");
		char *externalfile = strtok(NULL, " ");
		if(cpout(xv6file, externalfile) == -1){
			printf("Failing to copy xv6 file %s to external file %s.\n", xv6file, externalfile);
			printf(">> ");
		}
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
      
      if((fileDescriptor = open(filepath, O_RDWR, 0600)) == -1){ //JZ:Correct the typo
      
         printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
         return 1;
      }
	  //JZ: To load the existing file system.
	  lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
	  read(fileDescriptor, &superBlock, BLOCK_SIZE);
      printf("filesystem already exists and the same will be used.\n");
	  printf("Fsize: %d.\n", superBlock.fsize);
	  printf("Index for inode array: %d\n", superBlock.ninode);
	  printf("Isize: %d\n", superBlock.isize);
	  read(fileDescriptor, &inode, 64);
	printf("Finish restoring previous file system.\n");
	printf(">> ");
  } else {
  
        	if (!n1 || !n2){
              printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
			  printf(">> ");
            }
       		else {
          		numBlocks = atoi(n1);
          		numInodes = atoi(n2);
          		
          		if( initfs(filepath,numBlocks, numInodes )){
          		printf("The file system is initialized\n");
				printf(">> ");
          		} else {
            		printf("Error initializing file system. Exiting... \n");
            		return 1;
          		}
       		}
  }
}

int initfs(char* path, unsigned short blocks,unsigned short inodes) {
	
	//realInode = inodes; //JZ:deleted
   unsigned int buffer[BLOCK_SIZE/4];
   int bytes_written;
   
   unsigned short i = 0;
   superBlock.fsize = blocks;
   unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;
   
   if((inodes%inodes_per_block) == 0)
   superBlock.isize = inodes/inodes_per_block;
   else
   superBlock.isize = (inodes/inodes_per_block) + 1;
   
   if((fileDescriptor = open(path,O_RDWR|O_CREAT,0777))== -1)
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
	//inodeNum = superBlock.ninode; //JZ: New added line for the case when starting number of inodes less than 200
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
        
   for (i = 0; i < superBlock.isize; i++){
	  lseek(fileDescriptor, BLOCK_SIZE * (2 + i), SEEK_SET);//JZ: Change according to YS
   	  write(fileDescriptor, buffer, BLOCK_SIZE);
   }
   
   //int data_blocks = blocks - 2 - superBlock.isize; //JZ:Deleted
   int data_blocks_for_free_list = superBlock.fsize;
   
   // Create root directory
   create_root();
 
   for ( i = 2 + superBlock.isize + 1; i < data_blocks_for_free_list; i++ ) {
      add_block_to_free_list(i , buffer);
   }
   //JZ: Write superblock to file system
   lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
   write(fileDescriptor, &superBlock, BLOCK_SIZE);
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
  
  lseek(fileDescriptor, root_data_block * BLOCK_SIZE, 0); //JZ: Change according to YS
  write(fileDescriptor, &root, 16);
  
  root.filename[0] = '.';
  root.filename[1] = '.';
  root.filename[2] = '\0';
  
  write(fileDescriptor, &root, 16);
}
/*JZ:Method get from Yongli*/
//A function to update superblock if it's modified
int savesuper_block() {
    superBlock.time[0] = (unsigned int)time(NULL);
    lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, &superBlock, sizeof(superblock_type));
    return 0;
}
//JZ:Methods for printing file names in root directory
int printfile(){
	inode_type rootInode;
	dir_type curDir;
	int i, j;
	lseek(fileDescriptor, 2*BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, &rootInode, 64);
	printf("Files in file system: ");
	for(i = 0; i < ADDR_SIZE; i++){
		if(rootInode.addr[i] == 0)
			break;
		lseek(fileDescriptor, rootInode.addr[i] * BLOCK_SIZE, SEEK_SET);
		for(j = 0; j < BLOCK_SIZE / 16; j++){
			read(fileDescriptor, &curDir, 16);
			if(curDir. inode == 0){
				break;
			}
			if(strcmp(curDir.filename, ".") != 0 && strcmp(curDir.filename, "..") != 0){
				printf(" %s ", curDir.filename);
			}
		}
	}
	printf("\n");
	return 1;
}
/*method to allocate data block for new file*/
unsigned int allocateData(){
	//when getting the cell has index 0 in free array
	int i;
	unsigned int result;
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
	result = superBlock.free[superBlock.nfree];
	superBlock.free[superBlock.nfree] = 0;
	//JZ:Save superblock
	savesuper_block();
	return result;
}
//copy externalFile to V6 internal file
int cpin(char *externalfile, char *v6_file){
	int fde;
	int x;
	fde = open(externalfile, O_RDONLY);
	if(fde == -1){
		printf("Fail opening external file named %s\n", externalfile);
		return -1;
	}
	//Get the i_number
	unsigned short inum = allocateIno();
	printf("I_node number %d was allocated of new file.\n", inum);
	if(inum == 0 || inum == -1){
		printf("No inode was left for allocation. Can not create new file in the system.\n");
		return -1;
	}
	//Add i_number and newly created file name to root directory
	if(addToRoot(inum, v6_file) == -1){
		printf("Error when writing to root directory.\n");
		return -1;
	}
	//construct a new inode of the file
	inode_type newInode;
	//Implement the inode for the newly created file
	newInode.flags = inode_alloc_flag | dir_access_rights;
	newInode.nlinks = 1;
	newInode.uid = 0;
	newInode.gid = 0;
	int fileRead;
	char fileBuffer[1024];
	int addrIndex = 0;
	printf("Start copying external file to file system.\n");
	while(1){
		fileRead = read(fde, fileBuffer, 1024);
		if(fileRead > 0){
			printf("Read %d bytes from external file.\n", fileRead);
			//allocate a data block for the file
			unsigned int dataIndex = allocateData();
			//fill the addr[addrIndex] using new data block#
			newInode.addr[addrIndex] = dataIndex;
			//fill the data block just be allocated with 
			lseek(fileDescriptor, BLOCK_SIZE * dataIndex, SEEK_SET);
			write(fileDescriptor, fileBuffer, BLOCK_SIZE);		
		}
		if(fileRead < BLOCK_SIZE){
			//calculate the size of the file when reaching the end of the file
			int totalSize = addrIndex * BLOCK_SIZE + fileRead;
			newInode.size = totalSize;
			printf("Finish copying external file %s to the file %s. The size of the files are %d bytes.\n", externalfile, v6_file, totalSize);
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
	printfile();
	printf(">> ");
	return 1;
}

/*method to allocate inode for new file, return the # of inode being allocated*/
 //JZ: Method for allocating inode when superBlock.ninode == 0.
 int allocateIno (){
	int i, inodeNum; //JZ:inodeNum is for when the starting number of inode less than 200 
	inode_type curInode;
	if(superBlock.ninode == 0){
		for(i = 0; i < I_SIZE; i++){
			if(superBlock.inode[i] == 0){
				inodeNum = i;
				break;
			}
		}
		inodeNum++;
		lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
		for(i = 1; i <= inodeNum && i <= I_SIZE; i++){
			read(fileDescriptor, &curInode, 64);
			if(curInode.flags & 0100000 == 0){
				superBlock.inode[superBlock.ninode] = i;
				superBlock.ninode++;
			}
		}	
	}
	if(superBlock.ninode == 0){
		printf("Run out off inode. Exiting.\n");
		return -1;
	}
	superBlock.ninode--;
	savesuper_block();//JZ: Change according to YS
	return superBlock.inode[superBlock.ninode];
}
//Add dir_type with inodeno and fileN to root directory
int addToRoot(int inodeno, char* fileN){
	int fileFlag;
	int i, j;
	dir_type direct;
	inode_type rootInode;
	int newData;//JZ:
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
	rootInode.nlinks++;
	dir_type currentDir;
	for(i = 0; i < ADDR_SIZE; i++){
		//JZ: Escape the two-layer loop when fileFlag is true
		if(fileFlag == 1)
			break;
		//JZ: When the first data block of root directory has been used up, allocate new data block to root directory.
		if(rootInode.addr[i] == 0){
			newData = allocateData();
			rootInode.addr[i] = newData;
		}
		lseek(fileDescriptor, BLOCK_SIZE * rootInode.addr[i], SEEK_SET); //Go the data block pointed by root inode which contains dir entries
		for( j = 0; j < (BLOCK_SIZE / sizeof(dir_type)); j++){
			read(fileDescriptor, &currentDir, 16);
			if(currentDir.inode == 0){//Available entry founnd
				currentDir.inode = inodeno;
				strcpy(currentDir.filename, fileN);
				lseek(fileDescriptor, -16, SEEK_CUR);
				write(fileDescriptor, &currentDir, 16);
				//JZ: Set the fileFlag to 1 when file added.
				fileFlag = 1;
				break;
			}				
			/*else
				lseek(fileDescriptor, 16, SEEK_CUR);*/ //JZ:Deleted since read will also move the filedescriptor.
		}
	}
	if(fileFlag == 0){
		printf("Fail to write the file to root directory.\n");
		return -1;
	}
	//JZ: Write the content of new root inode to file system
	lseek(fileDescriptor, 2 *BLOCK_SIZE, SEEK_SET);
	write(fileDescriptor, &rootInode, 64);
	printf("Successfully add file to root directory\n");
	return 1;
}
//copy internal V6 file to external file and save
int cpout(char *v6file, char *externalfile){
	int i, j;
	dir_type direct;
	int iNum;
	int flag = 0;
	inode_type rootinode;
	printfile();
	lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, &rootinode, 64);
	//search through the root directory to find the file
	for(i = 0; i < 11; i++){
			if(flag == 1) break;
			lseek(fileDescriptor, BLOCK_SIZE * rootinode.addr[i], SEEK_SET);
		for(j = 0; j < BLOCK_SIZE / 16; j++){
			if(flag == 1) break;
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
	externalfd = open(externalfile, O_RDWR | O_CREAT, 0666);
	if(externalfd == -1){
		printf("Can not create external file %s. \n", externalfile);
		return -1;
	}
	//direct to the iNumth inode to get the addresses of the data_block for the xv6 file
	inode_type currentInode; //inode of the file
	lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
	lseek(fileDescriptor, (iNum - 1)* 64, SEEK_CUR);
	read(fileDescriptor, &currentInode, 64);
	printf("The size of the copied xv6 file is %d.\n", currentInode.size);
	char fileBuffer[BLOCK_SIZE]; //JZ: Variable for new coping process
	int bytes_write;//
	int totalBytes;//
	int num_bytes;//
	int blockNum = currentInode.size / 1024;//
	int remainingBytes = currentInode.size % 1024;//
	i = 0;
	while(1){
		if(currentInode.addr[i] == 0)
			break;
		lseek(fileDescriptor, BLOCK_SIZE * (currentInode).addr[i], SEEK_SET);
		if(i == blockNum){
			num_bytes = read(fileDescriptor, fileBuffer, remainingBytes);
			write(externalfd, fileBuffer, num_bytes);
			totalBytes += num_bytes;
			break;
		}
		num_bytes = read(fileDescriptor, fileBuffer, 1024);
		totalBytes += num_bytes;
		bytes_write = write(externalfd, fileBuffer, num_bytes);
		if(bytes_write <= 0){
			printf("Error when copying xv6 file to external file.\n");
			return -1;
		}
		i++;
	}
	printf("Finishing copying %d bytes to external file.\n", totalBytes);
	close(externalfd); //JZ_Change according to YS
	printf("File %s successfully copied to file %s. \n", v6file, externalfile);
	printf(">> ");
	return 1;
}