#define BUFFER_SIZE (4096)
#define MAX_NAME_SIZE (28)


typedef struct __Payload {
	int op;
	int inum;
	int block;
	int type;

	char name[MAX_NAME_SIZE];
	char buffer[BUFFER_SIZE];
	MFS_Stat_t stat;
} Payload;
