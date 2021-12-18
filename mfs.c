#include "mfs.h"
#include "stdlib.h"
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "udp.h"
#include <sys/time.h>
#include <unistd.h>
#include "lfs.h"

char* host_name;
int port_number;

int sendPacket(Payload *sentPacket, Payload *responsePacket, int maxTries) {
    int sd = UDP_Open(0);
    if(sd < -1) {
        return -1;
    }

    struct sockaddr_in addr, addr2;
    int rc = UDP_FillSockAddr(&addr, host_name, port_number);
    if(rc < 0) {
        return -1;
    }

    fd_set rfds;
    struct timeval tv;
    tv.tv_sec=3;
    tv.tv_usec=0;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd,&rfds);
        UDP_Write(sd, &addr, (char*)sentPacket, sizeof(Payload));
        if(select(sd+1, &rfds, NULL, NULL, &tv))
        {
            rc = UDP_Read(sd, &addr2, (char*)responsePacket, sizeof(Payload));
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
	host_name = malloc(strlen(hostname) + 1);
	strcpy(host_name, hostname);
	port_number = port;
	return 0;
}

int MFS_Lookup(int pinum, char *name){
	if(checkName(name) < 0)
		return -1;

	Payload sentPacket;
	Payload responsePacket;

	sentPacket.inum = pinum;
	sentPacket.op = 0;
	strcpy((char*)&(sentPacket.name), name);
	int rc = sendPacket(&sentPacket, &responsePacket, 3);
	if(rc < 0)
		return -1;
	
	rc = responsePacket.inum;
	return rc;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	Payload sentPacket;
	Payload responsePacket;

	sentPacket.inum = inum;
	sentPacket.op = 1;

	if(sendPacket(&sentPacket, &responsePacket, 3) < 0)
		return -1;

	memcpy(m, &(responsePacket.stat), sizeof(MFS_Stat_t));
	return 0;
}

int MFS_Write(int inum, char *buffer, int block){
	Payload sentPacket;
	Payload responsePacket;

	sentPacket.inum = inum;
	//strncpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	memcpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	sentPacket.block = block;
	sentPacket.op = 2;
	
	if(sendPacket(&sentPacket, &responsePacket, 3) < 0)
		return -1;
	
	return responsePacket.inum;
}

int MFS_Read(int inum, char *buffer, int block){

	Payload sentPacket;
	Payload responsePacket;

	sentPacket.inum = inum;
	sentPacket.block = block;
	sentPacket.op = 3;
	
	if(sendPacket(&sentPacket, &responsePacket, 3) < 0)
		return -1;

	if(responsePacket.inum > -1)
		memcpy(buffer, responsePacket.buffer, BUFFER_SIZE);
	
	return responsePacket.inum;
}

int MFS_Creat(int pinum, int type, char *name){

	if(checkName(name) < 0)
		return -1;

	Payload sentPacket;
	Payload responsePacket;

	sentPacket.inum = pinum;
	sentPacket.type = type;
	sentPacket.op = 4;

	strcpy(sentPacket.name, name);
	
	if(sendPacket(&sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inum;
}

int MFS_Unlink(int pinum, char *name){
	if(checkName(name) < 0)
		return -1;
	
	Payload sentPacket;
	Payload responsePacket;

	sentPacket.inum = pinum;
	sentPacket.op = 5;
	strcpy(sentPacket.name, name);
	
	if(sendPacket(&sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inum;
}

int MFS_Shutdown(){
	Payload sentPacket, responsePacket;
	sentPacket.op = 7;
	if(sendPacket(&sentPacket, &responsePacket, 3) < 0)
		return -1;
	
	return 0;
}

int checkName(char* name) {
	if(strlen(name) > 27)
		return -1;
	return 0;
}