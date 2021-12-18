#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "mfs.h"
#include "lfs.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "udp.h"

typedef struct __buf {
	char string [BLOCKSIZE/sizeof(char)];
} buf;
int newFS;

int inodemap[NINODES/16][16];			// block number of each INODE
int log_end;					// next block in the address space to be written
int fd;										// the file descriptor of the LFS

void update_CPR_inode_num(int INODE_num, int block){
	inodemap[INODE_num/16][INODE_num%16]=block;
}
int get_CPR_inode_num(int INODE_num){
	return inodemap[INODE_num/16][INODE_num%16];
}

int get_INODE(int inum, INODE* n) {
	
	if(inum < 0 || inum >= NINODES)		// check for invalid inum
	{
		printf("get_INODE: invalid inum\n");
		return -1;
	}
	
	int iblock = get_CPR_inode_num(inum);					// block where desired INODE is written
	
	lseek(fd, iblock*BLOCKSIZE, SEEK_SET);
	read(fd, n, sizeof(INODE));

	return 0;
}

// Returns block number of new block
// pinum is unused if firstBlock == 0
int build_dir_block(int firstBlock, int inum, int pinum)
{
	dir_entries db;
	int i;
	for(i = 0; i < NENTRIES; i++)
	{
		db.inums[i] = -1;
		strcpy(db.names[i], "DNE\0");
	}

	if(firstBlock)
	{
		db.inums[0] = inum;
		strcpy(db.names[0], ".\0");
		db.inums[1] = pinum;
		strcpy(db.names[1], "..\0");
	}
	
	//print_dir_entries(log_end);
	// write new block
	lseek(fd, log_end*BLOCKSIZE, SEEK_SET);
	write(fd, &db, BLOCKSIZE);
	log_end++;

	//print_dir_entries(log_end-1);

	return log_end-1;
}

void update_CR(int dirty_inum)
{
	if(dirty_inum != -1)
	{
		lseek(fd, dirty_inum*sizeof(int), SEEK_SET);		// update INODE table
		write(fd, &inodemap[dirty_inum/16][dirty_inum%16], sizeof(int));
	}

	lseek(fd, NINODES*sizeof(int), SEEK_SET);	// update log_end
	write(fd, &log_end, sizeof(int));
}

void init_node(INODE* node) {
	// Initialiing all other entries to -1
	for(int i = 0; i < NBLOCKS; i++) {
		node->filled[i] = 0;
		node->data[i] = -1;
	}
}

void init_dir(dir_entries *block) {
	// Initialiing dir entries to -1 and copying empty strings
	for(int i = 0; i < NENTRIES; i++) {
		block->inums[i] = -1;
		strcpy(block->names[i], "DNE\0");
	}
}

// int Server_Startup(int port, char* fsimage) {
// 	int fd = open(fsimage, O_RDWR);
// 	if(fd == -1) {
// 		// fsimage doesnt exist
// 		newFS = 1;

// 		fd = open(fsimage, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
// 		if(fd == -1) {
// 			// Open Failed
// 			return -1;
// 		}
// 		log_end = CPRSIZE;

// 		for(int i = 0; i < NINODES; i++) {
// 			// Setting offsets of INODEs to -1
// 			inodemap[i] = -1;
// 		}

// 		// Writing inodemap
// 		lseek(fd, 0, SEEK_SET);
// 		write(fd, inodemap, sizeof(int) * NINODES);

// 		// Writing checkpoint region
// 		write(fd, &log_end, sizeof(int));

// 		// Initialiing all other entries to -1
// 		INODE node;
// 		init_node(&node);

// 		// Filling Root Directory
// 		node.inum = 0;
// 		node.type = MFS_DIRECTORY;
// 		node.data[0] = log_end;
// 		node.size = BLOCKSIZE;
// 		node.filled[0] = 1;
		
// 		dir_entries rootdir;
// 		init_dir(&rootdir);
// 		rootdir.inums[0] = 0;
// 		strcpy(rootdir.names[0], ".\0");
// 		rootdir.inums[1] = 0;
// 		strcpy(rootdir.names[1], "..\0");

// 		// write rootdir
// 		lseek(fd, log_end * BLOCKSIZE, SEEK_SET);
// 		write(fd, &rootdir, sizeof(rootdir));
// 		log_end++;
		
// 		// Writing block number to inodemap
// 		inodemap[0] = log_end;

// 		// write INODE
// 		lseek(fd, log_end * BLOCKSIZE, SEEK_SET);
// 		write(fd, &node, sizeof(INODE));
// 		log_end++;

// 		lseek(fd, 0, SEEK_SET);
// 		write(fd, inodemap[0], sizeof(int));

