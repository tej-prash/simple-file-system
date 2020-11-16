
## FUSE
FUSE (Filesystem in Userspace) is an interface that lets us write our own filesystem for Linux in the user space. Using FUSE, a user defined file system can be mounted and programs can access the data using standard file operation system, which in turn calls the user defined functions.

## Design

### Phase 1
The primary objective of this phase was to understand the working of the system calls. Hence an n-ary tree data structure was used to store the information about the file system. Each node in the tree had the following members:

```
char name[10] # to store the name of the file
char* data  # to store the contents of the file
struct node* branch[MAX_BRANCHES]  # to store the address of nodes of files or subdirectories in the current directory. This field would be used only for directories
struct stat st # to store the metadata about the file
```

A function was implemented to locate a file in the tree structure when given a path, which was a primary requirement before implementing the other system calls. This implementation was non-persistent.

### Phase 2
The file system was made persistent by creating our own file system organisation and our own inode structure. The organisation is as follows-
- “Disk” size: 10MB
-  Block size: 1024 bytes

- 
|  |  |  |   |   |  |  |   |   |
| ------------- | ------------- | ------------- | ------------- | ------------- | ------------- | ------------- | ------------- | ------------- | 
| Super block |  Inode bitmap | Data bitmap | Inode Block 1 | Inode Block 2 | ... | Data Block 1 | Data Block 2 | ... |

- Super block
  - Stores 4 starting addresses of 4 bytes each:
      - Inode bitmap
      - Data bitmap
      - Inode blocks
      - Data blocks
  - One block reserved as super block
- Inode bitmap
  - Indexed by inode number
  - Each byte has 1 or 0 
  - Each block can trace 1024 inodes
- Data bitmap
  - Indexed by inode number
  - Assume each byte has 1 or 0
  - Each block can trace 1024 data blocks
- Inode block
  - Inode structure described below
  - Each inode of size 64 bytes
  - Each inode block can hold 16 inodes
- Data blocks
  - Stores contents of file or directory entries
  - During time of file creation, each new file is given one block
- 1 block is reserved for super block
- 1 block for inode bitmap
- 1 block for data bitmap 🡪 1024 data blocks
- 50 blocks for inodes 🡪 50*16 = 800 inodes
Inode structure contains all the information about a file and its properties like mode, user id, group ID etc. 
- 
| Size (in bytes) | Name |
| ---------------- | ---- |
| 4 | Mode |
| 4 | Size |
| 8 | Last access time |
| 8 | Last modified time |
| 8 | Time of creation |
| 2 | Number of hard links |
| 2 | Number of blocks allocated |
| 20 | Disk pointers (can track 5 data blocks) |
| 4 | User id |
| 4 | Group id |

- Directory entry structure contains the information about inode number of the subdirectories and the filenames within that directory

| Size (in bytes) | Name | 
| ------------ | ------- |
| 28 | Name of entry |
| 4 | Inode number of entry |

A file was created to implement the above design. The starting addresses of each of the inode bitmap, data bitmap, inode blocks and data blocks were written sequentially into the super block. The contents of the inode bitmap and the data bitmap were initialised appropriately so as to ensure that all inodes and data blocks are currently free.  

The root was created by assigning the 0th inode and the 0th data block for storing its details and updating the bitmaps to reflect the change in the free inodes and data blocks. The inode and the data block of the root were appropriately populated.

## Persistence
In order to ensure that the file system is persistent, any change made to the file system is reflected back in the disk-file that was created. Any access to the disk-file is done in terms of blocks. For example, if the inode structure of a particular inode is to be retrieved, the address of the block containing that inode is computed and the entire block is retrieved. After retrieving the entire block and storing it in a buffer, the inode is read from the buffer by moving to the right offset in the buffer. Any change made to the inode is written into the buffer after which the entire buffer is written back into the disk-file. This ensures that each and every operation’s effects is stored in the secondary memory. On unmounting the file system and mounting it again, the contents of the disk-file is read and the file system’s state brought back. Hence persistence is achieved. 

