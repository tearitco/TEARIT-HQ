/* legend_page_op - write an editor page (cells.txt) from a small text LEGEND (tear-it co plan, step 2, 2026-11-04). One character per cell; the legend says which tile token each character means.
 *   legend_page_op <template_cells.txt> <legend.txt> <out_cells.txt>
 * legend.txt, one directive per line:  `L <char> <token>`  (e.g. `L # 1,3,1` a wall cell, `L . 1,0,1` floor, `L ~ 0,0,0` empty/water),  `MAP` then one row per line (all rows the same width). Lines starting with # are comments.
 * The template's header (tile_px, atlas_cols, atlas_tiles, rock_slot ...) is copied; width= / height= come from the map. Output is deterministic: same legend in, byte-identical cells.txt out (no timestamps, no ordering from a hash table).
 * Refuses (exit 3, line and column on stdout): a character not in the legend, ragged rows, a row past the engine's 16384-byte line buffer, an empty map. Pure text, no Python. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXL 128
#define MAXR 4000
static char lc[MAXL]; static char lt[MAXL][24]; static int nl;
static char *rows[MAXR]; static int nr;
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: %s <template_cells.txt> <legend.txt> <out_cells.txt>\n", argv[0]); return 2; }
    FILE *g = fopen(argv[2], "r"); if (!g) { perror(argv[2]); return 1; } char ln[8192]; int in_map = 0, lineno = 0;
    while (fgets(ln, sizeof ln, g)) { lineno++; ln[strcspn(ln, "\r\n")] = 0;
        if (in_map) { if (ln[0] == 0) continue; if (nr >= MAXR) { printf("REFUSED: more than %d rows\n", MAXR); return 3; } rows[nr++] = strdup(ln); continue; }
        if (ln[0] == '#' || ln[0] == 0) continue;
        if (!strcmp(ln, "MAP")) { in_map = 1; continue; }
        if (ln[0] == 'L' && ln[1] == ' ' && ln[2] && ln[3] == ' ') { if (nl >= MAXL) { printf("REFUSED: too many legend entries\n"); return 3; } for (int i = 0; i < nl; i++) if (lc[i] == ln[2]) { printf("REFUSED: legend line %d: '%c' defined twice\n", lineno, ln[2]); return 3; } if (strlen(ln + 4) > 22 || !ln[4]) { printf("REFUSED: legend line %d: token must be 1..22 characters\n", lineno); return 3; } lc[nl] = ln[2]; memcpy(lt[nl], ln + 4, strlen(ln + 4) + 1); nl++; continue; }
        printf("REFUSED: legend line %d not understood: %s\n", lineno, ln); return 3; }
    fclose(g); if (!nr) { printf("REFUSED: empty map\n"); return 3; }
    int w = (int)strlen(rows[0]); long rb = 0;
    for (int y = 0; y < nr; y++) { if ((int)strlen(rows[y]) != w) { printf("REFUSED: row %d has width %d, row 1 has %d\n", y + 1, (int)strlen(rows[y]), w); return 3; }
        for (int x = 0; x < w; x++) { int f = 0; for (int i = 0; i < nl; i++) if (lc[i] == rows[y][x]) { f = 1; rb += (long)strlen(lt[i]) + 1; } if (!f) { printf("REFUSED: row %d column %d: '%c' is not in the legend\n", y + 1, x + 1, rows[y][x]); return 3; } } if (rb > 16383) { printf("REFUSED: row %d is %ld bytes, the engine reads rows through a 16384-byte buffer\n", y + 1, rb); return 3; } rb = 0; }
    FILE *t = fopen(argv[1], "r"); if (!t) { perror(argv[1]); return 1; } FILE *o = fopen(argv[3], "w"); if (!o) { perror(argv[3]); return 1; } int saw = 0;
    while (fgets(ln, sizeof ln, t)) { if (!strncmp(ln, "width=", 6)) fprintf(o, "width=%d\n", w); else if (!strncmp(ln, "height=", 7)) fprintf(o, "height=%d\n", nr); else if (!strncmp(ln, "cells", 5) && ln[5] != '=') { fputs("cells\n", o); saw = 1; break; } else fputs(ln, o); }
    fclose(t); if (!saw) { printf("REFUSED: template has no 'cells' line\n"); return 3; }
    for (int y = 0; y < nr; y++) { for (int x = 0; x < w; x++) { for (int i = 0; i < nl; i++) if (lc[i] == rows[y][x]) { fputs(lt[i], o); break; } fputc(x + 1 < w ? ' ' : '\n', o); } }
    fclose(o); printf("PAGE %dx%d legend=%d\n", w, nr, nl); return 0;
}
