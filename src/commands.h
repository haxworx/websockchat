#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <sea/hash.h>
#include <sea/server.h>
#include "users.h"

void
cmd_parse(hash_t *users, user_t *user);

void
cmd_list_users(hash_t *users, user_t *user);

#endif
