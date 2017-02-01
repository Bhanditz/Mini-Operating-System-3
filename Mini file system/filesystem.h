#include <time.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAXBLOCKS 128
#define BLOCKSIZE 16
#define FATENTRYCOUNT 8 //(BLOCKSIZE / sizeof(fatentry_t))
#define DIRENTRYCOUNT 8 //((BLOCKSIZE - (2*sizeof(int))) / sizeof(direntry_t))
#define MAXNAME 4
#define MAXPATHLENGTH 1024

#define UNUSED -1
#define ENDOFCHAIN 0
#define EOF -1

#define DATA 0
// #define FAT 1
#define DIR 2

typedef unsigned char Byte;

// create a type fatentry_t, we set this currently to short (16-bit)
//typedef short fatentry_t;

// a FAT block is a list of 16-bit entries that form a chain of disk addresses

//const int   fatentrycount = (blocksize / sizeof(fatentry_t));

typedef struct meta0 {
	short 	dir_entry;
	Byte	inodebitmap;
	Byte	databitmap;
	short	inoderegion;
	short	dataregion;
} metadata_t;
// disk metadata 一个block					B0

typedef struct direntry {
  Byte 	statues;
  char 	fname[4];
  short	flen;
  Byte	inode_num;
  long	no;
} direntry_t;
// directory entry 包含有16个Byte			B1-B8


// typedef Byte inode_bitmap //[DIRENTRYCOUNT]
// 8个byte，映射8个inode block的statues		B9
//typedef Byte data_bitmap //[DIRENTRYCOUNT]
// 8个byte，映射8个data block的statues		B10

typedef struct inode_block {
	Byte bitmap[16 * 4];
} inode_block_t;
// 32个inode block， 每个占4blocks x 16byte，每个byte映射一个data_block		B11-B42

typedef struct empty_block {
	Byte content[16];
}
// unused									B42-B63

// data 									B64-B127


typedef openfiletable {
	short statues;
	int offset;
	int index;	
} OFT;

int make_fs(char* disk_name);
int mount_fs(char* disk_name);
int dismount_fs(char* disk_name);

int fs_create(char* name);
int fs_open(char* name);
int fs_close(int fildes);
int fs_delete(char* name);
int fs_read(int fildes, void* buf, size_t nbyte);
int fs_write(int fildes, void* buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);
