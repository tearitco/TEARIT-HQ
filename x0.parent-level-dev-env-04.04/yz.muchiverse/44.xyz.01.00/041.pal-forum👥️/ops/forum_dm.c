/* forum_dm - PAL-FORUM-STANDARD.txt sec. 4. Appends a DM line to THIS
 * user's own copy of the conversation (users/<from>/dms/<to>.txt) AND
 * net/outbox.txt for propagation - the recipient's own node applies it
 * to ITS OWN copy (users/<to>/dms/<from>.txt) via forum_inbox_watcher.c,
 * each side keeping an independent local copy (egg-pals-13's own
 * per-pet-directory precedent, cited directly in the spec).
 *
 * v1 has NO privacy/encryption (sec. 4's own named gap, echoed here
 * rather than silently implied otherwise) - any peer on the mesh could
 * technically observe this line in transit.
 *
 * Self-contained, no shared headers.
 * Usage: forum_dm.+x <from_user_id> <to_user_id> <text> */
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

static void sanitize_line(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '|' || *p == '\n' || *p == '\r') *p = ' ';
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: forum_dm.+x <from_user_id> <to_user_id> <text>\n");
        return 1;
    }
    resolve_root();
    const char *from_id = argv[1];
    const char *to_id = argv[2];
    char text[MAX_LINE];
    snprintf(text, sizeof(text), "%s", argv[3]);
    sanitize_line(text);

    char from_dir[PATH_BUF];
    snprintf(from_dir, sizeof(from_dir), "%s/users/%s", project_root, from_id);
    struct stat st;
    if (stat(from_dir, &st) != 0) { fprintf(stderr, "No such user '%s'.\n", from_id); return 1; }
    char to_dir[PATH_BUF];
    snprintf(to_dir, sizeof(to_dir), "%s/users/%s", project_root, to_id);
    if (stat(to_dir, &st) != 0) { fprintf(stderr, "No such user '%s'.\n", to_id); return 1; }

    long ts = (long)time(NULL);
    char line[MAX_LINE + 256];
    snprintf(line, sizeof(line), "DM|%s|%s|%ld|%s", from_id, to_id, ts, text);

    char dms_dir[PATH_BUF];
    snprintf(dms_dir, sizeof(dms_dir), "%s/dms", from_dir);
    mkdir(dms_dir, 0755);
    char thread_path[PATH_BUF];
    snprintf(thread_path, sizeof(thread_path), "%s/%s.txt", dms_dir, to_id);
    FILE *tf = fopen(thread_path, "a");
    if (tf) { fprintf(tf, "%s\n", line); fclose(tf); }

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

    printf("Sent to %s.\n", to_id);
    return 0;
}
