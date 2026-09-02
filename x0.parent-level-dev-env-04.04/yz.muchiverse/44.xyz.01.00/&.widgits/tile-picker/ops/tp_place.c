/* tp_place - PLACE_TILE on focused mutaclysm map
 * Usage: tp_place.+x <widget_state_dir> <map_id> <x> <y> [glyph]
 * If glyph omitted, uses brush from widget_state/brush.txt or host brush.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (argc < 5) {
        fprintf(stderr, "Usage: tp_place.+x <widget_state_dir> <map_id> <x> <y> [glyph]\n");
        return 1;
    }
    const char *map_id = argv[2];
    int x = atoi(argv[3]);
    int y = atoi(argv[4]);
    char glyph = 0;
    if (argc >= 6) glyph = argv[5][0];

    char focus[PATH_BUF], inbox[PATH_BUF], brush_local[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", argv[1]);
    snprintf(brush_local, sizeof(brush_local), "%s/brush.txt", argv[1]);
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    if (!inbox[0]) {
        fprintf(stderr, "tp_place: no focus\n");
        return 1;
    }
    if (!glyph) {
        FILE *bf = fopen(brush_local, "r");
        char b[8];
        if (bf && fgets(b, sizeof(b), bf)) glyph = b[0];
        if (bf) fclose(bf);
        if (!glyph) glyph = 'T';
    }

    FILE *f = fopen(inbox, "a");
    if (!f) return 1;
    fprintf(f, "PLACE_TILE:%s:%d:%d:%c\n", map_id, x, y, glyph);
    fclose(f);
    printf("ENQUEUE PLACE_TILE:%s:%d:%d:%c\n", map_id, x, y, glyph);
    return 0;
}
