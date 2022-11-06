#include <stdlib.h>

#include "server.h"
#include "client.h"
#include "log.h"

// Create client with watcher and add it to start of list
client_t * add_client(server_t * server)
{
	client_t * client = calloc(1, sizeof(client_t) + sizeof(ev_io));
	if (client == NULL)
	{
		log_ERROR("calloc failed");
		return NULL;
	}

	client_t * old = server->clients.next;
	if (old == NULL)
	{
		server->clients.next = client;
		client->prev = &server->clients;
	}
	else
	{
		client->prev = old->prev;
		client->next = old;
		old->prev->next = client;
		old->prev = client;
	}

	return client;
}

void remove_client(client_t * client)
{
	client->prev->next = client->next;
	if (client->next)
		client->next->prev = client->prev;
	free(client);
}
