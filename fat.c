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

char cwd[256];
char FAT[4880];

struct Node {
  int value;
  struct Node *next;
  struct Node *prev;
};

struct Node *freeListHead = NULL;
struct Node *freeListTail = NULL;


static void* fat_init(struct fuse_conn_info *conn) {
  strcat(cwd, "/fat_disk");
  FILE *disk;

  // file exists  
  if (access(cwd, F_OK) != -1){
    disk = fopen(cwd, "r+");
    // read from offset 512 and read 4880 bytes in fat
    fseek(disk, 512, SEEK_SET);
    // init fat from file
    fgets(FAT, 4880, disk);
    fclose(disk);
  }
  
  // file doesn't exist
  else {
    disk = fopen(cwd, "w+");
    fseek(disk, 10000000-1, SEEK_SET);
    fputc(0, disk);
    fclose(disk);
    // init fat if no disk
    memset(FAT, 0, 4880);
  }
  // create free list
  for(int i=0; i < 4880; i = i+2){
    if(FAT[i] == 0 && FAT[i+1] == 0){
      if (freeListHead == NULL){
	freeListHead = (struct Node*)malloc(sizeof(struct Node));
	freeListHead -> value = i/2;
	freeListHead -> next = freeListTail;

      } else { // freelist head is not null + free list tail is null
	freeListTail = (struct Node*)malloc(sizeof(struct Node));
	freeListTail -> value = i/2;
	freeListTail -> next = NULL;
	freeListTail = freeListTail -> next;

      } 
      
    } 
  }
	return NULL;
}

static int fat_getattr(const char *path, struct stat *stbuf){
  

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
