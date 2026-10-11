/* cash_transfer - ONE verb: move money between two accounts, refusing to create money or overdraw (RING-BOARD-AS-RMMV-EVENTS-DESIGN.md section 3).
 *
 * Usage:  cash_transfer <state_dir> <from> <to> <amount> [<reason> [strict]]
 *   from / to  `bank` | `mint` | a player number | [file:]name read as a number ({var} allowed; 0 = the bank). Balances are variables:
 *              cash_<p> for a player, cash_bank for the bank. `mint` is the ONLY source of new money and only with reason `setup`.
 *   amount     integer literal or [file:]name. reason: a word for the ledger (default `-`).
 * Results (printed, and written to variables.txt as xfer_result 1/2/3 and xfer_moved):
 *   ok        amount moved in full                                                                                   exit 0
 *   bankrupt  the payer is a PLAYER holding less than amount and `strict` is not given: ALL the payer has moves to the payee (balance 0),
 *             the creditor is paid what exists, nothing is created                                                    exit 1
 *   refused   nothing moved: amount < 0, from == to, to == mint, mint without reason `setup`, the bank short of cash,
 *             or (with `strict`) a payer short of cash (overdraft refused, not bankrupt)                              exit 3
 * Appends (append-only, no timestamps): <state_dir>/cash_ledger.txt   XFER|<turn>|<from>|<to>|<requested>|<moved>|<result>|<reason>
 * (accounts as `bank`, `mint` or the player number; <turn> is the variable `turn`). Exit 2 = usage. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/cash_transfer.+x cash_transfer.c */
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
/* account: returns -1 mint, 0 bank, n player; fills the balance variable name */
static int account(const char *tok, char *bal) {
    long v;
    if (!strcmp(tok, "mint")) { bal[0] = 0; return -1; }
    if (!strcmp(tok, "bank")) v = 0; else operand(tok, &v);
    if (v == 0) snprintf(bal, 64, "cash_bank"); else snprintf(bal, 64, "cash_%ld", v);
    return (int)v;
}
int main(int argc, char **argv) {
    OPNAME = "cash_transfer";
    if (argc < 5 || argc > 7 || !set_dir(argv[1])) { fprintf(stderr, "usage: cash_transfer <state_dir> <from> <to> <amount> [<reason> [strict]]\n"); return 2; }
    const char *reason = argc > 5 ? argv[5] : "-"; int strict = argc > 6 && !strcmp(argv[6], "strict");
    char fb[64], tb[64]; int fa = account(argv[2], fb), ta = account(argv[3], tb);
    long amt; operand(argv[4], &amt); long turn; kv_get("variables.txt", "turn", &turn);
    long fbal = 0, tbal = 0, moved = 0; int res = 1;       /* 1 ok 2 bankrupt 3 refused */
    if (fa >= 0) kv_get("variables.txt", fb, &fbal);
    if (ta >= 0) kv_get("variables.txt", tb, &tbal);
    if (amt < 0 || fa == ta || ta == -1 || (fa == -1 && strcmp(reason, "setup")) || (fa == 0 && fbal < amt)) res = 3;
    else if (fa > 0 && fbal < amt) { if (strict) res = 3; else { res = 2; moved = fbal; } }
    else moved = amt;
    if (res != 3) {
        if (fa >= 0 && !kv_set("variables.txt", fb, fbal - moved)) { op_error("cannot write", fb); return 1; }
        if (!kv_set("variables.txt", tb, tbal + moved)) { op_error("cannot write", tb); return 1; }
    }
    kv_set("variables.txt", "xfer_result", res); kv_set("variables.txt", "xfer_moved", moved);
    char p[PL + 128]; fpath(p, "cash_ledger.txt"); FILE *f = fopen(p, "a");
    if (!f) { op_error("cannot append cash_ledger", ""); return 1; }
    char fn[32], tn[32]; snprintf(fn, sizeof fn, fa < 0 ? "mint" : fa == 0 ? "bank" : "%d", fa); snprintf(tn, sizeof tn, ta == 0 ? "bank" : "%d", ta);
    fprintf(f, "XFER|%ld|%s|%s|%ld|%ld|%s|%s\n", turn, fn, tn, amt, moved, res == 1 ? "ok" : res == 2 ? "bankrupt" : "refused", reason); fclose(f);
    puts(res == 1 ? "ok" : res == 2 ? "bankrupt" : "refused");
    return res == 1 ? 0 : res == 2 ? 1 : 3;
}
