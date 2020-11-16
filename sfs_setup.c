#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define block_size 1024

#define inode_bitmap_offset 1024
#define data_bitmap_offset 2048
#define inode_offset 3072
#define data_block_offset 54272

typedef struct inode
{
	mode_t mode;
	int size;
	uid_t uid;
	gid_t gid;
	time_t last_access_time;
	time_t last_modified_time;
	time_t create_time;
	short int hard_link_count;
	short int block_count;
	int data_block_pointers[5];

}inode;

void main()
{
	FILE* fptr;

	fptr = fopen("fs1","wb");

	int ibo = inode_bitmap_offset;
	int dbio = data_bitmap_offset;
	int io = inode_offset;
	int dbo = data_block_offset;

	fwrite(&ibo,sizeof(int),1,fptr);
	fwrite(&dbio,sizeof(int),1,fptr);
	fwrite(&io,sizeof(int),1,fptr);
	fwrite(&dbo,sizeof(int),1,fptr);

	char i = 0; 

	fseek(fptr, inode_bitmap_offset, SEEK_SET);

	for (int j = 0; j < 2*block_size; ++j)
	{
		fwrite(&i,sizeof(char),1,fptr);
	}

	//Creating root

	fseek(fptr, inode_bitmap_offset, SEEK_SET);
	i = 1;
	fwrite(&i,sizeof(int),1,fptr);	//updating inode bitmap

	inode inode_root;

	time_t now;
	time(&now);

	inode_root.size = 32;
	inode_root.last_access_time = now;
	inode_root.last_modified_time = now;
	inode_root.create_time = now;
	inode_root.mode = S_IFDIR|0777;
	inode_root.hard_link_count = 1;
	inode_root.block_count = 1;
	inode_root.uid = getuid();
	inode_root.gid = getgid();
	inode_root.data_block_pointers[0] = 54272;
	inode_root.data_block_pointers[1] = -1;
	inode_root.data_block_pointers[2] = -1;
	inode_root.data_block_pointers[3] = -1;
	inode_root.data_block_pointers[4] = -1;

	fseek(fptr, inode_offset, SEEK_SET);
	fwrite(&inode_root,sizeof(inode),1,fptr);

	fseek(fptr, data_bitmap_offset, SEEK_SET);
	fwrite(&i,sizeof(int),1,fptr);	

	typedef struct direntry
	{
		int inode_num;
		char filename[28];
	}direntry;

	direntry d;
	d.inode_num = 0;
	strcpy(d.filename,"."); 

	fseek(fptr, data_block_offset, SEEK_SET);
	fwrite(&d,sizeof(direntry),1,fptr);


}

