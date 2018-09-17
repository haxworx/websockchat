#include "commands.h"
#include "users.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#define TRUE true
#define FALSE false

static int
credentials_check(const char *username, char *guess)
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

static bool
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

static user_t *
_user_from_nick(hash_t *users, const char *nick)
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

static bool
cmd_message_send(hash_t *users, user_t *user, char *received)
{
   const char *format = "%s privately says: %s\n";
   char *to, *end, *msg, *message;
   int len;

   if (user->state != USER_STATE_AUTHENTICATED)
     return FALSE;

   to = strchr(received, ' ') + 1;
   if (!to) return FALSE;
   end = strchr(to, ' ');
   if (!end) return FALSE;
   *end = '\0';

   msg = end + 1;
   if (!msg) return FALSE;

   user_t *dest = _user_from_nick(users, to);
   if (!dest) return FALSE;

   len = strlen(format) + strlen(user->username) + strlen(msg) + 1;

   message = calloc(1, len);
   snprintf(message, len, format, user->username, msg);

   server_client_write(dest->client, message, len);

   free(message);

   return TRUE;
}

static bool
cmd_help_send(server_client_t *client, char *received)
{
   char desc[4096];
   const char *request, *cmds = "/QUIT, /NICK, /PASS, /REGISTER, /MSG, /HELP.";

   server_client_write(client, "\r\n", 2);

   if (!received)
     {
        snprintf(desc, sizeof(desc), "available cmds: %s\r\n", cmds);
        server_client_write(client, desc, strlen(desc));
        return TRUE;
     }

   request = strchr(received, ' ');
   if (!request || !request[0])
     {
        snprintf(desc, sizeof(desc), "available cmds: %s\r\n", cmds);
        server_client_write(client, desc, strlen(desc));
        return TRUE;
     }

   request += 1;

   if (!strcasecmp(request, "NICK"))
     {
        snprintf(desc, sizeof(desc), "NICK: use desired username.\r\n");
     }
   else if (!strcasecmp(request, "PASS"))
     {
        snprintf(desc, sizeof(desc), "PASS: authenticate with password.\r\n");
     }
   else if (!strcasecmp(request, "REGISTER"))
     {
        snprintf(desc, sizeof(desc), "REGISTER \"nick\" \"pass\": register nickname.\r\n");
     }
   else if (!strcasecmp(request, "MSG"))
     {
        snprintf(desc, sizeof(desc), "MSG: send message to desired user.\r\n");
     }
   else if (!strcasecmp(request, "MOTD"))
     {
        snprintf(desc, sizeof(desc), "MOTD: view server's message of the day.\r\n");
     }
   else if (!strcasecmp(request, "QUIT"))
     {
        snprintf(desc, sizeof(desc), "QUIT: quit this session.\r\n");
     }
   else
     {
        return FALSE;
     }

   server_client_write(client, desc, strlen(desc));

   return TRUE;
}

static bool
clients_username_exists(hash_t *users, const char *potential)
{
   const char *key;

   while ((key = hash_key_next(users)) != NULL)
     {
        user_t *tmp = hash_find(users, key);
        if (tmp->username && !strcasecmp(tmp->username, potential))
          return TRUE;
     }

   return FALSE;
}

static bool
cmd_identify(hash_t *users, user_t *user, char *received)
{
   char *potential;

   if (!strncasecmp(received, ":NICK ", 6))
     {
        potential = strchr(received, ' ') + 1;
        if (potential && potential[0])
          {
             if (_username_valid(potential) &&
                 !clients_username_exists(users, potential))
               {
                  snprintf(user->username, sizeof(user->username), "%s", potential);
                  user->state = USER_STATE_IDENTIFIED;
                  return TRUE;
               }
          }
     }

   return FALSE;
}

static bool
cmd_authenticate(user_t *user, char *received)
{
   char *guess;

   if (!strncasecmp(received, ":PASS ", 6))
     {
        guess = strchr(received, ' ') + 1;
        if (guess && guess[0])
          {
             if (credentials_check(user->username, guess))
               {
                  user->state = USER_STATE_AUTHENTICATED;
                  return TRUE;
               }
          }
     }

   return FALSE;
}

