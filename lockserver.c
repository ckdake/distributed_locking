#include "proj4.h"

struct tokenitem_t {
	char token[MAX_TOKENLEN];
	struct lockitem * readlocks;
	struct lockitem * writelocks;
	struct tokenitem * next;
};
typedef struct tokenitem_t tokenitem;

void print_locklist(tokenitem * locklist){
	tokenitem * curr;
	curr = locklist;
	printf("-------------------------------------------------\n- current locks:\n");
	while (curr) {
		lockitem * curlock;
		printf("- %s\n", curr->token);
		curlock = (lockitem *)curr->readlocks;
		while (curlock) {
			printf("- \tread: %s\n", curlock->client);
			curlock = (lockitem *)curlock->next;
		}
		curlock = (lockitem *)curr->writelocks;
		while (curlock) {
			printf("- \twrite: %s\n", curlock->client);
			curlock = (lockitem *)curlock->next;
		}
		curr = (tokenitem *)curr->next;
	}
	printf("-------------------------------------------------\n");
}

void free_tokenlist(tokenitem * locklist) {
	tokenitem * curr;
	void * tofree = NULL;
	curr = locklist;
	while (curr) {
		free_locklist((lockitem *)curr->readlocks);
		free_locklist((lockitem *)curr->writelocks);
		tofree = curr;
		curr = (tokenitem *)curr->next;
		free(curr);
	}
}

tokenitem * token_find(tokenitem ** locklist, char * token) {
	tokenitem * cur = *locklist;
	while(cur) {
		if (0 == strncmp(cur->token, token, strnlen(token, MAX_TOKENLEN))) {
			return cur;
		}
		cur = (tokenitem *)cur->next;
	}
	return NULL;
}

tokenitem * token_find_add(tokenitem ** locklist, char * token) {
	tokenitem * cur = *locklist;
	tokenitem * prev = NULL;
	while(cur) {
		if (0 == strncmp(cur->token, token, strnlen(token, MAX_TOKENLEN))) {
			return cur;
		}
		prev = cur;
		cur = (tokenitem *)cur->next;
	}

	cur = (tokenitem *)malloc(sizeof(tokenitem));
	memcpy(cur->token, token, strlen(token));
	cur->writelocks = NULL;
	cur->readlocks = NULL;
	cur->next = NULL;
	if (!*locklist) {
		*locklist = cur;
	} else {
		prev->next = cur;
	}
	return cur;
}

void removetoken(tokenitem ** locklist, char * token) {
	if (*locklist) {
		if (!((tokenitem *)*locklist)->next) {
			if (0 == strncmp((*locklist)->token, token, strnlen(token, MAX_TOKENLEN))) {
				free(*locklist);
				*locklist = NULL;
				return;
			}
		}

		tokenitem * cur = *locklist;
		tokenitem * prev = *locklist;
		while (cur) {
			if (0 == strncmp(cur->token, token, strnlen(token, MAX_GROUP_NAME))) {
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


char * format_locklist(tokenitem * token, int includereads) {
	static char locklist[MAX_LOCKLIST];
	bzero(locklist, MAX_LOCKLIST);
	char * p = locklist;
	lockitem * cur = (lockitem *)token->writelocks;
	while (cur) {
		memcpy(p, cur->client, strnlen(cur->client, MAX_GROUP_NAME));
		p += strnlen(cur->client, MAX_GROUP_NAME);
		*p = ';';
		p++;
		cur = (lockitem *)cur->next;
	}
	if (includereads) {
		cur = (lockitem *)token->readlocks;
		while (cur) {
			memcpy(p, cur->client, strnlen(cur->client, MAX_GROUP_NAME));
			p += strnlen(cur->client, MAX_GROUP_NAME);
			*p = ';';
			p++;
			cur = (lockitem *)cur->next;
		}
	}
	//printf("made list: %s\n", locklist);
	return locklist;
			
}

int main(int argc, char * argv[]) {

	tokenitem * locklist = NULL;

	spread_connect();

	while (1) {
		tokenitem * t;
		spread_receive();

		char * token = get_token();
		char * sender = get_sender();

		switch (get_message_type()) {
		case MSG_REQUEST_WRITE:
			// send lock or list of open read and write locks
			printf("%s requests write to %s\n", sender, token);
			
			t = token_find_add(&locklist, token);
			if (!t->readlocks && !t->writelocks) {
				addlock(&(t->writelocks), sender);
				spread_send(sender, "ack", token);
			} else {
				spread_send_list(sender, "client_list", token, format_locklist(t, 1));
			}
			break;
		case MSG_REQUEST_READ:
			// send lock or list of open write locks
			printf("%s requests read to %s\n", sender, token);

			t = token_find_add(&locklist, token);
			if (!t->writelocks) {
				addlock(&(t->readlocks), sender);
				spread_send(sender, "ack", token);
			} else {
				spread_send_list(sender, "client_list", token, format_locklist(t, 0));
			}
			break;
		case MSG_REVOKE_DONE:
			//update tables(this client has write, reads are gone) -> send ack
			printf("%s says revoke is done for %s\n", sender, token);
			t = token_find_add(&locklist, token);
			if (t->writelocks) {
				free_locklist(t->writelocks);
				t->writelocks = NULL;
			}
			if (t->readlocks) {
				free_locklist(t->readlocks);
				t->readlocks = NULL;
			}
			addlock(&(t->writelocks), sender);
			spread_send(sender, "ack", token);
			break;
		case MSG_DOWNGRADE_DONE:
			//updates tables(this client has read, writes are now reads) -> send ack
			printf("%s says downgrade is done for %s\n", sender, token);
			t = token_find_add(&locklist, token);
			if (t->writelocks) {
				movelocks(&(t->writelocks), &(t->readlocks));
			}
			if (t->readlocks) {
				removelock(&(t->readlocks), sender);
			}
			addlock(&(t->readlocks), sender);
			spread_send(sender, "ack", token);
			break;
		case MSG_CLOSE:
			//remove any locks for this token for this client
			printf("%s says it is closing %s\n", sender, token);

			t = token_find(&locklist, token);
			if (t) {
				if (t->readlocks) {
					removelock(&(t->readlocks), sender);
				}
				if (t->writelocks) {
					removelock(&(t->writelocks), sender);
				}
				if (!t->readlocks && !t->writelocks) {
					removetoken(&locklist, token);
				}
			}
			spread_send(sender, "ack", token);
			break;
		case MSG_UNKNOWN:
			printf("unknown message! (%s, %s)\n", sender, token);
		}
		print_locklist(locklist);
	}

	return 0;
}
