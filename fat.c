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
#define BLOCKS_TOTAL (int) (10000000-512)/(BLOCK_SIZE+2)

char cwd[256]; // current working directory
int FAT[BLOCKS_TOTAL];

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

/*
 * HELPER METHODS
 */

// computes address (offset in disk_fat file) of given block number
static int compute_address(int block_num){
  return 512 + (BLOCKS_TOTAL*4) + (BLOCK_SIZE*block_num);
}

// computes block of an address (offset of disk_fat file)
static int compute_block(int address){
  return (address - 512 - (BLOCKS_TOTAL*4))/BLOCK_SIZE;
}

// helper for writing to disk
static int pwrite_check(int fd, void *buf, size_t count, off_t offset){
  if(pwrite(fd, buf, count, offset) != count){
    return -EIO;
  }
  return 0;
}

// helper for reading from disk
static int pread_check(int fd, void *buf, size_t count, off_t offset){
  if(pread(fd, buf, count, offset) != count){
    return -EIO;
  }
  return 0;
}

// initializes a block for a new directory
// fills with EMPTY_T directory entries and populates ".", ".."
static int mkdir_helper(int current_block, int parent_block){

    // fill block with EMPTY_T directory entries
    struct dir_ent root_data[BLOCK_SIZE/32];
    for(int i=2; i < BLOCK_SIZE/32 ; i++){
      root_data[i].type = EMPTY_T;
    }

    // Directory entry for "."
    strcpy((char*) &root_data[0].file_name, (const char*) ".");
    root_data[0].type = DIR_T;
    root_data[0].first_cluster = current_block;
    root_data[0].size = BLOCK_SIZE;

    // Directory entry for ".."
    strcpy((char*) &root_data[1].file_name, (const char*) "..");
    root_data[1].type = DIR_T;
    root_data[1].first_cluster = parent_block;
    root_data[1].size = BLOCK_SIZE;

    FILE *disk = fopen(cwd, "r+");
    pwrite_check(fileno(disk), &root_data, BLOCK_SIZE, compute_address(current_block));
    
    fclose(disk);
    return 0;
}

