#define NUM_BLOCKS 	14
#define NUM_INODES 	4096
#define CHECK_POINT_SIZE	6
#define BLOCK_SIZE	4096
#define DSIZE	32
#define NUM_ENTRIES	(BLOCK_SIZE/DSIZE)
#define MAX_NAME_LEN	28
#define BUFFERSIZE (4096)
#define MAX_NAME_SIZE (28)


typedef struct __Payload {
	int op;
	int inum;
	int block;
	int type;

	char name[MAX_NAME_SIZE];
	char buffer[BUFFERSIZE];
	MFS_Stat_t stat;
} Payload;

typedef struct __dir_entries {
	char names[NUM_ENTRIES][MAX_NAME_LEN];
	int  inums[NUM_ENTRIES];
} dir_entries;

typedef struct __INODE {
	int inum;
	int size;
	int type;
	int filled[NUM_BLOCKS];
	int data[NUM_BLOCKS];
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
