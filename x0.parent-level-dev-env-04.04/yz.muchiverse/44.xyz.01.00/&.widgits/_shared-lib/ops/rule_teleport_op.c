/* rule_teleport_op - evaluate the teleport rule (../rule_teleport.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_teleport, FSM / AI via --describe); the game tick loop calls the SAME function in-process (decision D111).
 *
 * Usage: rule_teleport_op <scene.pdl>      |      rule_teleport_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   RULE  | teleport_fee | N     REQUIRED: the number is data, there is no default
 *   TILE  | teleport | <char>   REQUIRED: the tile character (data, as blocks.pdl BLOCK | teleport | <char>)
 *   HERO  | x | y | w | h | vy | vx   (vy and vx optional, default 0: the rule sets vy to 0 and keeps vx)
 *   SCORE | N              the score before (optional, default 0)
 *   ROW   | text          the level, top row first (up to 32 rows of up to 64 tiles, all of one width)
 * Output: `TELEPORTED <0|1> SCORE <s> FROM <x,y or -> TO <x,y or -> HERO <x,y> VX <vx> VY <vy> LEVEL <rows joined by />` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_teleport.c"
#define MAXR 32
#define STRIDE 65
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_teleport_op: IN scene rows RULE teleport_fee, TILE teleport <char>, HERO x y w h [vy [vx]], SCORE n, ROW text | OUT `TELEPORTED n SCORE s FROM x,y TO x,y HERO x,y VX vx VY vy LEVEL rows` and HASH | RULE keys: teleport_fee | module: _shared-lib/rule_teleport.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_teleport_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_teleport_op: cannot open scene\n"); return 2; }
    TP_Rule r = { -1 }; PF_Body hero; memset(&hero, 0, sizeof hero); long score = 0; char ch = 0, arena[(MAXR + 2) * STRIDE], *rows = arena + STRIDE; int nr = 0, w = -1; char ln[1024]; memset(arena, 0, sizeof arena);
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int k = 0; char *s = p; while (k < 8) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "RULE") && k >= 3) { if (!strcmp(c[1], "teleport_fee")) r.fee = atol(c[2]); else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "TILE") && k >= 3 && !strcmp(c[1], "teleport")) ch = c[2][0];
        else if (!strcmp(c[0], "HERO") && k >= 5) { hero.x = atol(c[1]); hero.y = atol(c[2]); hero.w = atol(c[3]); hero.h = atol(c[4]); if (k >= 6) hero.vy = atol(c[5]); if (k >= 7) hero.vx = atol(c[6]); }
        else if (!strcmp(c[0], "SCORE") && k >= 2) score = atol(c[1]);
        else if (!strcmp(c[0], "ROW") && k >= 2) { if (nr >= MAXR || strlen(c[1]) >= STRIDE) { fprintf(stderr, "rule_teleport_op: a level is at most 32 rows of 64 tiles\n"); return 2; } if (w >= 0 && (int)strlen(c[1]) != w) { fprintf(stderr, "rule_teleport_op: the rows are not all one width\n"); return 2; } w = (int)strlen(c[1]); memcpy(rows + nr * STRIDE, c[1], (size_t)w); nr++; }
        else { fprintf(stderr, "rule_teleport_op: unknown row %s\n", c[0]); return 2; }
    }
    fclose(f);
    if (r.fee < 0) { fprintf(stderr, "rule_teleport_op: the scene needs RULE teleport_fee: the number is data, there is no default\n"); return 2; }
    if (!ch) { fprintf(stderr, "rule_teleport_op: the scene needs TILE teleport <char>: the tile is data, there is no default\n"); return 2; }
    if (nr < 1) { fprintf(stderr, "rule_teleport_op: the scene needs ROW rows\n"); return 2; }
    /* every cell OUTSIDE the level holds the tile character: a rule that forgets a bounds test would use it, so the case can see that clause */
    for (int y = -1; y <= MAXR; y++) for (int x = 0; x < STRIDE; x++) if (y < 0 || y >= nr || x >= w) rows[y * STRIDE + x] = ch;
    int fx = 0, fy = 0, tx = 0, ty = 0; int got = tp_step(&hero, rows, STRIDE, w, nr, ch, &r, &score, &fx, &fy, &tx, &ty);
    char out[2400]; int m = snprintf(out, sizeof out, "TELEPORTED %d SCORE %ld FROM ", got, score);
    if (got) m += snprintf(out + m, sizeof out - m, "%d,%d TO %d,%d", fx, fy, tx, ty); else m += snprintf(out + m, sizeof out - m, "- TO -");
    m += snprintf(out + m, sizeof out - m, " HERO %ld,%ld VX %ld VY %ld LEVEL ", hero.x, hero.y, hero.vx, hero.vy); for (int y = 0; y < nr; y++) m += snprintf(out + m, sizeof out - m, "%s%.*s", y ? "/" : "", w, rows + y * STRIDE); m += snprintf(out + m, sizeof out - m, "\n");
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
