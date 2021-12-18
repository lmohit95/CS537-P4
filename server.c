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

int inodemap[NUM_INODES/16][16];
int log_end;
int fd;

typedef struct __buf {
	char string [BLOCK_SIZE/sizeof(char)];
} buf;

void update_CPR_inode_num(int INODE_num, int block){
	inodemap[INODE_num/16][INODE_num%16]=block;
}
int get_CPR_inode_num(int INODE_num){
	return inodemap[INODE_num/16][INODE_num%16];
}

int get_INODE(int inum, INODE* n) {
	if(inum < 0 || inum >= NUM_INODES) {
		printf("get_INODE: invalid inum\n");
		return -1;
	}
	int iblock = get_CPR_inode_num(inum);
	lseek(fd, iblock*BLOCK_SIZE, SEEK_SET);
	read(fd, n, sizeof(INODE));
	return 0;
}

int getBlock(int firstBlock, int inum, int pinum) {
	dir_entries db;
	for(int i = 0; i < NUM_ENTRIES; i++) {
		db.inums[i] = -1;
		strcpy(db.names[i], "\0");
	}

	if(firstBlock) {
		db.inums[0] = inum;
		strcpy(db.names[0], ".\0");
		db.inums[1] = pinum;
		strcpy(db.names[1], "..\0");
	}

	lseek(fd, log_end*BLOCK_SIZE, SEEK_SET);
	write(fd, &db, BLOCK_SIZE);
	log_end++;
	return log_end-1;
}

void update_CR(int inode_number) {
	if(inode_number != -1) {
		lseek(fd, inode_number * sizeof(int), SEEK_SET);
		write(fd, &inodemap[inode_number/16][inode_number%16], sizeof(int));
	}

	lseek(fd, NUM_INODES * sizeof(int), SEEK_SET);
	write(fd, &log_end, sizeof(int));
}

void init_node(INODE* node) {
	for(int i = 0; i < NUM_BLOCKS; i++) {
		node->filled[i] = 0;
		node->data[i] = -1;
	}
}

void init_dir(dir_entries *block) {
	for(int i = 0; i < NUM_ENTRIES; i++) {
		block->inums[i] = -1;
		strcpy(block->names[i], "\0");
	}
}

int Server_Startup(int port, char* path) {
	
	if((fd = open(path, O_RDWR)) == -1) {
		fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
		if(fd == -1)
			return -1;
		log_end = CHECK_POINT_SIZE;

		int i;
		for(i = 0; i < NUM_INODES; i++) {
			update_CPR_inode_num(i,-1);
		}

		lseek(fd, 0, SEEK_SET);
		write(fd, inodemap, sizeof(int)*NUM_INODES);
		write(fd, &log_end, sizeof(int));

		INODE node;
		node.inum = 0;
		node.type = MFS_DIRECTORY;
		node.size = BLOCK_SIZE;
		node.data[0] = log_end;
		node.filled[0] = 1;
		for(i = 1; i < NUM_BLOCKS; i++) {
			node.filled[i] = 0;
			node.data[i] = -1;
		}

		dir_entries baseBlock;
		baseBlock.inums[0] = 0;
		baseBlock.inums[1] = 0;
		strcpy(baseBlock.names[0], ".\0");
		strcpy(baseBlock.names[1], "..\0");

		for(i = 2; i < NUM_ENTRIES; i++) {
			baseBlock.inums[i] = -1;
			strcpy(baseBlock.names[i], "\0");
		}

		lseek(fd, log_end * BLOCK_SIZE, SEEK_SET);
		write(fd, &baseBlock, sizeof(dir_entries));
		log_end++;
		
		update_CPR_inode_num(0,log_end);

		lseek(fd, log_end*BLOCK_SIZE, SEEK_SET);
		write(fd, &node, sizeof(INODE));
		log_end++;

		update_CR(0);
	} else {
		printf("Exisiting file system\n");

		lseek(fd, 0, SEEK_SET);
		read(fd, inodemap, sizeof(int)*NUM_INODES);
		read(fd, &log_end, sizeof(int));
	}	
	return 0;
}

