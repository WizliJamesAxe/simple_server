#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <pthread.h>

#include "server.h"
#include "client.h"
#include "log.h"

#define MAX_CLIENTS_COUNT 10

/*
Между потоками предполагается передача такого типа данных
typedef struct __attribute__((packed)) {
	int client_fd;
	size_t size;
	uint8_t data[0];
} task_t;
*/

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

static int send_answer(int fd, const char * answer, size_t size)
{
	ssize_t rc = send(fd, answer, size, 0);
	if (rc == -1)
	{
		log_ERROR("send failed (%s)", strerror(errno));
		return -1;
	}

	return 0;
}

static int request_to_main_thread(server_t * server, int fd, const char * answer, size_t size)
{
	char buffer[2048];
	int * _fd = (int *)buffer;
	size_t * _size = (size_t *)(buffer + sizeof(int));
	char * data = buffer + sizeof(int) + sizeof(size_t);

	*_fd = fd;
	*_size = size;
	memcpy(data, answer, size);

	ssize_t rc = write(server->hfd[1], buffer, sizeof(int) + sizeof(size_t) + size);
	if (rc < 0)
	{
		log_ERROR("write failed (%s)", strerror(errno));
		return -1;
	} else if ((size_t)rc != sizeof(int) + sizeof(size_t) + size)
	{
		log_ERROR("write failed (truncation %ld != %zu)", rc, sizeof(int) + sizeof(size_t) + size);
		return -1;
	}

	return 0;
}

static void answer_handler(EV_P_ ev_io * w, int revents)
{
	if (EV_ERROR & revents)
	{
		log_ERROR("invalid event (%s)", strerror(errno));
		return;
	}

	char buffer[2048];
	ssize_t len = read(w->fd, buffer, sizeof(buffer));
	if (len < 0)
	{
		if (errno == EAGAIN)
			return;

		log_ERROR("read failed (%s)", strerror(errno));
		return;
	}

	if ((size_t)len < sizeof(int) + sizeof(size_t))
	{
		log_ERROR("not enought data");
		return;
	}

	int * client_fd = (int * )buffer;
	size_t * size = (size_t * )(buffer + sizeof(int));
	char * data = buffer + sizeof(int) + sizeof(size_t);

	if (*size + sizeof(int) + sizeof(size_t) != (size_t)len)
	{
		log_ERROR("invalid size");
		return;
	}

	if (send_answer(*client_fd, data, *size) == -1)
	{
		log_ERROR("send_answer failed");
		return;
	}
}

static void request_handler(EV_P_ ev_io * w, int revents)
{
	if (EV_ERROR & revents)
	{
		log_ERROR("invalid event (%s)", strerror(errno));
		return;
	}

	char buffer[2048];
	ssize_t len = read(w->fd, buffer, sizeof(buffer));
	if (len < 0)
	{
		if (errno == EAGAIN)
			return;

		log_ERROR("read failed (%s)", strerror(errno));
		return;
	}

	if ((size_t)len < sizeof(int) + sizeof(size_t))
	{
		log_ERROR("not enought data");
		return;
	}

	int * client_fd = (int *)buffer;
	size_t * size = (size_t * )(buffer + sizeof(int));
	char * data = buffer + sizeof(int) + sizeof(size_t);
	log_DEBUG("Get request, len=%ld (fd=%d, size=%zu)", len, *client_fd, *size);

	if (*size + sizeof(int) + sizeof(size_t) != (size_t)len)
	{
		log_ERROR("invalid size");
		return;
	}

	// reverse string
	for (size_t i = 0; i < *size / 2; i++)
	{
		char t = data[i];
		data[i] = data[*size - i - 1];
		data[*size - i - 1] = t;
	}

	ssize_t rc = write(((server_t * )ev_userdata(loop))->afd[1], buffer, (size_t)len);
	if (rc < 0)
	{
		log_ERROR("write failed (%s)", strerror(errno));
		return;
	} else if ((size_t)rc != sizeof(int) + sizeof(size_t) + *size)
	{
		log_ERROR("write failed (truncation)");
		return;
	}
}

