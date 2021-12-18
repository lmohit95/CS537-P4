#include "mfs.h"
#include "packets.h"
#include "stdlib.h"
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "cNetworkLib.h"
#include "udp.h"

char* serverHostname;
int serverPort;
int initialized = 0;

int MFS_Init(char *hostname, int port) {
	if(port < 0 || strlen(hostname) < 1)
		return -1;
	serverHostname = malloc(strlen(hostname) + 1);
	strcpy(serverHostname, hostname);
	serverPort = port;
	initialized = 1;
	return 0;
}

int MFS_Lookup(int pinum, char *name){
	if(!initialized)
		return -1;
	
	if(checkName(name) < 0)
		return -1;

	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = pinum;
	sentPacket.op = 0;
	strcpy((char*)&(sentPacket.name), name);
	int rc = sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3);
	if(rc < 0)
		return -1;
	
	rc = responsePacket.inum;
	return rc;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	if(!initialized)
		return -1;

	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = inum;
	sentPacket.op = 1;

	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	memcpy(m, &(responsePacket.stat), sizeof(MFS_Stat_t));
	return 0;
}

int MFS_Write(int inum, char *buffer, int block){
	if(!initialized)
		return -1;
	
	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = inum;
	//strncpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	memcpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	sentPacket.block = block;
	sentPacket.op = 2;
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;
	
	return responsePacket.inum;
}

int MFS_Read(int inum, char *buffer, int block){
	if(!initialized)
		return -1;
	
	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = inum;
	sentPacket.block = block;
	sentPacket.op = 3;
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	if(responsePacket.inum > -1)
		memcpy(buffer, responsePacket.buffer, BUFFER_SIZE);
	
	return responsePacket.inum;
}

int MFS_Creat(int pinum, int type, char *name){
	if(!initialized)
		return -1;
	
	if(checkName(name) < 0)
		return -1;

	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = pinum;
	sentPacket.type = type;
	sentPacket.op = 4;

	strcpy(sentPacket.name, name);
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inum;
}

int MFS_Unlink(int pinum, char *name){
	if(!initialized)
		return -1;
	
	if(checkName(name) < 0)
		return -1;
	
	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = pinum;
	sentPacket.op = 5;
	strcpy(sentPacket.name, name);
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inum;
}

int MFS_Shutdown(){
	Net_Packet sentPacket, responsePacket;
	sentPacket.op = 7;


	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;
	
	return 0;
}

int checkName(char* name) {
	if(strlen(name) > 27)
		return -1;
	return 0;
}