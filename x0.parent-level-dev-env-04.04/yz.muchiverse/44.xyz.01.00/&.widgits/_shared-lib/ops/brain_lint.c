/* brain_lint - checks the BRAIN rows of a game's verbs.pdl (D48 "one brain per game", pass 1, 2026-11-07; BOOK 08-roadmap/design-docs/PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS-2026-11-07.md).
 *
 * A BRAIN row says which verb a pet may CHOOSE, and which harness phases judge that choice:
 *     BRAIN | dec_key | verb | phases | ok_outcomes | bad_outcomes | note
 * Usage:  brain_lint <verbs.pdl> [--phases <file.pdl>]...
 *   --phases   a harness file whose `PHASE | <name>` or `CASE | <name>` rows name the phases a BRAIN row may cite (repeatable). Without it, phase names are not checked.
 * Checks (one line each on failure, `FAIL <code> <detail>`):
 *   B1 fields      at least 6 fields (note optional)             B2 dec_key   non-empty, [a-z0-9_.-] only, unique in the file
 *   B3 verb        names a VERB row of the same file             B4 phases    non-empty; every name exists in a --phases file when one is given
 *   B5 ok          ok_outcomes non-empty                         B6 disjoint  no outcome word is both ok and bad
 * Output ends with `brain_rows=<n> fail=<m>`; exit 0 = clean, 1 = a check failed, 2 = unreadable file or usage. Read-only: it writes nothing.
 * Only the game manifest rows GAME/VERB/BRAIN are read here; verbgen (events-hq) reads only GAME and VERB, so adding BRAIN rows does not change what it parses (read in its source).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/brain_lint.+x ops/brain_lint.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE 1024
#define MAXV 256
static char verbs[MAXV][48]; static int nverbs;
static char known[2048][48]; static int nknown;
static char keys[MAXV][48]; static int nkeys;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *s, char **f, int max) { int n = 0; char *p = s; f[n++] = p; for (; *p; p++) if (*p == '|' && n < max) { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int has(char t[][48], int n, const char *s) { for (int i = 0; i < n; i++) if (!strcmp(t[i], s)) return 1; return 0; }
static int in_list(const char *list, const char *w) { char b[LINE], *sp; snprintf(b, sizeof b, "%s", list); for (char *t = strtok_r(b, ",", &sp); t; t = strtok_r(NULL, ",", &sp)) if (!strcmp(trim(t), w)) return 1; return 0; }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: brain_lint <verbs.pdl> [--phases <file.pdl>]...\n"); return 2; }
    int check_phases = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--phases") || i + 1 >= argc) { fprintf(stderr, "brain_lint: bad argument %s\n", argv[i]); return 2; }
        FILE *pf = fopen(argv[++i], "r"); if (!pf) { fprintf(stderr, "brain_lint: cannot read %s\n", argv[i]); return 2; } check_phases = 1;
        char ln[LINE]; char *f[8];
        while (fgets(ln, sizeof ln, pf)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n >= 2 && (!strcmp(f[0], "PHASE") || !strcmp(f[0], "CASE")) && nknown < 2048) snprintf(known[nknown++], 48, "%s", f[1]); }
        fclose(pf);
    }
    FILE *fp = fopen(argv[1], "r"); if (!fp) { fprintf(stderr, "brain_lint: cannot read %s\n", argv[1]); return 2; }
    char ln[LINE]; char *f[10];
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 10); if (n >= 2 && !strcmp(f[0], "VERB") && nverbs < MAXV) snprintf(verbs[nverbs++], 48, "%s", f[1]); }
    rewind(fp);
    int rows = 0, fail = 0, lineno = 0;
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; if (ln[0] == '#') continue; char raw[LINE]; snprintf(raw, sizeof raw, "%s", ln); (void)raw;
        int n = split(ln, f, 10); if (n < 1 || strcmp(f[0], "BRAIN")) continue; rows++;
        if (n < 6) { printf("FAIL B1 line %d: BRAIN needs 6 fields (dec_key | verb | phases | ok | bad | note)\n", lineno); fail++; continue; }
        const char *key = f[1], *verb = f[2], *ph = f[3], *ok = f[4], *bad = f[5];
        int keyok = *key != 0; for (const char *c = key; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '.' || *c == '-')) keyok = 0;
        if (!keyok) { printf("FAIL B2 line %d: dec_key \"%s\" must be non-empty [a-z0-9_.-]\n", lineno, key); fail++; }
        else if (has(keys, nkeys, key)) { printf("FAIL B2 line %d: dec_key \"%s\" appears twice\n", lineno, key); fail++; }
        else if (nkeys < MAXV) snprintf(keys[nkeys++], 48, "%s", key);
        if (!has(verbs, nverbs, verb)) { printf("FAIL B3 line %d: verb \"%s\" is not a VERB row of this file\n", lineno, verb); fail++; }
        if (!*ph) { printf("FAIL B4 line %d: phases is empty\n", lineno); fail++; }
        else if (check_phases) { char b[LINE], *sp; snprintf(b, sizeof b, "%s", ph); for (char *t = strtok_r(b, ",", &sp); t; t = strtok_r(NULL, ",", &sp)) { char *w = trim(t); if (!has(known, nknown, w)) { printf("FAIL B4 line %d: phase \"%s\" is in no --phases file\n", lineno, w); fail++; } } }
        if (!*ok) { printf("FAIL B5 line %d: ok_outcomes is empty\n", lineno); fail++; }
        else { char b[LINE], *sp; snprintf(b, sizeof b, "%s", ok); for (char *t = strtok_r(b, ",", &sp); t; t = strtok_r(NULL, ",", &sp)) { char *w = trim(t); if (*bad && in_list(bad, w)) { printf("FAIL B6 line %d: outcome \"%s\" is both ok and bad\n", lineno, w); fail++; } } }
    }
    fclose(fp);
    printf("brain_rows=%d fail=%d\n", rows, fail); return fail ? 1 : 0;
}
