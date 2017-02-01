#include "filesystem.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "disk.h"

/*
int make_disk(char *name);    /* create an empty, virtual disk file            
int open_disk(char *name);    /* open a virtual disk (file)                *              
int close_disk();             /* close a previously opened disk (file)     *  

int block_write(int block, char *buf);
                              /* write a block of size BLOCK_SIZE to disk  
int block_read(int block, char *buf);
                              /* read a block of size BLOCK_SIZE from disk 
*/

/**********************************/
	Byte temp_data[16 * 42];	//buff
	
	metadata_t gsuperblock;	//B0
	direntry_t dir[8];	//B1-B8
	Byte inodestatues[8];	//B9
	Byte gdata_bitmap[8]; //B10
	inode_block_t inodeblock[8];	// B11 - B42
	empty_block gemptyblock[21];	// B43 - B63
	empty_block gdatablock[64];		// B64 - B127
	
	OFT goft[4];
	
	int fileNum = 0;
/**********************************/

int make_fs(char* disk_name)
{
	int namelen = strlen(disk_name);
	if (namelen > 4)
	{
		printf("File name exceeds 4 letters, please rename it\n")
		return -1;
	}
	
	if(make_disk(disk_name)!=0)
	return -1 ;
	if(open_disk(disk_name)==-1)
	return -1;
	
	// superblock B0
	metadata_t superblock;
	superblock.dir_entry = DIRENTRYCOUNT;
	superblock.inodebitmap = 0x00;
	superblock.databitmap = 0x00;
	superblock.inoderegion = 9;
	superblock.dataregion = 64;
	
	//direntry_t
	direntry_t entry[8] = {0};
	
	// inode_bitmap & data_bitmap	B9-B10
	Byte inode_bitmap[8] = {0};
	Byte data_bitmap[8] = {0};
	
	// 32个inode block， 每个占4blocks，每个byte映射一个data_block		B11-B42
	inode_block_t inode[32] = {0};
	
	// 32个unused blocks
	empty_block emptyblock[32];
	
	// 64个data block；
	empty_block datablock[64];
	
	block_write(1, (char*)&superblock); 
	block_write(8, (char*)&entry); 
	block_write(1, (char*)inode_bitmap);
	block_write(1, (char*)data_bitmap);
	block_write(32, (char*)inode);
	block_write(32, (char*)emptyblock);
	block_write(64, (char*)datablock);
	
	close_disk();
	
	return 0;
}

int mount_fs(char* disk_name)
{
	int i = 0;	
	
	if(open_disk(disk_name) == -1)
		return -1;
	block_read(42, temp_data);
	
	for(i = 0; i<8; i++)	// directory B1-B8
	{
		dir[i].statues = *(Byte*)temp_data + 16*1 + i*16;
		dir[i].fname[0] = *(char*)(temp_data + 16*1 + 1 + i*16);
		dir[i].fname[1] = *(char*)(temp_data + 16*1 + 2 + i*16);
		dir[i].fname[2] = *(char*)(temp_data + 16*1 + 3 + i*16);
		dir[i].fname[3] = *(char*)(temp_data + 16*1 + 4 + i*16);
		dir[i].flen = *(short*)(temp_data + 16*1 + 5 + i*16);
		dir[i].inode_num = *(Byte*)(temp_data + 16*1 + 7 + i*16);
	}
	
	for(i = 0; i<8; i++)
	{
		inodestatues[i] = *(Byte*) temp_data + 16*9 + i;
	}
	
	for(i = 0; i<8; i++)
	{
		gdata_bitmap[i] = *(Byte*) temp_data + 16*10 + i;
	}
	
	for(i = 0; i<8; i++)
	{
		inodeblock[i] = *(inode_block_t*) temp_data + 16*11 + i * 4 * 16;
	}
		
}

