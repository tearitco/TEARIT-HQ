/* vox_keys_op - (D115; a KEYMAP row with a 5th field, a command with $D = house root, runs that command instead of vox_op act: the RTA window ships such a keys.pdl, D116) the keys of the Voxel window (D115). The window's input op forwards a listed key ("<code> <ms>") to <app>/state/interact_relay.txt WHILE the xelector possesses a game-driven token (GAMEKEY rows, D101); this op turns each relayed key into ONE `vox_op act` call. It has no rule and no key of its own: the key -> act table is <data>/keys.pdl (KEYMAP rows), every rule stays in vox_op.
 * Usage: vox_keys_op once <data> <ledger> <relay> <offset>   read the relay bytes after <offset>, run each mapped key, print `KEYS read=N applied=A refused=R unmapped=U next=OFFSET`
 *        vox_keys_op tail <data> <ledger> <relay> <owner_pid>  the window's daemon: the same, polled every 250 ms from the END of the relay (keys pressed before it started are never replayed); exits when <owner_pid> is gone; the relay is truncated when it passes 65536 bytes (the cap, nothing else is written)
 *        vox_keys_op keybinds <keys.pdl> <keybinds.pdl>        append the missing `GAMEKEY | <act> | <code>` rows (idempotent)
 * A refused act (exit != 0) writes nothing to the ledger (vox_op's own rule) and is only counted. The board and panel are rewritten by `act` with the BOARD rows of <ledger dir>/vox_board.pdl.
 * Cost: idle = 4 wakeups/s of a stat() (no children); active = one vox_op fork per key. Files written: none by this op. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#define MK 32
typedef struct { int code; char act[16]; char cmd[512]; } Km;
static Km KM[MK]; static int nk; static char ACTOR[64] = "joe";
static char *fld(char *s, char **next) { while (*s == ' ') s++; char *b = strchr(s, '|'); if (b) { *b = 0; *next = b + 1; } else *next = NULL; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int load(const char *data) {
    char p[2048], l[512]; snprintf(p, sizeof p, "%s/keys.pdl", data); FILE *f = fopen(p, "r"); if (!f) return -1; nk = 0;
    while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; char *n, *t = fld(l, &n); if (!n) continue;
        if (!strcmp(t, "ACTOR")) { char *v = fld(n, &n); snprintf(ACTOR, sizeof ACTOR, "%s", v); }
        else if (!strcmp(t, "KEYMAP") && nk < MK) { char *c = fld(n, &n); if (!n) continue; char *a = fld(n, &n); KM[nk].code = atoi(c); snprintf(KM[nk].act, sizeof KM[nk].act, "%s", a); KM[nk].cmd[0] = 0; if (n) { fld(n, &n); if (n) { char *cm = fld(n, &n); snprintf(KM[nk].cmd, sizeof KM[nk].cmd, "%s", cm); } } nk++; } }
    fclose(f); return nk ? 0 : -1;
}
static int run_cmd(const char *ledger, const char *tmpl) {   /* a KEYMAP row with a 5th field runs that command instead of vox_op act: words split on spaces, $D = the house root (two levels above the ledger); no shell */
    char house[2048], b[1024], *w[40]; int nw = 0; snprintf(house, sizeof house, "%s", ledger); for (int i = 0; i < 2; i++) { char *s = strrchr(house, '/'); if (!s) return 2; *s = 0; }
    snprintf(b, sizeof b, "%s", tmpl); for (char *t = strtok(b, " "); t && nw < 39; t = strtok(NULL, " ")) { char *x = malloc(4096); if (!strncmp(t, "$D", 2)) snprintf(x, 4096, "%s%s", house, t + 2); else snprintf(x, 4096, "%s", t); w[nw++] = x; } w[nw] = NULL;
    pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { if (!freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr)) _exit(126); execv(w[0], w); _exit(127); }
    int st; waitpid(c, &st, 0); for (int i = 0; i < nw; i++) free(w[i]); return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