int Server_Lookup(int pinum, char *name) {
	INODE parent_INODE;
	int INODE_exist_status = get_INODE(pinum, &parent_INODE);
	if(INODE_exist_status == -1) {
		return -1;
	}

	printf("Parent Inode\n");
	printf("Parent Inde Type = %d , Size = %d\n", parent_INODE.type, parent_INODE.size);

	for (int i = 0; i < NUM_BLOCKS; i++) {
		if (parent_INODE.filled[i]) {

			dir_entries foundblock;
			lseek(fd, parent_INODE.data[i] * BLOCK_SIZE, SEEK_SET);
			read(fd, &foundblock, BLOCK_SIZE);

			for(int j = 0; j < NUM_ENTRIES; j++) {
				if(foundblock.inums[j] != -1) {					
					if(strcmp(foundblock.names[j], name) == 0) {
						return foundblock.inums[j];
					}
				}
			}
		}
	}

	return -1;
}

int Server_Stat(int INODE_num, MFS_Stat_t *mfs_stat) {
	INODE node;
	int INODE_exist_status = get_INODE(INODE_num, &node);
	if(INODE_exist_status == -1) {
		return -1;
	}
	mfs_stat->type = node.type;
	mfs_stat->size = node.size;
	return 0;
}

int Server_Write(int INODE_num, char *buffer, int block) {
	INODE node; 
	int INODE_exist_status = get_INODE(INODE_num, &node);
	if(INODE_exist_status == -1) {
		return -1;
	}
	
	if(node.type != MFS_REGULAR_FILE) {
		return -1;
	}

	if(block < 0 || block >= 14) {	
		return -1;
	}
	
	int new_node_size = 0;
	if ((block + 1) * BLOCK_SIZE > node.size) {
		new_node_size = (block + 1) * BLOCK_SIZE;
	} else {
		new_node_size = node.size;
	}
	node.size = new_node_size;
	node.filled[block] = 1;

	node.data[block] = log_end + 1;
	lseek(fd, log_end * BLOCK_SIZE, SEEK_SET);
	write(fd, &node, BLOCK_SIZE);
	update_CPR_inode_num(INODE_num, log_end);
	log_end++;
	lseek(fd, log_end * BLOCK_SIZE, SEEK_SET);
	write(fd, buffer, BLOCK_SIZE);
	log_end++;

	update_CR(INODE_num);
	return 0;
}

// int server_read(int fd, struct checkpoint* CPR, struct payload* data, struct payload * reply) {
// 	struct INODE inode;
// 	int status = get_inode(fd, CPR, data->inum, &inode);
// 		// 	   	lseek(fd, 15692, SEEK_SET);
// 		// struct INODE temp;
// 		// read(fd, &temp, sizeof(INODE_size));
// 		// printf("Test:: inodemap offset = %d\n", temp.data[0]);
// 	if (status == -1) {
// 		printf("server_read: invalid block");
// 		reply->status = -1;
// 		return -1;
// 	} else {
// 		int block_offset = inode.data[data->block];
// 		printf("server_read:: inode type = %d, inode size = %d\n", inode.type, inode.size);
// 		printf("block offset=%d\n",block_offset);
// 		if (block_offset == -1) {
// 			printf("server_read: invalid block");
// 			reply->status = -1;
// 			return -1;
// 		} else {
// 			lseek(fd, block_offset, SEEK_SET);
// 			read(fd, &reply->buffer, 4096);
// 			printf("server_read: Reading Buffer value =%s\n",reply->buffer);
// 			reply->status = 0;
// 			return 0;
// 		}
// 	}
// }

