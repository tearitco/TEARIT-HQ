/* chem_label - LABEL and SCORE a compound sheet deterministically against the locked element table. No model is involved.
 *
 * Usage:  chem_label <elements.pdl> <compounds.csv> <id_prefix> <out_labeled.pdl> <feedback_log>
 *   elements.pdl   `ELEMENT | Z | symbol | name | protons | neutrons | mass_number` (the ground truth)
 *   compounds.csv  a header row, then rows; columns found by header name (case-insensitive): name|compound_name, formula, emoji, category|type, hint|mcat_relevance,
 *                  state, melting_point, boiling_point, density, toxicity, reactivity, color_hex, icon_tile   (missing columns read as empty). Quoted fields with commas are handled.
 *   out_labeled    rewritten: COMPOUND | <id> | emoji=.. | name=.. | formula=<ascii> | formula_ok=0|1 | atoms=N | mass_number=N | elements=<symbols> | kind=.. | state=.. | mp=.. | bp=.. | density=.. | hint=..
 *                  (formula is converted to ASCII: Unicode subscripts 0-9 become digits; `·` hydrate dots are not supported -> formula_ok=0)
 *   feedback_log   APPENDED (idempotent by id): one FEEDBACK row per exam, layer=curriculum:
 *                    concept=chem_formula   +1 when the formula parses and every symbol is a real element, else -1
 *                    concept=chem_props     +1 / -1 only when both melting and boiling points are numbers: boiling > melting (a consistency check); no row otherwise
 * Prints `rows=N formula_ok=A formula_bad=B props_checked=C props_bad=D`. Exit 0 | 2 usage / unreadable file.
 * Paths all come from argv. Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o chem_label.+x chem_label.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define L 8192
typedef struct { char sym[4]; long p, n; } El;
static El E[130]; static int nE = 0;
static const El *el(const char *s, size_t len) { for (int i = 0; i < nE; i++) if (strlen(E[i].sym) == len && !strncmp(E[i].sym, s, len)) return &E[i]; return NULL; }
static void to_ascii(const char *in, char *out, size_t sz) {            /* UTF-8 subscripts (E2 82 80..89) -> digits; strip spaces */
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 2 < sz; ) {
        if (p[0] == 0xE2 && p[1] == 0x82 && p[2] >= 0x80 && p[2] <= 0x89) { out[o++] = (char)('0' + (p[2] - 0x80)); p += 3; }
        else if (*p == ' ') p++;
        else out[o++] = (char)*p++;
    }
    out[o] = 0;
}
/* recursive descent: group := (element|'(' group ')') number? ... ; returns 0 on error. atoms and mass accumulate multiplied by the caller's factor */
static const char *P_; static char SEEN[256]; static int OK_;
static long parse_num(void) { if (!isdigit((unsigned char)*P_)) return 1; long n = 0; while (isdigit((unsigned char)*P_)) { n = n * 10 + (*P_ - '0'); if (n > 100000) { OK_ = 0; return 1; } P_++; } if (n == 0) OK_ = 0; return n; }
static void parse_group(long *atoms, long *mass, char close) {
    while (OK_ && *P_ && *P_ != close) {
        if (*P_ == '(' || *P_ == '[') {
            char c = *P_ == '(' ? ')' : ']'; long a = 0, m = 0; P_++; parse_group(&a, &m, c);
            if (*P_ != c) { OK_ = 0; return; }
            P_++; long k = parse_num(); *atoms += a * k; *mass += m * k;
        } else if (isupper((unsigned char)*P_)) {
            size_t len = 1; if (islower((unsigned char)P_[1])) len = 2;
            const El *e = el(P_, len);
            if (!e && len == 2) { len = 1; e = el(P_, 1); }
            if (!e) { OK_ = 0; return; }
            if (strlen(SEEN) + 5 < sizeof SEEN && !strstr(SEEN, e->sym)) { strcat(SEEN, e->sym); strcat(SEEN, ","); }
            P_ += len; long k = parse_num(); *atoms += k; *mass += k * (e->p + e->n);
        } else { OK_ = 0; return; }
    }
}
static int split_csv(char *line, char **f, int max) {                  /* in place; quoted fields, "" = quote */
    int n = 0; char *p = line;
    while (n < max) {
        char *w = p; f[n++] = w;
        if (*p == '"') { p++; w = f[n - 1]; char *r = p; while (*r) { if (*r == '"' && r[1] == '"') { *w++ = '"'; r += 2; } else if (*r == '"') { r++; break; } else *w++ = *r++; } *w = 0; p = r; }
        else { while (*p && *p != ',') p++; }
        if (*p == ',') { *p++ = 0; continue; }
        if (!*p) break;
        if (*p != ',') { *p = 0; break; }
    }
    return n;
}
static int already(const char *log, const char *id) { FILE *f = fopen(log, "r"); char ln[L], key[256]; int hit = 0; snprintf(key, sizeof key, "id=%s |", id); if (!f) return 0; while (!hit && fgets(ln, sizeof ln, f)) if (strstr(ln, key)) hit = 1; fclose(f); return hit; }
static int colx(char **H, int nh, const char *a, const char *b) { for (int i = 0; i < nh; i++) if (!strcmp(H[i], a) || (b[0] && !strcmp(H[i], b))) return i; return -1; }
static int is_num(const char *s) { char *e; if (!*s) return 0; strtod(s, &e); return !*e; }
int main(int argc, char **argv) {
    if (argc != 6) { fprintf(stderr, "usage: chem_label <elements.pdl> <compounds.csv> <id_prefix> <out_labeled.pdl> <feedback_log>\n"); return 2; }
    char ln[L]; FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "ELEMENT |", 9) && nE < 130) { char *t = ln; int c = 0; for (; c < 2 && (t = strchr(t, '|')); c++) t++;
        if (!t) continue;
        while (*t == ' ') t++;
        if (sscanf(t, "%3[A-Za-z] | %*[^|]| %ld | %ld", E[nE].sym, &E[nE].p, &E[nE].n) == 3) nE++; }
    fclose(f);
    if (nE < 100) { fprintf(stderr, "element table too small (%d)\n", nE); return 2; }
    FILE *in = fopen(argv[2], "r"); if (!in) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    FILE *out = fopen(argv[4], "w"); if (!out) { fclose(in); fprintf(stderr, "cannot write %s\n", argv[4]); return 2; }
    char *H[40], *F[40]; int nh = 0, rows = 0, fok = 0, fbad = 0, pchk = 0, pbad = 0;
    if (!fgets(ln, sizeof ln, in)) { fclose(in); fclose(out); return 2; }
    ln[strcspn(ln, "\r\n")] = 0; nh = split_csv(ln, H, 40); for (int i = 0; i < nh; i++) for (char *q = H[i]; *q; q++) *q = (char)tolower((unsigned char)*q);
    #define COL(a, b) colx(H, nh, a, b)
    int cName = COL("compound_name", "name"), cFor = COL("formula", ""), cEmo = COL("emoji", ""), cKind = COL("category", "type"), cHint = COL("hint", "mcat_relevance"), cState = COL("state", ""),
        cMp = COL("melting_point", ""), cBp = COL("boiling_point", ""), cDen = COL("density", "");
    if (cName < 0 || cFor < 0) { fprintf(stderr, "csv needs name and formula columns\n"); fclose(in); fclose(out); return 2; }
    while (fgets(ln, sizeof ln, in)) {
        ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue;
        int nf = split_csv(ln, F, 40); rows++;
        #define FLD(c) ((c) >= 0 && (c) < nf ? F[c] : "")
        char fa[256], id[96]; to_ascii(FLD(cFor), fa, sizeof fa);
        long atoms = 0, mass = 0; SEEN[0] = 0; OK_ = fa[0] != 0; P_ = fa; parse_group(&atoms, &mass, 0); if (*P_) OK_ = 0; if (atoms <= 0) OK_ = 0;
        snprintf(id, sizeof id, "%.40s%d", argv[3], rows);
        char sy[256]; snprintf(sy, sizeof sy, "%s", OK_ ? SEEN : ""); size_t sl = strlen(sy); if (sl && sy[sl - 1] == ',') sy[sl - 1] = 0;
        fprintf(out, "COMPOUND | %s | emoji=%s | name=%s | formula=%s | formula_ok=%d | atoms=%ld | mass_number=%ld | elements=%s | kind=%s | state=%s | mp=%s | bp=%s | density=%s | hint=%s\n",
                id, FLD(cEmo), FLD(cName), fa, OK_ ? 1 : 0, OK_ ? atoms : 0, OK_ ? mass : 0, sy, FLD(cKind), FLD(cState), FLD(cMp), FLD(cBp), FLD(cDen), FLD(cHint));
        if (OK_) fok++; else fbad++;
        char fid[160]; snprintf(fid, sizeof fid, "%s#formula", id);
        if (!already(argv[5], fid)) { FILE *g = fopen(argv[5], "a"); if (g) { fprintf(g, "[chem] FEEDBACK | id=%s | target=chem-bank | valence=%+d | concept=chem_formula | intensity=1 | layer=curriculum | row=%s\n", fid, OK_ ? 1 : -1, id); fclose(g); } }
        if (is_num(FLD(cMp)) && is_num(FLD(cBp))) {
            int good = atof(FLD(cBp)) > atof(FLD(cMp)); pchk++; if (!good) pbad++;
            snprintf(fid, sizeof fid, "%s#props", id);
            if (!already(argv[5], fid)) { FILE *g = fopen(argv[5], "a"); if (g) { fprintf(g, "[chem] FEEDBACK | id=%s | target=chem-bank | valence=%+d | concept=chem_props | intensity=1 | layer=curriculum | row=%s\n", fid, good ? 1 : -1, id); fclose(g); } }
        }
    }
    fclose(in); fclose(out);
    printf("rows=%d formula_ok=%d formula_bad=%d props_checked=%d props_bad=%d\n", rows, fok, fbad, pchk, pbad);
    return 0;
}
