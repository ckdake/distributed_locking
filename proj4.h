#include <sp.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MESSLEN     102400
#define MAX_VSSETS      10
#define MAX_MEMBERS     100
// this is the max length of a file name. must be smaller than MAX_MESSLEN
#define MAX_TOKENLEN	256
// this must be smaller than MAX_MESSLEN
#define MAX_LOCKLIST	100000

#define GROUP_NAME "fileserver"
#define SPREAD_NAME "4803@localhost"
#define SERVER_NAME "#tokensrv#localhost"

#define MSG_REQUEST_WRITE 2
#define MSG_REQUEST_READ 3
#define MSG_REVOKE_DONE 4
#define MSG_DOWNGRADE_DONE 5
#define MSG_CLOSE 6
#define MSG_ACK 7
#define MSG_CLIENT_LIST 8
#define MSG_REVOKE 9
#define MSG_DOWNGRADE 10
#define MSG_UNKNOWN -1
size_t strnlen(const char *s, size_t maxlen);

struct lockitem_t {
	char client[MAX_GROUP_NAME];
	struct lockitem * next;
};

typedef struct lockitem_t lockitem;

void spread_connect();
void spread_send(char * target, char * method, char * token);
void spread_send_list(char * target, char * method, char * token, char * list);
void spread_receive();
char * get_token();
char * get_sender();
char * get_list();
int get_message_type();

void free_locklist(lockitem * locklist);
void addlock(lockitem ** locklist, char * client);
void addnlock(lockitem ** locklist, char * client, int maxlen);
void removelock(lockitem ** locklist, char * client);
void movelocks(lockitem ** fromlist, lockitem ** tolist);


