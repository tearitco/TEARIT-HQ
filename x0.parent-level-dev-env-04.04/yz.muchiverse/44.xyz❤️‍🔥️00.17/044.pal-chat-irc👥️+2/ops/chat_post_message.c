/* chat_post_message - PAL-CHAT-IRC-STANDARD.txt sec. 4. Appends a MSG
 * line to THIS node's own rooms/<room>/messages.txt AND net/outbox.txt
 * for propagation - exactly forum_post.c's own wall.txt + outbox.txt
 * double-write, room-scoped instead of user-scoped (sec. 0's own
 * "message belongs to a room, not a user" distinction).
 *
 * mkdir -p's the room if it doesn't exist locally yet (a room is
 * created implicitly by the first message posted or received naming
 * it - sec. 0).
 *
 * Self-contained, no shared headers.
 * Usage: chat_post_message.+x <room_name> <user_id> <text> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

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
        fprintf(stderr, "Usage: chat_post_message.+x <room_name> <user_id> <text>\n");
        return 1;
    }
    resolve_root();
    const char *room = argv[1];
    const char *user_id = argv[2];
    char text[MAX_LINE];
    snprintf(text, sizeof(text), "%s", argv[3]);
    sanitize_line(text);

    static long counter = 0;
    long ts = (long)time(NULL);
    char msg_id[64];
    snprintf(msg_id, sizeof(msg_id), "%ld-%ld-%d", ts, counter++, getpid());

    char line[MAX_LINE + 256];
    snprintf(line, sizeof(line), "MSG|%s|%s|%s|%ld|%s", msg_id, room, user_id, ts, text);

    char rooms_root[PATH_BUF];
    snprintf(rooms_root, sizeof(rooms_root), "%s/rooms", project_root);
    mkdir(rooms_root, 0755);
    char room_dir[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(room_dir, sizeof(room_dir), "%s/rooms/%s", project_root, room);
#pragma GCC diagnostic pop
    mkdir(room_dir, 0755);
    char messages_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(messages_path, sizeof(messages_path), "%s/messages.txt", room_dir);
#pragma GCC diagnostic pop
    FILE *mf = fopen(messages_path, "a");
    if (mf) { fprintf(mf, "%s\n", line); fclose(mf); }

    /* Master ledger: global, shared, append-only source of truth across
     * ALL rooms (mirrors 101.ledger-player-npc-simple+3's own
     * data/master_ledger.txt). rooms/<room>/messages.txt above remains
     * the per-room materialized view read by chat_compose_frame.c -
     * both are written together here so no separate resync pass is
     * needed for locally-posted messages. See ops/chat_replay_ledger.c
     * for the standalone recovery tool that rebuilds room views FROM
     * this file (e.g. after a crash or manual edit). */
    char data_dir[PATH_BUF];
    snprintf(data_dir, sizeof(data_dir), "%s/data", project_root);
    mkdir(data_dir, 0755);
    char ledger_path[PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", project_root);
    FILE *lf = fopen(ledger_path, "a");
    if (lf) { fprintf(lf, "%s\n", line); fclose(lf); }

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

    printf("Posted to %s.\n", room);
    return 0;
}