// 		lseek(fd, 4096 * sizeof(int), SEEK_SET);
// 		write(fd, &log_end, sizeof(int));
// 	} else {
// 		// Existing file image
// 		newFS = 0;
// 		lseek(fd, 0, SEEK_SET);
// 		// Reading inodemap and checkpointe end region
// 		read(fd, inodemap, sizeof(int) * NINODES);
// 		read(fd, &log_end, sizeof(int));
// 	}	
// 	serverListen(port);
// 	return 0;
// }
void serverListen(int port);
int Server_Startup(int port, char* path) {
	
	if((fd = open(path, O_RDWR)) == -1)
	{
		newFS = 1;
		//printf("CREATING NEW FILE SYSTEM\n\n");

		// create new file system
		fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
		if(fd == -1)
			return -1;
		log_end = CPRSIZE;
		
		//printf("fd = %d\n", fd);

		int i;
		for(i = 0; i < NINODES; i++)
		{
			update_CPR_inode_num(i,-1);
		}

		lseek(fd, 0, SEEK_SET);
		write(fd, inodemap, sizeof(int)*NINODES);
		write(fd, &log_end, sizeof(int));

		// create root
		//
		//printf("=================================\nRoot before doing anything:\n\n");
		//print_dir_entries(7);
		//printf("=================================\n\n\n\n\n");

		INODE n;
		n.inum = 0;
		n.size = BLOCKSIZE;
		n.type = MFS_DIRECTORY;
		n.filled[0] = 1;
		n.data[0] = log_end;
		for(i = 1; i < NBLOCKS; i++)
		{
			n.filled[i] = 0;
			n.data[i] = -1;
		}

		dir_entries baseBlock;
		baseBlock.inums[0] = 0;
		baseBlock.inums[1] = 0;
		strcpy(baseBlock.names[0], ".\0");
		strcpy(baseBlock.names[1], "..\0");

		for(i = 2; i < NENTRIES; i++)
		{
			baseBlock.inums[i] = -1;
			strcpy(baseBlock.names[i], "DNE\0");
		}

		// write baseBlock
		lseek(fd, log_end*BLOCKSIZE, SEEK_SET);
		write(fd, &baseBlock, sizeof(dir_entries));
		log_end++;
		
		// update imap
		update_CPR_inode_num(0,log_end);

		// write INODE
		lseek(fd, log_end*BLOCKSIZE, SEEK_SET);
		write(fd, &n, sizeof(INODE));
		log_end++;

		// write checkpoint region
		update_CR(0);

		//printf("=======================================\nRoot (block number %d) after creation:\n\n\n\n", imap[0]);
		//print_dir_entries(imap[0]);
		//printf("========================================\n\n\n\n");
	}
	else
	{
		newFS = 0;
		//printf("USING OLD FILE SYSTEM\n\n");

		lseek(fd, 0, SEEK_SET);
		read(fd, inodemap, sizeof(int)*NINODES);
		read(fd, &log_end, sizeof(int));
	}	
	serverListen(port);
	return 0;
}

int Server_Lookup(int pinum, char *name) {
	INODE parent_INODE;
	int INODE_exist_status = get_INODE(pinum, &parent_INODE);
	if(INODE_exist_status == -1) {
		// Inavlid Inode
		return -1;
	}

	printf("Parent Inode\n");
	printf("Parent Inde Type = %d , Size = %d\n", parent_INODE.type, parent_INODE.size);

	int i;
	for (i = 0; i < NBLOCKS; i++) {
		// If a filled block found
		if (parent_INODE.filled[i]) {

			dir_entries foundblock;
			lseek(fd, parent_INODE.data[i] * BLOCKSIZE, SEEK_SET);
			read(fd, &foundblock, BLOCKSIZE);

			for(int j = 0; j < NENTRIES; j++) {
				// Valid Inum Check
				if(foundblock.inums[j] != -1) {
					// Inode found
					if(strcmp(foundblock.names[j], name) == 0) {
						// Returning INODE number
						return foundblock.inums[j];
					}
				}
			}
		}
	}

	// Lookup Failed: Not Found
	return -1;
}

int Server_Stat(int INODE_num, MFS_Stat_t *mfs_stat) {
	INODE node;
	int INODE_exist_status = get_INODE(INODE_num, &node);
	if(INODE_exist_status == -1) {
		// Inavlid Inode
		return -1;
	}
	// Filling MFS_Stat structure
	mfs_stat->type = node.type;
	mfs_stat->size = node.size;
	return 0;
}

