/* worldscope - see a painted desk the way the board engine builds it, without a screenshot (owner 2026-10-23: "get comfortable and make skills inside these worlds ... build whatever toolsets and debugs and dumps you need").
 * It applies the SAME column rules as bv_render_3d.c (maker_load + the grid build of the GPU scene) to a desk's exported files (map.txt, cells.txt, extrusion.pdl):
 *   h  = EXTRUDE value of the glyph when > 1, else 1; a cell with a wall tile and h <= 1 is 3 high       (EXTRUDE 0 is ignored: reported by `warn`)
 *   zoff = the deepest DEPTH row / per-cell depth (cells.txt third token, 1..16)
 *   lo = zoff - depth(glyph); with any per-cell depth: lo = zoff - cell depth;  hi = zoff + h;  solid levels lo .. hi-1
 *   EXTRUDE value < 0 SINKS the glyph's surface by that many levels (hi = zoff + h - sink); a cell with neither tile, or with hi <= lo, is a HOLE (no block) once zoff > 0
 * Usage (desk_dir = .../maps/<book>/<room>):
 *   worldscope <desk_dir> cell <x> <y>              one column: glyph, tiles, h, lo, hi, in levels relative to the floor level (level zoff = 0)
 *   worldscope <desk_dir> row <y> [x0 x1]           side elevation west to east along row y, top level first ('#' solid, '=' the second run under a column, '.' air, '_' hole), glyphs on top
 *   worldscope <desk_dir> col <x> [y0 y1]           the same along column x (north to south)
 *   worldscope <desk_dir> exposed <x> <y> <dx> <dy> how many solid levels of cell (x,y) stand ABOVE its neighbour (x+dx,y+dy): the rows you see on that face
 *   worldscope <desk_dir> warn                      EXTRUDE rows the engine ignores
 * Exit 0 ok, 1 bad desk, 2 usage. Build: sh tools/build_worldscope.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define MW 256
static int W, H, zoff, cdep_any, rock; static char gl[MW][MW]; static int fl[MW][MW], wl[MW][MW], cd[MW][MW], un[MW][MW], ud[MW][MW], uk[MW][MW], zh[128], zs[128], dp[128]; static int ignored[16], nign; static char ignoredg[16];
static int load(const char *d) {
    char p[PATH_MAX + 32], ln[4096]; FILE *f; memset(gl, '.', sizeof gl);
    snprintf(p, sizeof p, "%s/cells.txt", d); if (!(f = fopen(p, "r"))) { fprintf(stderr, "worldscope: no %s\n", p); return 0; }
    int row = -1; while (fgets(ln, sizeof ln, f)) {
        if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) H = atoi(ln + 7); else if (!strncmp(ln, "rock_slot=", 10)) rock = atoi(ln + 10);
        else if (!strncmp(ln, "cells", 5)) row = 0;
        else if (row >= 0 && row < H && row < MW) { int x = 0; for (char *t = strtok(ln, " \n"); t && x < W && x < MW; t = strtok(NULL, " \n"), x++) { int a = 0, b = 0, c = 0, u1 = 0, u2 = 0, u3 = 0; sscanf(t, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &u1, &u2, &u3); fl[row][x] = a; wl[row][x] = b; if (u1 > 0 && u2 > 0 && u2 <= 16 && u3 >= 0 && u3 < u2) { un[row][x] = u1; ud[row][x] = u2; uk[row][x] = u3; if (u2 > zoff) zoff = u2; } if (c > 0 && c <= 16) { cd[row][x] = c; cdep_any = 1; if (c > zoff) zoff = c; } } row++; } }
    fclose(f);
    snprintf(p, sizeof p, "%s/map.txt", d); if ((f = fopen(p, "r"))) { int y = 0; while (y < H && y < MW && fgets(ln, sizeof ln, f)) { for (int x = 0; x < W && x < MW && ln[x] && ln[x] != '\n'; x++) gl[y][x] = ln[x]; y++; } fclose(f); }
    snprintf(p, sizeof p, "%s/extrusion.pdl", d); if ((f = fopen(p, "r"))) { while (fgets(ln, sizeof ln, f)) {
        int isd = strstr(ln, "DEPTH") != NULL; if (!isd && !strstr(ln, "EXTRUDE")) continue; char *bar = strchr(ln, '|'); if (!bar) continue; bar++; while (*bar == ' ') bar++; char g = *bar; char *b2 = strchr(bar, '|'); if (!b2 || g <= 32 || g >= 127) continue;
        char *v = b2 + 1; while (*v == ' ' || *v == '\t') v++; int hv = atoi(v);
        if (isd) { if (hv > 0 && hv <= 16) { dp[(unsigned char)g] = hv; if (hv > zoff) zoff = hv; } }
        else if (!strncmp(v, "sprite", 6)) { }
        else if (hv > 0 && hv <= 64) zh[(unsigned char)g] = hv;
        else if (hv < 0 && hv >= -16) zs[(unsigned char)g] = -hv;      /* a sunk surface (negative EXTRUDE) */
        else if (nign < 16) { ignored[nign] = hv; ignoredg[nign++] = g; } } fclose(f); }
    return W > 0 && H > 0;
}
static void col(int x, int y, int *hole, int *lo, int *hi) {
    unsigned char g = (unsigned char)gl[y][x]; int h = 1; if (g < 128 && zh[g] > 1) h = zh[g]; if (h <= 1 && wl[y][x]) h = 3;
    *lo = zoff - (g < 128 ? dp[g] : 0); *hi = zoff + h - (g < 128 ? zs[g] : 0); if (cdep_any) *lo = zoff - cd[y][x]; *hole = (zoff && !fl[y][x] && !wl[y][x]) || *hi <= *lo;
}
static void elev(const char *d, int along_row, int fixed, int a0, int a1) {
    (void)d; int top = -99, bot = 99, lo, hi, hole;
    for (int a = a0; a <= a1; a++) { int x = along_row ? a : fixed, y = along_row ? fixed : a; col(x, y, &hole, &lo, &hi); if (un[y][x]) { if (zoff - ud[y][x] < bot) bot = zoff - ud[y][x]; } if (hole) continue; if (hi > top) top = hi; if (lo < bot) bot = lo; }
    printf("%s %d, %s %d..%d  (level 0 = the floor level; floor TOP is at +1)\n", along_row ? "row" : "col", fixed, along_row ? "x" : "y", a0, a1);
    printf("       "); for (int a = a0; a <= a1; a++) { int x = along_row ? a : fixed, y = along_row ? fixed : a; printf("%c", gl[y][x]); } printf("\n");
    for (int lv = top - 1; lv >= bot; lv--) { printf("%+4d   ", lv - zoff); for (int a = a0; a <= a1; a++) { int x = along_row ? a : fixed, y = along_row ? fixed : a; col(x, y, &hole, &lo, &hi); { int ux = along_row ? a : fixed, uy = along_row ? fixed : a; int under = un[uy][ux] && lv >= zoff - ud[uy][ux] && lv < zoff + 1 - uk[uy][ux]; putchar(under ? '=' : hole ? '_' : (lv >= lo && lv < hi) ? '#' : '.'); } } putchar('\n'); }
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: worldscope <desk_dir> cell|row|col|exposed|warn ...\n"); return 2; }
    if (!load(argv[1])) return 1; const char *m = argv[2]; int lo, hi, hole;
    if (!strcmp(m, "warn")) { printf("zoff %d, per-cell depth %s, rock_slot %d\n", zoff, cdep_any ? "yes" : "no", rock); for (int i = 0; i < nign; i++) printf("IGNORED by the engine: EXTRUDE %c = %d (only values > 0 count)\n", ignoredg[i], ignored[i]); if (!nign) puts("no ignored EXTRUDE rows"); return 0; }
    if (!strcmp(m, "cell") && argc >= 5) { int x = atoi(argv[3]), y = atoi(argv[4]); if (x < 0 || y < 0 || x >= W || y >= H) return 2; col(x, y, &hole, &lo, &hi);
        printf("cell %d,%d glyph '%c' floor tile %d wall tile %d depth %d zoff %d: %s; solid levels %+d .. %+d (top at %+d)\n", x, y, gl[y][x], fl[y][x], wl[y][x], cd[y][x], zoff, hole ? "HOLE" : "solid", lo - zoff, hi - 1 - zoff, hi - zoff); return 0; }
    if (!strcmp(m, "row") && argc >= 4) { int y = atoi(argv[3]); elev(argv[1], 1, y, argc >= 6 ? atoi(argv[4]) : 0, argc >= 6 ? atoi(argv[5]) : W - 1); return 0; }
    if (!strcmp(m, "col") && argc >= 4) { int x = atoi(argv[3]); elev(argv[1], 0, x, argc >= 6 ? atoi(argv[4]) : 0, argc >= 6 ? atoi(argv[5]) : H - 1); return 0; }
    if (!strcmp(m, "exposed") && argc >= 7) { int x = atoi(argv[3]), y = atoi(argv[4]), dx = atoi(argv[5]), dy = atoi(argv[6]), lo2, hi2, hole2; col(x, y, &hole, &lo, &hi); col(x + dx, y + dy, &hole2, &lo2, &hi2);
        int top2 = hole2 ? lo - 0 : hi2, bottom = hole2 ? lo : (hi2 > lo ? hi2 : lo); int n = hi - bottom; if (n < 0) n = 0; printf("cell %d,%d face toward %d,%d: %d solid level(s) show (top %+d down to %+d; neighbour %s, its top at %+d)\n", x, y, x + dx, y + dy, n, hi - 1 - zoff, bottom - zoff, hole2 ? "is a hole: the face goes down to the hull bottom" : "stands there", top2 - zoff); return 0; }
    fprintf(stderr, "worldscope: bad arguments\n"); return 2;
}
