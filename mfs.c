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
int illegalname(char* name);
int sendPayload(Payload *sentPayload, Payload *responsePayload, int maxTries) {
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
        UDP_Write(sd, &addr, (char*)sentPayload, sizeof(Payload));
        if(select(sd+1, &rfds, NULL, NULL, &tv))
        {
            rc = UDP_Read(sd, &addr2, (char*)responsePayload, sizeof(Payload));
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
	if(illegalname(name) < 0)
		return -1;

	Payload sentPayload;
	Payload responsePayload;

	sentPayload.inum = pinum;
	sentPayload.op = 0;
	strcpy((char*)&(sentPayload.name), name);
	int rc = sendPayload(&sentPayload, &responsePayload, 3);
	if(rc < 0)
		return -1;
	
	rc = responsePayload.inum;
	return rc;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	Payload sentPayload;
	Payload responsePayload;

	sentPayload.inum = inum;
	sentPayload.op = 1;

	if(sendPayload(&sentPayload, &responsePayload, 3) < 0)
		return -1;

	memcpy(m, &(responsePayload.stat), sizeof(MFS_Stat_t));
	return 0;
}

int MFS_Write(int inum, char *buffer, int block){
	Payload sentPayload;
	Payload responsePayload;

	sentPayload.inum = inum;
	memcpy(sentPayload.buffer, buffer, BUFFER_SIZE);
	sentPayload.block = block;
	sentPayload.op = 2;
	
	if(sendPayload(&sentPayload, &responsePayload, 3) < 0)
		return -1;
	
	return responsePayload.inum;
}

int MFS_Read(int inum, char *buffer, int block){

	Payload sentPayload;
	Payload responsePayload;

	sentPayload.inum = inum;
	sentPayload.block = block;
	sentPayload.op = 3;
	
	if(sendPayload(&sentPayload, &responsePayload, 3) < 0)
		return -1;

	if(responsePayload.inum > -1)
		memcpy(buffer, responsePayload.buffer, BUFFER_SIZE);
	
	return responsePayload.inum;
}

int MFS_Creat(int pinum, int type, char *name){

	if(illegalname(name) < 0)
		return -1;

	Payload sentPayload;
	Payload responsePayload;

	sentPayload.inum = pinum;
	sentPayload.type = type;
	sentPayload.op = 4;

	strcpy(sentPayload.name, name);
	
	if(sendPayload(&sentPayload, &responsePayload, 3) < 0)
		return -1;

	return responsePayload.inum;
}

int MFS_Unlink(int pinum, char *name){
	if(illegalname(name) < 0)
		return -1;
	
	Payload sentPayload;
	Payload responsePayload;

	sentPayload.inum = pinum;
	sentPayload.op = 5;
	strcpy(sentPayload.name, name);
	
	if(sendPayload(&sentPayload, &responsePayload, 3) < 0)
		return -1;

	return responsePayload.inum;
}

int MFS_Shutdown(){
	Payload sentPayload, responsePayload;
	sentPayload.op = 7;
	if(sendPayload(&sentPayload, &responsePayload, 3) < 0)
		return -1;
	
	return 0;
}

int illegalname(char* name) {
	if(strlen(name) > 27)
		return -1;
	return 0;
}