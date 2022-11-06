#ifndef _MY_TEST_SERVER_H_
#define _MY_TEST_SERVER_H_
#include <stdbool.h>
#include <netinet/in.h>
#include <ev.h>

typedef struct server_s server_t;
#include "client.h"

typedef struct server_s {
	unsigned short port;
	struct sockaddr_in addr;
	int ln_sock;
	bool is_ready;
	client_t clients;
} server_t;

int init_server(server_t * server, unsigned short port);
int start_server(server_t * server);
int deinit_server(server_t * server);

#endif // _MY_TEST_SERVER_H_