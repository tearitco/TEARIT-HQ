/* auto_act - ONE verb into ONE window through its relay, checked.   auto_act --user DIR --manifest FILE --verb ID [--house DIR]
 * Order of checks (each refusal writes `REFUSE | ts | app | verb | why` and returns 4, writes NOTHING to any relay): auto=on, STOP file absent, window in scope (or studio=on), verb in the manifest, verb safety is not OWNER and is not in AUTO owner_gate, budget (AUTO budget_actions_per_min, default 20).
 * Then: window pid alive (pid file named by the manifest APP row), `relay_drive <pid> press <target>` (fork+exec, watchdog), then the manifest expectation: grew <file> (append-only marker: size must grow) | lastline-contains <file> <text> | lastline-absent <file> <text> | none.
 * Result row `ACT | ts | app | verb | tier | result | ms | note` (note = optional NOTE field of the file's last line). Exit 0 PASS, 1 FAIL, 4 refused, 5 budget, 6 window not running, 2 usage.
 * Manifest rows:  APP | name | pid file ;  VERB | id | press target | expect kind | file | text | timeout_s | safety (free|OWNER) ;  NOTE | verb | file | field number (1-based, last line of that file) ;  READY | verb | file | text | timeout_s (before pressing, wait until the file contains the text: a window that is still busy drops the press) ;  RELAY | path (optional) */
