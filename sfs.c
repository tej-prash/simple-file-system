#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>

#define inode_bitmap_offset 1024
#define data_bitmap_offset 2048
#define inode_offset 3072
#define data_block_offset 54272
#define block_size 1024
#define direntry_size 32

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

typedef struct direntry
{
	int inode_num;
	char filename[28];
}direntry;

unsigned char buffer_block[1024];



long int get_inode_block_address(int inode_number)
{
	int block_num = inode_number/16;
	return inode_offset+block_num*block_size;
}

long int get_data_block_address(int data_block_num)
{
	return data_block_offset+data_block_num*block_size;
}

int read_block_num(int fd, long int address)
{
	lseek(fd, address, 0);
	read(fd, buffer_block, 1024);
	return 0;
}

int write_block_num(int fd, long int address)
{
	lseek(fd, address, 0);
	write(fd, buffer_block, 1024);
	return 0;
}



//flag=0->find free inode block
//flag=1->find free data block
int find_free_block(int fd,int flag)
{
	int count=-1;
	char rd;
	if(!flag)
	{
		//lseek(fd,inode_bitmap_offset,SEEK_SET);
		read_block_num(fd, inode_bitmap_offset);
	}
	else
	{
		//lseek(fd,data_bitmap_offset,SEEK_SET);
		read_block_num(fd, data_bitmap_offset);
	}
	printf("Reading done\n");
	do
	{
		//read(fd,&rd,sizeof(char));		
		//count++;
		count++;
		memcpy(&rd,buffer_block+count,sizeof(char));
	}while(rd);
	printf("\tFree block is %d\n",count); 
	return count;
}


void free_block(int fd , int block_num, int flag)
{
	char y=0;
	if(!flag)
	{
		//lseek(fd,inode_bitmap_offset+block_num,SEEK_SET);
		read_block_num(fd, inode_bitmap_offset);
		buffer_block[block_num]=y;
		write_block_num(fd, inode_bitmap_offset);
	}
	else 
	{
		block_num=(block_num-data_block_offset)/block_size;
		//lseek(fd,data_bitmap_offset+block_num,SEEK_SET);
		read_block_num(fd, data_bitmap_offset);
		buffer_block[block_num]=y;
		write_block_num(fd, data_bitmap_offset);
	}
	
	//write(fd,&y,sizeof(char));

}

