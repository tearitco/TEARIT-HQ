/* fm_enqueue_cmd - file-menu sends a command to focused editor inbox.
 * Usage:
 *   fm_enqueue_cmd.+x <widget_state_dir> NEW
 *   fm_enqueue_cmd.+x <widget_state_dir> SAVE
 *   fm_enqueue_cmd.+x <widget_state_dir> LOAD <abs_path>
 *   fm_enqueue_cmd.+x <widget_state_dir> SAVE_AS <abs_path>
 *   fm_enqueue_cmd.+x <widget_state_dir> PING
 */
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
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: fm_enqueue_cmd.+x <widget_state_dir> <CMD> [path]\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *cmd = argv[2];
    const char *path_arg = (argc >= 4) ? argv[3] : "";

    char focus[PATH_BUF], inbox[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", wdir);
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    if (!inbox[0]) {
        fprintf(stderr, "fm_enqueue_cmd: no focus (run fm_set_focus first)\n");
        return 1;
    }

    /* ensure parent dir */
    char line[MAX_LINE];
    /* Editor text cmds */
    if (strcmp(cmd, "NEW") == 0 || strcmp(cmd, "SAVE") == 0 || strcmp(cmd, "PING") == 0)
        snprintf(line, sizeof(line), "%s\n", cmd);
    else if (strcmp(cmd, "LOAD") == 0) {
        if (!path_arg[0]) { fprintf(stderr, "LOAD needs path\n"); return 1; }
        snprintf(line, sizeof(line), "LOAD:%s\n", path_arg);
    } else if (strcmp(cmd, "SAVE_AS") == 0) {
        if (!path_arg[0]) { fprintf(stderr, "SAVE_AS needs path\n"); return 1; }
        snprintf(line, sizeof(line), "SAVE_AS:%s\n", path_arg);
    }
    /* Mutaclysm game-world cmds (focus kind=game_world) */
    else if (strcmp(cmd, "NEW_GAME") == 0 || strcmp(cmd, "SEED_DEMO") == 0
             || strcmp(cmd, "LIST_MAPS") == 0)
        snprintf(line, sizeof(line), "%s\n", cmd);
    else if (strcmp(cmd, "SAVE_GAME_AS") == 0) {
        if (!path_arg[0]) { fprintf(stderr, "SAVE_GAME_AS needs slot name\n"); return 1; }
        snprintf(line, sizeof(line), "SAVE_GAME_AS:%s\n", path_arg);
    } else if (strcmp(cmd, "LOAD_GAME") == 0) {
        if (!path_arg[0]) { fprintf(stderr, "LOAD_GAME needs slot name\n"); return 1; }
        snprintf(line, sizeof(line), "LOAD_GAME:%s\n", path_arg);
    } else if (strcmp(cmd, "SWITCH_MAP") == 0) {
        /* path_arg = map_id or map_id:x:y */
        if (!path_arg[0]) { fprintf(stderr, "SWITCH_MAP needs map_id\n"); return 1; }
        snprintf(line, sizeof(line), "SWITCH_MAP:%s\n", path_arg);
    } else {
        fprintf(stderr, "unknown cmd %s\n", cmd);
        return 1;
    }

    FILE *f = fopen(inbox, "a");
    if (!f) {
        fprintf(stderr, "cannot open inbox %s\n", inbox);
        return 1;
    }
    fputs(line, f);
    fclose(f);
    printf("ENQUEUE %s", line);
    return 0;
}
