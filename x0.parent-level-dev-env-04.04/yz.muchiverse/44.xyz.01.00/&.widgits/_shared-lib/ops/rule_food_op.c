/* rule_food_op - evaluate the food rule (../rule_food.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_food, FSM / AI via --describe); the game calls the SAME function in-process.
 *
 * Usage: rule_food_op <scene.pdl>      |      rule_food_op --describe
 * Scene rows (pipe rows, # comments, integers):
 *   BOARD | cols | rows       REQUIRED (1 to 64 each)
 *   SEED  | n                 the game's seed (default 0)
 *   K     | n                 how many foods have been spawned so far (default 0)
 *   BODY  | x | y             one snake body cell, head first; up to 1100
 *   MIX   | n                 optional: print `MIX <sm_mix(n)>` instead (pins the shared mixer)
 * Output: `FOOD <x>,<y> OK 1` (or `FOOD -1,-1 OK 0` for a full board) then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_food.c"
#define MAXB 1100
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_food_op: IN scene rows BOARD cols rows, SEED n, K n, BODY x y (head first), MIX n | OUT `FOOD x,y OK 1|0` (or `MIX v`) and HASH | RULE keys: none | module: _shared-lib/rule_food.c over seed_mix.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_food_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_food_op: cannot open scene\n"); return 2; }
    static int bx[MAXB], by[MAXB]; int cols = 0, rows = 0, len = 0, havemix = 0; long seed = 0, k = 0, mixn = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[4]; int n = 0; char *s = p; while (n < 4) { c[n++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < n; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "BOARD") && n >= 3) { cols = atoi(c[1]); rows = atoi(c[2]); }
        else if (!strcmp(c[0], "SEED") && n >= 2) seed = atol(c[1]);
        else if (!strcmp(c[0], "K") && n >= 2) k = atol(c[1]);
        else if (!strcmp(c[0], "BODY") && n >= 3 && len < MAXB) { bx[len] = atoi(c[1]); by[len] = atoi(c[2]); len++; }
        else if (!strcmp(c[0], "MIX") && n >= 2) { mixn = atol(c[1]); havemix = 1; }
    }
    fclose(f);
    char out[120]; int m;
    if (havemix) m = snprintf(out, sizeof out, "MIX %llu\n", (unsigned long long)sm_mix((uint64_t)mixn));
    else {
        if (cols < 1 || cols > 64 || rows < 1 || rows > 64) { fprintf(stderr, "rule_food_op: the scene needs BOARD | cols | rows (1 to 64 each)\n"); return 2; }
        int fx = -1, fy = -1, ok = fd_spawn(bx, by, len, cols, rows, seed, k, &fx, &fy); m = snprintf(out, sizeof out, "FOOD %d,%d OK %d\n", fx, fy, ok);
    }
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
