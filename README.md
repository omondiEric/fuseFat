# FUSE FAT
In this (2-part) assignment, we implement a variant of the FAT file system (as required, we implement the basic functionality of FAT, though it is not compatible with an existing FAT file system). The FUSE file system is implemented in a file named `fat.c`. For full details see: [Lab Handout](http://cs.williams.edu/~jannen/teaching/s19/cs333/labs/fuse/fuse_fat1.html)
 
The filesystem is backed by a SINGLE preallocated 10-MB file "fat_disk". When the filesystem is invoked, if this file doesn't exist, it is created and initialized. If it does exist, the backing file will be attached so its previous contents are made visible.


## Implementation details

Steve, Julia and Julia are the sole contributors to this repository. 

To run:
```
$ make
$ mkdir testdir
$ ./fat testdir
```

To end:
```
$ fusermount -u testdir
$ rm fat_disk           # optionally
```
