/* forum_switch_user - sets net/session.txt's own current_user_id to an
 * EXISTING user (refuses if the user directory doesn't exist) - v1 has
 * no password (see forum_create_user.c's own header comment for why:
 * the spec's own profile.txt field list never included one).
 *
 * Self-contained, no shared headers.
 * Usage: forum_switch_user.+x <user_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: forum_switch_user.+x <user_id>\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) {
        fprintf(stderr, "No such user '%s'.\n", user_id);
        return 1;
    }

    char net_dir[PATH_BUF];
    snprintf(net_dir, sizeof(net_dir), "%s/net", project_root);
    mkdir(net_dir, 0755);

    char session_path[PATH_BUF];
    snprintf(session_path, sizeof(session_path), "%s/net/session.txt", project_root);
    FILE *f = fopen(session_path, "w");
    if (!f) { fprintf(stderr, "Could not write session.\n"); return 1; }
    fprintf(f, "current_user_id=%s\n", user_id);
    fclose(f);

    printf("Switched to '%s'.\n", user_id);
    return 0;
}
