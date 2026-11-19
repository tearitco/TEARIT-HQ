/* rule_fireball_op - run the fireball rule (../rule_fireball.c) over a scene for N ticks and print the trace. The CLI face of the rule: a harness runs it alone, an event page / Blueprintz calls it by name (registry command rule_fireball),
 * an FSM or an AI tool call can discover it with --describe. The game's tick loop calls the SAME functions in-process.
 *
 * Usage: rule_fireball_op <scene.pdl> <ticks>      |      rule_fireball_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   ROW     | y | ....##..          the tile grid, '#' solid (row 0 on top); cells outside the grid are empty
 *   RULE    | fireball_speed | N    Q8 per tick          RULE | fireball_life | N  ticks      RULE | fireball_max | N  1 to 8 shots at once      RULE | fireball_score | N   (ALL FOUR REQUIRED: the numbers are data)
 *   SHOOTER | x | y | w | h | facing | armed     Q8 box; facing -1 or 1; armed 0 or 1 (an unarmed shooter spawns nothing)
 *   TARGET  | x | y | w | h | alive    a still walker (alive 0 = already dead, shots pass through it); up to 8
 *   FIRE    | tick                      the fire input on that tick (any number of rows)
 * Output: one line per tick `T <n> shots=<alive> kills=<total> score=<total> shot=x,y,life;... targets=<alive flags>` then `HASH <fnv1a-64 of the trace>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_fireball.c"
#define MAXR 256
#define MAXW 256
#define MAXS 8
static char rows[MAXR][MAXW]; static int nrows = 0, ncols = 0;
static int solid_at(void *ctx, long cx, long cy) { (void)ctx; if (cy < 0 || cy >= nrows || cx < 0 || cx >= ncols) return 0; return rows[cy][cx] == '#'; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_fireball_op: IN scene rows ROW, RULE fireball_speed/fireball_life/fireball_max/fireball_score, SHOOTER x y w h facing armed, TARGET x y w h alive, FIRE tick; ticks | OUT per-tick trace `T n shots kills score shot=x,y,life targets` and HASH | RULE keys: fireball_speed fireball_life fireball_max fireball_score | module: _shared-lib/rule_fireball.c"); return 0; }
    if (argc < 3) { fprintf(stderr, "usage: rule_fireball_op <scene.pdl> <ticks>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_fireball_op: cannot open scene\n"); return 2; }
    int ticks = atoi(argv[2]); FB_Rule r = { -1, -1, -1, -1 }; PF_Body sh; memset(&sh, 0, sizeof sh); int facing = 1, armed = 0; RP_Walker tg[MAXS]; memset(tg, 0, sizeof tg); int nt = 0; int fire[256], nfire = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int n = 0; char *s = p; while (n < 8) { c[n++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < n; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "ROW") && n >= 3) { int y = atoi(c[1]); if (y < 0 || y >= MAXR) { fprintf(stderr, "bad ROW\n"); return 2; } snprintf(rows[y], MAXW, "%s", c[2]); if (y + 1 > nrows) nrows = y + 1; if ((int)strlen(c[2]) > ncols) ncols = (int)strlen(c[2]); }
        else if (!strcmp(c[0], "RULE") && n >= 3) { long v = atol(c[2]); if (!strcmp(c[1], "fireball_speed")) r.speed = v; else if (!strcmp(c[1], "fireball_life")) r.life = (int)v; else if (!strcmp(c[1], "fireball_max")) r.max = (int)v; else if (!strcmp(c[1], "fireball_score")) r.score = v; else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "SHOOTER") && n >= 7) { sh.x = atol(c[1]); sh.y = atol(c[2]); sh.w = atol(c[3]); sh.h = atol(c[4]); facing = atoi(c[5]) < 0 ? -1 : 1; armed = atoi(c[6]) != 0; }
        else if (!strcmp(c[0], "TARGET") && n >= 6 && nt < MAXS) { tg[nt].b.x = atol(c[1]); tg[nt].b.y = atol(c[2]); tg[nt].b.w = atol(c[3]); tg[nt].b.h = atol(c[4]); tg[nt].alive = atoi(c[5]) != 0; nt++; }
        else if (!strcmp(c[0], "FIRE") && n >= 2 && nfire < 256) fire[nfire++] = atoi(c[1]);
    }
    fclose(f);
    if (r.speed < 1 || r.life < 1 || r.max < 1 || r.max > MAXS || r.score < 0) { fprintf(stderr, "rule_fireball_op: the scene needs RULE fireball_speed, fireball_life, fireball_max (1 to %d) and fireball_score: the numbers are data, there is no default\n", MAXS); return 2; }
    FB_Shot shots[MAXS]; memset(shots, 0, sizeof shots); uint64_t h = 1469598103934665603ULL; long kills = 0, score = 0; char out[600];
    for (int t = 1; t <= ticks; t++) {
        for (int i = 0; i < nfire; i++) if (fire[i] == t && armed) fb_spawn(shots, MAXS, &r, &sh, facing);
        int k = fb_step(shots, MAXS, tg, nt, solid_at, NULL); kills += k; score += (long)k * r.score;
        int alive = 0; for (int i = 0; i < MAXS; i++) if (shots[i].alive) alive++;
        int m = snprintf(out, sizeof out, "T %d shots=%d kills=%ld score=%ld shot=", t, alive, kills, score);
        for (int i = 0; i < MAXS; i++) if (shots[i].alive) m += snprintf(out + m, sizeof out - m, "%ld,%ld,%d;", shots[i].x, shots[i].y, shots[i].life);
        m += snprintf(out + m, sizeof out - m, " targets="); for (int i = 0; i < nt; i++) m += snprintf(out + m, sizeof out - m, "%d", tg[i].alive); m += snprintf(out + m, sizeof out - m, "\n");
        fputs(out, stdout); for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
