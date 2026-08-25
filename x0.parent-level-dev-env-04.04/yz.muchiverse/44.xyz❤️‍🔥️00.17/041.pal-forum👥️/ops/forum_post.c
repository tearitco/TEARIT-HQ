/* forum_post - PAL-FORUM-STANDARD.txt sec. 2. Appends a POST line to
 * <user_id>/wall.txt AND to this node's own net/outbox.txt so it
 * propagates over the mesh (palnet_peer.c, own_kind=forum_node - see
 * sec. 0). post_id = <user_id>-<timestamp>-<rand>, globally unique,
 * matching pal-chain's own tx_id shape.
 *
 * v1 has no multi-line text support (sec. 6's own named gap) - text is
 * taken as a single argv, pipe/newline characters in it would break
 * the line format, so they're stripped.
 *
 * Self-contained, no shared headers.
 * Usage: forum_post.+x <user_id> <text> [image_id] */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Strips '|' and '\n' - the pipe-delimited line format's own reserved
 * characters - so a post's text can never corrupt the line shape every
 * reader (forum_compute_feed.c, forum_inbox_watcher.c) depends on. */
static void sanitize_line(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '|' || *p == '\n' || *p == '\r') *p = ' ';
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: forum_post.+x <user_id> <text> [image_id]\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];
    char text[MAX_LINE];
    snprintf(text, sizeof(text), "%s", argv[2]);
    sanitize_line(text);
    const char *image_id = (argc >= 4) ? argv[3] : "-";

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) {
        fprintf(stderr, "No such user '%s'.\n", user_id);
        return 1;
    }

    long ts = (long)time(NULL);
    char post_id[128];
    snprintf(post_id, sizeof(post_id), "%s-%ld-%d", user_id, ts, rand() % 1000000);

    char line[MAX_LINE + 256];
    snprintf(line, sizeof(line), "POST|%s|%s|%ld|%s|%s", post_id, user_id, ts, text, image_id);

    char wall_path[PATH_BUF];
    snprintf(wall_path, sizeof(wall_path), "%s/wall.txt", user_dir);
    FILE *wf = fopen(wall_path, "a");
    if (wf) { fprintf(wf, "%s\n", line); fclose(wf); }

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

    printf("Posted (id=%s).\n", post_id);
    return 0;
}
