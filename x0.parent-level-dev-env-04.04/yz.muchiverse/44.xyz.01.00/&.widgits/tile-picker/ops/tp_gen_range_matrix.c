/* tp_gen_range_matrix - REAL, NEW 2026-09-30, direct instruction ("i
 * think placer should actually read placement layout from an external
 * matrix.txt of '#' symbols to decide its shape instead of being
 * hardcoded... an op can write that based on range of character...
 * its like a writer/renderer architecture"): this op is the WRITER.
 * tp_arm_placer_rmmv.c (the RENDERER) has zero shape math in it - it
 * only ever reads whatever '#'/'.' grid already exists in the matrix
 * file this op produces, same manager-writes/renderer-reads split
 * CENTROID_GOLD_STD.md already uses everywhere else in this house.
 *
 * Shape logic lives here, once, so it can be swapped later (a square,
 * a plus, a hand-authored irregular shape) without touching the
 * renderer at all - just point it at a different matrix file, or
 * teach this one op a second shape and a --shape flag.
 *
 * v1 shape: diamond (Chebyshev box -> Manhattan distance), matching
 * typical tactics-game move ranges - direct ask. Radius comes from
 * desk_grid.pdl's own `GRID | move_view_range` key (same key
 * tp_arm_placer_rmmv.c already reads for its bounding-box sizing -
 * this op duplicates that one small read rather than sharing a header,
 * this file family's own established convention - see
 * tp_arm_placer_rmmv.c's own load_grid_pdl_options() comment for why),
 * or TP_VIEW_RANGE env override for one real call (matches the
 * renderer's own existing override convention).
 *
 * Usage: tp_gen_range_matrix.+x <desktop_root> <out_file>
 * Writes an odd-sized (2*radius+1) square grid, one line per row,
 * '#' = in range (Manhattan distance from center <= radius),
 * '.' = out of range. radius<=0 writes a single '#' (a 1x1 "just this
 * cell" range) rather than an empty file - there's no useful "matrix
 * for zero cells" case worth representing differently. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

static int read_move_view_range(const char *desktop_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/desk_grid.pdl", desktop_root);
    FILE *f = fopen(path, "r");
    int range = 3; /* same built-in default tp_arm_placer_rmmv.c uses */
    if (!f) return range;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line, *tag, *key, *val, *nl;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "GRID", 4) != 0) continue;
        tag = strtok(p, "|");
        key = strtok(NULL, "|");
        val = strtok(NULL, "|");
        if (!tag || !key || !val) continue;
        while (*key == ' ' || *key == '\t') key++;
        { char *e = key + strlen(key); while (e > key && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0'; }
        while (*val == ' ' || *val == '\t') val++;
        nl = strchr(val, '\n'); if (nl) *nl = '\0';
        { char *e = val + strlen(val); while (e > val && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = '\0'; }
        if (strcmp(key, "move_view_range") == 0) {
            int n = atoi(val);
            if (n >= 0) range = n;
        }
    }
    fclose(f);
    return range;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tp_gen_range_matrix.+x <desktop_root> <out_file>\n");
        return 1;
    }
    const char *desktop_root = argv[1];
    const char *out_file = argv[2];

    int radius = read_move_view_range(desktop_root);
    const char *ov = getenv("TP_VIEW_RANGE");
    if (ov && ov[0]) { int n = atoi(ov); if (n >= 0) radius = n; }
    if (radius < 0) radius = 0;

    FILE *f = fopen(out_file, "w");
    if (!f) {
        fprintf(stderr, "tp_gen_range_matrix: cannot write %s\n", out_file);
        return 1;
    }

    int dim = 2 * radius + 1;
    for (int r = 0; r < dim; r++) {
        int dr = r - radius;
        for (int c = 0; c < dim; c++) {
            int dc = c - radius;
            int dist = (dc < 0 ? -dc : dc) + (dr < 0 ? -dr : dr);
            fputc(dist <= radius ? '#' : '.', f);
        }
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}
