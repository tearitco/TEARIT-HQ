/* tp_import_from_desktop - the missing reverse direction of
 * tp_place_desktop.c: take a tile package that's already living on
 * #.desktop/tiles/<name>/ and place it onto a focused map, the same way
 * tp_place.c does for a raw glyph.
 *
 * Usage: tp_import_from_desktop.+x <widget_state_dir> <package_dir> <map_id> <x> <y>
 *
 * Reads <package_dir>/glyph.txt (whatever glyph tp_place_desktop or
 * tp_desktop_window wrote), then appends PLACE_TILE to the same
 * focus.txt-resolved inbox tp_place.c already uses - so the map-side
 * consumer (mutaclysm's muta_widget_cmds.+x) needs no changes at all.
 * Does NOT delete the desktop package - importing is non-destructive by
 * default, matching #.desktop/README.txt's own "desktop is outside the
 * live world until imported" framing (the package can be imported onto
 * more than one map, or left as a living desktop window, independently).
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
    if (argc < 6) {
        fprintf(stderr,
            "Usage: tp_import_from_desktop.+x <widget_state_dir> <package_dir> <map_id> <x> <y>\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *package_dir = argv[2];
    const char *map_id = argv[3];
    int x = atoi(argv[4]);
    int y = atoi(argv[5]);

    char glyph_path[PATH_BUF], glyph_line[64];
    snprintf(glyph_path, sizeof(glyph_path), "%s/glyph.txt", package_dir);
    FILE *gf = fopen(glyph_path, "r");
    if (!gf || !fgets(glyph_line, sizeof(glyph_line), gf)) {
        fprintf(stderr, "tp_import_from_desktop: cannot read %s\n", glyph_path);
        if (gf) fclose(gf);
        return 1;
    }
    fclose(gf);
    glyph_line[strcspn(glyph_line, "\r\n")] = '\0';
    char glyph = glyph_line[0];
    if (glyph < 32 || glyph > 126) {
        fprintf(stderr, "tp_import_from_desktop: bad glyph in package\n");
        return 1;
    }

    char focus[PATH_BUF], inbox[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", wdir);
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    if (!inbox[0]) {
        fprintf(stderr, "tp_import_from_desktop: no focus\n");
        return 1;
    }

    FILE *f = fopen(inbox, "a");
    if (!f) return 1;
    fprintf(f, "PLACE_TILE:%s:%d:%d:%c\n", map_id, x, y, glyph);
    fclose(f);
    printf("ENQUEUE PLACE_TILE:%s:%d:%d:%c (from desktop package %s)\n",
           map_id, x, y, glyph, package_dir);
    return 0;
}