static void client_handler(EV_P_ ev_io * w, int revents)
{
	if (EV_ERROR & revents)
	{
		log_ERROR("invalid event (%s)", strerror(errno));
		goto close_watcher;
	}

	char buffer[2048];
	buffer[0] = '\0';

	ssize_t len = recv(w->fd, buffer, sizeof(buffer), 0);
	if (len < 0)
	{
		if (errno == EAGAIN)
			return;

		log_ERROR("recv failed (%s)", strerror(errno));
		goto close_watcher;
	}

	if (len == 0)
	{
		log_DEBUG("Client disconnected (fd=%d)", w->fd);
		goto close_watcher;
	}

	buffer[len] = '\0';

	log_DEBUG("Recieved from client(fd=%d): '%s' (len = %ld)", w->fd, buffer, len);

	int rc = request_to_main_thread((server_t * )ev_userdata(loop), w->fd, buffer, (unsigned) len);
	if (rc == -1)
	{
		log_ERROR("request_to_main_thread failed");
		goto close_watcher;
	}

	return;

close_watcher:
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

static void * listener_thread(void * arg)
{
	struct ev_loop * ln_loop = arg;

	log_DEBUG("Listener thread of server is started");
	//pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	ev_run(ln_loop, 0);

	log_DEBUG("Listener thread of server is stopped");
	return NULL;
}

static void listener_thread_stop(struct ev_loop * ln_loop, ev_async *w, int revents)
{
	ev_break(ln_loop, EVBREAK_ONE);
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

	rc = pipe(server->hfd);
	if (rc == -1)
	{
		log_ERROR("pipe failed (%s)", strerror(errno));
		return -1;
	}

	rc = pipe(server->afd);
	if (rc == -1)
	{
		log_ERROR("pipe failed (%s)", strerror(errno));
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

	pthread_attr_t thread_attrs;
	if (pthread_attr_init(&thread_attrs) != 0)
	{
		log_ERROR("pthread_attr_init failed");
		return -1;
	}

	// init listener event loop
	struct ev_loop * ln_loop = ev_loop_new(EVFLAG_AUTO);

	ev_io ln_sock_watcher;
	ev_io_init(&ln_sock_watcher, ln_sock_handler, server->ln_sock, EV_READ);
	ev_io_start(ln_loop, &ln_sock_watcher);

	ev_io pipe_watcher;
	ev_io_init(&pipe_watcher, answer_handler, server->afd[0], EV_READ);
	ev_io_start(ln_loop, &pipe_watcher);

	ev_async asyncWatcher;
	ev_async_init(&asyncWatcher, listener_thread_stop);
	ev_async_start(ln_loop, &asyncWatcher);

	ev_set_userdata(ln_loop, server);

	pthread_t thread;
	if (pthread_create(&thread, &thread_attrs, listener_thread, ln_loop) != 0)
	{
		log_ERROR("pthread_create failed");
		return -1;
	}

	if (pthread_attr_destroy(&thread_attrs) != 0)
	{
		log_ERROR("pthread_attr_destroy failed");
		return -1;
	}

	// init main event loop
	struct ev_loop * loop = EV_DEFAULT;

	ev_signal signal_watcher;
	ev_signal_init(&signal_watcher, sigint_handler, SIGINT);
	ev_signal_start(loop, &signal_watcher);

	ev_io request_watcher;
	ev_io_init(&request_watcher, request_handler, server->hfd[0], EV_READ);
	ev_io_start(loop, &request_watcher);

	ev_set_userdata(loop, server);

	log_DEBUG("Main thread of server is started");

	ev_run(loop, 0);

	log_DEBUG("Main thread of server is stoped");

	// wake up thread
	ev_async_send(ln_loop, &asyncWatcher);

	void * res;
	if (pthread_join(thread, &res) != 0)
	{
		log_ERROR("pthread_join failed");
		return -1;
	}

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