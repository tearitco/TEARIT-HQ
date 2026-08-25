/* tp_set_brush - set paint brush for focused mutaclysm (cmd bus SET_BRUSH)
 * Usage: tp_set_brush.+x <widget_state_dir> <glyph>
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

/* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
 * tho"): glyph widened from a single ASCII char to a real UTF-8 string -
 * chtpm_rgb_render's on-demand emoji rendering already handles any real
 * emoji character, this op just needed to stop clamping it to 32-126. */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tp_set_brush.+x <widget_state_dir> <glyph>\n");
        return 1;
    }
    const char *g = argv[2];
    if (!g[0]) return 1;

    char focus[PATH_BUF], inbox[PATH_BUF], brush_local[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", argv[1]);
    snprintf(brush_local, sizeof(brush_local), "%s/brush.txt", argv[1]);
    FILE *bf = fopen(brush_local, "w");
    if (bf) { fprintf(bf, "%s\n", g); fclose(bf); }

    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    if (!inbox[0]) {
        fprintf(stderr, "tp_set_brush: no focus inbox\n");
        return 1;
    }
    FILE *f = fopen(inbox, "a");
    if (!f) return 1;
    fprintf(f, "SET_BRUSH:%s\n", g);
    fclose(f);
    printf("ENQUEUE SET_BRUSH:%s\n", g);
    return 0;
}
