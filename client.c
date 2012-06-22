#include "proj4.h"
#include <signal.h>

char * server;
int  last_message;

void alarm_handler(int signal) {
	printf("releasing lock\n");
	last_message = MSG_CLOSE;
	spread_send(SERVER_NAME, "close", server);
}

void set_alarm_handler() {
	struct sigaction act;
	act.sa_handler = alarm_handler;
	act.sa_flags = SA_NOMASK;
	sigaction(SIGINT, &act, NULL);
}

void parse_client_list(lockitem ** clients, char * list) {
	int length = 0;
	int pos = 0;
	char *client = list;
	while (pos < strnlen(list, MAX_LOCKLIST)) {
		if (client[length] == ';') {
			addnlock(clients, client, length);
			client += length + 1;
			length = 0;
		} else {
			length++;
		}
		pos++;
	}
}

void message_others(lockitem * clients, char * message, char * token) {
	lockitem * cur = clients;
	while (cur) {
		spread_send(cur->client, message, token);
		cur = (lockitem *) cur->next;
	}
}

int main(int argc, char *argv[]) {
	char * message;
	lockitem * pendingclients = NULL; 

	if (argc < 3) {
		printf("usage: %s filename.txt [R|W]\n", argv[0]);
		return 0;
	}
	if (argv[2][0] == 'R') {
		last_message = MSG_REQUEST_READ;
		message = "request_read";
	} else if (argv[2][0] == 'W') {
		last_message = MSG_REQUEST_WRITE;
		message = "request_write";
	} else {
		printf("lock type must be R or W\n");
		return 0;
	}

	spread_connect();
	server = argv[1];

	spread_send(SERVER_NAME, message, server);

	set_alarm_handler();
	
	printf("SIGINT to quit\n");

	while (1) {
		spread_receive();
		char * token = get_token();
		char * sender = get_sender();
		char * list = get_list();

		switch(get_message_type()) {
		case MSG_REVOKE:
			printf("got a revoke message. quitting.\n");
			spread_send(sender, "ack", token);	
			exit(0);
			break;
		case MSG_DOWNGRADE:
			//TODO: write blocks to server
			//TODO: downgrade to read (currently no internal state of this)
			printf("got a downgrade message. doin it.\n");
			spread_send(sender, "ack", token);
			break;
		case MSG_CLIENT_LIST:
			if (last_message == MSG_REQUEST_WRITE) {
				printf("got a client list in response to a write request\n");
				parse_client_list(&pendingclients, list);
				last_message = MSG_REVOKE;
				message_others(pendingclients, "revoke", token);
			} else if (last_message == MSG_REQUEST_READ) {
				printf("got a client list in response to a read request\n");
				parse_client_list(&pendingclients, list);
				last_message = MSG_DOWNGRADE;
				message_others(pendingclients, "downgrade", token);
			} else {
				printf("don't know why we got a client list now\n");
			}
			break;
		case MSG_ACK:
			if (last_message == MSG_REQUEST_READ || last_message == MSG_REQUEST_WRITE ||
				last_message == MSG_DOWNGRADE_DONE || last_message == MSG_REVOKE_DONE) {
				printf("got lock!\n");
			} else  if (last_message == MSG_CLOSE) {
				//TODO: write blocks to server
				printf("got ack and quitting\n");
				return 0;
			} else if (last_message == MSG_DOWNGRADE) {
				printf("got ack for downgrade, removing from list\n");
				removelock(&pendingclients, sender);
				if (!pendingclients) {
					printf("downgrade is done!\n");
					last_message = MSG_DOWNGRADE_DONE;
					spread_send(SERVER_NAME, "downgrade_done", token);
				}
			} else if (last_message == MSG_REVOKE) {
				printf("got ack for revoke, removing from list\n");
				removelock(&pendingclients, sender);
				if (!pendingclients) {
					printf("revoke is done!\n");
					last_message = MSG_REVOKE_DONE;
					spread_send(SERVER_NAME, "revoke_done", token);
				}
			} else {
				printf("got ack for something other than close\n");
			}
			break;
		default: 
			printf("got a message for me, but what does it mean?\n");
		}
	}

	return 0;
}
