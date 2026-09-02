/* forum_follow - PAL-FORUM-STANDARD.txt sec. 1/3. Appends a FOLLOW (or
 * UNFOLLOW - same line shape, different verb) line to <user_id>/
 * following.txt AND to net/outbox.txt for propagation - an unfollow is
 * a NEW line, never a rewrite of the old FOLLOW line (sec. 1's own
 * "derive current state by replay" rule, same shape as pal-chain's own
 * balance derivation via replaying blockchain.txt). Current follow set
 * is derived by whichever reader needs it (forum_compute_feed.c)
 * replaying this file and taking the LAST action per target_user_id.
 *
 * Refuses to follow a user_id that doesn't exist locally (this node
 * has no record of them at all yet) - a real, named v1 limitation: a
 * user only learns a target exists once they've seen at least one
 * propagated POST/profile from them, or created them locally.
 *
 * Self-contained, no shared headers.
 * Usage: forum_follow.+x <user_id> <target_user_id> <follow|unfollow> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: forum_follow.+x <user_id> <target_user_id> <follow|unfollow>\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];
    const char *target_id = argv[2];
    const char *mode = argv[3];

    if (strcmp(mode, "follow") != 0 && strcmp(mode, "unfollow") != 0) {
        fprintf(stderr, "Mode must be 'follow' or 'unfollow'.\n");
        return 1;
    }
    if (strcmp(user_id, target_id) == 0) {
        fprintf(stderr, "Cannot follow yourself.\n");
        return 1;
    }

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    char target_dir[PATH_BUF];
    snprintf(target_dir, sizeof(target_dir), "%s/users/%s", project_root, target_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) { fprintf(stderr, "No such user '%s'.\n", user_id); return 1; }
    if (stat(target_dir, &st) != 0) { fprintf(stderr, "No such user '%s'.\n", target_id); return 1; }

    const char *verb = (strcmp(mode, "follow") == 0) ? "FOLLOW" : "UNFOLLOW";
    long ts = (long)time(NULL);
    char line[512];
    snprintf(line, sizeof(line), "%s|%s|%s|%ld", verb, user_id, target_id, ts);

    char following_path[PATH_BUF];
    snprintf(following_path, sizeof(following_path), "%s/following.txt", user_dir);
    FILE *ff = fopen(following_path, "a");
    if (ff) { fprintf(ff, "%s\n", line); fclose(ff); }

    char net_dir[PATH_BUF];
    snprintf(net_dir, sizeof(net_dir), "%s/net", project_root);
    mkdir(net_dir, 0755);
    char outbox_path[PATH_BUF];
    snprintf(outbox_path, sizeof(outbox_path), "%s/net/outbox.txt", project_root);
    {   struct stat ob_st;
        if (stat(outbox_path, &ob_st) == 0 && ob_st.st_size > 2560 * 1024) {
            FILE *zf = fopen(outbox_path, "w");
            if (zf) fclose(zf);
        }
    }
    FILE *of = fopen(outbox_path, "a");
    if (of) { fprintf(of, "%s\n", line); fclose(of); }

    printf("%s %s.\n", strcmp(mode, "follow") == 0 ? "Now following" : "Unfollowed", target_id);
    return 0;
}
