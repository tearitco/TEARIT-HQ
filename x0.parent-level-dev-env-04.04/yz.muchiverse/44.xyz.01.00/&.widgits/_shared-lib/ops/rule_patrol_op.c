/* rule_patrol_op - run the patrol rule (../rule_patrol.c) over a scene for N ticks and print the trace. The CLI face of the rule: a harness runs it alone, an event page / Blueprintz calls it by name
 * (registry command rule_patrol), an FSM or an AI tool call can discover it with --describe. The game's tick loop calls the SAME function in-process.
 *
 * Usage: rule_patrol_op <scene.pdl> <ticks>      |      rule_patrol_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   ROW    | y | ....##..         the tile grid, '#' solid (row 0 on top)
 *   TUNE   | name | value         the body tune (gravity jump_v run_max accel friction fall_max hyst coyote buffer), default pf_default_tune
 *   RULE   | patrol_speed | N     Q8 cells per tick (REQUIRED: the number is data)
 *   WALKER | x | y | w | h | dir  Q8 box, dir -1 left or 1 right
 * The pull is one plane field straight down. Output: one line per tick `T <n> x=.. y=.. vx=.. dir=.. alive=.. g=..`, then `HASH <fnv1a-64 of the trace>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_patrol.c"
#define MAXR 256
#define MAXW 256
static char rows[MAXR][MAXW]; static int nrows = 0, ncols = 0;
static int solid_at(void *ctx, long cx, long cy) { (void)ctx; if (cy < 0 || cy >= nrows || cx < 0 || cx >= ncols) return 0; return rows[cy][cx] == '#'; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_patrol_op: IN scene rows ROW, TUNE, RULE patrol_speed, WALKER x y w h dir; ticks | OUT per-tick trace `T n x y vx dir alive g` and HASH | RULE keys: patrol_speed | module: _shared-lib/rule_patrol.c"); return 0; }
    if (argc < 3) { fprintf(stderr, "usage: rule_patrol_op <scene.pdl> <ticks>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_patrol_op: cannot open scene\n"); return 2; }
    int ticks = atoi(argv[2]); PF_Tune tune = pf_default_tune(); PF_Field fld; memset(&fld, 0, sizeof fld); fld.dy = 1; long speed = -1; RP_Walker w; memset(&w, 0, sizeof w); w.b.w = 200; w.b.h = 200; w.b.field = -1; w.dir = -1; w.alive = 1; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int n = 0; char *s = p; while (n < 8) { c[n++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < n; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "ROW") && n >= 3) { int y = atoi(c[1]); if (y < 0 || y >= MAXR) { fprintf(stderr, "bad ROW\n"); return 2; } snprintf(rows[y], MAXW, "%s", c[2]); if (y + 1 > nrows) nrows = y + 1; if ((int)strlen(c[2]) > ncols) ncols = (int)strlen(c[2]); }
        else if (!strcmp(c[0], "TUNE") && n >= 3) { long v = atol(c[2]); const char *k = c[1];
            if (!strcmp(k, "gravity")) tune.gravity = v; else if (!strcmp(k, "jump_v")) tune.jump_v = v; else if (!strcmp(k, "run_max")) tune.run_max = v; else if (!strcmp(k, "accel")) tune.accel = v;
            else if (!strcmp(k, "friction")) tune.friction = v; else if (!strcmp(k, "fall_max")) tune.fall_max = v; else if (!strcmp(k, "hyst")) tune.hyst = v; else if (!strcmp(k, "coyote")) tune.coyote = (int)v; else if (!strcmp(k, "buffer")) tune.buffer = (int)v;
            else { fprintf(stderr, "unknown TUNE %s\n", k); return 2; } }
        else if (!strcmp(c[0], "RULE") && n >= 3) { if (!strcmp(c[1], "patrol_speed")) speed = atol(c[2]); else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "WALKER") && n >= 6) { w.b.x = atol(c[1]); w.b.y = atol(c[2]); w.b.w = atol(c[3]); w.b.h = atol(c[4]); w.dir = atoi(c[5]) < 0 ? -1 : 1; }
    }
    fclose(f);
    if (speed < 1) { fprintf(stderr, "rule_patrol_op: the scene needs RULE | patrol_speed | N (N >= 1): the number is data, there is no default\n"); return 2; }
    uint64_t h = 1469598103934665603ULL; char out[256]; long fall_y = (long)(nrows + 2) * PF_Q;
    for (int t = 1; t <= ticks; t++) {
        rp_step(&w, &tune, speed, &fld, 1, solid_at, NULL, fall_y);
        int m = snprintf(out, sizeof out, "T %d x=%ld y=%ld vx=%ld dir=%d alive=%d g=%d\n", t, w.b.x, w.b.y, w.b.vx, w.dir, w.alive, w.b.grounded);
        fputs(out, stdout); for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
