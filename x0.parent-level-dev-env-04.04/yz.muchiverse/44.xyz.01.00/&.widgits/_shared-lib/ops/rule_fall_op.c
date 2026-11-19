/* rule_fall_op - evaluate the fall rule (../rule_fall.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_fall, FSM / AI via --describe); the game calls the SAME functions in-process.
 *
 * Usage: rule_fall_op <scene.pdl>      |      rule_fall_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   RULE  | fall_margin | N        REQUIRED (0 to 64): whole cells below the world
 *   LEVEL | h                      the level height in whole cells, REQUIRED (1 or more)
 *   BODY  | y                      a body's top edge, Q8; up to 4
 * Output: `LIMIT <y> FELL <flags per body>` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_fall.c"
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_fall_op: IN scene rows RULE fall_margin, LEVEL h, BODY y (up to 4) | OUT `LIMIT y FELL flags` and HASH | RULE keys: fall_margin | module: _shared-lib/rule_fall.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_fall_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_fall_op: cannot open scene\n"); return 2; }
    long margin = -1, lh = -1, ys[4]; int n = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[4]; int k = 0; char *s = p; while (k < 4) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "RULE") && k >= 3) { if (!strcmp(c[1], "fall_margin")) margin = atol(c[2]); else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "LEVEL") && k >= 2) lh = atol(c[1]);
        else if (!strcmp(c[0], "BODY") && k >= 2 && n < 4) ys[n++] = atol(c[1]);
    }
    fclose(f);
    if (margin < 0 || margin > 64) { fprintf(stderr, "rule_fall_op: the scene needs RULE fall_margin (0 to 64): the number is data, there is no default\n"); return 2; }
    if (lh < 1) { fprintf(stderr, "rule_fall_op: the scene needs a LEVEL | h row (1 or more)\n"); return 2; }
    char out[100]; int m = snprintf(out, sizeof out, "LIMIT %ld FELL ", fa_limit(lh, margin));
    for (int i = 0; i < n; i++) { PF_Body b; memset(&b, 0, sizeof b); b.y = ys[i]; m += snprintf(out + m, sizeof out - m, "%d", fa_fell(&b, lh, margin)); }
    m += snprintf(out + m, sizeof out - m, "\n");
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