static int run_act(const char *data, const char *ledger, const char *act) {
    char bp[2200], ln[2300], desk[2048] = "", feed[2048] = "", png[2048] = "", self[2048] = ""; snprintf(bp, sizeof bp, "%s", ledger); char *sl = strrchr(bp, '/'); if (!sl) return 2; snprintf(sl, sizeof bp - (size_t)(sl - bp), "/vox_board.pdl");
    FILE *f = fopen(bp, "r"); if (f) { while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char k[32], v[2048]; if (sscanf(ln, "BOARD | %31[^|] | %2047[^\n]", k, v) != 2) continue; for (char *e = k + strlen(k); e > k && e[-1] == ' '; ) *--e = 0;
            if (!strcmp(k, "desk")) snprintf(desk, sizeof desk, "desk=%s", v); else if (!strcmp(k, "feed")) snprintf(feed, sizeof feed, "feed=%s", v); else if (!strcmp(k, "png")) snprintf(png, sizeof png, "png=%s", v); else if (!strcmp(k, "self")) snprintf(self, sizeof self, "%s", v); } fclose(f); }
    char vox[2300]; snprintf(vox, sizeof vox, "%s", self[0] ? self : ""); if (!vox[0]) snprintf(vox, sizeof vox, "%s/ops/+x/vox_op.+x", data);
    pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr); execl(vox, vox, "act", data, ledger, ACTOR, act, desk, feed, png, (char *)NULL); _exit(127); }
    int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
static long apply(const char *data, const char *ledger, const char *relay, long off, int *rd, int *ap, int *rf, int *um) {
    FILE *f = fopen(relay, "r"); if (!f) return off; if (fseek(f, off, SEEK_SET)) { fclose(f); return off; }
    char l[128]; long pos = off;
    while (fgets(l, sizeof l, f)) { size_t n = strlen(l); if (!n || l[n - 1] != '\n') break; pos += (long)n; int code = atoi(l); (*rd)++;
        int hit = -1; for (int i = 0; i < nk; i++) if (KM[i].code == code) { hit = i; break; }
        if (hit < 0) { (*um)++; continue; }
        if ((KM[hit].cmd[0] ? run_cmd(ledger, KM[hit].cmd) : run_act(data, ledger, KM[hit].act)) == 0) (*ap)++; else (*rf)++; }
    fclose(f); return pos;
}
static int keybinds(const char *kp, const char *bp) {
    FILE *f = fopen(kp, "r"); if (!f) return 2; char l[512], have[65536] = ""; FILE *b = fopen(bp, "r"); if (b) { size_t n = fread(have, 1, sizeof have - 1, b); have[n] = 0; fclose(b); }
    b = fopen(bp, "a"); if (!b) { fclose(f); return 2; }
    while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; char *n, *t = fld(l, &n); if (!n || strcmp(t, "KEYMAP")) continue; char *c = fld(n, &n); if (!n) continue; char *a = fld(n, &n); char row[128]; snprintf(row, sizeof row, "GAMEKEY      | %s | %s", a, c);
        char *at = strstr(have, row); size_t rl = strlen(row); if (at && (at[rl] == '\n' || at[rl] == 0)) continue; fprintf(b, "%s\n", row); }
    fclose(f); fclose(b); return 0;
}
int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "keybinds")) return keybinds(argv[2], argv[3]);
    if (argc != 6) { fprintf(stderr, "usage: vox_keys_op once|tail <data> <ledger> <relay> <offset|owner_pid> | keybinds <keys.pdl> <keybinds.pdl>\n"); return 2; }
    if (load(argv[2])) { fprintf(stderr, "missing %s/keys.pdl\n", argv[2]); return 9; }
    if (!strcmp(argv[1], "once")) { int rd = 0, ap = 0, rf = 0, um = 0; long nx = apply(argv[2], argv[3], argv[4], atol(argv[5]), &rd, &ap, &rf, &um); printf("KEYS read=%d applied=%d refused=%d unmapped=%d next=%ld\n", rd, ap, rf, um, nx); return 0; }
    if (!strcmp(argv[1], "tail")) { pid_t owner = (pid_t)atol(argv[5]); struct stat s; long cur = stat(argv[4], &s) ? 0 : (long)s.st_size;
        for (;;) { if (kill(owner, 0) && errno == ESRCH) return 0; struct timespec ts = { 0, 250000000 }; nanosleep(&ts, NULL);
            if (stat(argv[4], &s)) continue; if ((long)s.st_size < cur) cur = 0; if ((long)s.st_size > cur) { int a = 0, b = 0, c = 0, d = 0; cur = apply(argv[2], argv[3], argv[4], cur, &a, &b, &c, &d); }
            if (s.st_size > 65536 && cur >= (long)s.st_size) { if (truncate(argv[4], 0) == 0) cur = 0; } } }
    return 2;
}
