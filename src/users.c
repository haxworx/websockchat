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

   cmd_list_users(users, user);
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
}

bool
user_exists(hash_t *users, const char *potential)
{
   const char *key;

   while ((key = hash_key_next(users)) != NULL)
     {
        user_t *tmp = hash_find(users, key);
        if (tmp->username && !strcasecmp(tmp->username, potential))
          return true;
     }

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
   const char *key;

   while ((key = hash_key_next(users)) != NULL)
     {
        user_t *user = hash_find(users, key);
        if (user && !strcmp(user->username, nick))
          {
             return user;
          }
     }

   return NULL;
}

