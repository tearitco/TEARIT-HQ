/* blank_page_op - write a BLANK editor page (cells.txt) of a given size for the page-size test (step 0 of the tear-it co plan, 2026-11-04).
 *   blank_page_op <template_cells.txt> <out_cells.txt> <width> <height> [depth]
 * Keeps the template's header lines (tile_px, atlas_cols, atlas_tiles, rock_slot ...), replaces width= / height=, then writes H rows of W flat floor cells "1,0,<depth>" (floor tile 1, no wall, <depth> hung blocks under each cell, default 1; the engine honours 1..16 and ignores other values: z-limit test).
 * No tiles beyond the floor, no pets, no events: the page exists only to see whether the engine holds a page of that size. Prints "cells=<W*H> row_bytes=<n>" (the engine reads a row into a 16384-byte line buffer: row_bytes must stay under it).
 * Pure text, no Python; one process, runs once per size. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc != 5 && argc != 6) { fprintf(stderr, "usage: %s <template_cells.txt> <out_cells.txt> <width> <height> [depth]\n", argv[0]); return 2; }
    int w = atoi(argv[3]), h = atoi(argv[4]), dp = argc == 6 ? atoi(argv[5]) : 1; if (dp < 0 || dp > 999) { fprintf(stderr, "bad depth\n"); return 2; }
    char tok[24]; snprintf(tok, sizeof tok, "1,0,%d", dp); if (w < 1 || h < 1 || w > 4000 || h > 4000) { fprintf(stderr, "bad size\n"); return 2; }
    FILE *t = fopen(argv[1], "r"); if (!t) { perror(argv[1]); return 1; }
    FILE *o = fopen(argv[2], "w"); if (!o) { perror(argv[2]); return 1; }
    char ln[16384]; int saw = 0;
    while (fgets(ln, sizeof ln, t)) {
        if (!strncmp(ln, "width=", 6)) fprintf(o, "width=%d\n", w);
        else if (!strncmp(ln, "height=", 7)) fprintf(o, "height=%d\n", h);
        else if (!strncmp(ln, "cells", 5) && ln[5] != '=' ) { fputs("cells\n", o); saw = 1; break; }
        else fputs(ln, o);
    }
    fclose(t); if (!saw) { fprintf(stderr, "template has no 'cells' line\n"); return 1; }
    long rb = 0; for (int y = 0; y < h; y++) { long n = 0; for (int x = 0; x < w; x++) n += fprintf(o, x ? " %s" : "%s", tok); fputc('\n', o); rb = n + 1; }
    fclose(o); printf("cells=%ld row_bytes=%ld\n", (long)w * h, rb); return rb < 16384 ? 0 : 3;
}
