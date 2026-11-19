/* rule_input_op - evaluate the input rule (../rule_input.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_input, FSM / AI via --describe); the game calls the SAME functions in-process.
 *
 * Usage: rule_input_op <scene.pdl>      |      rule_input_op --describe
 * Scene rows (pipe rows, # comments, integers):
 *   LATCH | dir | jp | fire | armed | facing     dir -1, 0 or 1; jp, fire, armed 0 or 1; facing -1 or 1 (REQUIRED, one row)
 * Output: `MASK <m> SHOOT <s> FACING <f> AFTER jp=<j> fire=<f> dir=<d>` then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_input.c"
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_input_op: IN scene row LATCH dir jp fire armed facing | OUT `MASK m SHOOT s FACING f AFTER jp fire dir` and HASH | RULE keys: none | module: _shared-lib/rule_input.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_input_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_input_op: cannot open scene\n"); return 2; }
    int have = 0, dir = 0, jp = 0, fire = 0, armed = 0, facing = 1; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int k = 0; char *s = p; while (k < 8) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "LATCH") && k >= 6) { dir = atoi(c[1]); jp = atoi(c[2]) != 0; fire = atoi(c[3]) != 0; armed = atoi(c[4]) != 0; facing = atoi(c[5]); have = 1; }
    }
    fclose(f);
    if (!have) { fprintf(stderr, "rule_input_op: the scene needs a LATCH row\n"); return 2; }
    int shoot = 0, mask = in_controls(dir, jp, fire, armed, &facing, &shoot); in_consume(&jp, &fire);
    char out[120]; int m = snprintf(out, sizeof out, "MASK %d SHOOT %d FACING %d AFTER jp=%d fire=%d dir=%d\n", mask, shoot, facing, jp, fire, dir);
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