//Returns the inode offset of the file to be read. -1 in case of failure
int locate_file_persistent(const char* path){

	printf("Locate file called with path %s\n",path);

	if(strcmp(path,"/")==0)
			return 0;
	
	int fd;
	fd=open("fs1",O_RDWR);
	
	if(fd==-1)
		{
			printf("Error opening file");
			return -1;
		}
	
	


	//go to root data block
	//lseek(fd,data_block_offset,SEEK_SET);
	char *buffer=(char *)malloc(strlen(path));
	strcpy(buffer,path);

	//iterate through the path to locate the directory
	char *token=strtok(buffer,"/");
	inode n;		
	int ino_num;


	//reading the root inode
	lseek(fd,inode_offset,SEEK_SET);
	read(fd,&n,sizeof(inode));
	while(token!=NULL){
		//search for token in datablock
		printf("token = %s\n",token);
		if(S_ISREG(n.mode))
		{
			printf("This is a file!\n");
			return -1;
		}

		//int no_of_blocks = n.size/block_size;
		printf("\tNo of blocks in this directory = %d\n",n.block_count);		

		for(int i=0;i<n.block_count;i++){
			int parent_dir_offset=(n.data_block_pointers)[i];
			if(parent_dir_offset==-1)continue;
			direntry d;
			int size_block;
			if(i+1==n.block_count)
				size_block=n.size%block_size;
			else
				size_block=block_size;
			printf("\tSize of block %d = %d\n",i,size_block);
			//go to corresponding data block, which may contain the entry for sub-directory/file
			for(int k=0;k<size_block/direntry_size;k++){
				lseek(fd,parent_dir_offset+(k*direntry_size),SEEK_SET);
				read(fd,&d,direntry_size);
				printf("\tName of this direntry is %s\n",d.filename);
				ino_num=strcmp(token,d.filename)?-1:d.inode_num;
				if(ino_num!=-1)break;
			}	
			if(ino_num==-1)continue;
			else break;
		}	
		lseek(fd,inode_offset+(ino_num*sizeof(inode)),SEEK_SET);
		//read inode 
		read(fd,&n,sizeof(inode));
		token=strtok(NULL,"/");
	}
	free(buffer);
	close(fd);
    return ino_num;
}
int create_data_block(int fd,inode *n)
{
	int data_block_num=find_free_block(fd,1);
	int data_block=(data_block_offset)+(block_size*data_block_num);
	int i=0;
	while(n->data_block_pointers[i]!=-1 && i<5)i++;

	if(i==5)return -1;
	n->data_block_pointers[i]=data_block;
	return data_block;
}
static int do_truncate(const char *path, off_t length){
	printf("[in truncate]\n");
	printf("%d\n",length);
	char *buff=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff,path);
	int inode_num=locate_file_persistent(buff);
	int fd=open("fs1",O_RDWR);
	read_block_num(fd,get_inode_block_address(inode_num));
	inode n;
	memcpy(&n,buffer_block+((inode_num%16)*sizeof(inode)),sizeof(inode));
	int start_data_block=length/block_size;
	printf("start data block %d\n",start_data_block);
	int i=start_data_block;
	if(length%block_size)i++;
	int data_off=n.data_block_pointers[i];
	//int num_blocks_to_read=size/block_size +1;
	//char *text=(char *)malloc(sizeof(char)*(size+1));
	//int count=0;
	/*while((data_off=n.data_block_pointers[i])!=-1 && num_blocks_to_read--){
		
		read_block_num(fd,data_off);
		strncpy(text+count*block_size,buffer_block,block_size);
		//lseek(fd,data_off,SEEK_SET);
		//read(fd,text+block_size*count,block_size);
		count++;
		i++;
	}*/

	read_block_num(fd,data_bitmap_offset);
	//All blocks from data_off must be free
	while(i<5 && (data_off=n.data_block_pointers[i])!=-1){
		int data_block_num=(data_off-data_block_offset)/block_size;
		buffer_block[data_block_num]=0;
		if(n.block_count)n.block_count--;
		i++;
	}
	write_block_num(fd,data_bitmap_offset);

	n.size=length;

	read_block_num(fd,get_inode_block_address(inode_num));
	memcpy(buffer_block+((inode_num%16)*sizeof(inode)),&n,sizeof(inode));
	write_block_num(fd,get_inode_block_address(inode_num));
	free(buff);
	close(fd);

}
static int do_open(const char* path, struct fuse_file_info* fi)
{
	char *buff=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff,path);
	int inode_num=locate_file_persistent(buff);
	if(inode_num==-1)return -ENOENT;
	else{
		int fd = open("fs1",O_RDWR);
		inode n;

		read_block_num(fd,get_inode_block_address(inode_num));
		memcpy(&n,buffer_block+(inode_num%16)*sizeof(inode),sizeof(inode));

		//lseek(fd,inode_offset+(sizeof(inode)*inode_num),SEEK_SET);
		//read(fd,&n,sizeof(inode));
		short int m=n.mode&4095;
		if(n.uid==getuid()){
			
			//check read permissions
			if(m&256){
				return 0;
			}
			return -EACCES;
		}
		if(n.gid==getgid()){
			//check group read permissions
			//check read permissions
			if(m&32){
				return 0;
			}
			return -EACCES;
		}
		else{
			if(m&4){
				return 0;
			}
			return -EACCES;
		}
		close(fd);
	}
	free(buff);
	
	return 0;
}

