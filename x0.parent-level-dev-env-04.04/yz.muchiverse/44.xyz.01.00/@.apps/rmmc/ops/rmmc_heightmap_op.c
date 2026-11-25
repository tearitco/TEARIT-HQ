/* rmmc_heightmap_op - the SEED desk of the island -> vox's terrain.heightmap (a generated CONFIG, plan 8, gate G2; D137).
 *   rmmc_heightmap_op build <seed_desk_dir> <heightmap.pdl> <out.heightmap>     write the file (atomic: tmp then rename); prints  land=<columns> runs=<rows> shift=<n>
 *   rmmc_heightmap_op check <seed_desk_dir> <heightmap.pdl> <file.heightmap>    expand the file and compare EVERY column with the desk: every land column exactly once, no sky column, same ground_y, kinds and water_level;
 *                                                                               prints  land=<n> seen=<n> mismatches=<n>; exit 1 when mismatches > 0
 * Reads the desk's cells.txt (tokens floor,wall,depth), map.txt (glyph = height H) and legend.txt (GLYPH rows glyph|H|extrusion, SLOT rows number|name|png); heightmap.pdl (RULE shift, WATER rows) holds every number.
 * Axes (written once, tested by the harness case rmmc_heightmap): desk(x, row) = vox(x, y = height, z = row). The file format is documented in @.apps/vox/ops/vox_op.c (load_heightmap).
 * Exit: 0 ok, 2 usage, 3 a file is missing or unreadable, 4 a bad row or an unknown glyph/slot, 5 a bed below y 0 (raise RULE shift), 1 check found mismatches.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_heightmap_op.+x rmmc_heightmap_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define NM 32
#define MAXD 256
#define MAXS 64
typedef struct { int gy, wl; char kt[NM], kb[NM]; int land; } Col;
static Col C[MAXD][MAXD]; static int W, Hh, shift = -1;
static int gh[256], gh_ok[256]; static char slot_name[MAXS][NM]; static struct { char name[NM], bed[NM]; int depth; } WT[8]; static int nwt;
static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); char *b = malloc((size_t)n + 1); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; } b[n] = 0; fclose(f); return b; }
static int load_cfg(const char *p) {
    FILE *f = fopen(p, "r"); if (!f) return 3; char l[512], *x[8];
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 8);
        if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "shift")) shift = atoi(x[2]);
        else if (n >= 4 && !strcmp(x[0], "WATER") && nwt < 8) { snprintf(WT[nwt].name, NM, "%s", x[1]); WT[nwt].depth = atoi(x[2]); snprintf(WT[nwt].bed, NM, "%s", x[3]); nwt++; } }
    fclose(f); return shift < 0 ? 4 : 0;
}
static int load_desk(const char *dir) {
    char p[1100], l[1024], *x[8]; FILE *f;
    snprintf(p, sizeof p, "%s/legend.txt", dir); if (!(f = fopen(p, "r"))) return 3;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 8);
        if (n >= 4 && !strcmp(x[0], "GLYPH") && strlen(x[1]) == 1) { gh[(unsigned char)x[1][0]] = atoi(x[2]); gh_ok[(unsigned char)x[1][0]] = 1; }
        else if (n >= 3 && !strcmp(x[0], "SLOT") && isdigit((unsigned char)x[1][0])) { int s = atoi(x[1]); if (s > 0 && s < MAXS) snprintf(slot_name[s], NM, "%s", x[2]); } }
    fclose(f);
    snprintf(p, sizeof p, "%s/cells.txt", dir); char *ct = slurp(p); if (!ct) return 3;
    char *map = NULL; snprintf(p, sizeof p, "%s/map.txt", dir); map = slurp(p); if (!map) { free(ct); return 3; }
    char *s = ct; W = Hh = 0; int in_cells = 0, z = 0;
    char *ml = map;
    while (*s) { char *e = strchr(s, '\n'); size_t len = e ? (size_t)(e - s) : strlen(s); char ln[8192]; if (len >= sizeof ln) { free(ct); free(map); return 4; } memcpy(ln, s, len); ln[len] = 0; s += len + (e ? 1 : 0);
        if (!in_cells) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) Hh = atoi(ln + 7); else if (!strcmp(ln, "cells")) { in_cells = 1; if (W < 1 || Hh < 1 || W > MAXD || Hh > MAXD) { free(ct); free(map); return 4; } } continue; }
        if (z >= Hh) break;
        char *me = strchr(ml, '\n'); if (!me || (int)(me - ml) < W) { free(ct); free(map); return 4; }
        char *t = ln; for (int xx = 0; xx < W; xx++) { while (*t == ' ') t++; int fl, wl, dp; if (sscanf(t, "%d,%d,%d", &fl, &wl, &dp) < 2) { free(ct); free(map); return 4; } while (*t && *t != ' ') t++;
            Col *c = &C[z][xx]; memset(c, 0, sizeof *c); unsigned char g = (unsigned char)ml[xx];
            if (g == '.') { c->land = 0; continue; }
            if (!gh_ok[g] || fl < 1 || fl >= MAXS || !slot_name[fl][0] || wl < 0 || wl >= MAXS || (wl && !slot_name[wl][0])) { free(ct); free(map); fprintf(stderr, "bad glyph or slot at %d,%d\n", xx, z); return 4; }
            int H = gh[g]; c->land = 1; const char *fn = slot_name[fl], *wn = wl ? slot_name[wl] : fn; int wi = -1;
            for (int i = 0; i < nwt; i++) if (!strcmp(WT[i].name, fn)) wi = i;
            if (wi >= 0) { c->wl = H - 1 + shift; c->gy = c->wl - WT[wi].depth; snprintf(c->kt, NM, "%s", WT[wi].bed); snprintf(c->kb, NM, "%s", WT[wi].bed); }
            else { c->gy = H - 1 + shift; c->wl = -1; snprintf(c->kt, NM, "%s", fn); snprintf(c->kb, NM, "%s", wn); }
            if (c->gy < 0) { free(ct); free(map); fprintf(stderr, "bed below y 0 at %d,%d (raise RULE shift)\n", xx, z); return 5; } }
        ml = me + 1; z++; }
    free(ct); free(map); return z == Hh ? 0 : 4;
}
static int same(const Col *a, const Col *b) { return a->gy == b->gy && a->wl == b->wl && !strcmp(a->kt, b->kt) && !strcmp(a->kb, b->kb); }
int main(int argc, char **argv) {
    if (argc != 5) { fprintf(stderr, "usage: rmmc_heightmap_op build|check <seed_desk> <heightmap.pdl> <file>\n"); return 2; }
    int r = load_cfg(argv[3]); if (r) { fprintf(stderr, "bad config %s\n", argv[3]); return r; }
    r = load_desk(argv[2]); if (r) { fprintf(stderr, "bad desk %s\n", argv[2]); return r; }
    int land = 0; for (int z = 0; z < Hh; z++) for (int x = 0; x < W; x++) land += C[z][x].land;
    if (!strcmp(argv[1], "build")) {
        char tmp[1200]; snprintf(tmp, sizeof tmp, "%s.tmp", argv[4]); FILE *o = fopen(tmp, "w"); if (!o) return 3; int runs = 0;
        fprintf(o, "# terrain.heightmap - GENERATED by rmmc_heightmap_op from the seed desk (CONFIG: never edit, never written by the game). Format: @.apps/vox/ops/vox_op.c load_heightmap.\nMETA | width | %d\nMETA | height | %d\nMETA | shift | %d\n", W, Hh, shift);
        for (int z = 0; z < Hh; z++) for (int x = 0; x < W; ) { if (!C[z][x].land) { x++; continue; } int e = x + 1; while (e < W && C[z][e].land && same(&C[z][x], &C[z][e])) e++;
            fprintf(o, "RUN | %d | %d | %d | %d | %s | %s | %d\n", z, x, e - x, C[z][x].gy, C[z][x].kt, C[z][x].kb, C[z][x].wl); runs++; x = e; }
        if (fclose(o)) return 3;
        if (rename(tmp, argv[4])) return 3;
        printf("land=%d runs=%d shift=%d\n", land, runs, shift); return 0; }
    if (!strcmp(argv[1], "check")) {
        static int seen[MAXD][MAXD]; static Col H2[MAXD][MAXD]; FILE *f = fopen(argv[4], "r"); if (!f) return 3; char l[512], *x[10]; int bad = 0;
        while (fgets(l, sizeof l, f)) { int n = split(l, x, 10); if (n >= 8 && !strcmp(x[0], "RUN")) { int z = atoi(x[1]), x0 = atoi(x[2]), c = atoi(x[3]);
            if (z < 0 || z >= Hh || x0 < 0 || c < 1 || x0 + c > W) { bad++; continue; }
            for (int xx = x0; xx < x0 + c; xx++) { seen[z][xx]++; H2[z][xx].gy = atoi(x[4]); snprintf(H2[z][xx].kt, NM, "%s", x[5]); snprintf(H2[z][xx].kb, NM, "%s", x[6]); H2[z][xx].wl = atoi(x[7]); } } }
        fclose(f); int nseen = 0;
        for (int z = 0; z < Hh; z++) for (int xx = 0; xx < W; xx++) { nseen += seen[z][xx] ? 1 : 0;
            if (C[z][xx].land ? (seen[z][xx] != 1 || !same(&C[z][xx], &H2[z][xx])) : seen[z][xx] != 0) bad++; }
        printf("land=%d seen=%d mismatches=%d\n", land, nseen, bad); return bad ? 1 : 0; }
    fprintf(stderr, "usage: build|check\n"); return 2;
}
