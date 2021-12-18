#include "mfs.h"
#include "packets.h"
#include "stdlib.h"
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "udp.h"
#include <sys/time.h>
#include <unistd.h>

char* serverHostname;
int serverPort;
int initialized = 0;

int sendPacket(char *hostname, int port, Net_Packet *sentPacket, Net_Packet *responsePacket, int maxTries)
{
    int sd = UDP_Open(0);
    if(sd < -1)
    {
        perror("Error opening connection.\n");
        return -1;
    }

    struct sockaddr_in addr, addr2;
    int rc = UDP_FillSockAddr(&addr, hostname, port);
    if(rc < 0)
    {
        perror("Error looking up host.\n");
        return -1;
    }

    fd_set rfds;
    struct timeval tv;
    tv.tv_sec=3;
    tv.tv_usec=0;


    do {
        FD_ZERO(&rfds);
        FD_SET(sd,&rfds);
        UDP_Write(sd, &addr, (char*)sentPacket, sizeof(Net_Packet));
        if(select(sd+1, &rfds, NULL, NULL, &tv))
        {
            rc = UDP_Read(sd, &addr2, (char*)responsePacket, sizeof(Net_Packet));
            if(rc > 0)
            {
                UDP_Close(sd);
                return 0;
            }
        }else {
            maxTries -= 1;
        }
    }while(1);
}

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