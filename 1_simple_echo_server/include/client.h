#ifndef _MY_TEST_CLIENT_H_
#define _MY_TEST_CLIENT_H_
#include <ev.h>

typedef struct client_s client_t;
#include "server.h"

typedef struct client_s {
	struct client_s * prev;
	struct client_s * next;
	struct sockaddr_in addr;
	char name[64];
	// ev_io * watcher; invisible field
} client_t;

#define GET_WATCHER(client) \
	((ev_io * ) ((char * )client + sizeof(client_t)))

#define GET_CLIENT(watcher) \
	((client_t * ) ((char * )watcher - sizeof(client_t)))

#define FOREACH_CLIENT(_server, _client) \
	for (client_t * _client = (_server)->clients.next; (_client) != NULL; (_client) = (_client)->next)

client_t * add_client(server_t * server);
void remove_client(client_t * client);

#endif // _MY_TEST_CLIENT_H_