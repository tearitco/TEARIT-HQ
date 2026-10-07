/* var_math - ONE verb: arithmetic or comparison on variables, result into a target variable (RING-BOARD-AS-RMMV-EVENTS-DESIGN.md section 3: the missing
 * "variable compare / arithmetic" the digipet build found; prisc has addi/beq/bne only and the registry `if` tests a switch only).
 *
 * Usage:  var_math <state_dir> <op> <target> <a> [<b>]
 *   op      set (target = a) | add | sub | mul | mod (a mod b, b != 0) | lt le gt ge eq ne (target = 1 if a OP b else 0)
 *   target  [file:]name   written like prisc SYS_SET_KV_INT: a `key=int` line in <file> (default variables.txt) replaced in place, else appended.
 *                         e.g. `switches.txt:game_over` writes a switch.
 *   a, b    an integer literal, or [file:]name read from the same kv files (missing = 0), e.g. rules.pdl:go_pay.
 *   any name may hold {var}: replaced by that variable's value first, so `cash_{current}` is the current player's cash.
 * Same variables mechanism as the digipet (`key=int` lines, relative files under the state dir).
 * Exit 0 = written | 1 = bad op / bad operand / mod by zero (NOTHING written, an ERR row is appended to <state_dir>/op_errors.txt) | 2 = usage.
 * Prints the written value. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/var_math.+x var_math.c */
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
    OPNAME = "var_math";
    if (argc < 5 || !set_dir(argv[1])) { fprintf(stderr, "usage: var_math <state_dir> <op> <target> <a> [<b>]\n"); return 2; }
    const char *op = argv[2]; long a, b = 0, r;
    operand(argv[4], &a); if (argc > 5) operand(argv[5], &b);
    if (!strcmp(op, "set")) r = a; else if (!strcmp(op, "add")) r = a + b; else if (!strcmp(op, "sub")) r = a - b; else if (!strcmp(op, "mul")) r = a * b;
    else if (!strcmp(op, "mod")) { if (b == 0) { op_error("mod by zero", argv[4]); return 1; } r = ((a % b) + b) % b; }
    else if (!strcmp(op, "lt")) r = a < b; else if (!strcmp(op, "le")) r = a <= b; else if (!strcmp(op, "gt")) r = a > b;
    else if (!strcmp(op, "ge")) r = a >= b; else if (!strcmp(op, "eq")) r = a == b; else if (!strcmp(op, "ne")) r = a != b;
    else { op_error("bad op", op); return 1; }
    if ((!strcmp(op, "set") ? 0 : argc < 6) ) { op_error("missing second operand", op); return 1; }
    char f[512], n[512]; split_ref(argv[3], f, n);
    if (!n[0] || !kv_set(f, n, r)) { op_error("cannot write target", argv[3]); return 1; }
    printf("%ld\n", r); return 0;
}
