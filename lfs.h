#ifndef __LFS_H__
#define __LFS_H__
#define NBLOCKS 	14				// max number of blocks per INODE
#define NINODES 	4096			// max number of INODEs in system
#define CPRSIZE		6					// size (in blocks) of checkpoint region TODO
#define BLOCKSIZE	4096			// size (in bytes) of one block
#define DIRENTRYSIZE	32		// size (in bytes) of a directory entry
#define NENTRIES	(BLOCKSIZE/DIRENTRYSIZE)	// number of entries per block in a directory
#define NAMELENGTH	28			// length (in bytes) of a directory entry name

typedef struct __INODE {
	int inum;
	int size;								// number of bytes in the file. a multiple of BLOCKSIZE
	int type;
	int filled[NBLOCKS];			// used[i] is true if blocks[i] is used
	int data[NBLOCKS];		// address in memory of each block
} INODE;

typedef struct __dirBlock {
	char names[NENTRIES][NAMELENGTH];
	int  inums[NENTRIES];
} dirBlock;

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
#endif
