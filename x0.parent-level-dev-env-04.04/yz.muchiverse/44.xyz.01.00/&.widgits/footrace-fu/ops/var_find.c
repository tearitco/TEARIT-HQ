/* var_find - ONE verb: scan variables.txt for the variables whose NAME starts with <prefix> and whose VALUE lies in a distance window around <center>, and report the
 * nearest one. A page cannot loop over variable names (the same gap var_sweep closes); Footrace Fu needs it to find "an enemy ninja on this space" (collision),
 * "an enemy within attack range", "the nearest ally", and "my leading ninja" with ninja positions stored as pos_<seat>_<ninja>.
 *
 * Usage:  var_find <state_dir> <prefix> <skip_prefix|-> <center> <dmin> <dmax> <out>
 *   prefix       variable-name prefix to scan (e.g. pos_, or pos_{current}_); {var} allowed in it
 *   skip_prefix  names starting with this are ignored (e.g. pos_{current}_ to scan only the OTHER seats); `-` = skip nothing
 *   center, dmin, dmax   integer or [file:]name; a variable matches when dmin <= |value - center| <= dmax
 *   out          base name of the results, written to variables.txt:
 *                  <out>    number of matches (0 = none)
 *                  <out>_1  first number in the matched name after the prefix (for pos_2_3 with prefix pos_ this is 2), 0 if none
 *                  <out>_2  second number in the matched name (3), 0 if none
 *                  <out>_d  the distance |value - center| of the chosen match (0 if none)
 *                The chosen match is the nearest (smallest distance); ties go to the smallest <out>_1, then <out>_2, so the answer never depends on file order.
 * Exit 0 (even with no match) | 1 = could not write (ERR row in op_errors.txt) | 2 = usage. Prints the match count.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/var_find.+x var_find.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
/* --- kv helpers (copied per op on purpose: each op is self-contained, no header+link). A variables file is `key=int` lines; a missing key reads 0. --- */
#define PL 4096
static char DIR_[PL];
static const char *OPNAME = "op";
static void fpath(char *out, const char *file) { snprintf(out, PL + 128, "%s/%s", DIR_, file); }
static void op_error(const char *msg, const char *a) {            /* append-only error ledger: a pal `exec` hides exit codes, so failures must leave a row */
    char p[PL + 128]; fpath(p, "op_errors.txt"); FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "ERR|%s|%s|%s\n", OPNAME, msg, a ? a : ""); fclose(f); }
    fprintf(stderr, "%s: %s %s\n", OPNAME, msg, a ? a : "");
}
static int kv_get(const char *file, const char *key, long *out) {  /* 1 = found */
    char p[PL + 128], ln[1024]; size_t kl = strlen(key); fpath(p, file); FILE *f = fopen(p, "r"); *out = 0;
    if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { *out = atol(ln + kl + 1); fclose(f); return 1; }
    fclose(f); return 0;
}
__attribute__((unused)) static int kv_set(const char *file, const char *key, long v) {      /* replace the line in place, else append; temp file + rename */
    char p[PL + 128], tp[PL + 160], ln[1024]; size_t kl = strlen(key); int done = 0; fpath(p, file); snprintf(tp, sizeof tp, "%s.tmp", p);
    FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); if (!o) { if (f) fclose(f); return 0; }
    while (f && fgets(ln, sizeof ln, f)) { if (!done && !strncmp(ln, key, kl) && ln[kl] == '=') { fprintf(o, "%s=%ld\n", key, v); done = 1; } else fputs(ln, o); }
    if (f) fclose(f);
    if (!done) fprintf(o, "%s=%ld\n", key, v);
    fclose(o); return rename(tp, p) == 0;
}
/* a name may hold {var}: replaced by that variable's value (variables.txt), e.g. cash_{current} */
static void expand(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o + 24 < n; ) {
        if (*p == '{') { const char *e = strchr(p, '}'); if (e) { char nm[128]; size_t l = (size_t)(e - p - 1); if (l >= sizeof nm) l = sizeof nm - 1; memcpy(nm, p + 1, l); nm[l] = 0; long v; kv_get("variables.txt", nm, &v); o += (size_t)snprintf(out + o, n - o, "%ld", v); p = e + 1; continue; } }
        out[o++] = *p++;
    }
    out[o] = 0;
}
static int is_int(const char *s) { if (*s == '-' || *s == '+') s++; if (!*s) return 0; for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0; return 1; }
/* split `[file:]name` (after {var} expansion); default file variables.txt */
static void split_ref(const char *tok, char *file, char *name) {
    char ex[512]; expand(tok, ex, sizeof ex); char *c = strchr(ex, ':');
    if (c) { *c = 0; snprintf(file, 512, "%s", ex); snprintf(name, 512, "%s", c + 1); } else { snprintf(file, 512, "variables.txt"); snprintf(name, 512, "%s", ex); }
}
/* operand: an integer literal, or [file:]name read as a variable (missing = 0) */
static void operand(const char *tok, long *out) { char ex[512]; expand(tok, ex, sizeof ex); if (is_int(ex)) { *out = atol(ex); return; } char f[512], n[512]; split_ref(ex, f, n); kv_get(f, n, out); }
static int set_dir(const char *d) { struct stat sb; if (stat(d, &sb) || !S_ISDIR(sb.st_mode)) return 0; snprintf(DIR_, sizeof DIR_, "%s", d); return 1; }
int main(int argc, char **argv) {
    OPNAME = "var_find";
    if (argc != 8 || !set_dir(argv[1]) || !argv[2][0] || !argv[7][0]) { fprintf(stderr, "usage: var_find <state_dir> <prefix> <skip_prefix|-> <center> <dmin> <dmax> <out>\n"); return 2; }
    char prefix[512], skip[512]; expand(argv[2], prefix, sizeof prefix);
    if (strcmp(argv[3], "-")) expand(argv[3], skip, sizeof skip); else skip[0] = 0;
    long center, dmin, dmax; operand(argv[4], &center); operand(argv[5], &dmin); operand(argv[6], &dmax);
    char p[PL + 128], ln[1024]; fpath(p, "variables.txt"); FILE *f = fopen(p, "r");
    long count = 0, bd = 0, b1 = 0, b2 = 0; size_t pl = strlen(prefix), sl = strlen(skip);
    while (f && fgets(ln, sizeof ln, f)) {
        char *eq = strchr(ln, '='); if (!eq || strncmp(ln, prefix, pl)) continue;
        if (sl && !strncmp(ln, skip, sl)) continue;
        long v = atol(eq + 1), d = v > center ? v - center : center - v;
        if (d < dmin || d > dmax) continue;
        long a = 0, b = 0; const char *q = ln + pl;                  /* the numbers in the name after the prefix: 2_3 -> a=2, b=3 */
        if (isdigit((unsigned char)*q)) { a = atol(q); while (isdigit((unsigned char)*q)) q++; if (*q == '_' && isdigit((unsigned char)q[1])) b = atol(q + 1); }
        if (!count || d < bd || (d == bd && (a < b1 || (a == b1 && b < b2)))) { bd = d; b1 = a; b2 = b; }
        count++;
    }
    if (f) fclose(f);
    char o[600]; int ok = 1;
    snprintf(o, sizeof o, "%s", argv[7]); ok &= kv_set("variables.txt", o, count);
    snprintf(o, sizeof o, "%s_1", argv[7]); ok &= kv_set("variables.txt", o, b1);
    snprintf(o, sizeof o, "%s_2", argv[7]); ok &= kv_set("variables.txt", o, b2);
    snprintf(o, sizeof o, "%s_d", argv[7]); ok &= kv_set("variables.txt", o, bd);
    if (!ok) { op_error("cannot write variables", argv[7]); return 1; }
    printf("%ld\n", count); return 0;
}
