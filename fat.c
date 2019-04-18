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
#include <stdbool.h>


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
  int first_cluster;
  int size;
  char file_name[20];
};
  
union{
  struct fat_superblock s;
  char pad[512];
} superblock;

static int compute_address(int block_num){
  return 512 + (2440*4) + (4096*block_num);
}

static int compute_block(int address){
  return (address - 512 - (2440*4))/4096;
}

static int pwrite_check(int fd, void *buf, size_t count, off_t offset){
  if(pwrite(fd, buf, count, offset) != count){
    return -EIO;
  }
  return 0;
}

static int pread_check(int fd, void *buf, size_t count, off_t offset){
  if(pread(fd, buf, count, offset) != count){
    return -EIO;
  }
  return 0;
}


static int mkdir_helper(int current_block, int parent_block){
    struct dir_ent root_data[128];
    for(int i=2; i < 128 ; i++){
      root_data[i].type = EMPTY_T;
    }

    strcpy((char*) &root_data[0].file_name, (const char*) ".");
    root_data[0].type = DIR_T;
    root_data[0].first_cluster = current_block;
    root_data[0].size = 4096;
    
    strcpy((char*) &root_data[1].file_name, (const char*) "..");
    root_data[1].type = DIR_T;
    root_data[1].first_cluster = parent_block;
    root_data[1].size = 4096;

    FILE *disk = fopen(cwd, "r+");
    pwrite_check(fileno(disk), &root_data, 4096, compute_address(current_block));
    // read one block, or macro that calls pread or pwrite if fails returns appropriate error 
    fclose(disk);
    return 0;
}

static void* fat_init(struct fuse_conn_info *conn) {

  //  printf("\n\ndir size: %lu\n\n", sizeof(struct dir_ent));
  
  strcat(cwd, "/fat_disk");
  FILE *disk;

  // file exists  
  if (access(cwd, F_OK) != -1){
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), &FAT, 2440, 512);
    // read from offset 512 and read 4880 bytes in fat
    // read superblock
    pread_check(fileno(disk), &superblock, 512, 0);
    
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
    pwrite_check(fileno(disk), &superblock, 512, 0); // write to disk
    fclose(disk);
    
    // make an array of dir_ent and populate with . and .. 4096/32=128 , write to disk at block zero
    mkdir_helper(0, 0);
  }
  
  // create free list
  for(int i=1; i < 2440; i++){
    if(FAT[i] == 0){
      if (freeListHead == NULL){
	freeListHead = (struct Node*)malloc(sizeof(struct Node));
	freeListHead -> value = i;
	freeListTail = freeListHead;
      } else { 
	struct Node * newTail = (struct Node*)malloc(sizeof(struct Node));
	newTail -> value = i;
	// printf("tail val: %d\n", freeListTail->value);
	freeListTail -> next = newTail;
	freeListTail = freeListTail -> next;
      }
    }
  }
  return NULL;
}

static int fat_mkdir(const char* path, mode_t mode){
  
  struct dir_ent dir_data[128];
  int block_address = superblock.s.root_address;
  char * path_piece = strtok((char *)path, "/");
  bool exists = false;
  FILE *disk;
  char * new_dir_name;
  if(strcmp(path, "/")==0){ exists = true;}

  while(path_piece !=NULL){
    new_dir_name = path_piece;
    exists = false;
	
    // read in directory data
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), &dir_data, 4096, block_address);
    fclose(disk);

    // iterate through all dir_ent looking for one w/ file_name of path_piece
    for(int i=0; i<128; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	exists = true;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    path_piece = strtok(NULL, "/");
    if(!exists && path_piece!=NULL){return -ENOENT;}
  }

  // we are in the directory where the new directory will go
  disk = fopen(cwd, "r+");
  // read in all dir_ent 
  pread_check(fileno(disk), &dir_data, 4096, block_address);
  fclose(disk);

  // looking for first dir_ent that is empty - that is where we put the new dir
  for (int i=0; i<128; i++){
    if(dir_data[i].type==EMPTY_T){
      int new_block = freeListHead-> value;
      freeListHead = freeListHead->next; // update free list - add check for not null
      strcpy(dir_data[i].file_name, new_dir_name); //updates data by adding new dir as dir_ent
      dir_data[i].type = DIR_T;
      dir_data[i].size = 4096;
      dir_data[i].first_cluster = new_block;

      disk = fopen(cwd, "r+");
      pwrite_check(fileno(disk), &dir_data, 4096, block_address); // write to disk
      fclose(disk);

      // write dir_ents for new data 
      mkdir_helper(new_block, compute_block(block_address));
      return 0;
    }
  }
  return -ENOSPC;
}

