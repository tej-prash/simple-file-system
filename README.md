# simple-file-system
A simple file system built in C using FUSE

## Setup instructions (Ubuntu)

# Dependencies
File System in User Space (FUSE) must be installed. 

```
sudo apt-get install libfuse-dev
sudo apt-get install fuse
```

# Running SFS

- Compile and run sfs_setup.c using gcc

```
gcc sfs_setup.c -o sfs_setup
./sfs_setup
```

- Compile and run sfs.c using FUSE

```
gcc sfs.c -o sfs `pkg-config fuse --cflags --libs`
./ssfs -f <path-to-mount-point>
```
