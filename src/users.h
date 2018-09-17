#ifndef __USERS_H__
#define __USERS_H__

#include <sea/hash.h>
#include <sea/server.h>

typedef enum
{
   USER_STATE_DISCONNECTED,
   USER_STATE_CONNECTED,
   USER_STATE_DISCONNECT,
   USER_STATE_IDENTIFIED,
   USER_STATE_AUTHENTICATED,
} user_state_t;

typedef struct user_t user_t;
struct user_t
{
   user_state_t     state;
   int              sock;
   char             username[128];
   server_client_t *client;
};

void
users_add(hash_t *users, server_client_t *client);

void
users_del(hash_t *users, server_client_t *client);
#endif