int Server_Write(int INODE_num, char *buffer, int block) {
	INODE node; 
	int INODE_exist_status = get_INODE(INODE_num, &node);
	if(INODE_exist_status == -1) {
		// Invalid Inode
		return -1;
	}
	
	if(node.type != MFS_REGULAR_FILE) {
		// Trying to write into a directory
		return -1;
	}

	if(block < 0 || block >= 14) {	
		// Invalid Block
		return -1;
	}
	
	int new_node_size = 0;
	if ((block + 1) * BLOCKSIZE > node.size) {
		new_node_size = (block + 1) * BLOCKSIZE;
	} else {
		new_node_size = node.size;
	}
	node.size = new_node_size;
	node.filled[block] = 1;

	node.data[block] = log_end + 1;

	// Persist Inode
	lseek(fd, log_end * BLOCKSIZE, SEEK_SET);
	write(fd, &node, BLOCKSIZE);

	// Updating Inode
	update_CPR_inode_num(INODE_num, log_end);
	log_end++;
	
	// Write data block
	lseek(fd, log_end * BLOCKSIZE, SEEK_SET);
	write(fd, buffer, BLOCKSIZE);
	log_end++;

	update_CR(INODE_num);
	return 0;
}

int Server_Read(int INODE_num, char *buffer, int block){
	INODE node;
	if(get_INODE(INODE_num, &node) == -1)
	{
		printf("get_INODE failed for inum %d.\n", INODE_num);
		return -1;
	}

	if(block < 0 || block >= NBLOCKS || !node.filled[block])		// check for invalid block
	{
		printf("invalid block.\n");
		return -1;
	}

	// read
	if(node.type == MFS_REGULAR_FILE)																		// read regular file
	{
		//printf("Reading a file\n");
		if(lseek(fd, node.data[block]*BLOCKSIZE, SEEK_SET) == -1)
		{
			perror("Server_Read: lseek:");
			printf("Server_Read: lseek failed\n");
		}
		
		if(read(fd, buffer, BLOCKSIZE) == -1)
		{
			perror("Server_Read: read:");
			printf("Server_Read: read failed\n");
		}
		//printf("File reads: %s\n", buffer);
	}
	else																									// read directory
	{
		dir_entries db;																				// read dir_entries
		lseek(fd, node.data[block], SEEK_SET);
		read(fd, &db, BLOCKSIZE);

		MFS_DirEnt_t entries[NENTRIES];											// convert dir_entries to MRS_DirEnt_t
		int i;
		for(i = 0; i < NENTRIES; i++)
		{
			MFS_DirEnt_t entry ;
			strcpy(entry.name, db.names[i]);
			entry.inum = db.inums[i];
			entries[i] = entry;
		}

		memcpy(buffer, entries, sizeof(MFS_DirEnt_t)*NENTRIES);
	}
	return 0;
}

