/* sheet_dups_op: which tiles of a sheet are exact repeats (D34: "some of those arches look identical").
 * usage: sheet_dups_op <sheet.png> [tile=48]
 * Prints one line per group of 2+ identical NON-EMPTY tiles ("col,row col,row ..."), then a summary:
 * tiles, non_empty, unique, repeated (non_empty - unique). A tile is empty when every pixel has alpha 0 (or is pure black on an opaque sheet).
 * Read-only, no files written. Exit 0 always unless the file cannot be read (exit 2). */
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    int w, h, c, t = 48, tx, ty, i, j, n = 0, nonempty = 0, uniq = 0;
    unsigned char *px;
    int *col, *row, *grp;
    if (argc < 2) { fprintf(stderr, "usage: sheet_dups_op <sheet.png> [tile=48]\n"); return 2; }
    if (argc > 2) t = atoi(argv[2]);
    if (t < 4) t = 48;
    px = stbi_load(argv[1], &w, &h, &c, 4); if (!px) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    tx = w / t; ty = h / t; col = calloc((size_t)tx * ty, sizeof(int)); row = calloc((size_t)tx * ty, sizeof(int)); grp = calloc((size_t)tx * ty, sizeof(int));
    for (j = 0; j < ty; j++) for (i = 0; i < tx; i++) {
        int x, y, any = 0; for (y = 0; y < t && !any; y++) for (x = 0; x < t; x++) { unsigned char *q = px + ((size_t)(j * t + y) * w + i * t + x) * 4; if (q[3]) { any = 1; break; } }
        if (!any) continue;
        col[n] = i; row[n] = j; n++;
    }
    nonempty = n;
    for (i = 0; i < n; i++) grp[i] = -1;
    for (i = 0; i < n; i++) {
        int first = 1, y;
        if (grp[i] != -1) continue;
        grp[i] = i; uniq++;
        for (j = i + 1; j < n; j++) {
            int same = 1;
            if (grp[j] != -1) continue;
            for (y = 0; y < t && same; y++) same = !memcmp(px + ((size_t)(row[i] * t + y) * w + col[i] * t) * 4, px + ((size_t)(row[j] * t + y) * w + col[j] * t) * 4, (size_t)t * 4);
            if (!same) continue;
            grp[j] = i;
            if (first) { printf("repeat %d,%d", col[i], row[i]); first = 0; }
            printf(" %d,%d", col[j], row[j]);
        }
        if (!first) printf("\n");
    }
    printf("tiles=%d non_empty=%d unique=%d repeated=%d\n", tx * ty, nonempty, uniq, nonempty - uniq);
    return 0;
}
