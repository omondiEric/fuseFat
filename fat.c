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

static void* fat_init(struct fuse_conn_info *conn) {

  //  printf("\n\ndir size: %lu\n\n", sizeof(struct dir_ent));
  
  strcat(cwd, "/fat_disk");
  FILE *disk;

  // file exists  
  if (access(cwd, F_OK) != -1){
    disk = fopen(cwd, "r+");
    pread(fileno(disk), &FAT, 2440, 512);
    // read from offset 512 and read 4880 bytes in fat
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

    strcpy(&root_data[0].file_name, ".");
    root_data[0].type = DIR_T;
    root_data[0].first_cluster = 0;
    root_data[0].size = 4096;
    
    strcpy(&root_data[1].file_name, "..");
    root_data[1].type = DIR_T;
    root_data[1].first_cluster = 0;
    root_data[1].size = 4096;

    pwrite(fileno(disk), &root_data, 4096, 10272);
    
    fclose(disk);
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
  char * path_piece = strtok(path, "/");
  bool exists = false;
  FILE *disk;
  char * new_dir_name;
  if(strcmp(path, "/")==0){ exists = true;}

  while(path_piece !=NULL){
    new_dir_name = path_piece;
    exists = false;
	
    // read in directory data
    disk = fopen(cwd, "r+");
    pread(fileno(disk), &dir_data, 4096, block_address);
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
  disk = fopen(cwd, "r+");
  pread(fileno(disk), &dir_data, 4096, block_address);
  fclose(disk);

  for (int i=0; i<128; i++){
    if(dir_data[i].type==EMPTY_T){
      int new_block = freeListHead-> value;
      freeListHead = freeListHead->next;
      strcpy(dir_data[i].file_name, new_dir_name);
      dir_data[i].type = DIR_T;
      dir_data[i].size = 4096;
      dir_data[i].first_cluster = new_block;

      disk = fopen(cwd, "r+");
      pwrite(fileno(disk), &dir_data, 4096, block_address);

      struct dir_ent new_dir_data[128];
      for(int i=2; i<128; i++){
	new_dir_data[i].type = EMPTY_T;
      }

      strcpy(&new_dir_data[0].file_name, ".");
      new_dir_data[0].type = DIR_T;
      new_dir_data[0].first_cluster = new_block;
      new_dir_data[0].size = 4096;

      strcpy(&new_dir_data[1].file_name, "..");
      new_dir_data[1].type = DIR_T;
      new_dir_data[1].first_cluster = compute_block(block_address);
      new_dir_data[1].size = 4096;

      pwrite(fileno(disk), &new_dir_data, 4096, compute_address(new_block));
      fclose(disk);

      return 0;
    }
  }
}

static int fat_getattr(const char *path, struct stat *stbuf){
  
  memset(stbuf, 0, sizeof(struct stat));

  // case for root
  if(strcmp(path, "/")==0){
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_size = 4096;
    return 0;
  }

  int data_offset; // the offset within the block of the dir_ent for the directory we're looking for
  struct dir_ent dir_data[128]; // array of dir_ent to hold data of a directory
  int block_address = superblock.s.root_address; // block address starts as that of the superblock

  // parse path name, separating by "/"
  char * path_piece = strtok(path, "/");
  while(path_piece != NULL){
    
    // read in dir_data
    FILE *d = fopen(cwd, "r+");
    pread(fileno(d), &dir_data, 4096, block_address);
    fclose(d);
    
    data_offset = -1;
    // look through each dir_ent to find path_piece
    for (int i=0; i<128; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	data_offset = i;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    // if path_piece isn't a file_name of any dir_ent, it doesn't exist (data_offset hasn't been changed from initial -1 val)
    if(data_offset==-1){ return -ENOENT;}

    // look at next part of path name
    path_piece = strtok(NULL, "/");
  }
  stbuf->st_mode = S_IFDIR | 0755;
  stbuf->st_size = dir_data[data_offset].size;
  return 0;
}

static int fat_access(const char* path, int mask ){
  // root case
  if(strcmp(path, "/")==0){
    return 0;
  }
  
  struct dir_ent dir_data[128];
  int block_address = superblock.s.root_address;
  char * path_piece = strtok(path, "/");

  // look at each dir of path name to see if that dir exists
  bool exists;
  FILE *disk;
  while(path_piece != NULL){
    exists = false;
    disk = fopen(cwd, "r+");
    // read in directory entries
    pread(fileno(disk), &dir_data, 4096, block_address);
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
    path_piece = strtok(NULL, "/");
  }
  return 0;
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
  (void) offset;
  (void) fi;

  struct dir_ent dir_data[128];
  int block_address = superblock.s.root_address;
  char * path_piece = strtok(path, "/");

  bool exists = false;
  FILE *disk;
  if(strcmp(path, "/")==0){ exists = true;}

  while(path_piece != NULL){
    exists = false;

    // read in directory data
    disk = fopen(cwd, "r+");
    pread(fileno(disk), &dir_data, 4096, block_address);
    fclose(disk);

    // iterate through all dir_ent looking for one w/ file_name of path_piece
    for(int i=0; i<128; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	exists = true;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    if(!exists){return -ENOENT;}
    path_piece = strtok(NULL, "/");
  }
  disk = fopen(cwd, "r+");
  pread(fileno(disk), &dir_data, 4096, block_address);
  fclose(disk);
  
  for(int i=0; i<128; i++){
    if(dir_data[i].type != EMPTY_T){
      filler(buf, dir_data[i].file_name, NULL, 0);
    }
  }
  return 0;
}

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
