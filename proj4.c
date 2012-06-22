#include "proj4.h"

static char client_name[MAX_GROUP_NAME];
static mailbox Mbox;

char send_message[MAX_MESSLEN];
char receive_message[MAX_MESSLEN];
char receive_length;
char sender[MAX_GROUP_NAME];

char * get_sender() {
	return sender;
}

void spread_connect() {
	int ret;

	sp_time timeout;
	timeout.sec = 5;
	timeout.usec = 0;

	ret = SP_connect_timeout(SPREAD_NAME, CLIENT_NAME, 0, 1, &Mbox, client_name, timeout);
	if (ret != ACCEPT_SESSION) {
		printf("SP_connect failed\n");
		SP_error(ret);
		exit(1);
	}

	ret = SP_join(Mbox, GROUP_NAME);
	if (ret != 0) {
		printf("SP_join failed\n");
		SP_error(ret);
		exit(1);
	}
}

void spread_send(char * target, char * method, char * token) {
	spread_send_list(target, method, token, NULL);
}

void spread_send_list(char * target, char * method, char * token, char * list) {
	int ret;
	int pos = 0;

	bzero(send_message, MAX_MESSLEN);
	memcpy(send_message, target, strlen(target));
	pos += strlen(target);
	send_message[pos] = ':';
	pos++;
	memcpy(send_message + pos, method, strlen(method));
	pos += strlen(method);
	send_message[pos] = '(';
	pos++;
	memcpy(send_message + pos, token, strlen(token));
	pos += strlen(token);
	send_message[pos] = ')';
	pos++;
	if (list) {
		memcpy(send_message + pos, list, strlen(list));
	}

	ret = SP_multicast(Mbox, RELIABLE_MESS, GROUP_NAME, 0, strnlen(send_message, MAX_MESSLEN),
		send_message);
	if (ret != strnlen(send_message, MAX_MESSLEN)) {
		printf("message sending failed!\n");
		SP_error(ret);
		exit(1);
	}
	bzero(send_message, MAX_MESSLEN);
}

int message_for_me(char * message, int length) {
	int cmplen = length;
	if (strlen(client_name) > length) {
		return 0;
	} else {
		cmplen = strlen(client_name);
	}
	//printf("comparing (%s) to (%s) for %d bytes\n", message, client_name, cmplen);
	return ((0 == strncmp(message, client_name, cmplen)) &&
	    (message[cmplen] == ':'));
}

void spread_receive() {
	int service_type;
	int num_groups;
	char target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
	int16 mess_type;
	int endian_mismatch;

	bzero(receive_message, MAX_MESSLEN);
	do {
		receive_length = SP_receive( Mbox, &service_type, sender, 100, &num_groups,
				target_groups, &mess_type, &endian_mismatch, 
				sizeof(receive_message), receive_message );
		if (receive_length < 0) {
			printf("SP_receive failed\n");
			SP_error(receive_length);
		}
		/*else if (Is_regular_mess(service_type) && message_for_me(receive_message, receive_length)) {
			printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
					sender, mess_type, endian_mismatch, 
					num_groups, receive_length, receive_message );
		} else if (Is_regular_mess(service_type)) {
			printf("regular message\n");
		} else if (message_for_me(receive_message, receive_length)) {
			printf("message for me\n");
		} else {
			printf("wtf!\n");
		}*/
	} while (!(Is_regular_mess(service_type) && message_for_me(receive_message, receive_length)));
}

char * get_token() {
	int i;
	int startpos = 0;
	int length = 0;
	static char *token[MAX_TOKENLEN];

	bzero(token, MAX_TOKENLEN);

	for (i = 0; i <= strnlen(receive_message, MAX_MESSLEN); i++) {
		if (receive_message[i] == '(') {
			startpos = i+1;
		} else if ((startpos != 0) && (receive_message[i] == ')')) {
			length = i - startpos;
			memcpy(token, receive_message + startpos, length);
			token[length] = '\0';
			break;
		}
	}
	return token;
}

