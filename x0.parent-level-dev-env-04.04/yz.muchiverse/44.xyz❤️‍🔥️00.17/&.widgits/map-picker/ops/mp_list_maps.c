/* mp_list_maps - list maps for focused mutaclysm session (via focus.txt).
 * Usage: mp_list_maps.+x <widget_state_dir>
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
    if (argc < 2) {
        fprintf(stderr, "Usage: mp_list_maps.+x <widget_state_dir>\n");
        return 1;
    }
    char focus[PATH_BUF], live[PATH_BUF], sess[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", argv[1]);
    read_kv(focus, "live_world", live, sizeof(live));
    read_kv(focus, "session_root", sess, sizeof(sess));
    if (!live[0] && sess[0])
        snprintf(live, sizeof(live), "%s/pieces/world_01", sess);
    if (!live[0]) {
        fprintf(stderr, "mp_list_maps: no live_world (focus mutaclysm first)\n");
        return 1;
    }

    const char *inst = getenv("PRISC_INSTALL_ROOT");
    char bin[PATH_BUF];
    if (inst && inst[0])
        snprintf(bin, sizeof(bin), "%s/ops/+x/muta_map_io.+x", inst);
    else if (sess[0])
        snprintf(bin, sizeof(bin), "%s/ops/+x/muta_map_io.+x", sess);
    else {
        fprintf(stderr, "set PRISC_INSTALL_ROOT to mutaclysm install\n");
        return 1;
    }
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' list '%s'", bin, live);
    return system(cmd) == 0 ? 0 : 1;
}
