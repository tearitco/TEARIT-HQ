/* forum_retweet - PAL-FORUM-STANDARD.txt sec. 1. Appends a RETWEET
 * line to <user_id>/retweets.txt AND net/outbox.txt. Refuses a
 * duplicate retweet of the same post_id by the same user (same dedup
 * reasoning as forum_like.c - sec. 6 names "un-retweet" as a real,
 * not-yet-built gap).
 *
 * Self-contained, no shared headers.
 * Usage: forum_retweet.+x <user_id> <post_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int already_retweeted(const char *path, const char *user_id, const char *post_id) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", line);
        char *fields[4]; int nf = 0; char *cursor = copy;
        for (; nf < 3; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 3) continue;
        if (strcmp(fields[1], user_id) == 0 && strcmp(fields[2], post_id) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: forum_retweet.+x <user_id> <post_id>\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];
    const char *post_id = argv[2];

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) { fprintf(stderr, "No such user '%s'.\n", user_id); return 1; }

    char retweets_path[PATH_BUF];
    snprintf(retweets_path, sizeof(retweets_path), "%s/retweets.txt", user_dir);
    if (already_retweeted(retweets_path, user_id, post_id)) {
        fprintf(stderr, "Already retweeted %s.\n", post_id);
        return 1;
    }

    long ts = (long)time(NULL);
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "RETWEET|%s|%s|%ld", user_id, post_id, ts);

    FILE *rf = fopen(retweets_path, "a");
    if (rf) { fprintf(rf, "%s\n", line); fclose(rf); }

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

    printf("Retweeted %s.\n", post_id);
    return 0;
}
