/* ev_chain - ONE event-command op for the blockchain: lets an event page (any app or game) read a balance, send, and lock/pay out/refund escrow on a TEST or USER chain.
 * House op convention (see ring-board-rmmv/ops/var_math.c): single binary, <state_dir> first, results as `key=int` lines in <state_dir>/variables.txt,
 * a failure leaves an ERR row in <state_dir>/op_errors.txt (a pal `exec` hides exit codes), self-contained (no header+link), fork+exec with a watchdog (never popen).
 * Design: #.#.calendar-dox/!.HQ-IQ-BOOK/16.game/GAMES-AS-PACKAGES-AND-STORE-DESIGN.md section 17.
 *
 * Usage:  ev_chain <state_dir> <house_root> <chain_id> <verb> [args...]
 *   balance                      -> chain_balance=<n>
 *   send    <to> <amount>        -> chain_ok=1|0
 *   lock    <escrow_id> <amount> -> chain_ok         (moves coins from THIS entity's wallet into escrow; this wallet becomes the escrow agent)
 *   payout  <escrow_id> <to> <amount>               (only the lock's agent may; chain_escrow enforces it)
 *   refund  <escrow_id> <to> <amount>
 *   status  <escrow_id>          -> chain_locked chain_paid chain_refunded chain_remaining
 * Every call also sets chain_ok (1 = op exit 0) and chain_rc (op exit code), and appends a CHAINEV row to <state_dir>/chain_events.txt.
 *
 * SAFETY RULES (each is a refusal, tested):
 *  1. The wallet is NEVER a parameter: it is the one bound to the running entity (<state_dir>/wallet_id.txt). An event cannot spend another wallet.
 *  2. chain_id is [a-z0-9_-]+ and must be a chain created by chain_new (<house>/041.pal-chain<suffix>/chains/<id>/chain.pdl exists). `cones`, the real chain, has no chain.pdl
 *     and is refused: game events never move real cones.
 *  3. Ids and amounts are validated ([A-Za-z0-9_-]+, positive integer <= 1e9); chain_escrow re-checks everything at issue time and again at mining.
 *  4. A hard timeout (10 s) on the child; one-shot, no polling.
 * SIGNING (signatures, ev_chain slice; design section 28): on a chain with `SIGNING | required | 1` the op signs for the running entity through sig_op and passes <nonce> <sig> to chain_send /
 * chain_escrow, which verify it (and the miner again). The op NEVER holds a key: <state_dir>/signer.pdl names the account's key file (`SIGNER | key_file | <abs path>`) and the ENVIRONMENT VARIABLE
 * that carries its passphrase (`SIGNER | pass_env | <NAME>`); the key file must live OUTSIDE every 041.pal-chain* folder and be mode 0600 (no group/other bits), else the call is refused. A missing
 * or unusable key REFUSES the call (never an unsigned fallback). It signs only under a POLICY cap: <chain root>/policy.pdl `POLICY | chain:<id> or * | ev_cap_per_day | <millicones>`; coins moved per
 * wallet per UTC day by send / lock / payout / refund are summed from <chain root>/data/ev_spend.txt (rows SPEND|wallet|YYYYMMDD|verb|amount|ts, appended after a successful call); a call that would
 * exceed the cap, a missing cap row on a signing chain, or an out-of-range value (0..1e9; the default 0 refuses) is refused. A chain WITHOUT signing and WITHOUT a cap row behaves exactly as before.
 * Honest limit: the spend file is an ordinary file, so the cap binds the op, not a process that can write the chain folder (same honor boundary as the rest of the test chains).
 * Exit: 0 ok | 1 op failed / refused | 2 usage. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/ev_chain.+x ev_chain.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#define PL 4096
static char STATE[PL], HOUSE[PL], CHAIN[64], WALLET[128], ROOT[PL];

static void errrow(const char *msg, const char *a) {
    char p[PL + 64]; snprintf(p, sizeof p, "%s/op_errors.txt", STATE); FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "ERR|ev_chain|%s|%s\n", msg, a ? a : ""); fclose(f); }
    fprintf(stderr, "ev_chain: %s %s\n", msg, a ? a : "");
}
static int ident_ok(const char *s, size_t max) { size_t n = strlen(s); if (!n || n > max) return 0; for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-') return 0; return 1; }
static int amount_ok(const char *s) { if (!*s || strlen(s) > 10) return 0; for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0; long long v = atoll(s); return v > 0 && v <= 1000000000LL; }
static void setvar(const char *name, long long v) {                    /* `key=int` line replaced in place, else appended (same effect as prisc SYS_SET_KV_INT) */
    char p[PL + 64], t[PL + 80], line[512]; snprintf(p, sizeof p, "%s/variables.txt", STATE); snprintf(t, sizeof t, "%s.tmp.%d", p, (int)getpid());
    FILE *in = fopen(p, "r"), *out = fopen(t, "w"); if (!out) { if (in) fclose(in); return; }
    size_t nl = strlen(name); int done = 0;
    if (in) { while (fgets(line, sizeof line, in)) { if (!strncmp(line, name, nl) && line[nl] == '=') { fprintf(out, "%s=%lld\n", name, v); done = 1; } else fputs(line, out); } fclose(in); }
    if (!done) fprintf(out, "%s=%lld\n", name, v);
    fclose(out); rename(t, p);
}
static void ledger(const char *verb, const char *args, int rc, const char *res) {
    char p[PL + 64]; snprintf(p, sizeof p, "%s/chain_events.txt", STATE); FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "CHAINEV|%ld|%s|%s|%s|%s|rc=%d|%s\n", (long)time(NULL), CHAIN, WALLET, verb, args, rc, res); fclose(f); }
}
/* fork+exec with PRISC_PROJECT_ROOT set and a 10 s watchdog; stdout captured (first 2 KB). Returns the exit code, or 99 on spawn/timeout. */
static int run(char *const argv[], char *out, size_t cap) {
    int pf[2]; if (pipe(pf)) return 99;
    pid_t pid = fork(); if (pid < 0) return 99;
    if (!pid) { dup2(pf[1], 1); dup2(pf[1], 2); close(pf[0]); close(pf[1]); setenv("PRISC_PROJECT_ROOT", ROOT, 1); if (chdir(ROOT)) _exit(98); execv(argv[0], argv); _exit(97); }
    close(pf[1]); size_t n = 0; time_t t0 = time(NULL); int st = 0, fl = fcntl(pf[0], F_GETFL); fcntl(pf[0], F_SETFL, fl | O_NONBLOCK);
    for (;;) {
        ssize_t r = read(pf[0], out + n, cap - 1 - n); if (r > 0) { n += (size_t)r; if (n >= cap - 1) break; }
        pid_t w = waitpid(pid, &st, WNOHANG); if (w == pid) { while ((r = read(pf[0], out + n, cap - 1 - n)) > 0) { n += (size_t)r; if (n >= cap - 1) break; } out[n] = 0; close(pf[0]); return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
        if (time(NULL) - t0 > 10) { kill(pid, SIGKILL); waitpid(pid, &st, 0); out[n] = 0; close(pf[0]); return 99; }
        usleep(20000);
    }
    kill(pid, SIGKILL); waitpid(pid, &st, 0); out[n] = 0; close(pf[0]); return 99;
}
static int find_root(void) {                                           /* <house>/041.pal-chain<suffix>/chains/<id> with a chain.pdl: only chain_new chains qualify */
    DIR *d = opendir(HOUSE); if (!d) return 0; struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "041.pal-chain", 13)) continue;
        char p[PL]; snprintf(p, sizeof p, "%s/%s/chains/%s/chain.pdl", HOUSE, e->d_name, CHAIN);
        if (!access(p, R_OK)) { snprintf(ROOT, sizeof ROOT, "%s/%s/chains/%s", HOUSE, e->d_name, CHAIN); closedir(d); return 1; }
    }
    closedir(d); return 0;
}
static int bin(char *out, size_t cap, const char *name) {              /* the op binary of the PARENT root (a chain's ops dir is a symlink to it) */
    char p[PL]; snprintf(p, sizeof p, "%s/ops/+x/%s.+x", ROOT, name); if (access(p, X_OK)) return 0; snprintf(out, cap, "%s", p); return 1;
}
/* ---- signing under a policy cap (see the header) ---- */
static int pdl_row(const char *file, const char *sec, const char *scope, const char *key, char *out, size_t cap) {      /* `SEC | scope | key | value` (scope "" = two-column `SEC | key | value`); 1 = found */
    FILE *f = fopen(file, "r"); if (!f) return 0; char line[1024]; int found = 0;
    while (fgets(line, sizeof line, f)) { if (line[0] == '#') continue; line[strcspn(line, "\r\n")] = 0; char *c = strstr(line, " #"); if (c) *c = 0;
        char *fld[5]; int n = 0; char *p = line; while (n < 5) { fld[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; }
        for (int i = 0; i < n; i++) { char *a = fld[i]; while (*a == ' ' || *a == '\t') a++; char *e = a + strlen(a); while (e > a && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; fld[i] = a; }
        if (strcmp(fld[0], sec)) continue;
        if (scope[0]) { if (n >= 4 && !strcmp(fld[1], scope) && !strcmp(fld[2], key)) { snprintf(out, cap, "%s", fld[3]); found = 1; } }
        else if (n >= 3 && !strcmp(fld[1], key)) { snprintf(out, cap, "%s", fld[2]); found = 1; } }
    fclose(f); return found; }
static long long spent_today(const char *day) {
    char p[PL + 32], line[512]; snprintf(p, sizeof p, "%s/data/ev_spend.txt", ROOT); FILE *f = fopen(p, "r"); if (!f) return 0; long long sum = 0;
    while (fgets(line, sizeof line, f)) { char *fld[6]; int n = 0; char *q = line; line[strcspn(line, "\r\n")] = 0; while (n < 6) { fld[n++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; }
        if (n >= 5 && !strcmp(fld[0], "SPEND") && !strcmp(fld[1], WALLET) && !strcmp(fld[2], day)) sum += atoll(fld[4]); }
    fclose(f); return sum; }
static void today(char *day) { time_t t = time(NULL); struct tm g; gmtime_r(&t, &g); strftime(day, 16, "%Y%m%d", &g); }
static void note_spend(const char *verb, const char *amount) {
    char p[PL + 32], day[16]; today(day); snprintf(p, sizeof p, "%s/data/ev_spend.txt", ROOT); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "SPEND|%s|%s|%s|%s|%ld\n", WALLET, day, verb, amount, (long)time(NULL)); fclose(f); } }
static int g_signed = 0, g_capped = 0; static char g_nonce[24], g_sig[200], g_amount[16], g_verb[16];
/* Decides signing + cap for one money-moving call. 0 = go ahead (g_signed / g_capped say what applies), -1 = refused (errrow written). amount = the coins the call moves. */
static int prepare(const char *verb, const char *esc, const char *to, const char *amount) {
    g_signed = g_capped = 0; snprintf(g_amount, sizeof g_amount, "%s", amount); snprintf(g_verb, sizeof g_verb, "%s", verb);
    char chainpdl[PL + 32], polpdl[PL + 32], v[256]; snprintf(chainpdl, sizeof chainpdl, "%s/chain.pdl", ROOT); snprintf(polpdl, sizeof polpdl, "%s/policy.pdl", ROOT);
    int req = pdl_row(chainpdl, "SIGNING", "", "required", v, sizeof v) && atoi(v) == 1;
    char scope[80]; snprintf(scope, sizeof scope, "chain:%s", CHAIN); long long cap = 0; int have_cap = 0;
    if (pdl_row(polpdl, "POLICY", scope, "ev_cap_per_day", v, sizeof v) || pdl_row(polpdl, "POLICY", "*", "ev_cap_per_day", v, sizeof v)) {
        have_cap = 1; char *e = NULL; long long x = strtoll(v, &e, 10); if (e && !*e && v[0] && x >= 0 && x <= 1000000000LL) cap = x; else { errrow("policy ev_cap_per_day out of range or not a number, default 0 used", v); cap = 0; } }
    if (!req && !have_cap) return 0;                                       /* an unsigned chain with no policy: exactly the old behaviour */
    if (req && !have_cap) { errrow("signing chain without a policy cap (policy.pdl ev_cap_per_day): refused", CHAIN); return -1; }
    char day[16]; today(day); long long amt = atoll(amount), spent = spent_today(day);
    if (cap <= 0 || spent + amt > cap) { char m[96]; snprintf(m, sizeof m, "spend cap exceeded (cap %lld, spent today %lld, asked %lld)", cap, spent, amt); errrow(m, WALLET); return -1; }
    g_capped = 1; if (!req) return 0;
    char sp[PL + 32], kf[PL] = "", pe[96] = ""; snprintf(sp, sizeof sp, "%s/signer.pdl", STATE);
    if (!pdl_row(sp, "SIGNER", "", "key_file", kf, sizeof kf) || !pdl_row(sp, "SIGNER", "", "pass_env", pe, sizeof pe)) { errrow("no signer.pdl (SIGNER key_file and pass_env) for this entity: refused, never unsigned", ""); return -1; }
    char rk[PATH_MAX], rh[PATH_MAX]; if (kf[0] != '/' || !realpath(kf, rk)) { errrow("signer key file missing or not an absolute path", ""); return -1; }
    { char hp[PL + 32]; snprintf(hp, sizeof hp, "%s", HOUSE); if (realpath(hp, rh)) { size_t hl = strlen(rh); DIR *d = opendir(rh); struct dirent *e; int bad = 0;
        if (d) { while ((e = readdir(d))) { if (strncmp(e->d_name, "041.pal-chain", 13)) continue; char cp[PATH_MAX + 300]; snprintf(cp, sizeof cp, "%s/%s", rh, e->d_name); size_t cl = strlen(cp); if (!strncmp(rk, cp, cl) && (rk[cl] == '/' || !rk[cl])) bad = 1; } closedir(d); }
        (void)hl; if (bad) { errrow("the signing key must live outside every chain folder", ""); return -1; } } }
    struct stat st; if (stat(rk, &st) || !S_ISREG(st.st_mode) || (st.st_mode & 077)) { errrow("signing key file must be a regular file with mode 0600 (no group/other access)", ""); return -1; }
    const char *pass = getenv(pe); if (!pass || !*pass || !ident_ok(pe, 64)) { errrow("passphrase environment variable is not set", pe); return -1; }
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); snprintf(g_nonce, sizeof g_nonce, "%lld", (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000);
    char msg[640];
    if (!strcmp(verb, "send")) snprintf(msg, sizeof msg, "TX|%s|%s|%s|%s|%s", CHAIN, WALLET, to, amount, g_nonce);
    else if (!strcmp(verb, "lock")) snprintf(msg, sizeof msg, "LOCK|%s|%s|%s|%s|%s|%s", CHAIN, esc, WALLET, amount, WALLET, g_nonce);
    else snprintf(msg, sizeof msg, "%s|%s|%s|%s|%s|%s|%s", !strcmp(verb, "payout") ? "PAYOUT" : "REFUND", CHAIN, esc, WALLET, to, amount, g_nonce);
    char exe[PL], ctx[96], out[512] = "", *av[8]; if (!bin(exe, sizeof exe, "sig_op")) { errrow("sig_op binary missing", ""); return -1; }
    snprintf(ctx, sizeof ctx, "chain:%s", CHAIN); av[0] = exe; av[1] = "sign"; av[2] = rk; av[3] = ctx; av[4] = msg; av[5] = NULL;
    setenv("SIG_PASSPHRASE", pass, 1); int rc = run(av, out, sizeof out); unsetenv("SIG_PASSPHRASE");
    out[strcspn(out, "\r\n")] = 0; if (rc || strlen(out) != 128) { errrow("signing failed (wrong passphrase or unusable key)", ""); return -1; }
    snprintf(g_sig, sizeof g_sig, "%s", out); g_signed = 1; return 0; }
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: ev_chain <state_dir> <house_root> <chain_id> <verb> [args]\n"); return 2; }
    snprintf(STATE, sizeof STATE, "%s", argv[1]); snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); snprintf(CHAIN, sizeof CHAIN, "%s", argv[3]);
    const char *verb = argv[4]; char args[512] = ""; for (int i = 5; i < argc; i++) { strncat(args, argv[i], sizeof(args) - strlen(args) - 2); strncat(args, " ", sizeof(args) - strlen(args) - 1); }
    if (!ident_ok(CHAIN, 48) || !strcmp(CHAIN, "cones")) { errrow("refused chain (bad id or the real chain)", CHAIN); setvar("chain_ok", 0); return 1; }
    { char p[PL + 32]; snprintf(p, sizeof p, "%s/wallet_id.txt", STATE); FILE *f = fopen(p, "r");
      if (!f || !fgets(WALLET, sizeof WALLET, f)) { if (f) fclose(f); errrow("no wallet bound to this entity (wallet_id.txt)", ""); setvar("chain_ok", 0); return 1; }
      fclose(f); WALLET[strcspn(WALLET, "\r\n")] = 0; if (!ident_ok(WALLET, 120)) { errrow("bad wallet id", WALLET); setvar("chain_ok", 0); return 1; } }
    if (!find_root()) { errrow("unknown chain (no chains/<id>/chain.pdl)", CHAIN); setvar("chain_ok", 0); return 1; }
    char exe[PL], out[2048] = ""; char *av[12]; int rc = 1, n = 0;
    if (!strcmp(verb, "balance") && argc == 5) { if (!bin(exe, sizeof exe, "chain_balance")) goto nobin; av[n++] = exe; av[n++] = WALLET; av[n] = NULL; rc = run(av, out, sizeof out);
        if (!rc) setvar("chain_balance", atoll(out)); }
    else if (!strcmp(verb, "send") && argc == 7 && ident_ok(argv[5], 120) && amount_ok(argv[6])) { if (!bin(exe, sizeof exe, "chain_send")) goto nobin; if (prepare("send", "", argv[5], argv[6])) { setvar("chain_ok", 0); return 1; } av[n++] = exe; av[n++] = WALLET; av[n++] = argv[5]; av[n++] = argv[6]; if (g_signed) { av[n++] = g_nonce; av[n++] = g_sig; } av[n] = NULL; rc = run(av, out, sizeof out); }
    else if (!strcmp(verb, "lock") && argc == 7 && ident_ok(argv[5], 100) && amount_ok(argv[6])) { if (!bin(exe, sizeof exe, "chain_escrow")) goto nobin; if (prepare("lock", argv[5], "", argv[6])) { setvar("chain_ok", 0); return 1; } av[n++] = exe; av[n++] = "lock"; av[n++] = argv[5]; av[n++] = WALLET; av[n++] = argv[6]; av[n++] = WALLET; if (g_signed) { av[n++] = g_nonce; av[n++] = g_sig; } av[n] = NULL; rc = run(av, out, sizeof out); }
    else if ((!strcmp(verb, "payout") || !strcmp(verb, "refund")) && argc == 8 && ident_ok(argv[5], 100) && ident_ok(argv[6], 120) && amount_ok(argv[7])) { if (!bin(exe, sizeof exe, "chain_escrow")) goto nobin; if (prepare(verb, argv[5], argv[6], argv[7])) { setvar("chain_ok", 0); return 1; } av[n++] = exe; av[n++] = (char *)verb; av[n++] = argv[5]; av[n++] = WALLET; av[n++] = argv[6]; av[n++] = argv[7]; if (g_signed) { av[n++] = g_nonce; av[n++] = g_sig; } av[n] = NULL; rc = run(av, out, sizeof out); }
    else if (!strcmp(verb, "status") && argc == 6 && ident_ok(argv[5], 100)) { if (!bin(exe, sizeof exe, "chain_escrow")) goto nobin; av[n++] = exe; av[n++] = "status"; av[n++] = argv[5]; av[n] = NULL; rc = run(av, out, sizeof out);
        if (!rc) { const char *k[4] = {"locked=", "paid_out=", "refunded=", "remaining="}; const char *v[4] = {"chain_locked", "chain_paid", "chain_refunded", "chain_remaining"};
            for (int i = 0; i < 4; i++) { char *p = strstr(out, k[i]); if (p) setvar(v[i], atoll(p + strlen(k[i]))); } } }
    else { errrow("bad verb or arguments", verb); setvar("chain_ok", 0); return 2; }
    out[strcspn(out, "\n")] = 0; ledger(verb, args, rc, out); if (!rc && g_capped) note_spend(g_verb, g_amount);
    setvar("chain_rc", rc); setvar("chain_ok", rc == 0);
    if (rc) errrow("chain op failed", out);
    return rc ? 1 : 0;
nobin: errrow("chain op binary missing", verb); setvar("chain_ok", 0); return 1;
}