char * get_list() {
	int i;
	static char *list[MAX_LOCKLIST];

	bzero(list, MAX_LOCKLIST);
	
	for (i = 0; i <= strnlen(receive_message, MAX_MESSLEN); i++) {
		if (receive_message[i] == ')') {
			memcpy(list, receive_message + i + 1,
				strnlen(receive_message + i + 1, MAX_LOCKLIST));
		}
	}

	return list;
}

int get_message_type() {
	int cmplen = 0;
	char * realmessage = receive_message + strlen(client_name) + 1;

	if (strlen("request_write") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("request_write");
	}
	if (0 == strncmp(realmessage, "request_write", cmplen)) {
		return MSG_REQUEST_WRITE;
	}
	
	if (strlen("request_read") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("request_read");
	}
	if (0 == strncmp(realmessage, "request_read", cmplen)) {
		return MSG_REQUEST_READ;
	}

	if (strlen("revoke_done") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("revoke_done");
	}
	if (0 == strncmp(realmessage, "revoke_done", cmplen)) {
		return MSG_REVOKE_DONE;
	}

	if (strlen("downgrade_done") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("downgrade_done");
	}
	if (0 == strncmp(realmessage, "downgrade_done", cmplen)) {
		return MSG_DOWNGRADE_DONE;
	}
	
	if (strlen("close") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("close");
	}
	if (0 == strncmp(realmessage, "close", cmplen)) {
		return MSG_CLOSE;
	}

	if (strlen("ack") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("ack");
	}
	if (0 == strncmp(realmessage, "ack", cmplen)) {
		return MSG_ACK;
	}

	if (strlen("client_list") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("client_list");
	} 
	if (0 == strncmp(realmessage, "client_list", cmplen)) {
		return MSG_CLIENT_LIST;
	}

	if (strlen("revoke") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("revoke");
	} 
	if (0 == strncmp(realmessage, "revoke", cmplen)) {
		return MSG_REVOKE;
	}

	if (strlen("downgrade") > receive_length) {
		cmplen = receive_length;
	} else {
		cmplen = strlen("downgrade");
	} 
	if (0 == strncmp(realmessage, "downgrade", cmplen)) {
		return MSG_DOWNGRADE;
	}

	return MSG_UNKNOWN;
}

void free_locklist(lockitem * locklist) {
	lockitem * curlock = locklist;
	lockitem * tofree;
	while (curlock) {
		tofree = curlock;
		curlock = (lockitem *)curlock->next;
		free(tofree);
	}
}

void addlock(lockitem ** locklist, char * client) {
	addnlock(locklist, client, strnlen(client, MAX_GROUP_NAME));
}

void addnlock(lockitem ** locklist, char * client, int maxlen) {
	lockitem * new;
	lockitem * cur = *locklist;
	int length = strlen(client);
	
	if (length > maxlen) {
		length = maxlen;
	}

	new = (lockitem *)malloc(sizeof(lockitem));
	memcpy(new->client, client, length);
	new->next = NULL;
	
	if (!*locklist) {
		*locklist = new;
	} else {
		while (cur->next) {
			if (0 == strncmp(cur->client, client, maxlen)) {
				return;
			}
			cur = (lockitem *)cur->next;
		}
		cur->next = new;
	}
}

void removelock(lockitem ** locklist, char * client) {
	if (*locklist) {
		if (!((lockitem *)*locklist)->next) {
			if (0 == strncmp((*locklist)->client, client, strnlen(client, MAX_GROUP_NAME))) {
				free(*locklist);
				*locklist = NULL;
				return;
			}
		}
		
		lockitem * cur = *locklist;
		lockitem * prev = *locklist;
		while (cur) {
			if (0 == strncmp(cur->client, client, strnlen(client, MAX_GROUP_NAME))) {
				if (prev == cur) {
					*locklist = cur->next;
				} else {
					prev->next = cur->next;
				}
				free(cur);
				cur = NULL;
				return;
			}
			cur = cur->next;
		}
	}
}

void movelocks(lockitem ** fromlist, lockitem ** tolist) {
	while (*fromlist) {
		addlock(tolist, (*fromlist)->client);
		removelock(fromlist, (*fromlist)->client);
	}
}
