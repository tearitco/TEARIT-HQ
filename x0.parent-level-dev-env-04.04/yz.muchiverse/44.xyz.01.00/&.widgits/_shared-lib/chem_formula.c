/* chem_formula.c - pure text-include: a chemical formula -> atom count PER ELEMENT, checked against the locked elements.pdl (2026-10-23, claude).
 * Why it exists: concept-bank/ops/chem_label.c already parses formulas (nested parentheses, unicode subscripts) but only returns TOTAL atoms and mass, which cannot say whether a reaction conserves each element.
 * This file has the same grammar and adds the per-element counts. chem_label.c is NOT changed (its 21-check harness stays as it was); moving both onto this file is a later, separately gated step.
 * Needs only stdio / stdlib / string / ctype. Grammar: Element = Upper [lower]; a count is digits (default 1); ( ... )n groups nest; UTF-8 subscript digits (E2 82 80..89) count as digits; spaces are ignored; anything else is an error.
 * A side of a reaction is terms joined by '+', each term an optional integer coefficient then a formula: "2H2+O2", "6CO2+6H2O", "C6H12O6". Charges and states are not supported (a term with them is an error, not skipped).
 *   cf_load_elements(path)            reads `ELEMENT | Z | symbol | name | protons | neutrons | mass_number` rows; returns the count (0 = none read)
 *   cf_parse(formula, &count)         1 = ok (count.n[i] atoms of element i), 0 = bad formula (unknown symbol, unbalanced parenthesis, zero count, empty)
 *   cf_side(text, &count)             1 = ok: the sum over every term of coefficient * atoms; 0 = bad */
#ifndef CHEM_FORMULA_C
#define CHEM_FORMULA_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define CF_MAXEL 130
typedef struct { char sym[4]; int z; } CfEl;
typedef struct { long n[CF_MAXEL]; } CfCount;
static CfEl cf_el[CF_MAXEL]; static int cf_nel = 0;
static int cf_find(const char *s, size_t len) { for (int i = 0; i < cf_nel; i++) if (strlen(cf_el[i].sym) == len && !strncmp(cf_el[i].sym, s, len)) return i; return -1; }
static int cf_load_elements(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return 0; char ln[512]; cf_nel = 0;
    while (fgets(ln, sizeof ln, f)) {
        if (strncmp(ln, "ELEMENT", 7)) { continue; }
        char *fld[8]; int n = 0; char *p = ln;
        while (n < 8) { fld[n++] = p; char *bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; }
        if (n < 3) { continue; }
        char *sym = fld[2]; while (*sym == ' ') { sym++; }
        char *e = sym + strlen(sym); while (e > sym && (e[-1] == ' ' || e[-1] == '\n')) *--e = 0;
        if (!*sym || strlen(sym) > 2 || cf_nel >= CF_MAXEL) { continue; }
        snprintf(cf_el[cf_nel].sym, sizeof cf_el[cf_nel].sym, "%s", sym); cf_el[cf_nel].z = atoi(fld[1]); cf_nel++;
    }
    fclose(f); return cf_nel;
}
static void cf_ascii(const char *in, char *out, size_t sz) {
    size_t o = 0; for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < sz; ) {
        if (p[0] == 0xE2 && p[1] == 0x82 && p[2] >= 0x80 && p[2] <= 0x89) { out[o++] = (char)('0' + (p[2] - 0x80)); p += 3; }
        else if (*p == ' ') p++; else out[o++] = (char)*p++; }
    out[o] = 0;
}
static const char *cf_p; static int cf_ok;
static long cf_num(void) { if (!isdigit((unsigned char)*cf_p)) return 1; long n = 0; while (isdigit((unsigned char)*cf_p)) { n = n * 10 + (*cf_p - '0'); if (n > 1000000) { cf_ok = 0; return 1; } cf_p++; } if (n == 0) cf_ok = 0; return n; }
static void cf_group(CfCount *c, char close, int depth) {
    if (depth > 8) { cf_ok = 0; return; }
    while (cf_ok && *cf_p && *cf_p != close) {
        if (*cf_p == '(') { CfCount sub; memset(&sub, 0, sizeof sub); cf_p++; cf_group(&sub, ')', depth + 1); if (*cf_p != ')') { cf_ok = 0; return; } cf_p++; long m = cf_num(); for (int i = 0; i < CF_MAXEL; i++) c->n[i] += sub.n[i] * m; }
        else if (isupper((unsigned char)*cf_p)) { size_t len = 1; if (islower((unsigned char)cf_p[1])) len = 2; int i = cf_find(cf_p, len); if (i < 0 && len == 2) { len = 1; i = cf_find(cf_p, 1); if (i >= 0 && islower((unsigned char)cf_p[1])) i = -1; } if (i < 0) { cf_ok = 0; return; } cf_p += len; c->n[i] += cf_num(); }
        else { cf_ok = 0; return; }
    }
}
static int cf_parse(const char *formula, CfCount *out) {
    char a[512]; cf_ascii(formula, a, sizeof a); memset(out, 0, sizeof *out); if (!a[0]) return 0;
    cf_p = a; cf_ok = 1; cf_group(out, 0, 0); if (*cf_p) cf_ok = 0; return cf_ok;
}
static int cf_side(const char *text, CfCount *out) {
    char buf[1024]; snprintf(buf, sizeof buf, "%s", text); memset(out, 0, sizeof *out); int terms = 0;
    for (char *t = strtok(buf, "+"); t; t = strtok(NULL, "+")) {
        while (*t == ' ') { t++; }
        long coef = 1; if (isdigit((unsigned char)*t)) { char *e; coef = strtol(t, &e, 10); if (coef <= 0 || coef > 1000) return 0; t = e; }
        CfCount one; if (!cf_parse(t, &one)) return 0; for (int i = 0; i < CF_MAXEL; i++) out->n[i] += one.n[i] * coef; terms++;
    }
    return terms > 0;
}
#endif