int dismount_fs(char* disk_name)
{
	int i = 0;
	open_disk(disk_name);
	
	block_write(1, (char*)&gsuperblock); 	//0
	
	for(i = 0; i<8; i++)					// 1-8
		block_write(1, (char*)&dir[i]); 
	
	block_write(1, (char*)inodestatues);		// 9.10
	block_write(1, (char*)gdata_bitmap);
	
	for(i = 0; i<8; i++)					// 8 * 4 = 32 11-42
	{
		block_write(1, (char*)&inodeblock[i]);
		block_write(1, (char*)(&inodeblock[i])+1*16);
		block_write(1, (char*)(&inodeblock[i])+2*16);
		block_write(1, (char*)(&inodeblock[i])+3*16);
	}
	for(i = 0; i<21; i++)					// 43 - 63
		block_write(1, (char*)gemptyblock + i);


	for(i = 0; i<64; i++)
		block_write(1, (char*)gdatablock + i);
	
	close_disk();	
	
	return 0;
}

int fs_create(char* name)
{
	int namelen = strlen(name);
	int namelen=0;
	int nameptr=0;
	int offset=0;
	char *string; // file name
	string = (char *) malloc(4);

	if (namelen > 4)
	{
		printf("File name exceeds 4 letters, please rename it\n")
		return -1;
	}
	
	if(fileNum >= 8)
	{
		printf("file number exceeded\n");
		return -1;
	}
	
	// judge dup
	for (nameptr = 0; nameptr<8; nameptr++){
				
		for (offset=0;offset<4;offset++)	   
			string[offset]=dir[nameptr].fname[offset];
		if (strcmp(string,name)==0){
			printf("Duplicate files\n");
			return -1;
		}
	}
	
	// write Dir
	fileNum ++;
	for (nameptr = 0; nameptr<8; nameptr++){
		if( (int)dir[nameptr].statues==0){
			dir[nameptr].statues = 1;		// 1byte statues
			
			for (offset=0;offset<namelen;offset++)		// 4 byte name
				dir[nameptr].fname[offset] = name[offset];
			
			dir[nameptr].flen = 0;				// 2 byte file length
			dir[nameptr].inode_num = nameptr;	// inode block number
			break;
		}
	}
	return 0;
}

int fs_open(char* name)
{
	int namelen=0;
	int nameptr=0;
	int offset=0;
	char string[4]; // file name
	int fildes=0;
	int i;
	int m=0;
    int found=0;

	// judge valid name or not
	namelen=strlen(name);
	if (namelen > 4)
		return -1;

	// judge OFT full or not
	for(i = 0; i<4; i++){
		if (goft[i].statues == 1)
			m++;
	}
	if (m>=4){
		printf("full OFT\n");
		return -1;
	}

	// judge whether the file is on the disk
	for (nameptr = 0; nameptr<8; nameptr++){
		fildes++;
		for (offset=0;offset<4;offset++)	   
			string[offset]=dir[nameptr].fname[offset];
		if (strcmp(string,name)==0){
			found=1;
		 	break;
		}
	}
	if (found!=1){
        printf("no such file\n");
		return -1;
	}

	// judge whether the file has been opend
	for(i = 0; i < 4; i++)
	{
		if (goft[i].index == fildes){
			printf("This fiel has been opened");
                   return -1;
			}
	}

	// open the file write the oft
	for(i = 0; i < 4; i++){
		if(goft[i].statues == 0){
			goft[i].statues = 1;
			goft[i].index = fildes;
			goft[i].offset = 0;
			break;
		}
	}
	return fildes;
}

int fs_close(int fildes)
{
	int namelen=0;
	int offset=0;
	int i;
	int found=0;
	
	// judge whether the file has been opend
	for(i = 0; i < 4; i++){
		if(goft[i].index == fildes){
		    found=1;
		    goft[i].statues = 0;

			break;
		}
	}
	if (found != 1){
		printf("no such opened file4\n");
		return -1;
	}
	
	
	return 0;
}

