#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "server.h"
#include "client.h"
#include "log.h"

#define MAX_CLIENTS_COUNT 10

static void sigint_handler(EV_P_ ev_signal *w, int revents)
{
	ev_break(loop, EVBREAK_ALL);
}

static int setnonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags);
}

static int send_answer(int fd, const char * answer, unsigned size)
{
	ssize_t rc = send(fd, answer, size, 0);
	if (rc == -1)
	{
		log_ERROR("send failed (%s)", strerror(errno));
		return -1;
	}

	return 0;
}

static void client_handler(EV_P_ ev_io * w, int revents)
{
	if(EV_ERROR & revents)
	{
		log_ERROR("invalid event (%s)", strerror(errno));
		goto free_watcher;
	}

	char buffer[2048];
	buffer[0] = '\0';

	ssize_t len = recv(w->fd, buffer, sizeof(buffer), 0);
	if (len < 0)
	{
		if (errno == EAGAIN)
			return;

		log_ERROR("recv failed (%s)", strerror(errno));
		goto free_watcher;
	}

	if (len == 0)
	{
		log_DEBUG("Client disconnected (fd=%d)", w->fd);
		goto free_watcher;
	}

	buffer[len] = '\0';

	log_DEBUG("Recieved from client(fd=%d): '%s' (len = %ld)", w->fd, buffer, len);

	int rc = send_answer(w->fd, buffer, (unsigned) len);
	if (rc == -1)
	{
		log_ERROR("send answer failed");
		goto free_watcher;
	}

	return;

free_watcher:
	close(w->fd);
	ev_io_stop (EV_A_ w);
	remove_client(GET_CLIENT(w));
}

static void ln_sock_handler(EV_P_ ev_io * w, int revents)
{
	if(EV_ERROR & revents)
	{
		log_ERROR("invalid event (%s)", strerror(errno));
		goto break_loop;
	}

	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(client_addr);
	int client_sock = accept(w->fd, (struct sockaddr * ) &client_addr, &client_addr_size);
	if (client_sock == -1)
	{
		log_ERROR("accept failed (%s)", strerror(errno));
		goto break_loop;
	}

	setnonblock(client_sock);

	char ip[INET_ADDRSTRLEN];
	uint16_t port;

	inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
	port = htons(client_addr.sin_port);

	log_DEBUG("Connected client (fd=%d): %s:%d", client_sock, ip, port);

	client_t * client = add_client(ev_userdata(EV_A));
	if (client == NULL)
	{
		log_ERROR("add_client failed");
		goto break_loop;
	}

	memcpy(&client->addr, &client_addr, sizeof(client_addr));

	if (snprintf(client->name, sizeof(client->name), "%s:%d", ip, port) < 0)
	{
		log_ERROR("snprintf failed");
		goto break_loop;
	}

	ev_io * watcher = GET_WATCHER(client);

	ev_io_init(watcher, client_handler, client_sock, EV_READ);
	ev_io_start(loop, watcher);

	return;

break_loop:
	ev_io_stop (EV_A_ w);
	ev_break (EV_A_ EVBREAK_ALL);
}

int init_server(server_t * server, unsigned short port)
{
	memset(server, 0, sizeof(*server));
	server->port = port;

	server->ln_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server->ln_sock == -1)
	{
		log_ERROR("socket failed (%s)", strerror(errno));
		return -1;
	}

	int optval = 1, rc;
	rc = setsockopt(server->ln_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (rc == -1)
	{
		log_ERROR("setsockopt failed (%s)", strerror(errno));
		return -1;
	}

	rc = setsockopt(server->ln_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (rc == -1)
	{
		log_ERROR("setsockopt failed (%s)", strerror(errno));
		return -1;
	}

	server->addr.sin_family = AF_INET;
	server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server->addr.sin_port = htons(port);

	rc = bind(server->ln_sock, (struct sockaddr * ) &server->addr, sizeof(server->addr));
	if (rc == -1)
	{
		log_ERROR("bind failed (%s)", strerror(errno));
		return -1;
	}

	rc = listen(server->ln_sock, MAX_CLIENTS_COUNT);
	if (rc == -1)
	{
		log_ERROR("listen failed (%s)", strerror(errno));
		return -1;
	}

	server->is_ready = true;
	log_DEBUG("Server is initialized");
	return 0;
}

int start_server(server_t * server)
{
	if (!server->is_ready)
	{
		log_ERROR("server is not ready");
		return -1;
	}

	struct ev_loop *loop = EV_DEFAULT;

	ev_signal signal_watcher;
	ev_signal_init(&signal_watcher, sigint_handler, SIGINT);
	ev_signal_start(loop, &signal_watcher);

	ev_io ln_sock_watcher;
	ev_io_init(&ln_sock_watcher, ln_sock_handler, server->ln_sock, EV_READ);
	ev_io_start(loop, &ln_sock_watcher);

	ev_set_userdata(loop, server);

	log_DEBUG("Server is started");
	ev_run(loop, 0);

	return 0;
}

int deinit_server(server_t * server)
{
	if (close(server->ln_sock))
	{
		log_ERROR("close failed (%s)", strerror(errno));
		return -1;
	}

	while (server->clients.next != NULL)
	{
		ev_io * watcher = GET_WATCHER(server->clients.next);
		if (close(watcher->fd))
		{
			log_ERROR("close failed (%s)", strerror(errno));
			return -1;
		}
		remove_client(server->clients.next);
	}

	memset(server, 0, sizeof(*server));

	log_DEBUG("Server is deinitialized");
	return 0;
}