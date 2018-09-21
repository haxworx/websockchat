#include "commands.h"
#include "users.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#define BOOL bool
#define TRUE true
#define FALSE false

static int
_auth_check(const char *username, char *guess)
{
   FILE *p;
   struct sigaction newaction, oldaction;
   char cmdstring[1024];
   int i, status = 0;

   sigemptyset(&newaction.sa_mask);
   newaction.sa_flags = 0;
   newaction.sa_handler = SIG_DFL;
   sigaction(SIGCHLD, NULL, &oldaction);
   sigaction(SIGCHLD, &newaction, NULL);
   
   p = popen("./auth", "w");
   if (p)
     {
        snprintf(cmdstring, sizeof(cmdstring), "%s %s\n", username, guess);
        fwrite(cmdstring, 1, strlen(cmdstring), p);

        status = pclose(p);
        status = !WEXITSTATUS(status);
     }

   for (i = 0; i < strlen(guess); i++)
     guess[i] = '\0';

   for (i = 0; i < strlen(cmdstring); i++)
     cmdstring[i] = '\0';

   sigaction(SIGCHLD, &oldaction, NULL);

   return status;
}

static BOOL
_username_valid(const char *username)
{
   int i;
   size_t len = strlen(username);

   if (!len || len > 32)
     return FALSE;

   for (i = 0; i < len; i++)
     {
        if (isspace(username[i]) ||
            (!isalpha(username[i]) && !isdigit(username[i])))
          {
             return FALSE;
          }
     }

   return TRUE;
}

static BOOL
_cmd_msg_send(hash_t *users, user_t *user)
{
   const char *format = "%s privately says: %s\n";
   char *to, *end, *msg, *message, *received;
   int len;

   if (user->state != USER_STATE_AUTHENTICATED)
     return FALSE;

   received = user->received;

   to = strchr(received, ' ') + 1;
   if (!to) return FALSE;
   end = strchr(to, ' ');
   if (!end) return FALSE;
   *end = '\0';

   msg = end + 1;
   if (!msg) return FALSE;

   user_t *dest = user_by_nick(users, to);
   if (!dest) return FALSE;

   len = strlen(format) + strlen(user->username) + strlen(msg) + 1;

   message = calloc(1, len);
   snprintf(message, len, format, user->username, msg);

   server_client_write(dest->client, message, len);

   free(message);

   return TRUE;
}

static BOOL
_cmd_help_send(user_t *user)
{
   server_client_t *client;
   char desc[4096];
   const char *cmds = "/QUIT, /NICK, /PASS, /REGISTER, /MSG, /HELP.";

   client = user->client;

   server_client_write(client, "\r\n", 2);
   snprintf(desc, sizeof(desc), "available cmds: %s\r\n", cmds);
   server_client_write(client, desc, strlen(desc));

   return TRUE;
}

static BOOL
_cmd_identify(hash_t *users, user_t *user)
{
   char *potential, *received;

   if (!(user->state != USER_STATE_IDENTIFIED &&
        user->state != USER_STATE_AUTHENTICATED))
     return FALSE;

   received = user->received;

   if (!strncasecmp(received, ":NICK ", 6))
     {
        potential = strchr(received, ' ') + 1;
        if (potential && potential[0])
          {
             if (_username_valid(potential) &&
                 !user_exists(users, potential))
               {
                  snprintf(user->username, sizeof(user->username), "%s", potential);
                  user->state = USER_STATE_IDENTIFIED;
                  return TRUE;
               }
          }
     }

   return FALSE;
}

static BOOL
_cmd_authenticate(hash_t *users, user_t *user)
{
   char *guess, *received;

   if (!(user->state != USER_STATE_AUTHENTICATED))
     return FALSE;

   received = user->received;

   if (!strncasecmp(received, ":PASS ", 6))
     {
        guess = strchr(received, ' ') + 1;
        if (guess && guess[0])
          {
             if (_auth_check(user->username, guess))
               {
                  user->state = USER_STATE_AUTHENTICATED;
                  cmd_list_users_broadcast(users);
                  return TRUE;
               }
          }
     }

   return FALSE;
}

