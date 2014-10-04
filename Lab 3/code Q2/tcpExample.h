/* tcpExample.h- header used by both TCP server and client examples */

/* defines */
#define PLAYBACK_FILE "/tgtsvr/Audio.out"
#define SERVER_PORT_NUM		6097	/* server's port number for bind() */
#define CLIENT_PORT_NUM		6087	/* server's port number for bind() */
#define SERVER_WORK_PRIORITY 4   /* priority of server's work task */
#define CLIENT_WORK_PRIORITY 5  /* priority of server's work task */

#define SERVER_MAX_CONNECTIONS 4 /* max clients connected at a time */

#define MAX_REC_BLOCK 10  


void set_acgaa();
void get_acgaa();