// returns the address (file offset) of a given path name
static int find_file(const char *path){

  // root is at block 0
  if(strcmp(path, "/")==0){
    return compute_address(0);
  }
  
  int num_dir_ent = -1; // the offset within the block of the dir_ent for the directory we're looking for
  struct dir_ent dir_data[BLOCK_SIZE/32]; // array of dir_ent to hold data of a directory
  int block_address = superblock.s.root_address; // block address starts as that of the superblock

  // parse path name, separating by "/"
  char * path_piece = strtok((char *)path, "/");

  while(path_piece != NULL){
    
    // read in dir_data
    FILE *d = fopen(cwd, "r+");
    pread_check(fileno(d), &dir_data, BLOCK_SIZE, block_address);
    fclose(d);
    
    num_dir_ent = -1;
    // look through each dir_ent to find path_piece
    for (int i=0; i<BLOCK_SIZE/32; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	num_dir_ent = i;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    // if path_piece isn't a file_name of any dir_ent, it doesn't exist (data_offset hasn't been changed from initial -1 val)
    path_piece = strtok(NULL, "/");
    if(num_dir_ent==-1){ return -ENOENT;}
  }

  block_address = compute_address(dir_data[num_dir_ent].first_cluster);
  return block_address;
}

/*
// return the num_dir_ent at which the path's dir_ent is in dir_data and block_address 
// (and populates dir_data with the directory entries of the directory containing path)
struct file_info {
  int block_address;
  int num_dir_ent;
};
static struct file_info find_file_dir_ent(const char *path, struct dir_ent * dir_data){
  int block_address = superblock.s.root_address;
  char * path_piece = strtok((char *) path, "/");
  
  int num_dir_ent = -1;
  FILE *disk;
  struct file_info result;
  
  if (strcmp(path, "/")==0){
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), dir_data, BLOCK_SIZE, block_address);
    fclose(disk);
    result.block_address = block_address;
    result.num_dir_ent = 0;
    return result;
  }

  while(path_piece != NULL){
    num_dir_ent = -1;

    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), dir_data, BLOCK_SIZE, block_address);
    fclose(disk);

    for(int i=0; i<BLOCK_SIZE/32; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	num_dir_ent = i;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    if(num_dir_ent==-1){return;}
    path_piece = strtok(NULL, "/");
  }

  disk = fopen(cwd, "r+");
  pread_check(fileno(disk), dir_data, BLOCK_SIZE, block_address);
  fclose(disk);

  printf("pre: %d\n", block_address);
  result.block_address = block_address;
  result.num_dir_ent = num_dir_ent;
  return result;  
}
*/

/* 
 * FUSE functions
 */

static void* fat_init(struct fuse_conn_info *conn) {

  strcat(cwd, "/fat_disk");
  FILE *disk;

  // file exists  
  if (access(cwd, F_OK) != -1){
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), &FAT, BLOCKS_TOTAL, 512);
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
    memset(FAT, 0, BLOCKS_TOTAL);

    // make superblock
    superblock.s.root_address = 512 + BLOCKS_TOTAL*4; //512 + 2440*4 (superblock + FAT)
    superblock.s.block_size = BLOCK_SIZE; // our assumed uniform block size - check this
    pwrite_check(fileno(disk), &superblock, 512, 0); // write to disk
    fclose(disk);
    
    // make an array of dir_ent and populate with . and .. 4096/32=128 , write to disk at block zero
    mkdir_helper(0, 0);
  }
  
  // create free list
  for(int i=1; i < BLOCKS_TOTAL; i++){
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

static int make_new(const char* path, int mode){
  
  if(freeListHead==NULL){
    return -ENOMEM;
  }

  struct dir_ent dir_data[BLOCK_SIZE/32];
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
    pread_check(fileno(disk), &dir_data, BLOCK_SIZE, block_address);
    fclose(disk);

    // iterate through all dir_ent looking for one w/ file_name of path_piece
    for(int i=0; i<BLOCK_SIZE/32; i++){
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
  pread_check(fileno(disk), &dir_data, BLOCK_SIZE, block_address);
  fclose(disk);

  // looking for first dir_ent that is empty - that is where we put the new dir
  for (int i=0; i<BLOCK_SIZE/32; i++){
    if(dir_data[i].type==EMPTY_T){

      //      printf("mkdir dir_ent in %d \n %d \n\n", block_address, i);

      int new_block = freeListHead-> value;
      freeListHead = freeListHead->next; // update free list - add check for not null
      strcpy(dir_data[i].file_name, new_dir_name); //updates data by adding new dir as dir_ent

      if(mode==DIR_T){
	
	mkdir_helper(new_block, compute_block(block_address));
	
	dir_data[i].type = DIR_T;
	dir_data[i].size = BLOCK_SIZE;
	dir_data[i].first_cluster = new_block;
      }
      if(mode==FILE_T){
	dir_data[i].type = FILE_T;
	dir_data[i].size = BLOCK_SIZE; //???
	dir_data[i].first_cluster = new_block;
      }

      disk = fopen(cwd, "r+");
      pwrite_check(fileno(disk), &dir_data, BLOCK_SIZE, block_address); // write to disk
      fclose(disk);

      return 0;
    }
  }
  return -ENOSPC;
}

static int fat_mkdir(const char* path, mode_t mode){
  return make_new(path, DIR_T);
}

static int fat_getattr(const char *path, struct stat *stbuf){
  
  memset(stbuf, 0, sizeof(struct stat));
  
  // case for root
  if(strcmp(path, "/")==0){
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_size = BLOCK_SIZE;
    return 0;
  }

  int num_dir_ent = -1; // the offset within the block of the dir_ent for the directory we're looking for
  struct dir_ent dir_data[BLOCK_SIZE/32]; // array of dir_ent to hold data of a directory
  int block_address = superblock.s.root_address; // block address starts as that of the superblock

  // parse path name, separating by "/"
  char * path_piece = strtok((char *)path, "/");
  while(path_piece != NULL){
    
    // read in dir_data
    FILE *d = fopen(cwd, "r+");
    pread_check(fileno(d), &dir_data, BLOCK_SIZE, block_address);
    fclose(d);
    
    num_dir_ent = -1;
    // look through each dir_ent to find path_piece
    for (int i=0; i<BLOCK_SIZE/32; i++){
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
  
  struct dir_ent d = dir_data[num_dir_ent];
  // directory attrs
  if (d.type == DIR_T){
    stbuf->st_mode = S_IFDIR | 0755;
  }
  // file attrs
  else if(d.type == FILE_T){
    stbuf->st_mode = S_IFREG | 0666;
  }
  
  stbuf->st_size = dir_data[num_dir_ent].size;
  return 0;
}

static int fat_access(const char* path, int mask ){
  
  if (find_file(path) != -ENOENT){
    return 0;
  }
  return -ENOENT;
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
  (void) offset;
  (void) fi;
  FILE *disk;
  struct dir_ent dir_data[BLOCK_SIZE/32];

  int block_address = find_file(path);
  
  if (block_address == -ENOENT){
      return -ENOENT;
  }
  
  disk = fopen(cwd, "r+");
  // read dir_ent of that directory
  pread_check(fileno(disk), &dir_data, BLOCK_SIZE, block_address);
  fclose(disk);
  // iterate through dir_ents
  for(int i=0; i<BLOCK_SIZE/32; i++){
    if(dir_data[i].type != EMPTY_T){
      filler(buf, dir_data[i].file_name, NULL, 0);
    }
  }
  return 0;
}

static int fat_rmdir(const char* path){

  // cannot remove root
  if(strcmp(path, "/")==0){
    return -ENOENT;
  }
  
  FILE * d;
  int num_dir_ent = -1;
  struct dir_ent dir_data[BLOCK_SIZE/32];
  int block_address = superblock.s.root_address;
  int prev_addr = block_address;

  char * path_piece = strtok((char *)path, "/");
  while(path_piece != NULL){
    
    // read in dir_data
    d = fopen(cwd, "r+");
    pread_check(fileno(d), &dir_data, BLOCK_SIZE, block_address);
    fclose(d);
    
    num_dir_ent = -1;
    // look through each dir_ent to find path_piece
    for (int i=0; i<BLOCK_SIZE/32; i++){
      if(dir_data[i].type != EMPTY_T && strcmp(path_piece, dir_data[i].file_name)==0){
	num_dir_ent = i;
	prev_addr = block_address;
	block_address = compute_address(dir_data[i].first_cluster);
	break;
      }
    }
    path_piece = strtok(NULL, "/");
    if(num_dir_ent==-1){ return -ENOENT;}
  }

  // checks that directory is empty
  d = fopen(cwd, "r+");
  pread_check(fileno(d), &dir_data, BLOCK_SIZE, block_address);
  fclose(d);
  for(int i=2; i<BLOCK_SIZE/32; i++){
    if(dir_data[i].type !=EMPTY_T){
      return -ENOTEMPTY;
    }
  }

  d = fopen(cwd, "r+");
  pread_check(fileno(d), &dir_data, BLOCK_SIZE, prev_addr);
  fclose(d);

  // changes directory entry of removed dir to EMPTY
  dir_data[num_dir_ent].type = EMPTY_T;

  d = fopen(cwd, "r+");
  pwrite_check(fileno(d), &dir_data, BLOCK_SIZE, prev_addr);
  fclose(d);

  // add to free list
  if (freeListTail != NULL){
    struct Node * newTail = (struct Node*)malloc(sizeof(struct Node));
    newTail -> value = compute_block(block_address);
    freeListTail ->next = newTail;
    freeListTail = freeListTail -> next;
  } else{
    freeListHead = (struct Node *)malloc(sizeof(struct Node));
    freeListHead-> value = compute_block(block_address);
    freeListTail = freeListHead;
  }
  return 0;  
}

static int fat_open(const char* path, struct fuse_file_info* fi){
  int addr = find_file(path);
  if(addr==-ENOENT){
    return -ENOENT;
  }
  return 0;
}

static int fat_mknod(const char* path, mode_t mode, dev_t rdev){
  // only supports plain files
  if(mode!=S_IFREG){
    return -ENOSYS;
  }
  return make_new(path, FILE_T);
}

// makes a plain file
static int fat_fcreate(const char* path, mode_t mode){
  return make_new(path, FILE_T);
}

// free temporarily allocated data structures
static int fat_release(const char* path, struct fat_file_info* fi){
  return 0;
}

// read size bytes from file, starting at offset
static int fat_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info *fi){

  // block_address is the address at which data of path begins
  int block_address = find_file(path);
  if(block_address == -ENOENT){ return 0;}

  int start_read_block = compute_block(block_address);
  // account for offset (potentially a later block)
  while(offset > BLOCK_SIZE){
    start_read_block = FAT[start_read_block];
    if( start_read_block == 0){ return 0;}
    offset -= BLOCK_SIZE;
  }
  // now offset is offset%BLOCK_SIZE
  block_address = compute_address(start_read_block);

  char data_block[BLOCK_SIZE];
  FILE *disk;

  // First read might start at an offset of a block (not from start of block)
  // read a block of data from disk
  disk = fopen(cwd, "r+");
  pread_check(fileno(disk), &data_block, BLOCK_SIZE, block_address);
  fclose(disk);

  // put appropriate amount into read buffer
  int block_read_size = size;
  if(size + offset > BLOCK_SIZE){ block_read_size = BLOCK_SIZE - offset;}
  memcpy(buf, data_block+offset, block_read_size); // subtract offset from amount to put in buf 

  int amt_read = block_read_size;

  // now reading blocks from their start
  while(amt_read != size){ // since size could be larger than 1 block

    // use FAT to find next block of file
    int next_block = FAT[compute_block(block_address)];
    if(next_block ==0){ return amt_read; }
    block_address = compute_address(next_block);

    // read a block of data from disk
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), &data_block, BLOCK_SIZE, block_address);
    fclose(disk);

    // calculate amount of this block to put in buf
    block_read_size = size - amt_read;
    if(block_read_size >= BLOCK_SIZE){ block_read_size = BLOCK_SIZE;}
    memcpy(buf+amt_read, data_block, block_read_size);
    
    amt_read += block_read_size;
  }
  return amt_read;
}

//write size bytes to buff
static int fat_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info *fi){
  
  // block_address is the address at which data of path begins
  int block_address = find_file(path);
  if(block_address == -ENOENT){ return -ENOENT;}

  int start_write_block = compute_block(block_address);
  int next_block = start_write_block;
  // account for offset (potentially a later block)
  while(offset > BLOCK_SIZE){
    //    start_write_block = FAT[start_write_block];
    next_block = FAT[start_write_block];
    // file not big enough
    if( next_block == 0){
      if(freeListHead==NULL){ return -ENOMEM; }
      // gets next block from free list
      FAT[start_write_block] = freeListHead->value;
      next_block = freeListHead->value;
      
      freeListHead = freeListHead->next;
    }
    start_write_block = next_block;
    offset -= BLOCK_SIZE;
  }
  // now offset is offset%BLOCK_SIZE
  block_address = compute_address(start_write_block);

  char data_block[BLOCK_SIZE];
  FILE *disk;

  // First write might start at an offset of a block (not from start of block)
  // write a block of data to disk
  disk = fopen(cwd, "r+");
  pread_check(fileno(disk), &data_block, BLOCK_SIZE, block_address);
  fclose(disk);

  // put appropriate amount into write buffer
  int block_write_size = size;
  if(size + offset > BLOCK_SIZE){ block_write_size = BLOCK_SIZE - offset; }
    
  memcpy(data_block+offset, buf, block_write_size); // subtract offset from amount to put in buf 

  // write to disk
  disk = fopen(cwd, "r+");
  pwrite_check(fileno(disk), &data_block, BLOCK_SIZE, block_address);
  fclose(disk);
  
  int amt_written = block_write_size-offset;

  // now writing to blocks from their start
  while(amt_written != size){ // since size could be larger than 1 block

    // use FAT to find next block of file
    int next_block = FAT[compute_block(block_address)];
    if(next_block == 0){ return amt_written; }
    block_address = compute_address(next_block);

    // read a block of data from disk
    disk = fopen(cwd, "r+");
    pread_check(fileno(disk), &data_block, BLOCK_SIZE, block_address);
    fclose(disk);

    // calculate amount of the buf to put in data
    block_write_size = size - amt_written;
    if(block_write_size >= BLOCK_SIZE){ block_write_size = BLOCK_SIZE;}
    // put the buf into data
    memcpy(data_block, buf+amt_written, block_write_size);

    // write to disk
    disk = fopen(cwd, "r+");
    pwrite_check(fileno(disk), &data_block, BLOCK_SIZE, block_address);
    fclose(disk);
    
    amt_written += block_write_size;
  }
  return amt_written;
}