#define _GNU_SOURCE
#include "auto_common.c"
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
static void msleep(int m) { usleep((useconds_t)m * 1000); }
static long now_ms(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec * 1000L + t.tv_usec / 1000; }
static int run(char *const argv[], int timeout_s) { pid_t p = fork(); if (p < 0) return -1; if (p == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); } execv(argv[0], argv); _exit(127); } for (int t = 0; t < timeout_s * 10; t++) { int st; if (waitpid(p, &st, WNOHANG) == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; msleep(100); } kill(p, SIGKILL); waitpid(p, NULL, 0); return -1; }
int main(int argc, char **argv) {
    const char *user = NULL, *mf = NULL, *verb = NULL; char house[PL]; if (!getcwd(house, sizeof house)) house[0] = 0; const char *eh = getenv("HOUSE"); if (eh) snprintf(house, sizeof house, "%s", eh);
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--user") && i + 1 < argc) user = argv[++i]; else if (!strcmp(argv[i], "--manifest") && i + 1 < argc) mf = argv[++i]; else if (!strcmp(argv[i], "--verb") && i + 1 < argc) verb = argv[++i]; else if (!strcmp(argv[i], "--house") && i + 1 < argc) snprintf(house, sizeof house, "%s", argv[++i]); else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    if (!user || !mf || !verb) { fprintf(stderr, "usage: auto_act --user DIR --manifest FILE --verb ID [--house DIR]\n"); return 2; }
    char rfile[PL] = "", rtext[128] = ""; int rto = 0; char app[128] = "?", pidf[PL] = "", relay[PL] = "", target[256] = "", ekind[32] = "none", efile[PL] = "", etext[512] = "", safety[32] = "free", nfile[PL] = ""; int to = 10, nfield = 0, have_verb = 0; long ts = (long)time(NULL);
    FILE *m = fopen(mf, "r"); if (!m) { fprintf(stderr, "cannot read manifest %s\n", mf); return 2; } char ln[PL];
    while (fgets(ln, sizeof ln, m)) { if (ln[0] == '#') continue; char *f[9]; int n = ac_split(ln, f, 9);
        if (n >= 3 && !strcmp(f[0], "APP")) { snprintf(app, sizeof app, "%s", f[1]); ac_abs(house, f[2], pidf, sizeof pidf); }
        else if (n >= 2 && !strcmp(f[0], "RELAY")) ac_abs(house, f[1], relay, sizeof relay);
        else if (n >= 8 && !strcmp(f[0], "VERB") && !strcmp(f[1], verb)) { have_verb = 1; snprintf(target, sizeof target, "%s", f[2]); snprintf(ekind, sizeof ekind, "%s", f[3]); ac_abs(house, f[4], efile, sizeof efile); snprintf(etext, sizeof etext, "%s", f[5]); to = atoi(f[6]); if (to < 1) to = 10; snprintf(safety, sizeof safety, "%s", f[7]); }
        else if (n >= 5 && !strcmp(f[0], "READY") && !strcmp(f[1], verb)) { ac_abs(house, f[2], rfile, sizeof rfile); snprintf(rtext, sizeof rtext, "%s", f[3]); rto = atoi(f[4]); }
        else if (n >= 4 && !strcmp(f[0], "NOTE") && !strcmp(f[1], verb)) { ac_abs(house, f[2], nfile, sizeof nfile); nfield = atoi(f[3]); } }
    fclose(m);
    const char *why = NULL; char og[PL] = "", scope[PL] = "";
    if (!ac_is_on(user, "state")) why = "auto is off"; else if (ac_stop_present(user)) why = "STOP file present";
    else if (!ac_is_on(user, "studio") && !(ac_get(user, "scope", scope, sizeof scope) && ac_in_list(scope, app))) why = "window not in scope";
    else if (!have_verb) why = "verb not in manifest"; else if (!strcmp(safety, "OWNER")) why = "owner-gated verb (manifest)"; else if (ac_get(user, "owner_gate", og, sizeof og) && ac_in_list(og, verb)) why = "owner-gated verb (auto.pdl)";
    if (why) { ac_ledger(user, "REFUSE | %ld | %s | %s | %s", ts, app, verb, why); fprintf(stderr, "refused: %s\n", why); return 4; }
    char bv[32]; int budget = ac_get(user, "budget_actions_per_min", bv, sizeof bv) ? atoi(bv) : 20; if (ac_recent_acts(user, 60) >= budget) { ac_ledger(user, "REFUSE | %ld | %s | %s | budget %d per minute reached", ts, app, verb, budget); fprintf(stderr, "refused: budget\n"); return 5; }
    long pid = 0; { FILE *pf = fopen(pidf, "r"); if (pf) { if (fscanf(pf, "%ld", &pid) != 1) pid = 0; fclose(pf); } } if (pid <= 0 || kill((pid_t)pid, 0) != 0) { ac_ledger(user, "REFUSE | %ld | %s | %s | window not running", ts, app, verb); fprintf(stderr, "window not running\n"); return 6; }
    if (!relay[0]) ac_abs(house, "&.widgits/_shared-lib/ops/+x/relay_drive.+x", relay, sizeof relay); if (access(relay, X_OK)) { fprintf(stderr, "relay_drive not built: %s\n", relay); return 2; }
    if (rfile[0]) { int ready = 0; for (int i = 0; i < rto * 5 && !ready; i++) { FILE *rf = fopen(rfile, "r"); char rl[PL]; while (rf && fgets(rl, sizeof rl, rf)) if (strstr(rl, rtext)) ready = 1; if (rf) fclose(rf); if (!ready) msleep(200); } if (!ready) { ac_ledger(user, "ACT | %ld | %s | %s | rules | FAIL | 0 | window never became ready (%s)", ts, app, verb, rtext); fprintf(stderr, "window not ready\n"); return 1; } }
    long before = ac_size(efile); long t0 = now_ms(); char ps[32]; snprintf(ps, sizeof ps, "%ld", pid); char *ra[] = { relay, ps, "press", target, NULL }; int rc = run(ra, 40); int ok = (rc == 0); const char *res = "FAIL"; char note[512] = "press failed";
    if (ok) { snprintf(note, sizeof note, "-"); ok = !strcmp(ekind, "none"); if (ok) { /* no expectation */ } else { for (int i = 0; i < to * 5 && !ok; i++) { char last[PL];
            if (!strcmp(ekind, "grew")) ok = ac_size(efile) > before; else if (!strcmp(ekind, "lastline-contains")) ok = ac_last_line(efile, last, sizeof last) && strstr(last, etext) && ac_size(efile) > before; else if (!strcmp(ekind, "lastline-absent")) ok = ac_size(efile) > before && ac_last_line(efile, last, sizeof last) && !strstr(last, etext);
            if (!ok) msleep(200); }
            if (!ok) snprintf(note, sizeof note, "expectation %s not met in %d s", ekind, to); else if (!strcmp(ekind, "grew") && etext[0]) { char last[PL]; if (ac_last_line(efile, last, sizeof last) && strstr(last, etext)) { /* grew AND last line contains: also required when text is given */ } else { ok = 0; snprintf(note, sizeof note, "file grew but last line lacks \"%s\"", etext); } } }
        if (ok && nfile[0] && nfield > 0) { char last[PL]; if (ac_last_line(efile[0] && !strcmp(nfile, efile) ? efile : nfile, last, sizeof last)) { char *f[16]; int n = ac_split(last, f, 16); if (n >= nfield) snprintf(note, sizeof note, "%s", f[nfield - 1]); } } }
    if (ok) res = "PASS"; long ms = now_ms() - t0; ac_ledger(user, "ACT | %ld | %s | %s | rules | %s | %ld | %s", ts, app, verb, res, ms, note); printf("%s %s %s (%ld ms) %s\n", res, app, verb, ms, note); return ok ? 0 : 1;
}
