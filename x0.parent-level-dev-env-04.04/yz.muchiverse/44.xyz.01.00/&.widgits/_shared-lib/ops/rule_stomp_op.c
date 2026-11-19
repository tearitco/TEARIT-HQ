/* rule_stomp_op - evaluate the stomp rule (../rule_stomp.c) once over a scene and print the result. The CLI face of the rule: a harness runs it alone, an event page / Blueprintz calls it by name (registry command rule_stomp),
 * an FSM or an AI tool call can discover it with --describe. The game's tick loop calls the SAME function in-process.
 *
 * Usage: rule_stomp_op <scene.pdl>      |      rule_stomp_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   RULE   | stomp_margin | N    RULE | stomp_bounce | N    RULE | stomp_score | N      (ALL THREE REQUIRED: the numbers are data)
 *   HERO   | x | y | w | h | vy                  the body, Q8 box and vertical speed (positive = falling)
 *   WALKER | x | y | w | h | alive               up to 8, in order
 * Output: `STOMPS <n> HIT <0|1> SCORE <n x stomp_score> HERO vy=<vy> WALKERS=<alive flags>` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_stomp.c"
#define MAXS 8
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_stomp_op: IN scene rows RULE stomp_margin/stomp_bounce/stomp_score, HERO x y w h vy, WALKER x y w h alive | OUT `STOMPS n HIT h SCORE s HERO vy WALKERS flags` and HASH | RULE keys: stomp_margin stomp_bounce stomp_score | module: _shared-lib/rule_stomp.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_stomp_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_stomp_op: cannot open scene\n"); return 2; }
    ST_Rule r = { -1, -1, -1 }; PF_Body hero; memset(&hero, 0, sizeof hero); RP_Walker w[MAXS]; memset(w, 0, sizeof w); int nw = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int n = 0; char *s = p; while (n < 8) { c[n++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < n; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "RULE") && n >= 3) { long v = atol(c[2]); if (!strcmp(c[1], "stomp_margin")) r.margin = v; else if (!strcmp(c[1], "stomp_bounce")) r.bounce = v; else if (!strcmp(c[1], "stomp_score")) r.score = v; else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "HERO") && n >= 6) { hero.x = atol(c[1]); hero.y = atol(c[2]); hero.w = atol(c[3]); hero.h = atol(c[4]); hero.vy = atol(c[5]); }
        else if (!strcmp(c[0], "WALKER") && n >= 6 && nw < MAXS) { w[nw].b.x = atol(c[1]); w[nw].b.y = atol(c[2]); w[nw].b.w = atol(c[3]); w[nw].b.h = atol(c[4]); w[nw].alive = atoi(c[5]) != 0; nw++; }
    }
    fclose(f);
    if (r.margin < 0 || r.bounce < 0 || r.score < 0) { fprintf(stderr, "rule_stomp_op: the scene needs RULE stomp_margin, stomp_bounce and stomp_score: the numbers are data, there is no default\n"); return 2; }
    ST_Result res = st_contact(&hero, w, nw, &r); char out[300]; int m = snprintf(out, sizeof out, "STOMPS %d HIT %d SCORE %ld HERO vy=%ld WALKERS=", res.stomps, res.hit, (long)res.stomps * r.score, hero.vy);
    for (int i = 0; i < nw; i++) m += snprintf(out + m, sizeof out - m, "%d", w[i].alive);
    m += snprintf(out + m, sizeof out - m, "\n");
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
