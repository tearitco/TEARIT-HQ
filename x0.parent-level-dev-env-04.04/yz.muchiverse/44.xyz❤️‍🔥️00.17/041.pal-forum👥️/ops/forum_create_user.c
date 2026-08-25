/* forum_create_user - PAL-FORUM-STANDARD.txt sec. 1. Creates a new
 * pal-forum user directory: pal-forum/users/<user_id>/ with an initial
 * profile.txt + empty wall.txt/following.txt/likes.txt/retweets.txt.
 * No password (the spec's own profile.txt field list has none - v1 is
 * local-first with no login/auth, matching the doc's own silence on
 * the subject rather than inventing an undocumented auth layer).
 *
 * Refuses if user_id already exists (same collision rule as
 * pal-chain's own chain_create_wallet.c).
 *
 * Self-contained, no shared headers.
 * Usage: forum_create_user.+x <user_id> <display_name> */
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

static void touch_empty(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: forum_create_user.+x <user_id> <display_name>\n");
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
    char dms_dir[PATH_BUF];
    snprintf(dms_dir, sizeof(dms_dir), "%s/dms", user_dir);
    mkdir(dms_dir, 0755);

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/profile.txt", user_dir);
    FILE *pf = fopen(path, "w");
    if (pf) {
        fprintf(pf, "user_id=%s\n", user_id);
        fprintf(pf, "display_name=%s\n", display_name);
        fprintf(pf, "created_at=%ld\n", (long)time(NULL));
        fprintf(pf, "bio=\n");
        fclose(pf);
    }

    snprintf(path, sizeof(path), "%s/wall.txt", user_dir); touch_empty(path);
    snprintf(path, sizeof(path), "%s/following.txt", user_dir); touch_empty(path);
    snprintf(path, sizeof(path), "%s/likes.txt", user_dir); touch_empty(path);
    snprintf(path, sizeof(path), "%s/retweets.txt", user_dir); touch_empty(path);
    snprintf(path, sizeof(path), "%s/feed_cache.txt", user_dir); touch_empty(path);

    printf("User '%s' created.\n", user_id);
    return 0;
}
