/* dsk_label_probe_op - numbers out of a Teropoly play desk (cells.txt + cells.rgba) so a harness case can check the board LABELS without Python and without looking (2026-11-09; case dsk_labels).
 * It composites the page itself (cell tile -> atlas pixels, exactly what `deskopoly preview` and the engine do) and counts pixels of two EXACT colours (the case sets odd label colours no texture contains).
 *   dsk_label_probe_op ring <desk> <n> <block> <px> <base_tiles> <text rrggbb> <outline rrggbb> [inset]
 *        one row per ring tile i (tiles = 4 x (n - 1), clockwise from the top-left block): `T <i> <bx> <by> new=<cells whose atlas index >= base_tiles> distinct=<1 if those indexes are all different> text=<px> outline=<px>
 *        margin=<smallest distance of any text pixel to the edge of the tile block, minus the optional inset (a control: inset 3 makes the check fail where text is nearer than 3 px), `none` if no text> bbox=<x0,y0,x1,y1 of the text inside the block>`; then `SUM tiles=<n> with_text=<tiles with text> text_in_tiles=<sum>
 *        text_outside=<text pixels of the page that are NOT inside a ring tile block> outline_in_tiles=<..> outline_outside=<..> max_new_per_tile=<..> min_new_per_tile=<..>`; text_outside=0 proves no ink fell outside a ring tile block.
 *   dsk_label_probe_op area <desk> <bx> <by> <block> <px>        `AREA <hash>`: FNV-1a of the composited block, to compare two tiles / two desks
 *   dsk_label_probe_op bg <desk> <base.rgba> <cx> <cy> <base_tile> <px> <text> <outline>     one cell: pixels that are not label ink equal to the pixels of plain atlas tile <base_tile> (same=) or not (differ=); `ink=` the label pixels. A cell
 *        whose label tile still shows its plain floor / owner marker / house has differ=0.
 * Exit 0 with output; 2 on a usage error or an unreadable desk. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/dsk_label_probe_op.+x dsk_label_probe_op.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int W, H, PX, COLS, TILES; static int *CT; static unsigned char *AT; static long ASZ;
static unsigned char *slurp(const char *p, long *n) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f); unsigned char *b = malloc((size_t)*n + 1); if (!b || fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); return NULL; } fclose(f); b[*n] = 0; return b; }
static int load(const char *dir) {
    char p[4400]; long n; snprintf(p, sizeof p, "%s/cells.txt", dir); unsigned char *t = slurp(p, &n); if (!t) return 0; char *s = (char *)t, *ln = strtok(s, "\n"); int y = -1;
    while (ln) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) H = atoi(ln + 7); else if (!strncmp(ln, "tile_px=", 8)) PX = atoi(ln + 8); else if (!strncmp(ln, "atlas_cols=", 11)) COLS = atoi(ln + 11); else if (!strncmp(ln, "atlas_tiles=", 12)) TILES = atoi(ln + 12);
        else if (!strncmp(ln, "cells", 5)) { if (W < 1 || H < 1) return 0; CT = calloc((size_t)W * H, sizeof(int)); y = 0; } else if (y >= 0 && y < H) { const char *q = ln; for (int x = 0; x < W; x++) { char *e; CT[y * W + x] = (int)strtol(q, &e, 10); q = e; if (*q == ',') { strtol(q + 1, &e, 10); q = e; } while (*q == ' ') q++; } y++; }
        ln = strtok(NULL, "\n"); }
    if (y != H) return 0;
    snprintf(p, sizeof p, "%s/cells.rgba", dir); AT = slurp(p, &ASZ); if (!AT) return 0;
    return (size_t)ASZ >= (size_t)((TILES + COLS - 1) / COLS) * PX * COLS * PX * 4;
}
static const unsigned char *apx(int tile, int x, int y) { return AT + ((size_t)((tile / COLS) * PX + y) * COLS * PX + (size_t)(tile % COLS) * PX + x) * 4; }
static const unsigned char *ppx(int X, int Y) { return apx(CT[(Y / PX) * W + X / PX], X % PX, Y % PX); }
static int is_col(const unsigned char *p, unsigned c) { return p[0] == (c >> 16 & 255) && p[1] == (c >> 8 & 255) && p[2] == (c & 255) && p[3] == 255; }
static void ring_xy(int idx, int n, int *bx, int *by) { int s = n - 1; if (idx <= s) { *bx = idx; *by = 0; } else if (idx <= 2 * s) { *bx = s; *by = idx - s; } else if (idx <= 3 * s) { *bx = s - (idx - 2 * s); *by = s; } else { *bx = 0; *by = s - (idx - 3 * s); } }
int main(int argc, char **argv) {
    if (argc < 3 || !load(argv[2])) { fprintf(stderr, "usage: dsk_label_probe_op ring|area|bg <desk> ... (and the desk must be readable)\n"); return 2; }
    if (!strcmp(argv[1], "ring") && argc >= 9) { int n = atoi(argv[3]), B = atoi(argv[4]), px = atoi(argv[5]), base = atoi(argv[6]); unsigned tc = (unsigned)strtoul(argv[7], 0, 16), oc = (unsigned)strtoul(argv[8], 0, 16); int inset = argc >= 10 ? atoi(argv[9]) : 0;
        if (px != PX || n * B != W) { fprintf(stderr, "ring: n x block x px do not match the desk\n"); return 2; }
        int A = B * PX, nt = 4 * (n - 1); long tin = 0, oin = 0, tall = 0, oall = 0; int with = 0, mx = 0, mn = 1 << 30;
        for (int Y = 0; Y < H * PX; Y++) for (int X = 0; X < W * PX; X++) { const unsigned char *p = ppx(X, Y); tall += is_col(p, tc); oall += is_col(p, oc); }
        for (int i = 0; i < nt; i++) { int bx, by; ring_xy(i, n, &bx, &by); long t = 0, o = 0; int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1, margin = 1 << 30, nw = 0, ids[64], dist = 1;
            for (int ky = 0; ky < B; ky++) for (int kx = 0; kx < B; kx++) { int id = CT[(by * B + ky) * W + bx * B + kx]; if (id >= base) { for (int j = 0; j < nw; j++) if (ids[j] == id) dist = 0; if (nw < 64) ids[nw++] = id; } }
            for (int y = 0; y < A; y++) for (int x = 0; x < A; x++) { const unsigned char *p = ppx(bx * A + x, by * A + y); if (is_col(p, oc)) o++; if (is_col(p, tc)) { t++; if (x < x0) x0 = x; if (y < y0) y0 = y; if (x > x1) x1 = x; if (y > y1) y1 = y;
                    int m = x < y ? x : y; int m2 = (A - 1 - x) < (A - 1 - y) ? (A - 1 - x) : (A - 1 - y); if (m2 < m) m = m2; if (m < margin) margin = m; } }
            if (t) printf("T %d %d %d new=%d distinct=%d text=%ld outline=%ld margin=%d bbox=%d,%d,%d,%d\n", i, bx, by, nw, dist, t, o, margin - inset, x0, y0, x1, y1); else printf("T %d %d %d new=%d distinct=%d text=0 outline=%ld margin=none bbox=none\n", i, bx, by, nw, dist, o);
            tin += t; oin += o; if (t) with++; if (nw > mx) mx = nw; if (nw < mn) mn = nw; }
        printf("SUM tiles=%d with_text=%d text_in_tiles=%ld text_outside=%ld outline_in_tiles=%ld outline_outside=%ld max_new_per_tile=%d min_new_per_tile=%d\n", nt, with, tin, tall - tin, oin, oall - oin, mx, mn); return 0; }
    if (!strcmp(argv[1], "area") && argc >= 7) { int bx = atoi(argv[3]), by = atoi(argv[4]), B = atoi(argv[5]), A = B * PX; uint64_t h = 1469598103934665603ULL;
        for (int y = 0; y < A; y++) for (int x = 0; x < A; x++) { const unsigned char *p = ppx(bx * A + x, by * A + y); for (int k = 0; k < 4; k++) { h ^= p[k]; h *= 1099511628211ULL; } }
        printf("AREA %016llx\n", (unsigned long long)h); return 0; }
    if (!strcmp(argv[1], "bg") && argc >= 10) { long bn; unsigned char *base = slurp(argv[3], &bn); if (!base) return 2; int cx = atoi(argv[4]), cy = atoi(argv[5]), bt = atoi(argv[6]); unsigned tc = (unsigned)strtoul(argv[8], 0, 16), oc = (unsigned)strtoul(argv[9], 0, 16); long same = 0, diff = 0, ink = 0;
        for (int y = 0; y < PX; y++) for (int x = 0; x < PX; x++) { const unsigned char *p = ppx(cx * PX + x, cy * PX + y); if (is_col(p, tc) || is_col(p, oc)) { ink++; continue; }
            const unsigned char *q = base + ((size_t)((bt / COLS) * PX + y) * COLS * PX + (size_t)(bt % COLS) * PX + x) * 4; if (!memcmp(p, q, 4)) same++; else diff++; }
        printf("BG same=%ld differ=%ld ink=%ld\n", same, diff, ink); return 0; }
    fprintf(stderr, "dsk_label_probe_op: bad arguments\n"); return 2;
}
