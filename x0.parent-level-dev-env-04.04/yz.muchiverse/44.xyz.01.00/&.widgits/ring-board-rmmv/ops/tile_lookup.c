/* tile_lookup - ONE verb: read one field of one board tile from <state_dir>/board.pdl (the data the Land pages branch on).
 *
 * Usage:  tile_lookup <state_dir> <n> <field> [<target_var>]
 *   n       tile index 0..39: an integer literal or [file:]name ({var} allowed)
 *   field   kind (numeric code: 1 go, 2 prop, 3 tax, 4 jail, 5 gotojail, 6 parking) | kindname | name | price | rent | group
 *           | tax (a tax tile's amount: rules.pdl `tax_<group>=int`)
 *   board.pdl rows (pipe-delimited, '#' comments):  TILE | n | kind | name | price | rent | group
 * Prints the value. With <target_var> and a numeric field, also writes it to variables.txt (a page can then test it with a COND kv row).
 * Exit 0 | 1 = tile / field / tax rule missing (ERR row in op_errors.txt, nothing written) | 2 = usage.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tile_lookup.+x tile_lookup.c */
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
static void trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0; }
int main(int argc, char **argv) {
    OPNAME = "tile_lookup";
    if (argc < 4 || argc > 5 || !set_dir(argv[1])) { fprintf(stderr, "usage: tile_lookup <state_dir> <n> <field> [<target_var>]\n"); return 2; }
    long n; operand(argv[2], &n); const char *field = argv[3];
    char p[PL + 128], ln[1024]; fpath(p, "board.pdl"); FILE *f = fopen(p, "r");
    if (!f) { op_error("no board.pdl", ""); return 1; }
    char *fld[8]; int found = 0;
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == '#') continue;
        int c = 0; char *q = ln; while (c < 8) { fld[c++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; }
        for (int i = 0; i < c; i++) trim(fld[i]);
        if (c >= 7 && !strcmp(fld[0], "TILE") && atol(fld[1]) == n && is_int(fld[1])) { found = 1; break; }
    }
    fclose(f);
    if (!found) { op_error("no such tile", argv[2]); return 1; }
    const char *kind = fld[2]; char out[256]; long num = 0; int numeric = 1;
    if (!strcmp(field, "kind")) { num = !strcmp(kind, "go") ? 1 : !strcmp(kind, "prop") ? 2 : !strcmp(kind, "tax") ? 3 : !strcmp(kind, "jail") ? 4 : !strcmp(kind, "gotojail") ? 5 : !strcmp(kind, "parking") ? 6 : 0; if (!num) { op_error("unknown kind", kind); return 1; } }
    else if (!strcmp(field, "kindname")) { snprintf(out, sizeof out, "%s", kind); numeric = 0; }
    else if (!strcmp(field, "name")) { snprintf(out, sizeof out, "%s", fld[3]); numeric = 0; }
    else if (!strcmp(field, "price")) num = atol(fld[4]);
    else if (!strcmp(field, "rent")) num = atol(fld[5]);
    else if (!strcmp(field, "group")) { snprintf(out, sizeof out, "%s", fld[6]); numeric = 0; }
    else if (!strcmp(field, "tax")) { char key[300]; snprintf(key, sizeof key, "tax_%s", fld[6]); if (!kv_get("rules.pdl", key, &num)) { op_error("no tax rule", key); return 1; } }
    else { op_error("unknown field", field); return 1; }
    if (numeric) printf("%ld\n", num); else printf("%s\n", out);
    if (argc == 5 && numeric && !kv_set("variables.txt", argv[4], num)) { op_error("cannot write target", argv[4]); return 1; }
    return 0;
}
