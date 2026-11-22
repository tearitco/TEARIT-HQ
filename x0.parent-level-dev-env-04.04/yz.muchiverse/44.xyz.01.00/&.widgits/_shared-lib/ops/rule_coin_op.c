/* rule_coin_op - evaluate the coin rule (../rule_coin.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_coin, FSM / AI via --describe); the game tick loop calls the SAME function in-process (decision D106).
 *
 * Usage: rule_coin_op <scene.pdl>      |      rule_coin_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   RULE  | coin_value | N         REQUIRED: the number is data, there is no default
 *   TILE  | coin | <char>   REQUIRED: the tile character of a coin (data, as board.pdl LEVEL | coin | <char>)
 *   HERO  | x | y | w | h
 *   SCORE | N              the score before (optional, default 0)
 *   ROW   | text          the level, top row first (up to 32 rows of up to 64 tiles, all of one width)
 * Output: `COLLECTED <n> SCORE <s> CELLS <x,y;x,y or -> LEVEL <rows joined by />` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_coin.c"
#define MAXR 32
#define STRIDE 65
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_coin_op: IN scene rows RULE coin_value, TILE coin <char>, HERO x y w h, SCORE n, ROW text | OUT `COLLECTED n SCORE s CELLS cells LEVEL rows` and HASH | RULE keys: coin_value | module: _shared-lib/rule_coin.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_coin_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_coin_op: cannot open scene\n"); return 2; }
    CN_Rule r = { -1 }; PF_Body hero; memset(&hero, 0, sizeof hero); long vy = 0, score = 0; char ch = 0, arena[(MAXR + 2) * STRIDE], *rows = arena + STRIDE; int nr = 0, w = -1; char ln[1024]; memset(arena, 0, sizeof arena);
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int k = 0; char *s = p; while (k < 8) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "RULE") && k >= 3) { if (!strcmp(c[1], "coin_value")) r.value = atol(c[2]); else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "TILE") && k >= 3 && !strcmp(c[1], "coin")) ch = c[2][0];
        else if (!strcmp(c[0], "HERO") && k >= 5) { hero.x = atol(c[1]); hero.y = atol(c[2]); hero.w = atol(c[3]); hero.h = atol(c[4]); if (k >= 6) vy = atol(c[5]); }
        else if (!strcmp(c[0], "SCORE") && k >= 2) score = atol(c[1]);
        else if (!strcmp(c[0], "ROW") && k >= 2) { if (nr >= MAXR || strlen(c[1]) >= STRIDE) { fprintf(stderr, "rule_coin_op: a level is at most 32 rows of 64 tiles\n"); return 2; } if (w >= 0 && (int)strlen(c[1]) != w) { fprintf(stderr, "rule_coin_op: the rows are not all one width\n"); return 2; } w = (int)strlen(c[1]); memcpy(rows + nr * STRIDE, c[1], (size_t)w + 1); nr++; }
    }
    fclose(f);
    if (r.value < 0) { fprintf(stderr, "rule_coin_op: the scene needs RULE coin_value: the number is data, there is no default\n"); return 2; }
    if (!ch) { fprintf(stderr, "rule_coin_op: the scene needs TILE coin <char>: the tile is data, there is no default\n"); return 2; }
    if (nr < 1) { fprintf(stderr, "rule_coin_op: the scene needs ROW rows\n"); return 2; }
    /* every cell OUTSIDE the level (the guard rows above and below, the bytes right of the last column) holds the tile character: a rule that forgets a bounds test would take it, so the case can see that clause */
    for (int y = -1; y <= MAXR; y++) for (int x = 0; x < STRIDE; x++) if (y < 0 || y >= nr || x >= w) rows[y * STRIDE + x] = ch;
    int cx[4], cy[4]; int got = cn_collect(&hero, rows, STRIDE, w, nr, ch, &r, &score, cx, cy, 4); (void)vy;
    char out[2400]; int m = snprintf(out, sizeof out, "COLLECTED %d SCORE %ld CELLS ", got, score);
    if (!got) { m += snprintf(out + m, sizeof out - m, "-"); }
    for (int i = 0; i < got; i++) m += snprintf(out + m, sizeof out - m, "%s%d,%d", i ? ";" : "", cx[i], cy[i]);
    m += snprintf(out + m, sizeof out - m, " LEVEL "); for (int y = 0; y < nr; y++) m += snprintf(out + m, sizeof out - m, "%s%.*s", y ? "/" : "", w, rows + y * STRIDE); m += snprintf(out + m, sizeof out - m, "\n");
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
