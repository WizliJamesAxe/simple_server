#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ev.h>

int start_server(unsigned short port)
{
	struct sockaddr_in my_addr, peer_addr;
	socklen_t peer_addr_size;

	printf("Log: 0\n");
	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1)
	{
		printf("socket failed, errno = %d\n", errno);
		return errno;
	}

    int          optval = 1;
	if(setsockopt(server_sock, SOL_SOCKET,  SO_REUSEADDR, &optval, sizeof(optval)) != 0)
	{
		printf("socket failed, errno = %d\n", errno);
		return errno;
	}
	if(setsockopt(server_sock, SOL_SOCKET,  SO_REUSEPORT, &optval, sizeof(optval)) != 0)
	{
		printf("socket failed, errno = %d\n", errno);
		return errno;
	}

	printf("Log: 1\n");
	memset(&my_addr, 0, sizeof(my_addr));

	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr.sin_port = htons(port);

	printf("Log: 2\n");
	int rc = bind(server_sock, (struct sockaddr * ) &my_addr, sizeof(my_addr));
	if (rc == -1)
	{
		perror("");
		printf("bind failed, errno = %d\n", errno);
		return errno;
	}

	printf("Log: 3\n");
	rc = listen(server_sock, 2);
	if (rc == -1)
	{
		printf("listen failed, errno = %d\n", errno);
		return errno;
	}

   /* Now we can accept incoming connections one
      at a time using accept(2) */

	printf("Log: 4\n");
	peer_addr_size = sizeof(peer_addr);
	int client_sock = accept(server_sock, (struct sockaddr * ) &peer_addr, &peer_addr_size);
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
	close(server_sock);

	return 0;
}