int Server_Read(int INODE_num, char *buffer, int block){
	INODE node;
	if(get_INODE(INODE_num, &node) == -1)
	{
		printf("get_INODE failed for inum %d.\n", INODE_num);
		return -1;
	}

	if(block < 0 || block >= NUM_BLOCKS || !node.filled[block]) {
		printf("invalid block.\n");
		return -1;
	}

	if(node.type == MFS_REGULAR_FILE) {
		if(lseek(fd, node.data[block]*BLOCK_SIZE, SEEK_SET) == -1) {
			perror("Server_Read: lseek:");
			printf("Server_Read: lseek failed\n");
		}
		
		if(read(fd, buffer, BLOCK_SIZE) == -1) {
			perror("Server_Read: read:");
			printf("Server_Read: read failed\n");
		}
	} else {
		dir_entries db;
		lseek(fd, node.data[block], SEEK_SET);
		read(fd, &db, BLOCK_SIZE);

		MFS_DirEnt_t entries[NUM_ENTRIES];
		int i;
		for(i = 0; i < NUM_ENTRIES; i++) {
			MFS_DirEnt_t entry ;
			strcpy(entry.name, db.names[i]);
			entry.inum = db.inums[i];
			entries[i] = entry;
		}

		memcpy(buffer, entries, sizeof(MFS_DirEnt_t)*NUM_ENTRIES);
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
	for(int i = 0; i < NUM_INODES; i++) {
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
	for(block_index = 0; block_index < NUM_BLOCKS; block_index++) {
		if(parent_node.filled[block_index]) {
			lseek(fd, parent_node.data[block_index]*BLOCK_SIZE, SEEK_SET);
			read(fd, &block, BLOCK_SIZE);

			for(entry = 0; entry < NUM_ENTRIES; entry++) {
				if(block.inums[entry] == -1) {						
						lseek(fd, inodemap[pinum/16][pinum%16]*BLOCK_SIZE, SEEK_SET);
						write(fd, &parent_node, BLOCK_SIZE);

						block.inums[entry] = INODE_num;
						strcpy(block.names[entry], name);
						lseek(fd, parent_node.data[block_index]*BLOCK_SIZE, SEEK_SET);
						write(fd, &block, BLOCK_SIZE);

						INODE node;
						node.inum = INODE_num;
						node.size = 0;
						for(int i = 0; i < NUM_BLOCKS; i++) {
							node.filled[i] = 0;
							node.data[i] = -1;
						}
						node.type = type;	
						if(type == MFS_DIRECTORY) {
							node.filled[0] = 1;
							node.data[0] = log_end;
							
							getBlock(1, INODE_num, pinum);
							node.size += BLOCK_SIZE;
						} else if (type != MFS_DIRECTORY && type != MFS_REGULAR_FILE) {
							return -1;
						}
						update_CPR_inode_num(INODE_num, log_end);
						lseek(fd, log_end*BLOCK_SIZE, SEEK_SET);
						write(fd, &node, sizeof(INODE));
						log_end++;
						update_CR(INODE_num);
						return 0;
				}
			}
		} else {
			int dir_block = getBlock(0, INODE_num, -1);
			parent_node.size += BLOCK_SIZE;
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
		for(block_index = 0; block_index < NUM_BLOCKS; block_index++) {
			if(toRemove.filled[block_index]) {
				dir_entries block;
				lseek(fd, toRemove.data[block_index] * BLOCK_SIZE, SEEK_SET);
				read(fd, &block, BLOCK_SIZE);

				int entry;
				for(entry = 0; entry < NUM_ENTRIES; entry++) {
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
	for(; blockindex < NUM_BLOCKS && !unlink_done; blockindex++) {
		if(parent_INODE.filled[blockindex]) {
			dir_entries block;
			lseek(fd, parent_INODE.data[blockindex]*BLOCK_SIZE, SEEK_SET);
			read(fd, &block, BLOCK_SIZE);

			int entry_index;
			for(entry_index = 0; entry_index < NUM_ENTRIES && !unlink_done; entry_index++) {
				if(block.inums[entry_index] != -1) {
					if(strcmp(block.names[entry_index], name) == 0) {
						block.inums[entry_index] = -1;
						strcpy(block.names[entry_index], "\0");
						unlink_done = 1;
					}
				}
			}

			if(unlink_done) {
				lseek(fd, log_end * BLOCK_SIZE, SEEK_SET);
				write(fd, &block, BLOCK_SIZE);
				log_end++;

				parent_INODE.data[blockindex] = log_end-1;
				lseek(fd, log_end*BLOCK_SIZE, SEEK_SET);
				write(fd, &parent_INODE, BLOCK_SIZE);
				log_end++;
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

// int server_unlink(int fd,struct checkpoint* CPR_pointer, struct payload* data, struct payload * reply ){
// 	   reply->status=-1;
// 	   reply->inum=-1;
// 	   //Check CPR inodemap pointer array
// 	   struct checkpoint CPR=*CPR_pointer;
// 	   int inode_map_index=data->pinum/16;
// 	   //invalid pinum
// 	   if (inode_map_index>255 || inode_map_index<0){
// 		   reply->status=-1;
// 		   return -1;
// 	   }
// 	   int inode_map_offset = CPR.inodemap[inode_map_index];
// 	   lseek(fd,inode_map_offset,SEEK_SET);
// 	   struct inode_map inodemap;
// 	   read(fd,&inodemap,sizeof(inodemap));
//        //Get inodemap
// 	   int inode_index = (data->pinum)%16;
// 	   int inode_offset = inodemap.node[inode_index];
// 	   lseek(fd,inode_offset,SEEK_SET);
// 	   struct INODE parent_inode;
// 	   //Get Parent Directory Inode
// 	   read(fd,&parent_inode,sizeof(parent_inode));
// 	   //Search if 'name' already exists
// 	   MFS_DirEnt_t dir_block_array[128];
// 	   MFS_DirEnt_t* dir_block =dir_block_array;// (MFS_DirEnt_t*)malloc(4096);
// 	   MFS_DirEnt_t* dir_block_start=dir_block;
// 	   MFS_DirEnt_t* dir_block_end=&dir_block_array[128]+sizeof(MFS_DirEnt_t);
// 	   int found=0;
// 	   int parent_data_block_index=0;
// 	   for (;parent_data_block_index<14;parent_data_block_index++){
// 		   int dir_offset=parent_inode.data[parent_data_block_index];
// 		   lseek(fd,dir_offset,SEEK_SET);
// 		   read(fd,dir_block,4096);

// 		   //Search for file/directory in pinum 
// 		   while(dir_block!= dir_block_end && dir_block->inum!=-1){//strcmp(dir_block->name, "") != 0){
// 			   if(strcmp(dir_block->name,data->name)==0){
// 				   printf("File/Directory exists\n");
// 				   found=1;
// 				   reply->inum= dir_block->inum; 
// 				   //We need to remove this entry
// 				   //if dir first check if dir is empty
// 				   if(data->type==1){
// 					   //check if dir empty
// 					   struct INODE inode;
// 					   int status = get_inode(fd,&CPR,dir_block->inum,&inode);
// 					   //assert(status==0);
// 					   if (status==-1){
// 						   printf("Illegal inode!!!\n");
// 						   return -1;
// 					   }
// 					   if (inode.size>3*sizeof(MFS_DirEnt_t)){
// 						   reply->status=-1;
// 						   printf("Directory not empty!\n");
// 						   return -1;
// 					   }
// 					   else{
// 						strcpy(dir_block->name,"");
// 						dir_block->inum=-1;						   
// 					   }
// 				   }
// 				   else{
// 					   strcpy(dir_block->name,"");
// 					   dir_block->inum=-1;
// 				   }
// 				   break;
// 			   }
// 			   dir_block++;
// 		   }
// 		   if(found){
// 			   //if the file was found, we have updated the data block
// 			   //write the data block to EoLog
// 			   int unlink_block_offset=lseek(fd,0,CPR_pointer->log_end);
// 			   write(fd,dir_block_start,4096);
// 			   //update block offset in inode
// 			   parent_inode.data[parent_data_block_index] = unlink_block_offset;
// 			   int status = write_inode_etc( fd, CPR_pointer, &parent_inode,&inodemap, data->pinum);
// 			   if(status==-1){
// 				   printf("unlink: couldnt update data block with removed file/dir\n");
// 				   return -1;
// 			   }
// 			   break;}
// 	   }
// 	   reply->status=0;
// 	   return reply->inum;
// }

int Server_Shutdown() {
	fsync(fd);
	exit(0);
}

int main(int argc, char *argv[]) {
	int portNumber = atoi(argv[1]);
	char *fileSysPath = argv[2];

	Server_Startup(portNumber, fileSysPath);
	
	int sd = UDP_Open(portNumber);
	if(sd < 0) {
		printf("Error opening socket on port %d\n", portNumber);
		exit(1);
	}

    printf("Starting server...\n");
    while (1) {
		struct sockaddr_in s;
		Payload payload;
		int rc = UDP_Read(sd, &s, (char *)&payload, sizeof(Payload));
		if (rc > 0) {
		    Payload responsePayload;

		    if (payload.op == 0) {
				responsePayload.inum = Server_Lookup(payload.inum, payload.name);
			} else if (payload.op == 1) {
				responsePayload.inum = Server_Stat(payload.inum, &(responsePayload.stat));
			} else if (payload.op == 2) {
				responsePayload.inum = Server_Write(payload.inum, payload.buffer, payload.block);
			} else if (payload.op == 3) {		    	
				responsePayload.inum = Server_Read(payload.inum, responsePayload.buffer, payload.block);
			} else if (payload.op == 4) {
				responsePayload.inum = Server_Creat(payload.inum, payload.type, payload.name);
			} else if (payload.op == 5) {
				responsePayload.inum = Server_Unlink(payload.inum, payload.name);
			}

		    responsePayload.op = 6;
		    rc = UDP_Write(sd, &s, (char*)&responsePayload, sizeof(Payload));
		    if(payload.op == 7)
		    	Server_Shutdown();
		}
	}
	return 0;
}