int fs_delete(char* name)
{
	int namelen = 0;
	int nameptr = 0;
	int offset = 0;
	int fildes = 0;
	char string[4]; // file name
	int i;
	int found = 0;
	int delptr = 0;

	// judge valid name or not
	namelen = strlen(name);
	if (namelen > 4)
		return -1;

	// judge whether the file is on the disk
	for (nameptr = 0; nameptr < 8; nameptr++){
		fildes++;
		for (offset=0;offset<4;offset++)	   
			string[offset]=dir[nameptr].fname[offset];
		if (strcmp(string,name) == 0){
			found = 1;
		 	break;
		}
	}
	if (found != 1){
        printf("no such file\n");
		return -1;
	}

	// judge whether the file has been opend
	for(i = 0; i < 4; i++){
		if (goft[i].index == fildes){
			printf("Has been opened cannot be deleted");
			return -1;
		}
	}
	
	// delete the file ( clean the dir_statues)
	dir[fildes - 1].statues = 0;
	fileNum --;
	for(i = 0; i<64; i++)
	{
		if(inodeblock[fildes].bitmap[i] == 1)
		{
			dsetDatablockAndinode(i, fildes);
			inodeblock[fildes].bitmap[i] = 0;
		}
	}
	
	
	return 0;
}

int fs_read(int fildes, void* buf, size_t nbyte)
{
	char *buffer;
	buffer = (char *) malloc(BUFFER_SIZE);
	char *tempbuffer;
	tempbuffer = (char *) malloc(BLOCK_SIZE);
	int i = 1;//length of the block series
	int j = 0;
	int foundfile = 0;
	int baseptr = 0;
	int blockloc[10];
	int flength = 0;
	int readoffset = 0;
	int off_set = 0;
	int temp_offset;
	
	
	// judge whether the file has been opend
	if (fildes == 0){
		printf("Invalid file name\n");
		return -1;
	}
	for(j = 0; j < 4; j++){
		if (goft[j].index == fildes){
		    foundfile = 1;
		    off_set = goft[j].offset;
		    break;
		}
	}
	if (foundfile != 1){
		printf("No such opened file\n");
		return -1;
	}
	
	// judge the filepointer
	flength = fs_get_filesize(fildes);
	if (off_set == flength){
		printf("At the end of file\n");
		return 0;
	}
 	// update OFT offset
	if ((off_set+nbyte) <= flength)
		goft[j].offset = off_set + nbyte;
	else
		goft[j].offset = flength;
	
	
	temp_offset = off_set;
	
	for(j = 0; j<64; j++)
	{
		if(inodeblock[fildes].bitmap[j] == 1)
		{
			if(temp_offset >= 16)
			{
				temp_offset -= 16;
				continue;
			}
			else{
				 baseptr = j;
				 break;
			}
		}
	}
	
	// read the whole file out;
		// first block location
	//blockloc[0] = (int)metaBuf[baseptr+7];
	blockloc[0] = gdatablock[baseptr];
	readoffset = temp_offset + nbyte;
	i = 0;
	
	for(j = baseptr + 1; j<64; j++)
	{
		if(readoffset >= 16)
		{
			if(inodeblock[fildes].bitmap[j] == 1)
			{
				i++;
				blockloc[i] = gdatablock[j];
				readoffset -= 16;
			}
		}
		
		else break;
	}
		// read the file in buffer
	// using offset and nbyte to write the tempBuf
	strncpy(tempBuf, blockloc + temp_offset, goft[j].offset - off_set;);

	return goft[j].offset - off_set;

}

int fs_write(int fildes, void* buf, size_t nbyte)
{
	char buffer[16];
	int i=1;
	int foundfile=0;
	int foundspace=0;
	int j=0;
	int m=0;
	int n=0;// length base
	int l=0; //length offset
	int baseptr=0;
	int blen=0;
	int fatptr=0;
	int linkptr=0;
	int off_set=0;
 	int blockloc[blen];
	int cur = 0;
	int write_left = 0;
	
	// judge the length for link list
	if (nbyte%16==0)
		blen=nbyte/16;
    else 
		blen=nbyte/16+1;

    // initialize blockloc
	for(j=0;j<blen;j++){
		blockloc[j]=0;
	}
	
	// judge whether the file has been opend
	if (fildes==0){
		printf("Invalid file name\n");
		return -1;
	}
	for(j=0;j<4;j++){
		if (goft[j].index == fildes){
		    off_set=goft[j].offset;
		    foundfile=1;
		    break;
		}
	}
	if (foundfile!=1){
        printf("No such opened file1\n");
		return -1;
	}
	
	m= nbyte/16;  // Num of blocks for current write 
	n= (nbyte+off_set)/16; // length base
	l= (nbyte+off_set)%16; //length offset
	
	// update the file length
	dir[fildes].flen = nbyte + off_set;
	for(i = 0; i<4; i++)
	{
		if(goft[i].index == fildes){
			goft[i].offset = nbyte + off_set;
			break;
		}
	}
	
	write_left = nbyte;

	while(write_left > 0)
	{
		cur = find_next_datablock(cur);
		if(cur == -1)
		{
			nbyte -= write_left;
			break;
		}
		for(i = 0; i<16 && i<write_left; i++)
		{
			gdatablock[cur].content[i] = (Byte)buf[cursor++];
		}
		setDatablockAndinode(cur, fildes);
		write_left -= 16;
	}
	return nbyte;
}