static bool
cmd_register(hash_t *users, user_t *user, char *received)
{
   const char *username, *password;
   char buf[1024];

   if (!strncasecmp(received, ":REGISTER ", 10))
     {
        username = received + 10;
        char *delim = strchr(username, ' ');
        if (!delim || !delim[0] || !delim[1]) return FALSE;
        *delim = '\0';

        password = delim + 1;

        if (clients_username_exists(users, username) || !_username_valid(username))
          return FALSE;

        snprintf(buf, sizeof(buf), "./auth %s %s", username, password);
        int result = system(buf);
        if (!result)
          {
             snprintf(user->username, sizeof(user->username), "%s", username);
             user->state = USER_STATE_AUTHENTICATED;
             return TRUE;
          }
     }

   return FALSE;
}

static void
cmd_list_users(hash_t *users, server_client_t *client, user_t *user)
{
   user_t *u;
   char *json;
   void *tmp;
   char buf[1024];
   int i = 0;
   size_t len = 0;
   const char *key, *username = NULL;

   snprintf(buf, sizeof(buf), "{ \"users\": \n[\n");

   len = strlen(buf);

   json = malloc(len + 1);
   json[0] = 0x00;
   strcat(json, buf);

   while ((key = hash_key_next(users)) != NULL)
     {
        u = hash_find(users, key);
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
        ++i;
     }

   snprintf(buf, sizeof(buf), "{ \"nick\": \"\" } \n]\n}");

   len += strlen(buf) + 1;
   tmp = realloc(json, len);
   if (!tmp) exit(10);
   json = tmp;
   strcat(json, buf);

   server_client_write(client, json, strlen(json));

   free(json);
}

static bool
cmd_message_broadcast(hash_t *users, user_t *user, char *received)
{
   const char *format = "%s says: %s\n";
   const char *key;
   char *message;
   int len;

   if (!received || !received[0]) return FALSE;

   len = strlen(user->username) + strlen(format) + strlen(received) + 1;
   message = calloc(1, len);

   snprintf(message, len, format, user->username, received);

   while ((key = hash_key_next(users)) != NULL)
     {
        user_t *u = hash_find(users, key);
        if (u)
          {
             server_client_write(u->client, message, len);
          }
     }

   free(message);

   return TRUE;
}

static void
cmd_failure(server_client_t *client)
{
   server_client_write(client, "FAIL!\r\n", 7);
}

static void
cmd_success(server_client_t *client)
{
   server_client_write(client, "OK!\r\n", 5);
}

void
cmd_parse(hash_t *users, server_client_t *client, char *received)
{
   char key[128];
   bool success = TRUE;

   snprintf(key, sizeof(key), "%d", client->sock);
   user_t *user = hash_find(users, key);
   if (user)
     {
        if (!strncasecmp(received, ":HELP", 5))
          { 
             cmd_help_send(client, received);
          }
        else if (!strncasecmp(received, ":REGISTER ", 10) &&
            user->state != USER_STATE_IDENTIFIED &&
            user->state != USER_STATE_AUTHENTICATED)
          {
             success = cmd_register(users, user, received);
             if (success)
               cmd_list_users(users, client, user);
          }
        else if (!strncasecmp(received, ":PASS", 5) &&
                 user->state != USER_STATE_AUTHENTICATED)
          {
             success = cmd_authenticate(user, received);
             if (success)
               cmd_list_users(users, client, user);
          }
        else if (!strncasecmp(received, ":NICK", 5) &&
                 user->state != USER_STATE_IDENTIFIED &&
                 user->state != USER_STATE_AUTHENTICATED)
          {
             success = cmd_identify(users, user, received);
          }
        else if (!strncasecmp(received, ":MSG ", 5))
          {
             success = cmd_message_send(users, user, received);
          }
        else if (!strncasecmp(received, "_USERS", 6))
          {
             cmd_list_users(users, client, user);
             return;
          }
        else if (!strncasecmp(received, ":QUIT", 5))
         {
            server_client_del(client->server, client);
            return;
         }
        else
          {
             if (user->state != USER_STATE_AUTHENTICATED)
               {
                  success = FALSE;
               }
             else
               {
                  success = cmd_message_broadcast(users, user, received);
               }
          }

        if (success)
          cmd_success(client);
        else
          cmd_failure(client);
     }
}
