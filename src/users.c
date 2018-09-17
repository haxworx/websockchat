#include "users.h"
#include "commands.h"

void
users_add(hash_t *users, server_client_t *client)
{
   char key[128];

   snprintf(key, sizeof(key), "%d", client->sock);

   user_t *user = calloc(1, sizeof(user_t));
   user->sock = client->sock;
   user->client = malloc(sizeof(server_client_t));
   user->client->sock = client->sock;
   user->client->ssl = client->ssl;
   user->client->server = client->server;

   hash_add(users, key, user);

   cmd_list_users(users, client);
}

void
users_del(hash_t *users, server_client_t *client)
{
   char key[128];
   
   snprintf(key, sizeof(key), "%d", client->sock);
   user_t *user = hash_find(users, key);
   if (user)
     {
        free(user->client);
        hash_del(users, key);
     }
}
