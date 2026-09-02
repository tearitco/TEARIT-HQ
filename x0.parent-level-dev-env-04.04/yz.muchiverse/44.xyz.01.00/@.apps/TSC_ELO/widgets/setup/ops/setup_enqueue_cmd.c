/* setup_enqueue_cmd - the Match Setup WIDGIT's side of the host cmd bus.
 * Reads focus.txt (written by setup_set_focus) for the host's inbox, then
 * appends one command line. Host-side drainer is ops/tsc_setup.+x
 * (background loop in the host's button.sh). Modeled directly on
 * &.widgits/file-menu/ops/fm_enqueue_cmd.c.
 *
 * Self-contained, no shared headers.
 * Usage:
 *   setup_enqueue_cmd.+x <widget_state_dir> MATCH <mode>   -> MATCH:<mode>
 *   setup_enqueue_cmd.+x <widget_state_dir> RATING <n>     -> RATING:<n>
 *   setup_enqueue_cmd.+x <widget_state_dir> PLAYER <name>  -> PLAYER:<name>
 *   setup_enqueue_cmd.+x <widget_state_dir> START          -> START
 *   setup_enqueue_cmd.+x <widget_state_dir> PING           -> PING
 *   setup_enqueue_cmd.+x <widget_state_dir> CHALLENGE      -> PVP:CHALLENGE
 *   setup_enqueue_cmd.+x <widget_state_dir> ACCEPT         -> PVP:ACCEPT
 *   setup_enqueue_cmd.+x <widget_state_dir> MOVE <action>  -> MOVE:<action>
 *   setup_enqueue_cmd.+x <widget_state_dir> RESIGN         -> RESIGN */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: setup_enqueue_cmd.+x <widget_state_dir> <CMD> [arg]\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *cmd = argv[2];
    const char *arg = (argc >= 4) ? argv[3] : "";

    char focus[PATH_BUF], inbox[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", wdir);
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    if (!inbox[0]) {
        fprintf(stderr, "setup_enqueue_cmd: no focus (run setup_set_focus first)\n");
        return 1;
    }

    char line[MAX_LINE];
    if (strcmp(cmd, "START") == 0 || strcmp(cmd, "PING") == 0 ||
        strcmp(cmd, "CHALLENGE") == 0 || strcmp(cmd, "ACCEPT") == 0 ||
        strcmp(cmd, "RESIGN") == 0) {
        if (strcmp(cmd, "CHALLENGE") == 0) {
            snprintf(line, sizeof(line), "PVP:CHALLENGE\n");
        } else if (strcmp(cmd, "ACCEPT") == 0) {
            snprintf(line, sizeof(line), "PVP:ACCEPT\n");
        } else {
            snprintf(line, sizeof(line), "%s\n", cmd);
        }
    } else if (strcmp(cmd, "MATCH") == 0 || strcmp(cmd, "RATING") == 0 ||
               strcmp(cmd, "PLAYER") == 0 || strcmp(cmd, "MOVE") == 0) {
        if (!arg[0]) {
            fprintf(stderr, "%s needs an argument\n", cmd);
            return 1;
        }
        snprintf(line, sizeof(line), "%s:%s\n", cmd, arg);
    } else {
        fprintf(stderr, "unknown cmd %s\n", cmd);
        return 1;
    }

    FILE *f = fopen(inbox, "a");
    if (!f) {
        fprintf(stderr, "cannot open host inbox %s\n", inbox);
        return 1;
    }
    fputs(line, f);
    fclose(f);
    printf("ENQUEUE %s", line);
    return 0;
}
