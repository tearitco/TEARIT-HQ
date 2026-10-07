/* rand_roll - ONE verb: a deterministic die of N sides (N=2 is the coin) from (seed, counter), written to a named variable. Footrace Fu uses it for the coin
 * (tie-break, Warp Ally, Banish Foe, Shadow blink length) and for the d4 of Time Skip; dice_roll (ring-board-rmmv) stays the 2d6 op. No srand(time), no rand():
 * an integer hash, so a restored game that restores `rand_counter` flips exactly what the uninterrupted game would.
 *
 * Usage:  rand_roll <state_dir> <seed> <counter> <sides> <out_var>
 *   seed, counter, sides   an integer literal, or [file:]name read from the kv files (e.g. dice_seed rand_counter abilities.pdl:ab_1_die), {var} allowed in names.
 *   out_var                [file:]name written (default variables.txt) with a value 1..sides.
 * Hash: splitmix64 of (seed*0x9E3779B97F4A7C15 + counter*0xBF58476D1CE4E5B9 + 0xD1B54A32D192ED03), a different stream than dice_roll; value = (h >> 33) % sides + 1.
 * Writes: <out_var>, variables.txt rand_counter = counter + 1.  Appends (append-only, no timestamps): <state_dir>/rand_ledger.txt   RAND|<seed>|<counter>|<sides>|<value>
 * Prints the value. Exit 0 | 1 = sides < 2 or could not write (ERR row in op_errors.txt, nothing written) | 2 = usage.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rand_roll.+x rand_roll.c */
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
#include <stdint.h>
static uint64_t mix64(uint64_t z) { z += 0x9E3779B97F4A7C15ULL; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
int main(int argc, char **argv) {
    OPNAME = "rand_roll";
    if (argc != 6 || !set_dir(argv[1])) { fprintf(stderr, "usage: rand_roll <state_dir> <seed> <counter> <sides> <out_var>\n"); return 2; }
    long seed, ctr, sides; operand(argv[2], &seed); operand(argv[3], &ctr); operand(argv[4], &sides);
    if (sides < 2 || sides > 1000000) { op_error("sides out of range", argv[4]); return 1; }
    uint64_t h = mix64((uint64_t)seed * 0x9E3779B97F4A7C15ULL + (uint64_t)ctr * 0xBF58476D1CE4E5B9ULL + 0xD1B54A32D192ED03ULL);
    long v = (long)((h >> 33) % (uint64_t)sides) + 1;
    char f[512], n[512]; split_ref(argv[5], f, n);
    if (!n[0] || !kv_set(f, n, v) || !kv_set("variables.txt", "rand_counter", ctr + 1)) { op_error("cannot write variables", argv[5]); return 1; }
    char p[PL + 128]; fpath(p, "rand_ledger.txt"); FILE *l = fopen(p, "a");
    if (!l) { op_error("cannot append rand_ledger", ""); return 1; }
    fprintf(l, "RAND|%ld|%ld|%ld|%ld\n", seed, ctr, sides, v); fclose(l);
    printf("%ld\n", v); return 0;
}