int min(int a,int b)
{
	return a>b?b:a;
}
static int do_unlink (const char *path){
	printf("[unlink] called with path %s\n",path);
	char *buff=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff,path);
	int inode_num=locate_file_persistent(buff);
	if(inode_num==-1)return -ENOENT;

	int parent_inode_num=locate_file_persistent(dirname(buff));
	if(parent_inode_num==-1)return -ENOENT;

	int fd=open("fs1",O_RDWR);
	read_block_num(fd,get_inode_block_address(inode_num));
	inode n;
	memcpy(&n,buffer_block+(inode_num%16)*sizeof(inode),sizeof(inode));
	int data_block_addr=0;
	int i=0;

	//mark corresponding data blocks as being free
	read_block_num(fd,data_bitmap_offset);


	while((data_block_addr=n.data_block_pointers[i])!=-1){
		int data_block_num=(data_block_addr-data_block_offset)/block_size;
		buffer_block[data_block_num]=0;
		i++;
	}
	write_block_num(fd,data_bitmap_offset);
	printf("Removed data blocks\n");
	//mark the inode as free
	read_block_num(fd,inode_bitmap_offset);
	buffer_block[inode_num]=0;
	write_block_num(fd,inode_bitmap_offset);
	printf("Marked inode as free\n");
	//read parent directory's inode
	read_block_num(fd,get_inode_block_address(parent_inode_num));
	inode parent_inode;
	memcpy(&parent_inode,buffer_block+(parent_inode_num%16)*sizeof(inode),sizeof(inode));


	//delete direntry
	i=0;
	data_block_addr=0;
	int num_entries=0;
	direntry *d;
	int flag=0;
	int location=0;
	char *buff_path=(char *)malloc(sizeof(char)*28);
	strcpy(buff_path,basename(path));
	while(!flag && (data_block_addr=parent_inode.data_block_pointers[i])!=-1){
		//read data block
		read_block_num(fd,data_block_addr);
		printf("Parent inode size\n");
		printf("%d",parent_inode.size);
		if(parent_inode.size>=(i+1)*block_size){
			num_entries=block_size/direntry_size;			
		}
		else{
			//
			num_entries=(parent_inode.size%block_size)/direntry_size;
			printf("Num entries %d\n",num_entries);
		}	
		d=(direntry *)malloc(direntry_size*num_entries);
		int j=0;
		while(j<num_entries){
			memcpy(d+j,buffer_block+j*direntry_size,direntry_size);	
			printf("-----%s---- ",d[j].filename);
			if(!strcmp(d[j].filename,buff_path)){
				flag=1;
				location=j;
			}	
			j++;
		}
		i++;
	}
	i--;
	//j--;
	// if(num_entries==1){
	// 	//special case
	// 	printf("Special case\n");
	// 	printf("Data block %d of inode\n",i);
	// 	if(parent_inode.size>block_size){
	// 		int data_block_num=(parent_inode.data_block_pointers[i]-data_block_offset)/block_size;
	// 		read_block_num(fd,data_bitmap_offset);
	// 		buffer_block[data_block_num]=0;
	// 		write_block_num(fd,data_bitmap_offset);
	// 		parent_inode.data_block_pointers[i]=-1;
	// 	}
	// }
	
		printf("value of location,num entries is %d %d\n",location,num_entries);
		for(int k=location+1;k<num_entries;k++){
			d[k-1]=d[k];
		}
		for(int i=0;i<num_entries-1;i++)
		{
			printf("`````%s```` ",d[i].filename);
		}
		//for(int k=0;k<num_entries-1;k++){
			//memcpy(buffer_block+k*direntry_size,&d[k],direntry_size);
		//}
		for(int i=0;i<1024;i++)
		{

			buffer_block[i]=0;
		}
		memcpy(buffer_block,d,direntry_size*(num_entries-1));

		write_block_num(fd,data_block_addr);
		

	
	printf("deleted direntry\n");
	//update parent directory's inode's access time, modified time
	parent_inode.size=parent_inode.size-direntry_size;
	time_t now;
	time(&now);
	parent_inode.last_access_time=now;
	parent_inode.last_modified_time=now;
	read_block_num(fd,get_inode_block_address(parent_inode_num));
	memcpy(buffer_block+(parent_inode_num%16)*sizeof(inode),&parent_inode,sizeof(inode));
	write_block_num(fd,get_inode_block_address(parent_inode_num));
	read_block_num(fd,data_block_addr);
	printf("****%d*** ",parent_inode.size);
	int ne=parent_inode.size/32;
	direntry dj;
	memcpy(&dj,buffer_block+2*direntry_size,direntry_size);
	printf("******%s******",dj.filename );
	// for(int i=0;i<ne;i++)
	// {
	// 	memcpy(&dj,buffer_block+i*direntry_size,direntry_size);	
	// 	printf("*****%s***",dj.filename);
	// }
	free(d);
	free(buff_path);
	close(fd);
	return 0;
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	char *buff=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff,path);
	int fd = open("fs1",O_RDWR);
	int node_exists=locate_file_persistent(buff);
	if(node_exists!=-1)return -EEXIST;
	char *buff2=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff2,path);
	int parent_inode_num=locate_file_persistent(dirname(buff2));
	inode parent_inode;

	//read the parent inode datablock
	int parent_inode_address=get_inode_block_address(parent_inode_num);
	int ret=read_block_num(fd,parent_inode_address);
	if(ret==-1)return -EPERM;

	memcpy(&parent_inode,buffer_block+(parent_inode_num%16)*sizeof(inode),sizeof(inode));

	//lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
	//read(fd,&parent_inode,sizeof(inode));

	int i=0;
	int start_data_block=parent_inode.size/block_size;
	if(!(parent_inode.size%block_size)){
		printf("Allocate a new data block\n");
	}
	else{
		printf("In else clause");
		//update entries of parent directory
		char y = 1;
		//find inode number
		int ino_num_child=find_free_block(fd,0);
		printf("Free inode block = %d\n",ino_num_child);
		
		int ret=read_block_num(fd,inode_bitmap_offset);
		if(ret==-1)return -EPERM;

		buffer_block[ino_num_child]=1;	
		write_block_num(fd,inode_bitmap_offset);	

		//lseek(fd,inode_bitmap_offset+ino_num_child,SEEK_SET);
		//write(fd,&y,sizeof(char));

		//create new entry;
		char *buff3=(char *)malloc(sizeof(char)*strlen(path));
		strcpy(buff3,path);
		direntry d;
		printf("buff %s\n",buff3);
		strcpy(d.filename,basename(buff3));
		printf("Creating new file with name = %s\n",d.filename);
		d.inode_num=ino_num_child;
		int free_dir_ent=(parent_inode.size%block_size)/direntry_size;
		printf("Write direntry into address: %d\n",parent_inode.data_block_pointers[start_data_block]+(free_dir_ent*direntry_size));
		
		read_block_num(fd,parent_inode.data_block_pointers[start_data_block]);
		memcpy(buffer_block+(free_dir_ent*direntry_size),&d,direntry_size);
		int x=write_block_num(fd,parent_inode.data_block_pointers[start_data_block]);

		//lseek(fd,parent_inode.data_block_pointers[start_data_block]+(free_dir_ent*direntry_size),SEEK_SET);
		//int x=write(fd,&d,direntry_size);
		time_t now;
		time(&now);
		/*
		fflush(fd);
		if(fflush(fd)==EOF || !x)
		{
			perror("Did not write\n");
		}
		*/
		//fill inode of parent directory
		parent_inode.size=parent_inode.size+direntry_size;
		parent_inode.last_access_time=now;
		parent_inode.last_modified_time=now;

		printf("Write parent inode in address %lu\n",inode_offset+(sizeof(inode)*parent_inode_num));
		printf("New size of parent directory = %d\n",parent_inode.size);
		

		parent_inode_address=get_inode_block_address(parent_inode_num);
		ret=read_block_num(fd,parent_inode_address);
		if(ret==-1)return -EPERM;

		memcpy(buffer_block+(parent_inode_num%16)*sizeof(inode),&parent_inode,sizeof(inode));
		ret=write_block_num(fd,parent_inode_address);
		if(ret==-1)return -EPERM;

		//lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
		//x=write(fd,&parent_inode,sizeof(inode));
		/*
		if(fflush(fd)==EOF || !x)
		{
			perror("Did not write\n");
		}
		*/
		//for every sub-directory 


		//create a new datablock for directory
		
		int data_block_num=find_free_block(fd,1);
		printf("Free data block = %d\n",data_block_num);

		read_block_num(fd,data_bitmap_offset);
		buffer_block[data_block_num]=1;
		write_block_num(fd,data_bitmap_offset);

		//lseek(fd,data_bitmap_offset+data_block_num,SEEK_SET);
		//write(fd,&y,sizeof(char));
		//fill inode structure

		inode inode_child;
		inode_child.size = 0;
		inode_child.last_access_time = now;
		inode_child.last_modified_time = now;
		inode_child.create_time = now;
		inode_child.mode = S_IFREG|mode;
		inode_child.hard_link_count = 1;
		inode_child.block_count = 1;
		inode_child.uid = getuid();
		inode_child.gid = getgid();
		inode_child.data_block_pointers[0] = data_block_offset+(block_size*data_block_num);
		inode_child.data_block_pointers[1] = -1;
		inode_child.data_block_pointers[2] = -1;
		inode_child.data_block_pointers[3] = -1;
		inode_child.data_block_pointers[4] = -1;

		read_block_num(fd,get_inode_block_address(ino_num_child));
		memcpy(buffer_block+(ino_num_child%16)*sizeof(inode),&inode_child,sizeof(inode));
		write_block_num(fd,get_inode_block_address(ino_num_child));


		//lseek(fd, inode_offset+(sizeof(inode)*ino_num_child), SEEK_SET);
		//x=write(fd,&inode_child, sizeof(inode));
		/*
		if(fflush(fd)==EOF || !x)
		{
			perror("Did not write\n");
		}
		*/
		free(buff3);
	}
	free(buff);free(buff2);//free(buff3);
	return 0;
}
static int do_write(const char *path, const char *buffer2, size_t size, off_t offset,struct fuse_file_info *fi)
{

	char *buff2=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff2,path);
	int inode_num=locate_file_persistent(buff2);
	if(inode_num==-1){
		printf("creating file");
		do_create(path,0755,fi);
		inode_num=locate_file_persistent(buff2);
	}

	int fd = open("fs1",O_RDWR);
	if(fd==-1){
		printf("Error opening file\n");
		return -1;
	}
	char *buffer=(char *)calloc(size,sizeof(char));
	strcpy(buffer,buffer2);
	//buffer[size-1]='\0';
	//size=size-1;
	inode n;
	read_block_num(fd,get_inode_block_address(inode_num));
	memcpy(&n,buffer_block+(inode_num%16)*sizeof(inode),sizeof(inode));

	printf("Printing buffer from terminal %s\n",buffer);
	//lseek(fd,inode_offset+(sizeof(inode)*inode_num),SEEK_SET);
	//read(fd,&n,sizeof(inode));
	printf("Read from inode num %d\n",inode_num);

	//write permissions
	short int m=n.mode&4095;
		if(n.uid==getuid()){
			
			//check read permissions
			if(!(m&128)){
				return -EACCES;			
			}
			
		}
		else if(n.gid==getgid()){
			//check group read permissions
			//check read permissions
			if(!(m&16)){
				return -EACCES;
			}
		}
		else{
			if(!(m&2)){
				return -EACCES;
			}
		}


	int flag=0;
	if(n.size<offset+size)flag=1;
	//offset begins at existing data block
	int bytes_written=0;
	int start_data_block=offset/block_size;
	int space_left=block_size-(n.size%block_size);
	if(n.data_block_pointers[start_data_block]==-1){
		//offset does not begin at existing data block
		printf("Allocating a new data block");
		if(start_data_block==5)return -EFBIG;
		//Allocate a new data block
		int new_data_block=create_data_block(fd,&n);
		n.block_count++;
		//if(new_data_block==-1)return -EFBIG;
	}
	if(n.data_block_pointers[start_data_block]!=-1){
		int data_block=n.data_block_pointers[start_data_block];
		//read_block_num(fd,data_block);


		lseek(fd,data_block+(offset%block_size),SEEK_SET);
		bytes_written+=min(space_left,size);
		offset+=bytes_written;
		//buffer_block[(offset%block_size)+min(space_left,size)]='\0';
		//strncpy(buffer_block+(offset%block_size),buffer,min(space_left,size));
		
		//write_block_num(fd,data_block);
		write(fd,buffer,sizeof(char)*min(space_left,size));
		int count=start_data_block;
		count++;
		printf("buffer block%s\n",buffer_block);
		printf("length %d\n",strlen(buffer_block));
		while(bytes_written<size){
			data_block=n.data_block_pointers[count];
			if(data_block==-1){
				//create new data block from data bitmap
				data_block=create_data_block(fd,&n);
				if(data_block==-1)return -EFBIG;
				n.block_count++;
			}	

			//read_block_num(fd,data_block);
			//memcpy(buffer_block+(offset%block_size),buffer+bytes_written,sizeof(char)*min(block_size,size-bytes_written));
			lseek(fd,data_block+(offset%block_size),SEEK_SET);
			write(fd,buffer+bytes_written,sizeof(char)*min(block_size,size-bytes_written));
			//write_block_num(fd,data_block);

			bytes_written+=min(block_size,size-bytes_written);
			offset+=bytes_written;
			count++;
		}
	}
	time_t now;
	time(&now);
	//update file's inode

	if(flag)n.size=offset;
	n.last_access_time=now;
	n.last_modified_time=now;

	read_block_num(fd,get_inode_block_address(inode_num));
	memcpy(buffer_block+(inode_num%16)*sizeof(inode),&n,sizeof(inode));
	write_block_num(fd,get_inode_block_address(inode_num));

	//lseek(fd,inode_offset+(sizeof(inode)*inode_num),SEEK_SET);
	//write(fd,&n,sizeof(inode));
	//update parent directory's inode access time
	int parent_inode_num=locate_file_persistent(dirname(buff2));
	inode parent_inode;

	read_block_num(fd,get_inode_block_address(parent_inode_num));
	memcpy(&parent_inode,buffer_block+(parent_inode_num%16)*sizeof(inode),sizeof(inode));

	//lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
	//read(fd,&parent_inode,sizeof(inode));
	parent_inode.last_access_time=now;

	memcpy(buffer_block+(parent_inode_num%16)*sizeof(inode),&parent_inode,sizeof(inode));
	write_block_num(fd,get_inode_block_address(parent_inode_num));
	//lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
	//write(fd,&parent_inode,sizeof(inode));
	//fflush(fd);
	close(fd);
	printf("BYTES WRITTEN! AND OFFSET AND SIZE%d %d %d\n",bytes_written,offset,size);
	free(buffer);
	return bytes_written;

}
static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
	char *buff2=(char *)malloc(sizeof(char)*strlen(path));
	strcpy(buff2,path);
	int inode_num=locate_file_persistent(buff2);
	if(inode_num==-1)
	{
		return -ENOENT;
	}

	int fd = open("fs1",O_RDWR);
	if(fd==-1)
	{
		printf("Error opening file\n");
		return -1;
	}
	inode n;

	read_block_num(fd,get_inode_block_address(inode_num));
	memcpy(&n,buffer_block+(inode_num%16)*sizeof(inode),sizeof(inode));

	//lseek(fd,inode_offset+(sizeof(inode)*inode_num),SEEK_SET);
	//read(fd,&n,sizeof(inode));
	printf("Read from inode num %d\n",inode_num);

	//check for read permissions
	short int m=n.mode&4095;
		if(n.uid==getuid()){
			
			//check read permissions
			if(!(m&256)){
				return -EACCES;			
			}
			
		}
		else if(n.gid==getgid()){
			//check group read permissions
			//check read permissions
			if(!(m&32)){
				return -EACCES;
			}
		}
		else{
			if(!(m&4)){
				return -EACCES;
			}
		}


	if(offset>=n.size)return 0;

	//find the data block to start reading from 
	int start_data_block=offset/block_size;
	int i=start_data_block;
	int data_off=0;
	int num_blocks_to_read=size/block_size +1;
	char *text=(char *)malloc(sizeof(char)*(size+1));
	int count=0;
	data_off=n.data_block_pointers[i];
	int space_left=(block_size)-(offset%block_size);
	read_block_num(fd,data_off);
	strncpy(text+count*space_left,buffer_block,space_left);
	count++;
	num_blocks_to_read--;
	i++;
	while((data_off=n.data_block_pointers[i])!=-1 && num_blocks_to_read--){
		
		read_block_num(fd,data_off);
		strncpy(text+count*block_size,buffer_block,block_size);
		//lseek(fd,data_off,SEEK_SET);
		//read(fd,text+block_size*count,block_size);
		count++;
		i++;

	}
	text[size]='\0';
	printf("Read from file %s\n",text);
	memcpy(buffer,text,size);
	free(text);
	close(fd);
	free(buff2);
	//free(buffer);
	return size;
}
static int do_getattr( const char *path, struct stat *st )
{

	printf( "[getattr] Called\n" );
	printf( "\tAttributes of %s requested\n", path );

	int inode_num=locate_file_persistent(path);
	if(inode_num==-1)
		return -ENOENT;

	int fd = open("fs1",O_RDWR);

	if(fd==-1)
		{
			printf("Error opening file");
			return -1;
		}
	
	inode n;
	//lseek(fd,inode_offset+(inode_num*sizeof(inode)),SEEK_SET);
	//read(fd,&n,sizeof(inode));

	printf("Address of inode block corresponding to inode_num %d is %ld\n",inode_num,get_inode_block_address(inode_num) );
	read_block_num(fd, get_inode_block_address(inode_num));
	memcpy(&n,buffer_block+((inode_num%16)*sizeof(inode)),sizeof(inode));

	printf("Inode_num=%d\n",inode_num);
	st->st_ino=(unsigned int)inode_num;
	st->st_mode=n.mode;
	st->st_nlink=(int) n.hard_link_count;
	st->st_uid=(int) n.uid;
	st->st_gid=(int) n.gid;
	st->st_size=(int) n.size;
	st->st_atime=n.last_access_time;
	st->st_mtime=n.last_modified_time;
	st->st_ctime=n.create_time;
	
	printf("%lu\t%d\t%d\t%d\n",st->st_ino,st->st_nlink,st->st_uid,st->st_size );
	int count=0;
	for(int i=0;i<5;i++){
		if(n.data_block_pointers[i]!=-1)count++;
	}
	st->st_blocks=(int)count;
	close(fd);
	return 0;
}


