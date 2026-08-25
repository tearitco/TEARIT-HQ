/* chat_create_user - PAL-CHAT-IRC-STANDARD.txt sec. 1. Creates a new
 * pal-chat-irc user directory: users/<user_id>/profile.txt. No
 * password (same v1 shape as forum_create_user.c/
 * userpal_create_account.c - no app in this family invents an auth
 * layer where none was asked for).
 *
 * Refuses if user_id already exists (same collision rule as every
 * other create-account op in this family).
 *
 * Self-contained, no shared headers.
 * Usage: chat_create_user.+x <user_id> <display_name> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int valid_user_id(const char *id) {
    if (!id[0]) return 0;
    for (const char *p = id; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chat_create_user.+x <user_id> <display_name>\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];
    const char *display_name = argv[2];

    if (!valid_user_id(user_id)) {
        fprintf(stderr, "Invalid user_id - letters, digits, _ and - only.\n");
        return 1;
    }

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) == 0) {
        fprintf(stderr, "User '%s' already exists.\n", user_id);
        return 1;
    }

    char users_root[PATH_BUF];
    snprintf(users_root, sizeof(users_root), "%s/users", project_root);
    mkdir(users_root, 0755);
    if (mkdir(user_dir, 0755) != 0) {
        fprintf(stderr, "Could not create user directory.\n");
        return 1;
    }

    char path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(path, sizeof(path), "%s/profile.txt", user_dir);
#pragma GCC diagnostic pop
    FILE *pf = fopen(path, "w");
    if (pf) {
        fprintf(pf, "user_id=%s\n", user_id);
        fprintf(pf, "display_name=%s\n", display_name);
        fprintf(pf, "created_at=%ld\n", (long)time(NULL));
        fclose(pf);
    }

    printf("Account '%s' created.\n", user_id);
    return 0;
}
