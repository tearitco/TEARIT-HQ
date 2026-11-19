/* rule_pickup_op - evaluate the pickup rule (../rule_pickup.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_pickup, FSM / AI via --describe); the game tick loop calls the SAME function in-process.
 *
 * Usage: rule_pickup_op <scene.pdl>      |      rule_pickup_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   RULE  | pickup_score | N        REQUIRED: the number is data
 *   HERO  | x | y | w | h | armed   the collector, Q8 box, armed 0 or 1 at the start
 *   ITEM  | cx | cy | taken         an item cell (whole cells), taken 0 or 1; up to 4
 *   SCORE | N                      the score before (optional, default 0)
 * Output: `COLLECTED <n> ARMED <a> SCORE <s> ITEMS=<taken flags>` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_pickup.c"
#define MAXS 4
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_pickup_op: IN scene rows RULE pickup_score, HERO x y w h armed, ITEM cx cy taken, SCORE n | OUT `COLLECTED n ARMED a SCORE s ITEMS flags` and HASH | RULE keys: pickup_score | module: _shared-lib/rule_pickup.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_pickup_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_pickup_op: cannot open scene\n"); return 2; }
    PK_Rule r = { -1 }; PF_Body hero; memset(&hero, 0, sizeof hero); int armed = 0, cx[MAXS], cy[MAXS], taken[MAXS], n = 0; long score = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int k = 0; char *s = p; while (k < 8) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "RULE") && k >= 3) { if (!strcmp(c[1], "pickup_score")) r.score = atol(c[2]); else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "HERO") && k >= 6) { hero.x = atol(c[1]); hero.y = atol(c[2]); hero.w = atol(c[3]); hero.h = atol(c[4]); armed = atoi(c[5]) != 0; }
        else if (!strcmp(c[0], "ITEM") && k >= 4 && n < MAXS) { cx[n] = atoi(c[1]); cy[n] = atoi(c[2]); taken[n] = atoi(c[3]) != 0; n++; }
        else if (!strcmp(c[0], "SCORE") && k >= 2) score = atol(c[1]);
    }
    fclose(f);
    if (r.score < 0) { fprintf(stderr, "rule_pickup_op: the scene needs RULE pickup_score: the number is data, there is no default\n"); return 2; }
    int got = pk_collect(&hero, cx, cy, taken, n, &r, &armed, &score); char out[200]; int m = snprintf(out, sizeof out, "COLLECTED %d ARMED %d SCORE %ld ITEMS=", got, armed, score);
    for (int i = 0; i < n; i++) m += snprintf(out + m, sizeof out - m, "%d", taken[i]);
    m += snprintf(out + m, sizeof out - m, "\n");
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
