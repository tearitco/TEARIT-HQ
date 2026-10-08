/* assoc_lint - JUDGE for the hidden-weights step (NIGHT 9 smallest step): a model proposes, this code decides whether the proposal is well formed. Deterministic.
 *
 * Usage:  assoc_lint <phrases.pdl> <reply.txt> <draft_out.txt> [proposer]
 *   phrases.pdl  the corpus: `PHRASE | text` rows (numbered 1..N in file order).
 *   reply.txt    the model's answer: exactly N lines  `<n>|<energy>|<force>|<motion>`  (n = the phrase number; each weight a decimal in [-1.00, 1.00], at most 2 decimals).
 *                Blank lines and lines starting with # or ``` are ignored; anything else is a failure.
 *   draft_out    written ONLY when every check passes: a header + one row per phrase, joined by number (the model never retypes the phrase text):
 *                ASSOC | <n> | <phrase> | energy=<w> | force=<w> | motion=<w> | proposer=<name> | status=candidate
 * A candidate is NOT a weight: it enters the bank only through a person's review (status stays candidate until then).
 * Rules (each prints ASSOC_LINT|FAIL|<rule>|<detail>): format, number-range, number-duplicate, number-missing, weight-range, weight-decimals. Last line ASSOC_LINT|VERDICT|PASS|FAIL|rows=N|failed=M.
 * Exit 0 pass | 1 fail | 2 usage/unreadable. All paths from argv. Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o assoc_lint.+x assoc_lint.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#define MAXP 512
#define LN 1024
static char *P[MAXP]; static int nP = 0; static int seen[MAXP + 1]; static double W[MAXP + 1][3];
static int failed = 0;
static int num_ok(const char *s, double *out) {                 /* optional sign, digits, optional .d or .dd ; decimals reported separately */
    const char *p = s; if (*p == '+' || *p == '-') p++;
    if (!isdigit((unsigned char)*p)) return 0;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') { p++; if (!isdigit((unsigned char)*p)) return 0; while (isdigit((unsigned char)*p)) p++; }
    if (*p) return 0;
    *out = strtod(s, NULL); return 1;
}
static int decimals(const char *s) { const char *d = strchr(s, '.'); return d ? (int)strlen(d + 1) : 0; }
static void bad(const char *rule, const char *fmt, const char *a) { failed++; printf("ASSOC_LINT|FAIL|%s|", rule); printf(fmt, a); printf("\n"); }
int main(int argc, char **argv) {
    if (argc < 4 || argc > 5) { fprintf(stderr, "usage: assoc_lint <phrases.pdl> <reply.txt> <draft_out.txt> [proposer]\n"); return 2; }
    const char *prop = argc == 5 ? argv[4] : "model";
    FILE *f = fopen(argv[1], "r"); char ln[LN]; if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "PHRASE | ", 9) && nP < MAXP) { ln[strcspn(ln, "\r\n")] = 0; P[nP++] = strdup(ln + 9); }
    fclose(f);
    FILE *r = fopen(argv[2], "r"); if (!r || !nP) { fprintf(stderr, "cannot read %s or no phrases\n", argv[2]); return 2; }
    int rows = 0;
    while (fgets(ln, sizeof ln, r)) {
        ln[strcspn(ln, "\r\n")] = 0; char *s = ln; while (*s == ' ') s++;
        for (size_t z = strlen(s); z && (s[z - 1] == ' ' || s[z - 1] == '\t'); ) s[--z] = 0;      /* trailing blanks are harmless */
        if (!*s || *s == '#' || !strncmp(s, "```", 3)) continue;
        char *f1 = strtok(s, "|"), *f2 = strtok(NULL, "|"), *f3 = strtok(NULL, "|"), *f4 = strtok(NULL, "|"), *extra = strtok(NULL, "|");
        double n, e, fo, m;
        for (char **q = (char *[]){ f1, f2, f3, f4 }, **qe = q + 4; q < qe; q++) if (*q) { while (**q == ' ') (*q)++; for (size_t z = strlen(*q); z && ((*q)[z - 1] == ' ' || (*q)[z - 1] == '\t'); ) (*q)[--z] = 0; }
        if (!f1 || !f2 || !f3 || !f4 || extra || !num_ok(f1, &n) || !num_ok(f2, &e) || !num_ok(f3, &fo) || !num_ok(f4, &m)) { bad("format", "not <n>|<e>|<f>|<m>: %.60s", s); continue; }
        int k = (int)n; rows++;
        if (decimals(f1) || k < 1 || k > nP) { bad("number-range", "phrase number %.20s outside 1..N", f1); continue; }
        if (seen[k]++) { bad("number-duplicate", "phrase %.20s given twice", f1); continue; }
        const char *t[3] = { f2, f3, f4 }; double v[3] = { e, fo, m };
        for (int j = 0; j < 3; j++) {
            if (v[j] < -1.0 || v[j] > 1.0) bad("weight-range", "weight %.20s outside [-1,1]", t[j]);
            else if (decimals(t[j]) > 2) bad("weight-decimals", "more than 2 decimals: %.20s", t[j]);
            W[k][j] = v[j];
        }
    }
    fclose(r);
    for (int k = 1; k <= nP; k++) if (!seen[k]) { char b[16]; snprintf(b, sizeof b, "%d", k); bad("number-missing", "phrase %s has no row", b); }
    printf("ASSOC_LINT|VERDICT|%s|rows=%d|failed=%d\n", failed ? "FAIL" : "PASS", rows, failed);
    if (failed) return 1;
    FILE *o = fopen(argv[3], "w"); if (!o) { fprintf(stderr, "cannot write %s\n", argv[3]); return 2; }
    fprintf(o, "# ASSOC drafts: how strongly each Eden phrase relates to the three masters. PROPOSED by %s, linted by assoc_lint; status=candidate until a person reviews every row.\n", prop);
    fprintf(o, "# ASSOC | n | phrase | energy | force | motion | proposer | status\n");
    for (int k = 1; k <= nP; k++) fprintf(o, "ASSOC | %d | %s | energy=%.2f | force=%.2f | motion=%.2f | proposer=%s | status=candidate\n", k, P[k - 1], W[k][0], W[k][1], W[k][2], prop);
    fclose(o); return 0;
}
