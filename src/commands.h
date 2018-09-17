#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <sea/hash.h>
#include <sea/server.h>

void
cmd_parse(hash_t *users, server_client_t *client, char *received);

void
cmd_list_users(hash_t *users, server_client_t *client);

#endif
