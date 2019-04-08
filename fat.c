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
	//	printf("tail val: %d\n", freeListTail->value);
	freeListTail -> next = newTail;
	freeListTail = freeListTail -> next;
      }
    }
  }
  return NULL;
}


static int fat_mkdir(const char* path, mode_t mode){
  struct dir_ent block[128];
  int i = 2;
  while(block[i] != NULL && block[i].type!=EMPTY_T){
      i++;
  }

  //  const char *token = strtok("/new", "/");
  int new_block = freeListHead->value;
  freeListHead = freeListHead->next;

  //  memset(block[i].file_name, 0, 24);
  //  strcpy(block[i].file_name,token);
  block[i].type = DIR_T;
  block[i].first_cluster = new_block;
  
  FILE *disk1 = fopen(cwd, "r+");
  pwrite(fileno(disk1), &block, 4096, 10272);
  fclose(disk1);

  struct dir_ent b[128];
  FILE *disk2 = fopen(cwd, "r+");
  pread(fileno(disk2), &b, 4096, 10272);
  fclose(disk2);
  printf("head: %d\n\n", b[i].first_cluster);
  if(freeListHead!=NULL){
    printf("next: %d\n\n", freeListHead->value);
  }
  return 0;
}

static int fat_getattr(const char *path, struct stat *stbuf){
  //  printf("attr: %d\n\n", freeListHead->value);
  memset(stbuf, 0, sizeof(struct stat));

  struct dir_ent s[128];
  FILE * d = fopen(cwd, "r+");
  pread(fileno(d), &s, 4096, 10272);
  fclose(d);

  if(strcmp(path, "/")==0){
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_size = s[0].first_cluster;
    return 0;
  }

  return 0;

}

static int fat_access(const char* path, int mask ){
  return 0;
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
}

static struct fuse_operations fat_operations = {
	.init		= fat_init,
	.getattr	= fat_getattr,
	.mkdir		= fat_mkdir,
	.access		= fat_access,
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
