#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>

#endif

#include <stdlib.h>


#define EMPTY_T 0
#define FILE_T 1
#define DIR_T 2
#define BLOCK_SIZE 4096

char cwd[256]; // current working directory
int FAT[2440];

struct Node {
  int value;
  struct Node *next;
  struct Node *prev;
};

struct Node *freeListHead = NULL;
struct Node *freeListTail = NULL;
struct fat_superblock{
  int root_address;
  int block_size;
  
};
struct dir_ent {
  int type;
  long first_cluster;
  char file_name[24];
};
  
union{
  struct fat_superblock s;
  char pad[512];
} superblock;

static void* fat_init(struct fuse_conn_info *conn) {
  strcat(cwd, "/fat_disk");
  FILE *disk;

  // file exists  
  if (access(cwd, F_OK) != -1){
    disk = fopen(cwd, "r+");
    pread(fileno(disk), &FAT, 2440, 512);
    // read from offset 512 and read 4880 bytes in fat
    //fseek(disk, 512, SEEK_SET);
    // init fat from file
    //fgets(FAT, 2440, disk);
    
    // read superblock
    if (pread(fileno(disk), &superblock, 512, 0) == -1) {
      return -1;
    }
    
    fclose(disk);
  }
  
  // file doesn't exist
  else {
    disk = fopen(cwd, "w+");
    
    fseek(disk, 10000000-1, SEEK_SET);
    fputc(0, disk);
    
    // init fat if no disk
    memset(FAT, 0, 2440);

    // make superblock
    superblock.s.root_address = 10272; //512 + 2440*4 (superblock + FAT)
    superblock.s.block_size = 4096; // our assumed uniform block size - check this
    pwrite(fileno(disk), &superblock, 512, 0); // write to disk

    // make an array of dir_ent and populate with . and .. 4096/32=128 , write to disk at block zero
    struct dir_ent root_data[128];
    for(int i=2; i < 128 ; i++){
      root_data[i].type = EMPTY_T;
    }
    
    root_data[0].file_name[0] = ".";
    root_data[0].type = DIR_T;
    root_data[0].first_cluster = 0;

    root_data[1].file_name[0]= "..";
    root_data[1].type = DIR_T;
    root_data[1].first_cluster = 0;
     
    fclose(disk);
  }
  // create free list
  for(int i=1; i < 2440; i++){
    if(FAT[i] == 0){
      if (freeListHead == NULL){
	freeListHead = (struct Node*)malloc(sizeof(struct Node));
	freeListHead -> value = i;
	freeListHead -> next = freeListTail;

      } else { // freelist head is not null + free list tail is null
	freeListTail = (struct Node*)malloc(sizeof(struct Node));
	freeListTail -> value = i;
	freeListTail -> next = NULL;
	freeListTail = freeListTail -> next;

      } 
      
    }
  }
  
  
	return NULL;
}


static int fat_mkdir(const char* path, mode_t mode){
}

static int fat_getattr(const char *path, struct stat *stbuf){  
}

static int fat_access(const char* path, int mask ){
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
}

static struct fuse_operations fat_operations = {
	.init		= fat_init,
	.getattr	= fat_getattr,
	
/*
	.fgetattr	= NULL,
	.access		= NULL,
	.readlink	= NULL,
	.readdir	= NULL,
	.mknod		= NULL,
	.create		= NULL,
	.mkdir		= NULL,
	.symlink	= NULL,
	.unlink		= NULL,
	.rmdir		= NULL,
	.rename		= NULL,
	.link		= NULL,
	.chmod		= NULL,
	.chown		= NULL,
	.truncate	= NULL,
	.utimens	= NULL,
	.open		= NULL,
	.read		= NULL,
	.write		= NULL,
	.statfs		= NULL,
	.release	= NULL,
	.fsync		= NULL,
#ifdef HAVE_SETXATTR
	.setxattr	= NULL,
	.getxattr	= NULL,
	.listxattr	= NULL,
	.removexattr	= NULL,
#endif
*/
};



int main(int argc, char *argv[]) {
	umask(0);
	if (getcwd(cwd, sizeof(cwd))==NULL){
	    perror("error getting current directory");
	  }
	// printf("%s\n", cwd);
	return fuse_main(argc, argv, &fat_operations, NULL);
}