static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
	printf( "[readdir] Called\n" );
	printf( "\tList of files of %s requested\n", path );

	int inode_num=locate_file_persistent(path);
	if(inode_num==-1)
		return -ENOENT;
	
	int fd = open("fs1",O_RDWR);

	if(fd==-1)
	{
		printf("Error opening file");
		return -1;
	}
	
	inode i;
	/*
	lseek(fd,inode_offset+(inode_num*sizeof(inode)),SEEK_SET);
	read(fd,&i,sizeof(inode));
	*/

	read_block_num(fd, get_inode_block_address(inode_num));
	memcpy(&i,buffer_block+((inode_num%16)*sizeof(inode)),sizeof(inode));

	for(int j=0;j<5;j++)
	{
		if(i.data_block_pointers[j]==-1)
			return 0;
		int data_block_address = i.data_block_pointers[j];

		//lseek(fd,data_block_address,SEEK_SET);
		read_block_num(fd,data_block_address);

		int number_entries = i.size/direntry_size;
		printf("No of entries in dir %s = %d\n",path,number_entries);

		int count=0;
		while(number_entries)
		{
			direntry de;
			//read(fd,&de,direntry_size);
			memcpy(&de,buffer_block+(count*direntry_size),direntry_size);
			printf("Filename: %s\n",de.filename);
			if(strcmp(de.filename,".") && strcmp(de.filename,".."))
				filler( buffer, de.filename, NULL, 0 );
			number_entries--;
			count++;
		}
	}
	close(fd);
	return 0;
}