static BOOL
_cmd_register(hash_t *users, user_t *user)
{
   const char *username, *password;
   char *received;
   char buf[1024];

   if (!(user->state != USER_STATE_IDENTIFIED &&
       user->state != USER_STATE_AUTHENTICATED)) return false;

   received = user->received;

   if (!strncasecmp(received, ":REGISTER ", 10))
     {
        username = received + 10;
        char *delim = strchr(username, ' ');
        if (!delim || !delim[0] || !delim[1]) return FALSE;
        *delim = '\0';

        password = delim + 1;

        if (user_exists(users, username) || !_username_valid(username))
          return FALSE;

        snprintf(buf, sizeof(buf), "./auth %s %s", username, password);
        int result = system(buf);
        if (!result)
          {
             snprintf(user->username, sizeof(user->username), "%s", username);
             user->state = USER_STATE_AUTHENTICATED;
             cmd_list_users_broadcast(users);
             return TRUE;
          }
     }

   return FALSE;
}

void
cmd_list_users(hash_t *users, user_t *user)
{
   user_t *u;
   char *json;
   void *tmp;
   char buf[1024];
   size_t len;
   const char *username = NULL;

   snprintf(buf, sizeof(buf), "{ \"users\": \n[\n");

   len = strlen(buf);

   json = malloc(len + 1);
   json[0] = 0x00;
   strcat(json, buf);

   char **keys = hash_keys_get(users);

   for (int i = 0; keys[i] != NULL; i++)
     {
        u = hash_find(users, keys[i]);
        if (u->state == USER_STATE_AUTHENTICATED)
          username = u->username;
        else
          username = "";

        snprintf(buf, sizeof(buf), "{ \"nick\": \"%s\" },\n", username);

        len += strlen(buf) + 1;
        tmp = realloc(json, len);
        if (!tmp) exit(100);
        json = tmp;
        strcat(json, buf);
     }

   hash_keys_free(keys);

   snprintf(buf, sizeof(buf), "{ \"nick\": \"\" } \n]\n}");

   len += strlen(buf) + 1;
   tmp = realloc(json, len);
   if (!tmp) exit(10);
   json = tmp;
   strcat(json, buf);

   server_client_write(user->client, json, strlen(json));

   free(json);
}

void
cmd_list_users_broadcast(hash_t *users)
{
   char **keys = hash_keys_get(users);

   for (int i = 0; keys[i] != NULL; i++)
     {
        user_t *user = hash_find(users, keys[i]);
        if (user)
          cmd_list_users(users, user);
     }

   hash_keys_free(keys);
}

static BOOL
_cmd_msg_broadcast(hash_t *users, user_t *user)
{
   const char *format = "%s says: %s\r\n";
   char **keys;
   char *message, *received;
   int len;

   received = user->received;

   if (!received || !received[0]) return FALSE;

   len = strlen(user->username) + strlen(format) + strlen(received) + 1;
   message = calloc(1, len);

   snprintf(message, len, format, user->username, received);

   keys = hash_keys_get(users);

   for (int i = 0; keys[i] != NULL; i++)
     {
        user_t *u = hash_find(users, keys[i]);
        if (u)
          {
             server_client_write(u->client, message, len);
          }
     }

   hash_keys_free(keys);

   free(message);

   return TRUE;
}

static void
_cmd_failure(user_t *user)
{
   server_client_write(user->client, "FAIL!\r\n", 7);
}

static void
_cmd_success(user_t *user)
{
   server_client_write(user->client, "OK!\r\n", 5);
}

static void
_trim(char *string)
{
   char *p = string;
 
   while (*p)
     {
        if (*p == '\r' || *p == '\n')
          {
             *p = 0x00;
             return;
          }
        p++;
     }
}

void
cmd_parse(hash_t *users, user_t *user)
{
   BOOL success = TRUE;

   if (!strncasecmp(user->received, ":QUIT", 5))
     {
        server_client_del(user->client->server, user->client);
        return;
     }

   _trim(user->received);

   if (!strncasecmp(user->received, ":HELP", 5))
     {
        _cmd_help_send(user);
     }
   else if (!strncasecmp(user->received, ":REGISTER ", 10))
     {
        success = _cmd_register(users, user);
     }
   else if (!strncasecmp(user->received, ":PASS", 5))
     {
        success = _cmd_authenticate(users, user);
     }
   else if (!strncasecmp(user->received, ":NICK", 5))
     {
        success = _cmd_identify(users, user);
     }
   else if (!strncasecmp(user->received, ":MSG ", 5))
     {
        success = _cmd_msg_send(users, user);
     }
   else
     {
        if (user->state != USER_STATE_AUTHENTICATED)
          {
             success = FALSE;
          }
        else
          {
             _cmd_msg_broadcast(users, user);
             return;
          }
     }

   if (success)
     _cmd_success(user);
   else
     _cmd_failure(user);
}