int find_next_datablock(int idx)
{
	int index = (idx+1)/8;
	int loffset = (idx+1)%8
	int i = 0;
	int j = 0;
	
	for(i = index; i<8; i++)
	{
		for(j = loffset; j<8; j++)
		{
			if((gdata_bitmap[i] >> j) & 0x01 == 1)
				return i*8 + j;
		}
	}
	return -1;
}

void setDatablockAndinode(int cur, int fildes)
{
	int index = cur/8;
	int loffset = cur%8;
	Byte temp = 1 << loffset;
	gdata_bitmap[index] | temp;
	
	inodeblock[fildes].bitmap[cur] = 1;
	
}

void dsetDatablockAndinode(int cur, int fildes)
{	
	int index = cur/8;
	int loffset = cur%8;
	Byte inff = 0xff;
	Byte temp = 1 << loffset;
	inff ^= temp;
	gdata_bitmap[index] &= inff;
	
	inodeblock[fildes].bitmap[cur] = 0;
	
}

int fs_get_filesize(int fildes)
{
	int i = 0;
	int found = 0;
	if(dir[fildes].status == 1)
		return dir[fildes].flen;
	
	return -1;
}

int fs_lseek(int fildes, off_t offset)
{
	int foundfile=0;
	int flength=0;
        
       flength=fs_get_filesize(fildes);
	// judge whether the file has been opend
	if (fildes==0){
		printf("Invalid file name\n");
		return -1;
	}
	for(i=0;i<4;i++){
		if (goft[i].index==fildes){
		    foundfile=1;
			break;
		}
	}
	if (foundfile!=1){
        printf("Invalid file name\n");
		return -1;
	
	}
        // judge whether the file is out of bound
	if(goft[i].offset+offset<0||goft[i].offset+offset>flength){
	 printf("File pointer out of bound\n"); 
         return -1;
	}
	goft[i].offset=goft[i].offset+offset;
	return 0;	
}

 int fs_truncate(int fildes, off_t length){
	char *tempbuffer;
	tempbuffer=(char *) malloc(BLOCK_SIZE);
	int i=1;//length of the block series
	int j=0;// use to track the OFT
	int m=0;// iteration 
	int n=0;// length base
	int l=0;// length offset
	int foundfile=0;
	int cutBlock=length/16;
	int baseptr=6*16+(fildes-1)*8;
	int blockloc[10];
	int flength=0;
	int linkptr=0;
	int off_set=0;
	
	// initialized the tempbuffer
	
           tempbuffer="";      
	
	// judge whether the file has been opend
	if (fildes==0){
		printf("Invalid file name\n");
		return -1;
	}
	for(j=0;j<4;j++){
		if (goft[j].index==fildes){
		    foundfile=1;
		    off_set=goft[j].offset;
		    goft[j].offset = length;//after the trucate the offset should be zero
		    break;
		}
	}
	if (foundfile!=1){
           printf("no such opened file3\n");
	return -1;
	}
	// judge the length of the file
	flength=fs_get_filesize(fildes);
	if(length>flength){
		printf("cannot extend file by this command\n");
		return -1;
	}
	// read the truncated length message into a buffer
	fs_read(fildes, tempBuf, length); 
	
	for(i = 0; i<64; i++)
	{
		if(inodeblock[fildes].bitmap[i] == 1)
		{
			dsetDatablockAndinode(i, fildes);
			inodeblock[fildes].bitmap[i] = 0;
		}
	}
	
	fs_write(fildes, tempBuf, length);
	
	return 0;
 }
	
	
	
	