int Server_Creat(int pinum, int type, char *name){
	int exists = Server_Lookup(pinum, name);
	if (exists != -1) {
		printf("Already Exists\n");
		return 0;
	}

	INODE parent_node;
	int INODE_exists = get_INODE(pinum, &parent_node);
	if(INODE_exists == -1) {
		printf("Inode does not exist\n");
		return -1;
	}

	if(parent_node.type != MFS_DIRECTORY) {
		printf("Parent is not a directory");
		return -1;
	}
	
	int INODE_num = -1;
	for(int i = 0; i < NINODES; i++) {
		if(get_CPR_inode_num(i) == -1) {
			INODE_num = i;
			printf("Next available Inode num = %d\n", INODE_num);
			break;
		}
	}

	if(INODE_num == -1) {
		printf("No more available INODEs");
		return -1;
	}

	int block_index;
	int entry; 
	dir_entries block;
	for(block_index = 0; block_index < NBLOCKS; block_index++) {
		if(parent_node.filled[block_index]) {
			lseek(fd, parent_node.data[block_index]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			for(entry = 0; entry < NENTRIES; entry++) {
				if(block.inums[entry] == -1) {
						// write parent
						lseek(fd, inodemap[pinum/16][pinum%16]*BLOCKSIZE, SEEK_SET);
						write(fd, &parent_node, BLOCKSIZE);

						block.inums[entry] = INODE_num;
						strcpy(block.names[entry], name);
						lseek(fd, parent_node.data[block_index]*BLOCKSIZE, SEEK_SET);
						write(fd, &block, BLOCKSIZE);

						// create INODE
						INODE node;
						node.inum = INODE_num;
						node.size = 0;
						for(int i = 0; i < NBLOCKS; i++) {
							node.filled[i] = 0;
							node.data[i] = -1;
						}
						node.type = type;	
						if(type == MFS_DIRECTORY) {
							node.filled[0] = 1;
							node.data[0] = log_end;
							
							build_dir_block(1, INODE_num, pinum);
							node.size += BLOCKSIZE;
						} else if (type != MFS_DIRECTORY && type != MFS_REGULAR_FILE) {
							return -1;
						}

						// update imap
						update_CPR_inode_num(INODE_num, log_end);

						// write INODE
						lseek(fd, log_end*BLOCKSIZE, SEEK_SET);
						write(fd, &node, sizeof(INODE));
						log_end++;

						// write checkpoint region
						update_CR(INODE_num);
						return 0;
				}
			}
		} else {
			int dir_block = build_dir_block(0, INODE_num, -1);
			parent_node.size += BLOCKSIZE;
			parent_node.filled[block_index] = 1;
			parent_node.data[block_index] = dir_block;
			block_index--;
		}
	}

	return -1;

	

}

int Server_Unlink(int pinum, char *name){
	INODE toRemove;
	INODE parent_INODE;

	int exists = get_INODE(pinum, &parent_INODE);
	if(exists == -1) {
		printf("Parent Inode does not exist\n");
		return -1;
	}

	printf("Finding INODE numebr of file/dir name = %s to be unlinked\n", name);
	int INODE_num = Server_Lookup(pinum, name);
	int INODE_exists = get_INODE(INODE_num, &toRemove);
	if(INODE_exists == -1) {
		printf("Name not existing is not a failure\n");
		return 0;
	}

	if(toRemove.type == MFS_DIRECTORY) {
		int block_index;
		for(block_index = 0; block_index < NBLOCKS; block_index++) {
			if(toRemove.filled[block_index]) {
				dir_entries block;
				lseek(fd, toRemove.data[block_index] * BLOCKSIZE, SEEK_SET);
				read(fd, &block, BLOCKSIZE);

				int entry;
				for(entry = 0; entry < NENTRIES; entry++) {
					if(block.inums[entry] != -1 && strcmp(block.names[entry], ".") != 0 && strcmp(block.names[entry], "..") != 0) {
						printf("Directory not empty\n");
						return -1;
					}
				}
			}
		}
	}
	
	int unlink_done = 0;
	int blockindex = 0;
	for(; blockindex < NBLOCKS && !unlink_done; blockindex++) {
		if(parent_INODE.filled[blockindex]) {
			dir_entries block;
			lseek(fd, parent_INODE.data[blockindex]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			int entry_index;
			for(entry_index = 0; entry_index < NENTRIES && !unlink_done; entry_index++) {
				if(block.inums[entry_index] != -1) {
					if(strcmp(block.names[entry_index], name) == 0) {
						block.inums[entry_index] = -1;
						strcpy(block.names[entry_index], "DNE\0");
						unlink_done = 1;
					}
				}
			}

			if(unlink_done) {
				lseek(fd, log_end * BLOCKSIZE, SEEK_SET);
				write(fd, &block, BLOCKSIZE);
				log_end++;

				// inform parent INODE of new block location
				parent_INODE.data[blockindex] = log_end-1;
				lseek(fd, log_end*BLOCKSIZE, SEEK_SET);
				write(fd, &parent_INODE, BLOCKSIZE);
				log_end++;

				// update inodemap
				update_CPR_inode_num(pinum, log_end-1);
				update_CR(pinum);
			}
		}
	}

	printf("Removing INODE from INODE map");
	update_CPR_inode_num(INODE_num, -1);
	update_CR(INODE_num);
	return 0;
}

int Server_Shutdown()
{
	fsync(fd);			// not sure if this is necessary
	exit(0);
	return -1;	// if we reach this line of code, there was an error
}

void serverListen(int port)
{
	int sd = UDP_Open(port);
	if(sd < 0)
	{
		printf("Error opening socket on port %d\n", port);
		exit(1);
	}

    printf("Starting server...\n");
    while (1) {
		struct sockaddr_in s;
		Payload packet;
		int rc = UDP_Read(sd, &s, (char *)&packet, sizeof(Payload));
		if (rc > 0) {
		    Payload responsePacket;

		    if (packet.op == 0) {
				responsePacket.inum = Server_Lookup(packet.inum, packet.name);
			} else if (packet.op == 1) {
				responsePacket.inum = Server_Stat(packet.inum, &(responsePacket.stat));
			} else if (packet.op == 2) {
				responsePacket.inum = Server_Write(packet.inum, packet.buffer, packet.block);
			} else if (packet.op == 3) {		    	
				responsePacket.inum = Server_Read(packet.inum, responsePacket.buffer, packet.block);
			} else if (packet.op == 4) {
				responsePacket.inum = Server_Creat(packet.inum, packet.type, packet.name);
			} else if (packet.op == 5) {
				responsePacket.inum = Server_Unlink(packet.inum, packet.name);
			}

		    responsePacket.op = 6;
		    rc = UDP_Write(sd, &s, (char*)&responsePacket, sizeof(Payload));
		    if(packet.op == 7)
		    	Server_Shutdown();
		}
	}
}

int main(int argc, char *argv[]) {
	int portNumber = atoi(argv[1]);
	char *fileSysPath = argv[2];

	Server_Startup(portNumber, fileSysPath);

	return 0;
}