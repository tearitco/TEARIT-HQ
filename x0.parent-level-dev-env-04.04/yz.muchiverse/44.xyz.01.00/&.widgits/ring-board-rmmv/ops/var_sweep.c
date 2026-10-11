/* var_sweep - ONE verb: for every variable whose name starts with <prefix> and whose value equals <from>, set it to <to> (the smallest op the Bankruptcy
 * rule needed: "owned tiles return to the bank" = `var_sweep . owner_ <player> 0`; a page cannot loop over variable names, prisc ecall keys are literals).
 *
 * Usage:  var_sweep <state_dir> <prefix> <from> <to>      from/to: an integer literal, or [file:]name read from the kv files ({var} allowed).
 * Works on variables.txt only. Prints the number of variables changed. Exit 0 | 1 write failure (ERR row) | 2 usage.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/var_sweep.+x var_sweep.c */
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
    OPNAME = "var_sweep";
    if (argc != 5 || !set_dir(argv[1]) || !argv[2][0]) { fprintf(stderr, "usage: var_sweep <state_dir> <prefix> <from> <to>\n"); return 2; }
    long from, to, n = 0; operand(argv[3], &from); operand(argv[4], &to);
    char p[PL + 128], tp[PL + 160], ln[1024]; size_t pl = strlen(argv[2]); fpath(p, "variables.txt"); snprintf(tp, sizeof tp, "%s.tmp", p);
    FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); if (!o) { if (f) fclose(f); op_error("cannot write variables", ""); return 1; }
    while (f && fgets(ln, sizeof ln, f)) {
        char *eq = strchr(ln, '=');
        if (eq && !strncmp(ln, argv[2], pl) && atol(eq + 1) == from) { fprintf(o, "%.*s=%ld\n", (int)(eq - ln), ln, to); n++; } else fputs(ln, o);
    }
    if (f) fclose(f);
    fclose(o); if (rename(tp, p)) { op_error("rename failed", ""); return 1; }
    printf("%ld\n", n); return 0;
}