static int fat_getattr(const char *path, struct stat *stbuf){
  
  memset(stbuf, 0, sizeof(struct stat));

  //  char *temp_path = NULL;
  // strcpy(temp_path, path);
  
  // case for root
  if(strcmp(path, "/")==0){
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_size = 4096;
    return 0;
  }

  int num_dir_ent = -1; // the offset within the block of the dir_ent for the directory we're looking for
  struct dir_ent dir_data[128]; // array of dir_ent to hold data of a directory
  int block_address = superblock.s.root_address; // block address starts as that of the superblock

  // parse path name, separating by "/"
  char * path_piece = strtok((char *)path, "/");
  while(path_piece != NULL){
    
    // read in dir_data
    FILE *d = fopen(cwd, "r+");
    pread_check(fileno(d), &dir_data, 4096, block_address);
    fclose(d);
    
    num_dir_ent = -1;
    // look through each dir_ent to find path_piece
    for (int i=0; i<128; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	num_dir_ent = i;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    // if path_piece isn't a file_name of any dir_ent, it doesn't exist (data_offset hasn't been changed from initial -1 val)
    if(num_dir_ent==-1){ return -ENOENT;}

    // look at next part of path name
    path_piece = strtok(NULL, "/");
  }
  stbuf->st_mode = S_IFDIR | 0755;
  stbuf->st_size = dir_data[num_dir_ent].size;
  return 0;
}

static int fat_access(const char* path, int mask ){
  // root case
  if(strcmp(path, "/")==0){
    return 0;
  }
  
  struct dir_ent dir_data[128];
  int block_address = superblock.s.root_address;
  char * path_piece = strtok((char *)path, "/");

  // look at each dir of path name to see if that dir exists
  bool exists;
  FILE *disk;
  while(path_piece != NULL){
    exists = false;
    disk = fopen(cwd, "r+");
    // read in directory entries
    pread_check(fileno(disk), &dir_data, 4096, block_address);
    fclose(disk);

    // iterate through all dir_ent looking for path_piece
    for(int i=0; i<128; i++){
      // it exists!
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	exists = true;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    if(!exists){
      return -ENOENT;
    }
    path_piece = strtok(NULL, "/"); // move on to next path piece
  }
  return 0;
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
  (void) offset;
  (void) fi;

 
  struct dir_ent dir_data[128];
  int block_address = superblock.s.root_address;
  char * path_piece = strtok((char *)path, "/");

  bool exists = false;
  FILE *disk;
  // start looking in root for the thing you want to read
  if(strcmp(path, "/")==0){ exists = true;}

  while(path_piece != NULL){
    exists = false;

    // read in directory data
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), &dir_data, 4096, block_address);
    fclose(disk);

    // iterate through all dir_ent looking for one w/ file_name of path_piece
    for(int i=0; i<128; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){ // found the match
	exists = true;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    if(!exists){return -ENOENT;}
    path_piece = strtok(NULL, "/"); // moving on to next part of path name
  }
  disk = fopen(cwd, "r+");
  // read dir_ent of that directory
  pread_check(fileno(disk), &dir_data, 4096, block_address);
  fclose(disk);
  // iterate through dir_ents
  for(int i=0; i<128; i++){
    if(dir_data[i].type != EMPTY_T){
      filler(buf, dir_data[i].file_name, NULL, 0);
    }
  }
  return 0;
}

// removes the given file but not the directory 
static int fat_unlink(const char* path){}

// removes the directory, if empty (except for "." "..") 
static int fat_rmdir(const char* path){}

// makes a plain file
static int fat_create(const char* path, mode_t mode){}

// free temporarily allocated data structures
static int fat_release(const char* path, struct fat_file_info* fi){}

// write size bytes from buf to file, starting at offset
static int fat_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info *fi){}

// read size bytes from file, starting at offset
static int fat_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info *fi){}

//
static int fat_truncate(const char* path, off_t size){}

//
static int fat_fgetattr(const char* path, struct stat* stbuff, struct fuse_file_info* fi){}

//
static int fat_mknod(const char* path, mode_t mode, dev_t rdev){}

//
static int fat_statfs(const char* path, struct statvfs* stbuf){}

//
static int fat_symlink(const char* to, const char* from){}

static struct fuse_operations fat_operations = {
	.init		= fat_init,
	.getattr	= fat_getattr,
	.mkdir		= fat_mkdir,
	.access		= fat_access,
	.readdir        = fat_readdir
/*
	.fgetattr	= NULL,
	.access		= NULL,
	.readlink	= NULL,
	.readdir	= NULL,
	.mknod		= NULL,
	.create		= NULL,
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
