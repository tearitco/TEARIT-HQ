/* pron_check - validate a pronunciation map: pron_check <pron.pdl> <phonemes.txt>        (slice S2, BOOK 19.audio/PRONUNCIATION-MAP.md)
 * Rules (each has a bad file in data/audio/pron_bad/): row is  PRON | word | phonemes | source | status ;  exactly 5 fields; word 1..40 chars of a-z 0-9 ' - only (lower case);
 * phonemes 1..60 UTF-8 characters, every one in phonemes.txt (or a space); source in cmudict|espeak|owner|model|llm; status in candidate|active; no two ACTIVE rows with the same word
 * (a candidate may repeat a word: it is a proposed change); every row must be a PRON row (comments # and the SECTION/dash header lines are skipped).
 * Prints one line per problem and a final VALID/INVALID line. Exit 0 valid, 1 invalid, 2 usage/unreadable. Pure, deterministic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXR 4096
static char allowed[8192]; static char *rows_w[MAXR]; static int nrows;
static int split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static int ulen(unsigned char c) { return c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1; }
static int is_allowed(const char *ch, int len) { if (len == 1 && ch[0] == ' ') return 1; for (const char *p = allowed; *p;) { int l = ulen((unsigned char)*p); if (l == len && !memcmp(p, ch, (size_t)len)) return 1; p += l; } return 0; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: pron_check pron.pdl phonemes.txt\n"); return 2; }
    FILE *pf = fopen(argv[2], "r"); if (!pf) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; } char ln[1024]; size_t al = 0;
    while (fgets(ln, sizeof ln, pf)) { if (ln[0] == '#' || !strncmp(ln, "SPACE", 5)) continue; ln[strcspn(ln, "\r\n")] = 0; size_t l = strlen(ln); if (al + l < sizeof allowed) { memcpy(allowed + al, ln, l); al += l; } } allowed[al] = 0; fclose(pf);
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; } int bad = 0, lineno = 0, valid_rows = 0;
    while (fgets(ln, sizeof ln, f)) { lineno++; if (ln[0] == '#' || ln[0] == '\n' || !strncmp(ln, "SECTION", 7) || !strncmp(ln, "-----", 5)) continue; char *fl[8]; int n = split(ln, fl, 7);
        if (strcmp(fl[0], "PRON")) { printf("line %d: not a PRON row\n", lineno); bad++; continue; } if (n != 5) { printf("line %d: need exactly 5 fields (a | inside a value?), got %d\n", lineno, n); bad++; continue; }
        const char *w = fl[1], *ph = fl[2], *src = fl[3], *st = fl[4]; int ok = 1; size_t wl = strlen(w);
        if (wl < 1 || wl > 40) { printf("line %d: word length %zu not in 1..40\n", lineno, wl); ok = 0; } for (size_t i = 0; i < wl; i++) { char c = w[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '-')) { printf("line %d: word '%s' has a character that is not a-z 0-9 ' - (lower case only)\n", lineno, w); ok = 0; break; } }
        int nch = 0, badch = 0; for (const char *p = ph; *p;) { int l = ulen((unsigned char)*p); if (!is_allowed(p, l)) badch = 1; nch++; p += l; } if (nch < 1 || nch > 60) { printf("line %d: phonemes must be 1..60 characters, got %d\n", lineno, nch); ok = 0; } if (badch) { printf("line %d: phonemes '%s' use a character that is not in phonemes.txt\n", lineno, ph); ok = 0; }
        if (strcmp(src, "cmudict") && strcmp(src, "espeak") && strcmp(src, "owner") && strcmp(src, "model") && strcmp(src, "llm")) { printf("line %d: source '%s' not one of cmudict espeak owner model llm\n", lineno, src); ok = 0; }
        if (strcmp(st, "candidate") && strcmp(st, "active")) { printf("line %d: status '%s' not candidate or active\n", lineno, st); ok = 0; }
        if (ok && !strcmp(st, "active")) { for (int i = 0; i < nrows; i++) if (!strcmp(rows_w[i], w)) { printf("line %d: a second active row for '%s'\n", lineno, w); ok = 0; break; } if (ok && nrows < MAXR) rows_w[nrows++] = strdup(w); }
        if (!ok) bad++; else valid_rows++; }
    fclose(f); printf("%s: %d valid rows, %d problems\n", bad ? "INVALID" : "VALID", valid_rows, bad); return bad ? 1 : 0;
}