// resize stuff
static int fat_truncate(const char* path, off_t size){

  // see how many blocks size takes (rounding up (size+(size%BLOCK_SIZE)/BLOCK_SIZE)
  
  // follow FAT, counting how many blocks are in the file
  
  // if file is too short, add blocks from freeList (updating FAT)
  // if file is too long, iterate over remaining blocks and add to free list (& update FAT[] to 0)

  // write FAT to disk

  // update dir_ent size for path
}

static int fat_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi){
  return fat_getattr(path, stbuf);
}

// iterate through free list and count++
static int fat_statfs(const char* path, struct statvfs* stbuf){
  int count = 0;
  struct Node * iter = freeListHead;
  while( iter != NULL){
    count++;
    iter = iter->next;
  }
  return count;
}

//
//static int fat_symlink(const char* to, const char* from){}

static struct fuse_operations fat_operations = {
	.init		= fat_init,
	.getattr	= fat_getattr,
	.mkdir		= fat_mkdir,
	.access		= fat_access,
	.readdir        = fat_readdir,
	.fgetattr       = fat_fgetattr,
	.statfs		= fat_statfs,
	.rmdir		= fat_rmdir,
	.open		= fat_open,
	.mknod		= fat_mknod,
	.write          = fat_write,
	.read           = fat_read,
	.create		= fat_fcreate
/*
	.readlink	= NULL,
	.symlink	= NULL,
	.unlink		= NULL,
	.rename		= NULL,
	.link		= NULL,
	.chmod		= NULL,
	.chown		= NULL,
	.truncate	= NULL,
	.utimens	= NULL,
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
	return fuse_main(argc, argv, &fat_operations, NULL);
}
