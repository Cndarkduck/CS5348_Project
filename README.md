# project2_5348
This is a group project of 2 people. Form your own group by finding a project buddy from the class.
V6 file system is highly restrictive. A modification has been done: Block size is 1024 Bytes, i-node size is 64 Bytes and i-node’s structure has been modified as well.
You are provided with the source for a program called fsaccess.c that has the changes to the super block and i-nodes. Use the code made available to you as the starting point.
In the program fsaccess.c, the following command has been implemented.
$ initfs fname n1 n2
fname is the name of the (external) file in your Unix machine that represents the V6 file system.
n1 is the number of blocks in the disk (fsize) and n2 is the total number of i-nodes.
This command initializes the file system. All data blocks are in the free list (except for one data block that is allocated to the root /.
An example is: initfs /user/venky/disk 8000 300
You need to implement the following commands: Keep in mind that all files in v6 file system that you need to deal with in this project part are in the root directory and there will be no subdirectories.
(a) cpin externalfile v6-file
Creat a new file called v6-file in the v6 file system and fill the contents of the newly created file with the contents of the externalfile.
(b) cpout v6-file externalfile
If the v6-file exists, create externalfile and make the externalfile's contents equal to v6-file.
(c) q
Save all changes and quit. Keep in mind that all file names not starting with / are absolute path names and those not starting with / are relative to current working directory. Due date: October 30, 2020 11:55 pm.
How do I read block # 100 of the file system? [I opened the file /user/Venky/disk]
fd=open(“/user/Venky/disk”,2);
lseek(fd,100*blocksize,0); //move to the position after skipping 100*blocksize number of bytes //starting from 0 (beginning of file). Do a $ man lseek