static int do_mkdir (const char *path, mode_t mode)
{	
	char *buffer=(char *)malloc(strlen(path));
	strcpy(buffer,path);
	char *d_name=(char *)malloc(strlen(path));
	strcpy(d_name,dirname(buffer));
	printf("mkdir called in %s\n",d_name);
	int ino_num=locate_file_persistent(d_name);
	free(d_name);
	int x;
	if(ino_num==-1)
		return -ENOENT;
	//traverse and find a free data block
	int fd;
	fd=open("fs1",O_RDWR);
	if(fd==-1)
	{
		perror("File didn't open because:");
		return 0;
	}
	inode n;
	//lseek(fd,inode_offset+(ino_num*sizeof(inode)),SEEK_SET);
	//read(fd,&n,sizeof(inode));

	read_block_num(fd, get_inode_block_address(ino_num));
	memcpy(&n,buffer_block+((ino_num%16)*sizeof(inode)),sizeof(inode));	

	int free_data_block_pointer=n.size/block_size;
	printf("Free Data pointer = %d\n",free_data_block_pointer);

	if(n.size==block_size*5)
	{
		close(fd);
		return -ENOMEM;
	}

	if(!n.size%block_size){
		printf("Oh no.Creating new data block\n");
		printf("Allocating a new data block");
		if(free_data_block_pointer==5)return -EFBIG;
		//Allocate a new data block
		int new_data_block=create_data_block(fd,&n);
		n.block_count++;

		close(fd);
		return 0;
	}
	else{
		//update entries of parent directory
		char y = 1;
		//find inode number
		int ino_num_child=find_free_block(fd,0);
		printf("Free inode block = %d\n",ino_num_child);
		
		//lseek(fd,inode_bitmap_offset+ino_num_child,SEEK_SET);
		//write(fd,&y,sizeof(char));

		read_block_num(fd,inode_bitmap_offset);
		buffer_block[ino_num_child]=y;
		write_block_num(fd,inode_bitmap_offset);



		//create new entry;
		direntry d;
		strcpy(d.filename,basename(path));
		printf("Creating new dir with name = %s\n",d.filename);
		d.inode_num=ino_num_child;
		int free_dir_ent=(n.size%block_size)/direntry_size;
		printf("Write direntry into address: %d\n",n.data_block_pointers[free_data_block_pointer]+(free_dir_ent*direntry_size));
		
		//lseek(fd,n.data_block_pointers[free_data_block_pointer]+(free_dir_ent*direntry_size),SEEK_SET);
		//x=write(fd,&d,direntry_size);
		read_block_num(fd, n.data_block_pointers[free_data_block_pointer]);
		memcpy(buffer_block+(free_dir_ent*direntry_size),&d,direntry_size);
		write_block_num(fd, n.data_block_pointers[free_data_block_pointer]);

		time_t now;
		time(&now);
		/*
		fflush(fd);
		if(fflush(fd)==EOF || !x)
		{
			perror("Did not write\n");
		}
		*/
		//fill inode of parent directory
		n.size=n.size+direntry_size;
		n.last_access_time=now;
		n.last_modified_time=now;


		//for every sub-directory 
		n.hard_link_count++;
		printf("Write parent inode in address %d\n",inode_offset+(sizeof(inode)*ino_num));
		printf("New size of parent directory = %d\n",n.size);
		//lseek(fd,inode_offset+(sizeof(inode)*ino_num),SEEK_SET);
		//write(fd,&n,sizeof(inode));
		read_block_num(fd,get_inode_block_address(ino_num));
		memcpy(buffer_block+((ino_num%16)*sizeof(inode)),&n,sizeof(inode));	
		write_block_num(fd,get_inode_block_address(ino_num));	

		//create a new datablock for directory
		
		int data_block_num=find_free_block(fd,1);
		printf("Free data block = %d\n",data_block_num);
		/*
		lseek(fd,data_bitmap_offset+data_block_num,SEEK_SET);
		write(fd,&y,sizeof(char));
		*/
		read_block_num(fd,data_bitmap_offset);
		buffer_block[data_block_num]=y;
		write_block_num(fd,data_bitmap_offset);

		direntry d_new;
		strcpy(d_new.filename,".");
		d_new.inode_num=ino_num_child;
		direntry d_new_parent;
		strcpy(d_new_parent.filename,"..");
		d_new_parent.inode_num=ino_num;
		/*
		lseek(fd,data_block_offset+(block_size*data_block_num),SEEK_SET);
		write(fd,&d_new,direntry_size);
		write(fd,&d_new_parent,direntry_size);
		*/

		read_block_num(fd,data_block_offset+(block_size*data_block_num));
		memcpy(buffer_block,&d_new,direntry_size);
		memcpy(buffer_block+direntry_size,&d_new_parent,direntry_size);
		write_block_num(fd,data_block_offset+(block_size*data_block_num));

		/*
		if(fflush(fp)==EOF || !x)
		{
			perror("Did not write\n");
		}
		//fill inode structure
		*/
		inode inode_child;
		inode_child.size = direntry_size*2;
		inode_child.last_access_time = now;
		inode_child.last_modified_time = now;
		inode_child.create_time = now;
		inode_child.mode = S_IFDIR|mode;
		inode_child.hard_link_count = 1;
		inode_child.block_count = 1;
		inode_child.uid = getuid();
		inode_child.gid = getgid();
		inode_child.data_block_pointers[0] = data_block_offset+(block_size*data_block_num);
		inode_child.data_block_pointers[1] = -1;
		inode_child.data_block_pointers[2] = -1;
		inode_child.data_block_pointers[3] = -1;
		inode_child.data_block_pointers[4] = -1;

		/*
		lseek(fd, inode_offset+(sizeof(inode)*ino_num_child), SEEK_SET);
		write(fd,&inode_child, sizeof(inode));
		*/

		read_block_num(fd,get_inode_block_address(ino_num_child));
		memcpy(buffer_block+((ino_num_child%16)*sizeof(inode)),&inode_child,sizeof(inode));
		write_block_num(fd,get_inode_block_address(ino_num_child));

		/*
		if(fflush(fp)==EOF || !x)
		{
			perror("Did not write\n");
		}
		*/
	}
	/*while(n.data_block_pointers[i++]!=-1 && n.size==1024){

	}	
	direntry d;
	lseek(fd,n.data_block_pointers[i-1],SEEK_SET);
	read(fd,&d,direntry_size);
	for(int k=0;k<block_size/direntry_size;k++){
			fseek(fp,parent_dir_offset+(k*direntry_size),SEEK_SET);
			read(fd,&d,direntry_size);
			ino_num=strcmp(token,d.filename)?-1:d.inode_num;
			if(ino_num!=-1)break;
	}

	*/
	//fflush(fp);
	close(fd);
	printf("Returning from mkdir\n");
	return 0;

}

