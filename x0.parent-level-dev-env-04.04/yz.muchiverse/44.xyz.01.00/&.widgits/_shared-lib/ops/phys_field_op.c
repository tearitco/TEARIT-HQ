/* phys_field_op - run the shared gravity-field body step (../phys_field.c) over a scene file for N ticks and print the trace (harness driver and a way to try a physics setup without a game).
 *
 * Usage: phys_field_op <scene.pdl> <ticks>
 * Scene rows (pipe rows, # comments, all numbers integers, Q8 = 1/256 cell):
 *   ROW   | y | ....##..          the tile grid, '#' solid, anything else empty (row 0 on top; the same rows a Teario level file has)
 *   TUNE  | name | value          gravity jump_v run_max accel friction fall_max hyst coyote buffer
 *   FIELD | id | plane | dx dy | g                      one constant pull (0 1 = down); g 0 = the tune gravity
 *   FIELD | id | point | cx cy | reach | axis or radial | g
 *   BODY  | x | y | w | h        Q8
 *   INPUT | tick | mask          1 left, 2 right, 4 jump; held until the next INPUT row (a latch)
 * Output: one line per tick `T <n> x=.. y=.. vx=.. vy=.. u=<ux>,<uy> f=<field> g=<grounded>`, then `HASH <fnv1a-64 of the trace>` (a run is reproducible byte for byte).
 * Exit: 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../phys_field.c"
#define MAXR 256
#define MAXW 256
#define MAXF 16
#define MAXI 256
static char rows[MAXR][MAXW]; static int nrows = 0, ncols = 0;
static int solid_at(void *ctx, long cx, long cy) { (void)ctx; if (cy < 0 || cy >= nrows || cx < 0 || cx >= ncols) return 0; return rows[cy][cx] == '#'; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: phys_field_op <scene.pdl> <ticks>\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "phys_field_op: cannot open scene\n"); return 2; }
    int ticks = atoi(argv[2]); PF_Tune tune = pf_default_tune(); PF_Field fl[MAXF]; int nf = 0; PF_Body b; memset(&b, 0, sizeof b); b.w = 200; b.h = 200; b.field = -1;
    struct { int tick, mask; } in[MAXI]; int ni = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int n = 0; char *s = p; while (n < 8) { c[n++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < n; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "ROW") && n >= 3) { int y = atoi(c[1]); if (y < 0 || y >= MAXR) { fprintf(stderr, "bad ROW\n"); return 2; } snprintf(rows[y], MAXW, "%s", c[2]); if (y + 1 > nrows) nrows = y + 1; if ((int)strlen(c[2]) > ncols) ncols = (int)strlen(c[2]); }
        else if (!strcmp(c[0], "TUNE") && n >= 3) { long v = atol(c[2]); const char *k = c[1];
            if (!strcmp(k, "gravity")) tune.gravity = v; else if (!strcmp(k, "jump_v")) tune.jump_v = v; else if (!strcmp(k, "run_max")) tune.run_max = v; else if (!strcmp(k, "accel")) tune.accel = v;
            else if (!strcmp(k, "friction")) tune.friction = v; else if (!strcmp(k, "fall_max")) tune.fall_max = v; else if (!strcmp(k, "hyst")) tune.hyst = v; else if (!strcmp(k, "coyote")) tune.coyote = (int)v; else if (!strcmp(k, "buffer")) tune.buffer = (int)v;
            else { fprintf(stderr, "unknown TUNE %s\n", k); return 2; } }
        else if (!strcmp(c[0], "FIELD") && n >= 4 && nf < MAXF) { PF_Field *q = &fl[nf]; memset(q, 0, sizeof *q); q->id = atoi(c[1]);
            if (!strcmp(c[2], "plane")) { q->kind = 0; sscanf(c[3], "%d %d", &q->dx, &q->dy); if (n >= 5) q->g = atol(c[4]); }
            else if (!strcmp(c[2], "point") && n >= 6) { q->kind = 1; sscanf(c[3], "%ld %ld", &q->cx, &q->cy); q->reach = atol(c[4]); q->dirmode = !strcmp(c[5], "radial"); if (n >= 7) q->g = atol(c[6]); }
            else { fprintf(stderr, "bad FIELD\n"); return 2; } nf++; }
        else if (!strcmp(c[0], "BODY") && n >= 5) { b.x = atol(c[1]); b.y = atol(c[2]); b.w = atol(c[3]); b.h = atol(c[4]); }
        else if (!strcmp(c[0], "INPUT") && n >= 3 && ni < MAXI) { in[ni].tick = atoi(c[1]); in[ni].mask = atoi(c[2]); ni++; }
    }
    fclose(f);
    uint64_t h = 1469598103934665603ULL; int mask = 0, ii = 0; char out[256];
    for (int t = 1; t <= ticks; t++) {
        while (ii < ni && in[ii].tick <= t) { mask = in[ii].mask; ii++; }
        pf_step(&b, &tune, fl, nf, mask, solid_at, NULL);
        int m = snprintf(out, sizeof out, "T %d x=%ld y=%ld vx=%ld vy=%ld u=%d,%d f=%d g=%d\n", t, b.x, b.y, b.vx, b.vy, b.ux, b.uy, b.field, b.grounded);
        fputs(out, stdout); for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
