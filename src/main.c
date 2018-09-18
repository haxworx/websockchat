#include <string.h>
#include "users.h"
#include "commands.h"
#include <sea/server.h>
#include <sea/hash.h>

static int
_on_add_cb(server_event_t *event, void *data)
{
   server_t *server;
   const char *welcome = "Welcome to the twighlight zone!\r\n";
   char *address;
   hash_t *users = data;

   server = event->server;

   server_client_write(event->client, welcome, strlen(welcome));

   address = server_client_address_get(event->client);
   if (address)
     {
        users_add(users, event->client);
        printf("added client #%d origin: %s\n", server->socket_count - 1, address);
        free(address);
     }

   return 0;
}

static int
_on_data_cb(server_event_t *event, void *data)
{
   char *string;
   user_t *user;
   server_data_t *received;
   hash_t *users = data;

   received = event->received;

   string = malloc(received->size + 1);
   memcpy(string, received->data, received->size);
   string[received->size] = 0x00;

   user = user_by_client(users, event->client);
   if (user)
     {
        user->received = string;
        cmd_parse(users, user);
     }

   free(string);

   return 0;
}

static int
_on_err_cb(server_event_t *event, void *data)
{
   hash_t *users = data;

   users_del(users, event->client);

   return 0;
}

static int
_on_del_cb(server_event_t *event, void *data)
{
   hash_t *users;
   const char *goodbye = "Bye now!\r\n";

   users = data;

   printf("disconnected: %d\n", event->client->sock);

   server_client_write(event->client, goodbye, strlen(goodbye));

   users_del(users, event->client);

   return 0;
}

static void
usage(void)
{
   printf("Usage: websockchat [ADDRESS]\n"
          "   Where ADDRESS can be one of\n"
          "   IP:PORT or IPv6:PORT or file://PATH\n"
          "   e.g. 10.1.1.1:12345         (listen on 10.1.1.1 port 12345)\n"
          "        ::1:12345              (listen on ::1 port 12345\n"
          "        *:4444                 (listen on all addresses port 4444\n"
          "        file:///tmp/socket.0   (listen on UNIX local socket at path)\n");

   exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
   server_t *server;
   const char *address;
   hash_t *users;

   if (argc == 2)
     {
        address = argv[1];
     }
   else
     {
        usage();
     }

   server = server_new();
   if (!server)
     exit(EXIT_FAILURE);

   if (!server_config_address_port_set(server, address))
     {
        fprintf(stderr, "Invalid listen address and port\n");
        exit(EXIT_FAILURE);
     }

   server_config_clients_max_set(server, 512);

   server_config_client_timeout_set(server, 60);

   printf("PID %d listening at %s, max clients %d\n",
        getpid(), server->local_address, server->sockets_max - 1);

   users = hash_new();

   server_event_callback_set(server, SERVER_EVENT_CALLBACK_ADD, _on_add_cb, users);
   server_event_callback_set(server, SERVER_EVENT_CALLBACK_DEL, _on_del_cb, users);
   server_event_callback_set(server, SERVER_EVENT_CALLBACK_ERR, _on_err_cb, users);
   server_event_callback_set(server, SERVER_EVENT_CALLBACK_DATA, _on_data_cb, users);

//   server_websocket_enabled_set(server, true);

   server_run(server);

   hash_free(users);

   server_free(server);

   return EXIT_SUCCESS;
}

