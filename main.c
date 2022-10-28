#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "server.h"

#define MAX_PORT_NUMBER 65535

void usage(const char * program_name)
{
	printf("Usage: %s <port>\n", program_name);
	printf("  port - number (0-%d)\n", MAX_PORT_NUMBER);
}

// On error, returns -1
// On success, returns port number in range (0, 65535)
int get_port_from_string(const char * port_str)
{
	if (NULL == port_str)
		return -1;

	const char * str = port_str;
	while (isdigit(*str))
		str++;

	if ('\0' != *str)
		return -1;

	int port;
	int rc = sscanf(port_str, "%d", &port);
	if (rc < 0 || rc != 1)
	{
		perror("sscanf failed");
		return -1;
	}

	if (port < 0 || port > MAX_PORT_NUMBER)
		return -1;

	return port;
}

int main(int argc, char * argv[])
{
	if (argc != 2)
	{
		usage(argv[0]);
		return -1;
	}

	int port = get_port_from_string(argv[1]);
	if (port < 0)
	{
		usage(argv[0]);
		return -1;
	}

	server_t server;
	if (init_server(&server, (unsigned short) port) == -1)
	{
		printf("failed to init server\n");
		return -1;
	}

	return start_server(&server);
}
