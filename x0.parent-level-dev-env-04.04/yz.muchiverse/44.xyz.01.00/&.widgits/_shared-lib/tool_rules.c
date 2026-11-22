/* tool_rules.c - pure text-include: read `RULE | key | value` rows (tool_rules.pdl, concept-bank) for the tool-use ops. No state beyond the table; a missing key is the CALLER's refusal. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TR_MAX 128
#define TR_LEN 128
static char tr_k[TR_MAX][TR_LEN], tr_v[TR_MAX][TR_LEN]; static int tr_n;
static char *tr_trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int tr_load(const char *path) {
    FILE *f = fopen(path, "r"); char ln[512]; tr_n = 0; if (!f) return -1;
    while (fgets(ln, sizeof ln, f) && tr_n < TR_MAX) {
        char *p = tr_trim(ln); if (strncmp(p, "RULE", 4)) continue;
        char *a = strchr(p, '|'); if (!a) continue; a++; char *b = strchr(a, '|'); if (!b) continue; *b++ = 0;
        snprintf(tr_k[tr_n], TR_LEN, "%s", tr_trim(a)); snprintf(tr_v[tr_n], TR_LEN, "%s", tr_trim(b)); tr_n++;
    }
    fclose(f); return 0;
}
/* first value of key, or NULL */
static const char *tr_get(const char *key) { for (int i = 0; i < tr_n; i++) if (!strcmp(tr_k[i], key)) return tr_v[i]; return NULL; }
/* is val one of the values listed under key (a list rule) */
static int tr_in(const char *key, const char *val) { for (int i = 0; i < tr_n; i++) if (!strcmp(tr_k[i], key) && !strcmp(tr_v[i], val)) return 1; return 0; }
/* required numeric rule: returns 0 and sets *out, or -1 and prints the refusal */
static int tr_num(const char *op, const char *key, double *out) {
    const char *v = tr_get(key); char *end; if (!v || !*v) { fprintf(stderr, "%s: refused: RULE %s missing in the rules file\n", op, key); return -1; }
    *out = strtod(v, &end); if (*end) { fprintf(stderr, "%s: refused: RULE %s is not a number\n", op, key); return -1; } return 0;
}
