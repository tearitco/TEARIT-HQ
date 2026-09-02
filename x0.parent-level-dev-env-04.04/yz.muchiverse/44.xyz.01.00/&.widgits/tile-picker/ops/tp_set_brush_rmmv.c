/* tp_set_brush_rmmv - set the armed RMMV-tile brush for the palette
 * picker's "click desktop to place" flow (TILE-SYSTEM-DESIGN.md §4b.3,
 * §6 item 6 - the real, previously-unbuilt gap: everything the design
 * doc needed already existed EXCEPT this arming step + its matching
 * placement op).
 *
 * Deliberately a SEPARATE op from tp_set_brush.c rather than a
 * generalization of it: tp_set_brush.c's brush.txt is a bare glyph
 * string (single line, consumed by the emoji_gen_atlas/emoji_xtract
 * FreeType pipeline). An RMMV tile brush is a real 4-field identity
 * (which sprite.csv to copy + which tileset/category/kind it came
 * from, for future map-save/autotile-recompute use) - different shape,
 * different consumer, same real "duplicate+adapt per payload shape"
 * convention this house already uses (see stats_hq_manager.c matching
 * db manager's shape, or per-domain mr_* event ops).
 *
 * Usage: tp_set_brush_rmmv.+x <widget_state_dir> <sprite_csv_dir> <tileset_key> <category> <kind_label>
 * Writes <widget_state_dir>/brush_rmmv.txt:
 *   sprite_dir=<sprite_csv_dir>
 *   tileset=<tileset_key>
 *   category=<category>
 *   kind_label=<kind_label>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr,
            "Usage: tp_set_brush_rmmv.+x <widget_state_dir> <sprite_csv_dir> <tileset_key> <category> <kind_label>\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *sprite_dir = argv[2];
    const char *tileset_key = argv[3];
    const char *category = argv[4];
    const char *kind_label = argv[5];
    if (!sprite_dir[0] || !tileset_key[0] || !category[0]) return 1;

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/brush_rmmv.txt", wdir);
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "sprite_dir=%s\n", sprite_dir);
    fprintf(f, "tileset=%s\n", tileset_key);
    fprintf(f, "category=%s\n", category);
    fprintf(f, "kind_label=%s\n", kind_label);
    fclose(f);
    printf("ENQUEUE SET_BRUSH_RMMV:%s/%s (%s)\n", tileset_key, category, kind_label);
    return 0;
}
