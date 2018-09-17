#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#define CREDENTIALS_FILE "./users.txt"

#define SUPER_SECRET "Иисус милосерден"

/* Obv just an example auth implementation */

typedef struct
{
   char *username;
   char *password;
} Credentials;

char *
password_sha256sum(const char *password)
{
   SHA256_CTX ctx;
   int i, j;
   unsigned char result[SHA256_DIGEST_LENGTH] = { 0 };
   char sha256[2 * SHA256_DIGEST_LENGTH + 1 ] = { 0 };
   char combined[1024];

   snprintf(combined, sizeof(combined), "%s:%s", SUPER_SECRET, password);

   SHA256_Init(&ctx);
   SHA256_Update(&ctx, combined, strlen(combined));
   SHA256_Final(result, &ctx);

   for (i = 0, j = 0; i < SHA256_DIGEST_LENGTH; i++)
     {
        snprintf(&sha256[j], sizeof(sha256) - j, "%02x", (unsigned int) result[i]);
        j += 2;
     }

   return strdup(sha256);
}

static char *
_ipc_sem_name_create(const char *name)
{
   char compat_name[4096];

#if defined(__linux__)
   return strdup(name);
#else
   snprintf(compat_name, sizeof(compat_name), "/%s_sem", name);
#endif

   return strdup(compat_name);
}

Credentials **
credentials_get(int *count)
{
   FILE *f;
   char buf[1024];
   int index = 0;
   Credentials *c, **creds = NULL;

   f = fopen(CREDENTIALS_FILE, "r");
   if (!f)
     return NULL;

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        if (buf[0] == '#')
          continue;

        const char *user = buf;
        char *delim = strchr(buf, ':');
        const char *pass;

        if (!user || !delim || !(delim + 1)) continue;

        *delim = '\0';

        pass = delim + 1;

        delim = strrchr(pass, '\n');
        if (delim) *delim = '\0';

        if (!creds)
          {
             creds = malloc(1 * sizeof(Credentials *));
             c = malloc(sizeof(Credentials));
             c->username = strdup(user);
             c->password = strdup(pass);
             creds[index++] = c;
          }
        else
          {

             void *tmp = realloc(creds, (1 + index) * sizeof(Credentials *));
             if (!tmp) exit(100);
             creds = tmp;

             c = malloc(sizeof(Credentials));
             c->username = strdup(user);
             c->password = strdup(pass);
             creds[index++] = c;
          }

     }

   fclose(f);

   *count = index;

   return creds;
}

int user_exists(const char *username)
{
   FILE *f;
   char buf[1024];
   int exists = 0;

   f = fopen(CREDENTIALS_FILE, "r");
   if (!f) return 0;

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        const char *user = buf;
        char *end = strchr(buf, ':');
        if (!user || !end || ! (end + 1))
          continue;
        *end = '\0';
        if (!strcasecmp(user, username))
          {
             exists = 1;
             break;
          }
     }

   fclose(f);

   return exists;
}

int
credentials_add(const char *username, const char *password)
{
   Credentials **known_users, *user;
   int i, count;
   int updated = 0;
   int ret = 1;

   if (!username || !username[0] || !password || !password[0])
     return 1;

   if (user_exists(username))
     return 1;

   char *sem_name = _ipc_sem_name_create("lock");
   sem_t *sem;

   sem_unlink(sem_name);

#if defined(__linux__)
   sem = sem_open(sem_name, O_CREAT | O_RDWR, 0644, 1);
#else
   sem = sem_open(sem_name, O_CREAT, 0644, 1);
#endif
   if (sem == SEM_FAILED)
     goto error;

   if (!sem_wait(sem))
     {
        known_users = credentials_get(&count);

        for (i = 0; i < count; i++)
          {
              if (!known_users) break;
              user = known_users[i];
              if (!strcasecmp(username, user->username))
                {
                   free(user->password);
                   user->password = password_sha256sum(password);
                   updated = 1;
                }
          }

        FILE *f = fopen(CREDENTIALS_FILE, "w");
        if (!f)
          {
             sem_post(sem);
             goto error;
          }

        for (i = 0; i < count; i++)
          {
             if (!known_users) break;
             user = known_users[i];
             fprintf(f, "%s:%s\n", user->username, user->password);
             free(user->username);
             free(user->password);
             free(user);
          }

        if (known_users)
          free(known_users);

        if (!updated)
          {
             char *hashed = password_sha256sum(password);
             fprintf(f, "%s:%s\n", username, hashed);
          }

        fclose(f);

        sem_post(sem);
     }

   sem_close(sem);
   ret = 0;
error:
   sem_unlink(sem_name);
   free(sem_name);

   return ret;
}

int
credentials_check(const char *username, char *guess)
{
   Credentials *user;
   int count;
   int i = 0;
   int result = 1;

   Credentials **known_users = credentials_get(&count);
   if (!known_users)
     return 1;

   for (i = 0; i < count; i++)
     {
        user = known_users[i];
        if (!strcasecmp(user->username, username))
          {
             char *hashed = password_sha256sum(guess);
             if (!strcmp(user->password, hashed))
               result = 0;
             free(hashed);
             break;
          }
     }

   for (i = 0; i < strlen(guess); i++)
     guess[i] = 0x00;

   for (i = 0; i < count; i++)
     {
        user = known_users[i];
        free(user->username);
        free(user->password);
        free(user);
     }

   free(known_users);

   return result;
}

int
main(int argc, char **argv)
{
   char *user, *end, *guess;
   char buf[4096], byte[1];
   int i = 0;

   if (argc == 3)
     {
        return credentials_add(argv[1], argv[2]);
     }

   while (read(STDIN_FILENO, byte, sizeof(byte)) > 0)
     {
        buf[i++] = byte[0];
        if (i == sizeof(buf) - 1)
          break;
     }

   buf[i] = 0x00;

   user = &buf[0];
   end = strchr(buf, ' ');
   if (!end) return 1;

   guess = strchr(buf, ' ') + 1;
   if (guess && guess[0])
     {
        *end = '\0';
        end = strrchr(guess, '\r'); if (!end) end = strrchr(guess, '\n');
        if (end) *end = '\0';
        return credentials_check(user, guess);
     }
   return 1;
}

