#define NBLOCKS 	14
#define NINODES 	4096
#define CPRSIZE		6
#define BLOCKSIZE	4096
#define DIRENTRYSIZE	32
#define NENTRIES	(BLOCKSIZE/DIRENTRYSIZE)
#define NAMELENGTH	28

typedef struct __dir_entries {
	char names[NENTRIES][NAMELENGTH];
	int  inums[NENTRIES];
} dir_entries;

typedef struct __INODE {
	int inum;
	int size;
	int type;
	int filled[NBLOCKS];
	int data[NBLOCKS];
} INODE;

int get_INODE(int inum, INODE* n);
int build_dir_block(int firstBlock, int inum, int pinum);
void update_CR(int dirty_inum);
int Server_Startup();
int Server_Lookup(int pinum, char *name);
int Server_Stat(int inum, MFS_Stat_t *m);
int Server_Write(int inum, char *buffer, int block);
int Server_Read(int inum, char *buffer, int block);
int Server_Creat(int pinum, int type, char *name);
int Server_Unlink(int pinum, char *name);
int Server_Shutdown();
