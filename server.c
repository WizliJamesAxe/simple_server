#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ev.h>

#include "server.h"
#include "log.h"

#define MAX_CLIENTS_COUNT 10

int init_server(server_t * server, unsigned short port)
{
	server->is_ready = false;
	server->port = port;

	server->ln_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server->ln_sock == -1)
	{
		log_ERROR("socket failed (%s)\n", strerror(errno));
		return -1;
	}

	int optval = 1, rc;
	rc = setsockopt(server->ln_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (rc == -1)
	{
		log_ERROR("setsockopt failed (%s)\n", strerror(errno));
		return -1;
	}

	rc = setsockopt(server->ln_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (rc == -1)
	{
		log_ERROR("setsockopt failed (%s)\n", strerror(errno));
		return -1;
	}

	memset(&server->addr, 0, sizeof(server->addr));

	server->addr.sin_family = AF_INET;
	server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server->addr.sin_port = htons(port);

	rc = bind(server->ln_sock, (struct sockaddr * ) &server->addr, sizeof(server->addr));
	if (rc == -1)
	{
		log_ERROR("bind failed (%s)\n", strerror(errno));
		return -1;
	}

	rc = listen(server->ln_sock, MAX_CLIENTS_COUNT);
	if (rc == -1)
	{
		log_ERROR("listen failed (%s)\n", strerror(errno));
		return -1;
	}

	server->is_ready = true;
	return 0;
}

int start_server(server_t * server)
{
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_size;

	peer_addr_size = sizeof(peer_addr);
	int client_sock = accept(server->ln_sock, (struct sockaddr * ) &peer_addr, &peer_addr_size);
	if (client_sock == -1)
	{
		printf("accept failed, errno = %d\n", errno);
		return errno;
	}

	printf("Log: 5\n");
	while (1)
	{
		char buffer[2048];
		buffer[0] = 0;

		printf("Log: 6\n");
		ssize_t len = recv(client_sock, buffer, sizeof(buffer), 0);
		if (len < 0)
		{
			printf("recv failed, errno = %d\n", errno);
			return errno;
		}

		printf("recieved: %s\n", buffer);

		if (0 == strcmp("exit", buffer))
			break;
	}

	printf("Log: 7\n");
	close(client_sock);
	// close(server_sock);

	return 0;
}