/* chat_replay_ledger - standalone recovery/audit tool for the
 * data/master_ledger.txt architecture (see chat_post_message.c /
 * chat_inbox_watcher.c's own header comments for the write side).
 *
 * Reads data/master_ledger.txt FULLY, groups MSG lines by their room
 * field, and rewrites every rooms/<room>/messages.txt from scratch to
 * match. This is the actual "replay" half of the ledger pattern -
 * without it, master_ledger.txt would just be a second copy written in
 * lockstep, not a real source of truth you could recover FROM. Safe to
 * run any time: idempotent, and only ever touches rooms/ (never
 * data/master_ledger.txt itself). Useful after a crash mid-write, after
 * manually editing/merging the ledger (e.g. the one-time backfill of
 * pre-ledger room history), or to rebuild a room view that got deleted.
 *
 * Not part of the live hot path - chat_post_message.c and
 * chat_inbox_watcher.c already dual-write both files directly for
 * normal operation. This op is the fallback/verification tool.
 *
 * Self-contained, no shared headers.
 * Usage: chat_replay_ledger.+x (no args - always a full rebuild) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_ROOMS 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

typedef struct {
    char name[128];
    FILE *tmp;
    char tmp_path[PATH_BUF];
    char real_path[PATH_BUF];
} room_slot;

static room_slot g_rooms[MAX_ROOMS];
static int g_room_count = 0;

static room_slot *get_room_slot(const char *room) {
    for (int i = 0; i < g_room_count; i++) {
        if (strcmp(g_rooms[i].name, room) == 0) return &g_rooms[i];
    }
    if (g_room_count >= MAX_ROOMS) return NULL;

    room_slot *slot = &g_rooms[g_room_count++];
    snprintf(slot->name, sizeof(slot->name), "%s", room);

    char rooms_root[PATH_BUF];
    snprintf(rooms_root, sizeof(rooms_root), "%s/rooms", project_root);
    mkdir(rooms_root, 0755);
    char room_dir[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(room_dir, sizeof(room_dir), "%s/rooms/%s", project_root, room);
#pragma GCC diagnostic pop
    mkdir(room_dir, 0755);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(slot->real_path, sizeof(slot->real_path), "%s/messages.txt", room_dir);
    snprintf(slot->tmp_path, sizeof(slot->tmp_path), "%s/messages.txt.replay_tmp", room_dir);
#pragma GCC diagnostic pop

    slot->tmp = fopen(slot->tmp_path, "w");
    return slot;
}

int main(void) {
    resolve_root();

    char ledger_path[PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", project_root);

    FILE *lf = fopen(ledger_path, "r");
    if (!lf) {
        printf("No master ledger found at %s - nothing to replay.\n", ledger_path);
        return 1;
    }

    char line[MAX_LINE];
    int total = 0, applied = 0;
    while (fgets(line, sizeof(line), lf)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        total++;

        /* MSG|<msg_id>|<room_name>|<user_id>|<timestamp>|<text> */
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", line);
        char *fields[6]; int nf = 0; char *cursor = copy;
        for (; nf < 5; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 5) continue;
        if (strcmp(fields[0], "MSG") != 0) continue;
        const char *room = fields[2];

        room_slot *slot = get_room_slot(room);
        if (!slot || !slot->tmp) continue;
        fprintf(slot->tmp, "%s\n", line);
        applied++;
    }
    fclose(lf);

    for (int i = 0; i < g_room_count; i++) {
        if (g_rooms[i].tmp) fclose(g_rooms[i].tmp);
        rename(g_rooms[i].tmp_path, g_rooms[i].real_path);
    }

    printf("Replayed %d/%d ledger lines across %d room(s).\n", applied, total, g_room_count);
    return 0;
}
