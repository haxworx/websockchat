#include "users.h"
#include "commands.h"
#include <string.h>

void
users_add(hash_t *users, server_client_t *client)
{
   user_t *user;
   char key[128];

   snprintf(key, sizeof(key), "%d", client->sock);

   user = calloc(1, sizeof(user_t));
   user->sock = client->sock;
   user->client = malloc(sizeof(server_client_t));
   user->client->sock = client->sock;
   user->client->ssl = client->ssl;
   user->client->server = client->server;

   hash_add(users, key, user);
}

void
users_del(hash_t *users, server_client_t *client)
{
   user_t *user;
   char key[128];
   
   snprintf(key, sizeof(key), "%d", client->sock);
   user = hash_find(users, key);
   if (user)
     {
        free(user->client);
        hash_del(users, key);
     }
   cmd_list_users_broadcast(users);
}

bool
user_exists(hash_t *users, const char *potential)
{
   char **keys = hash_keys_get(users);

   for (int i = 0; keys[i]; i++)
     {
        user_t *tmp = hash_find(users, keys[i]);
        if (tmp && tmp->username[0] && !strcasecmp(tmp->username, potential))
          {
             hash_keys_free(keys);
             return true;
          }
     }

   hash_keys_free(keys);

   return false;
}

user_t *
user_by_client(hash_t *users, server_client_t *client)
{
   user_t *user;
   char key[128];

   snprintf(key, sizeof(key), "%d", client->sock);
   user = hash_find(users, key);

   return user;
}

user_t *
user_by_nick(hash_t *users, const char *nick)
{
   char **keys = hash_keys_get(users);

   for (int i = 0; keys[i]; i++)
     {
        user_t *user = hash_find(users, keys[i]);
        if (user && !strcmp(user->username, nick))
          {
             return user;
          }
     }

   hash_keys_free(keys);

   return NULL;
}

