/* mp_switch_map - enqueue SWITCH_MAP via file-menu-style focus inbox
 * OR call muta_map_io directly.
 *
 * Usage: mp_switch_map.+x <widget_state_dir> <map_id> [x] [y]
 * Prefer: write inbox + caller runs muta_widget_cmds, OR direct switch.
 * This op does direct switch (simpler for harness) using session_root.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
        fprintf(stderr, "Usage: mp_switch_map.+x <widget_state_dir> <map_id> [x] [y]\n");
        return 1;
    }
    char focus[PATH_BUF], sess[PATH_BUF], inbox[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", argv[1]);
    read_kv(focus, "session_root", sess, sizeof(sess));
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    if (!sess[0]) {
        fprintf(stderr, "mp_switch_map: no session_root in focus\n");
        return 1;
    }

    const char *map_id = argv[2];
    int x = argc >= 4 ? atoi(argv[3]) : 5;
    int y = argc >= 5 ? atoi(argv[4]) : 4;

    /* Prefer cmd bus so file-menu/map-picker share one path */
    if (inbox[0]) {
        FILE *f = fopen(inbox, "a");
        if (f) {
            fprintf(f, "SWITCH_MAP:%s:%d:%d\n", map_id, x, y);
            fclose(f);
            printf("ENQUEUE SWITCH_MAP:%s:%d:%d\n", map_id, x, y);
            return 0;
        }
    }

    const char *inst = getenv("PRISC_INSTALL_ROOT");
    char bin[PATH_BUF];
    if (inst && inst[0])
        snprintf(bin, sizeof(bin), "%s/ops/+x/muta_map_io.+x", inst);
    else
        snprintf(bin, sizeof(bin), "%s/ops/+x/muta_map_io.+x", sess);
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' switch '%s' '%s' %d %d", bin, sess, map_id, x, y);
    return system(cmd) == 0 ? 0 : 1;
}