static int do_rmdir (const char *path)
{
	char *buffer=(char *)malloc(strlen(path));
	strcpy(buffer,path);
	char *d_name=(char *)malloc(strlen(path));
	strcpy(d_name,dirname(buffer));
	printf("[rmdir] called for %s\n", path);

	int inode_num_child=locate_file_persistent(path);
	if(inode_num_child==-1)
	{
		free(d_name);	
		free(buffer);
		return -ENOENT;
	}

	//free(d_name);	
	free(buffer);

	int fd = open("fs1",O_RDWR);
	
	inode inode_child;

	//lseek(fd,inode_offset+sizeof(inode)*inode_num_child,SEEK_SET);
	//read(fd,&inode_child,sizeof(inode));

	read_block_num(fd, get_inode_block_address(inode_num_child));
	memcpy(&inode_child,buffer_block+((inode_num_child%16)*sizeof(inode)),sizeof(inode));


	if(inode_child.size!=2*direntry_size)
	{
		printf("Child size = %d\n",inode_child.size);
		return -ENOTEMPTY;
	}

	for (int i = 0; i < 5; ++i)
	{
		if(inode_child.data_block_pointers[i]!=-1)
			free_block(fd,inode_child.data_block_pointers[i],1);
	}

	free_block(fd,inode_num_child,0);

	int inode_num_parent=locate_file_persistent(d_name);
	printf("Parent directory inode num = %d\n",inode_num_parent);
	inode inode_parent;
	int flag=0;
	char child_dir_name[28];
	strcpy(child_dir_name,basename(path));

	//lseek(fd,inode_offset+sizeof(inode)*inode_num_parent,SEEK_SET);
	//read(fd,&inode_parent,sizeof(inode));

	read_block_num(fd, get_inode_block_address(inode_num_parent));
	memcpy(&inode_parent,buffer_block+((inode_num_parent%16)*sizeof(inode)),sizeof(inode));

	for(int i=0;i<inode_parent.block_count;i++)
	{
			int parent_dir_offset=(inode_parent.data_block_pointers)[i];
			if(parent_dir_offset==-1)continue;
			direntry d;
			int size_block;
			if(i+1==inode_parent.block_count)
				size_block=inode_parent.size%block_size;
			else
				size_block=block_size;
			printf("\tSize of block %d = %d\n",i,size_block);
			//go to corresponding data block, which may contain the entry for sub-directory/file
			read_block_num(fd,(inode_parent.data_block_pointers)[i]);
			int offset = 0;
			for(int k=0;k<size_block/direntry_size;k++)
			{
				//lseek(fd,parent_dir_offset+(k*direntry_size),SEEK_SET);
				//read(fd,&d,direntry_size);

				memcpy(&d,buffer_block+k*direntry_size,direntry_size);
				offset=k*direntry_size+direntry_size;

				printf("\tName of this direntry is %s\n",d.filename);
				if(!strcmp(child_dir_name,d.filename))
				{
					flag=1;
					int no_of_entries_below=size_block/32-(k+1);
					if(!no_of_entries_below)
						break;
					direntry dentries[no_of_entries_below];
					//read(fd,dentries,direntry_size*no_of_entries_below);
					//lseek(fd,parent_dir_offset+(k*direntry_size),SEEK_SET);
					//write(fd,dentries,direntry_size*no_of_entries_below);
					
					memcpy(dentries,buffer_block+offset-direntry_size,direntry_size*no_of_entries_below);
					write_block_num(fd,parent_dir_offset);

					break;
				}
			}	
			if(flag)
				break;
		}	


	if(inode_parent.size%block_size==direntry_size)
	{
		inode_parent.data_block_pointers[inode_parent.block_count-1]=-1;
	}

	inode_parent.size-=direntry_size;
	//lseek(fd,inode_offset+sizeof(inode)*inode_num_parent,SEEK_SET);
	//write(fd,&inode_parent,sizeof(inode));
	read_block_num(fd,get_inode_block_address(inode_num_parent));
	memcpy(buffer_block+((inode_num_parent%16)*sizeof(inode)),&inode_parent,sizeof(inode));
	write_block_num(fd,get_inode_block_address(inode_num_parent));



	free(d_name);
	close(fd);
	return 0;	
}

static struct fuse_operations operations = {

    .getattr	= do_getattr ,
	.readdir	= do_readdir  ,
	.mkdir		= do_mkdir ,
	.rmdir		= do_rmdir ,
	.create     = do_create,
	.write      = do_write ,
	.open       = do_open,
	.read       = do_read, 
	 .unlink=do_unlink,
	 .truncate=do_truncate
};

int main( int argc, char *argv[] )
{
	buffer_block[1024]='\0';
	return fuse_main( argc, argv, &operations, NULL );

}