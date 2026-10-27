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
 * NOT a substitute for signing: the chain ops' agent/by fields are still honor fields (chain_escrow.c header), so this is meaningful on test chains only.
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
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: ev_chain <state_dir> <house_root> <chain_id> <verb> [args]\n"); return 2; }
    snprintf(STATE, sizeof STATE, "%s", argv[1]); snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); snprintf(CHAIN, sizeof CHAIN, "%s", argv[3]);
    const char *verb = argv[4]; char args[512] = ""; for (int i = 5; i < argc; i++) { strncat(args, argv[i], sizeof(args) - strlen(args) - 2); strncat(args, " ", sizeof(args) - strlen(args) - 1); }
    if (!ident_ok(CHAIN, 48) || !strcmp(CHAIN, "cones")) { errrow("refused chain (bad id or the real chain)", CHAIN); setvar("chain_ok", 0); return 1; }
    { char p[PL + 32]; snprintf(p, sizeof p, "%s/wallet_id.txt", STATE); FILE *f = fopen(p, "r");
      if (!f || !fgets(WALLET, sizeof WALLET, f)) { if (f) fclose(f); errrow("no wallet bound to this entity (wallet_id.txt)", ""); setvar("chain_ok", 0); return 1; }
      fclose(f); WALLET[strcspn(WALLET, "\r\n")] = 0; if (!ident_ok(WALLET, 120)) { errrow("bad wallet id", WALLET); setvar("chain_ok", 0); return 1; } }
    if (!find_root()) { errrow("unknown chain (no chains/<id>/chain.pdl)", CHAIN); setvar("chain_ok", 0); return 1; }
    char exe[PL], out[2048] = ""; char *av[8]; int rc = 1, n = 0;
    if (!strcmp(verb, "balance") && argc == 5) { if (!bin(exe, sizeof exe, "chain_balance")) goto nobin; av[n++] = exe; av[n++] = WALLET; av[n] = NULL; rc = run(av, out, sizeof out);
        if (!rc) setvar("chain_balance", atoll(out)); }
    else if (!strcmp(verb, "send") && argc == 7 && ident_ok(argv[5], 120) && amount_ok(argv[6])) { if (!bin(exe, sizeof exe, "chain_send")) goto nobin; av[n++] = exe; av[n++] = WALLET; av[n++] = argv[5]; av[n++] = argv[6]; av[n] = NULL; rc = run(av, out, sizeof out); }
    else if (!strcmp(verb, "lock") && argc == 7 && ident_ok(argv[5], 100) && amount_ok(argv[6])) { if (!bin(exe, sizeof exe, "chain_escrow")) goto nobin; av[n++] = exe; av[n++] = "lock"; av[n++] = argv[5]; av[n++] = WALLET; av[n++] = argv[6]; av[n++] = WALLET; av[n] = NULL; rc = run(av, out, sizeof out); }
    else if ((!strcmp(verb, "payout") || !strcmp(verb, "refund")) && argc == 8 && ident_ok(argv[5], 100) && ident_ok(argv[6], 120) && amount_ok(argv[7])) { if (!bin(exe, sizeof exe, "chain_escrow")) goto nobin; av[n++] = exe; av[n++] = (char *)verb; av[n++] = argv[5]; av[n++] = WALLET; av[n++] = argv[6]; av[n++] = argv[7]; av[n] = NULL; rc = run(av, out, sizeof out); }
    else if (!strcmp(verb, "status") && argc == 6 && ident_ok(argv[5], 100)) { if (!bin(exe, sizeof exe, "chain_escrow")) goto nobin; av[n++] = exe; av[n++] = "status"; av[n++] = argv[5]; av[n] = NULL; rc = run(av, out, sizeof out);
        if (!rc) { const char *k[4] = {"locked=", "paid_out=", "refunded=", "remaining="}; const char *v[4] = {"chain_locked", "chain_paid", "chain_refunded", "chain_remaining"};
            for (int i = 0; i < 4; i++) { char *p = strstr(out, k[i]); if (p) setvar(v[i], atoll(p + strlen(k[i]))); } } }
    else { errrow("bad verb or arguments", verb); setvar("chain_ok", 0); return 2; }
    out[strcspn(out, "\n")] = 0; ledger(verb, args, rc, out);
    setvar("chain_rc", rc); setvar("chain_ok", rc == 0);
    if (rc) errrow("chain op failed", out);
    return rc ? 1 : 0;
nobin: errrow("chain op binary missing", verb); setvar("chain_ok", 0); return 1;
}
