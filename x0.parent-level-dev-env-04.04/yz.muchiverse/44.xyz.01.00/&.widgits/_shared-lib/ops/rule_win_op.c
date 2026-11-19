/* rule_win_op - evaluate the win rule (../rule_win.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_win, FSM / AI via --describe); the game calls the SAME function in-process.
 *
 * Usage: rule_win_op <scene.pdl>      |      rule_win_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   GOAL | gx | gy              the goal cell (whole cells), REQUIRED
 *   HERO | x | y | w | h        a body (a seat), in seat order; up to 8
 * Output: `WINNER <seat or -1> OVER <0|1>` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_win.c"
#define MAXS 8
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_win_op: IN scene rows GOAL gx gy, HERO x y w h (seat order, up to 8) | OUT `WINNER seat|-1 OVER 0|1` and HASH | RULE keys: none | module: _shared-lib/rule_win.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_win_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_win_op: cannot open scene\n"); return 2; }
    int gx = -1, gy = -1, have = 0, n = 0; PF_Body b[MAXS]; memset(b, 0, sizeof b); char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int k = 0; char *s = p; while (k < 8) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "GOAL") && k >= 3) { gx = atoi(c[1]); gy = atoi(c[2]); have = 1; }
        else if (!strcmp(c[0], "HERO") && k >= 5 && n < MAXS) { b[n].x = atol(c[1]); b[n].y = atol(c[2]); b[n].w = atol(c[3]); b[n].h = atol(c[4]); n++; }
    }
    fclose(f);
    if (!have) { fprintf(stderr, "rule_win_op: the scene needs a GOAL | gx | gy row\n"); return 2; }
    int w = wn_winner(b, n, gx, gy); char out[80]; int m = snprintf(out, sizeof out, "WINNER %d OVER %d\n", w, w >= 0);
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
