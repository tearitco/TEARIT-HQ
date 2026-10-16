/* auto_plan - run a list of verbs (T0 rules driver: no model).   auto_plan --user DIR --manifest FILE --plan FILE [--house DIR] [--keep-going]
 * Plan rows:  DO | verb | count   (count default 1). Each action is one auto_act call (all of its checks apply; a refusal or FAIL stops the plan unless --keep-going). Prints one line per step and `PLAN | done | passed | failed | refused` ; exit 0 only if every step passed.
 * auto_act is looked up next to this binary. */
#define _GNU_SOURCE
#include "auto_common.c"
#include <fcntl.h>
#include <sys/wait.h>
int main(int argc, char **argv) {
    const char *user = NULL, *mf = NULL, *plan = NULL, *house = NULL; int keep = 0;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--user") && i + 1 < argc) user = argv[++i]; else if (!strcmp(argv[i], "--manifest") && i + 1 < argc) mf = argv[++i]; else if (!strcmp(argv[i], "--plan") && i + 1 < argc) plan = argv[++i]; else if (!strcmp(argv[i], "--house") && i + 1 < argc) house = argv[++i]; else if (!strcmp(argv[i], "--keep-going")) keep = 1; else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    if (!user || !mf || !plan) { fprintf(stderr, "usage: auto_plan --user DIR --manifest FILE --plan FILE [--house DIR] [--keep-going]\n"); return 2; }
    char self[PL], act[PL]; ssize_t k = readlink("/proc/self/exe", self, sizeof self - 1); if (k <= 0) return 1; self[k] = 0; char *sl = strrchr(self, '/'); if (sl) *sl = 0; snprintf(act, sizeof act, "%s/auto_act.+x", self);
    FILE *p = fopen(plan, "r"); if (!p) { fprintf(stderr, "cannot read plan\n"); return 2; } char ln[PL]; int done = 0, pass = 0, fail = 0, ref = 0, stop = 0;
    while (!stop && fgets(ln, sizeof ln, p)) { if (ln[0] == '#') continue; char *f[4]; int n = ac_split(ln, f, 4); if (n < 2 || strcmp(f[0], "DO")) continue; int cnt = n >= 3 ? atoi(f[2]) : 1; if (cnt < 1) cnt = 1;
        for (int i = 0; i < cnt && !stop; i++) { pid_t c = fork(); if (c == 0) { char *a[12]; int na = 0; a[na++] = act; a[na++] = "--user"; a[na++] = (char *)user; a[na++] = "--manifest"; a[na++] = (char *)mf; a[na++] = "--verb"; a[na++] = f[1]; if (house) { a[na++] = "--house"; a[na++] = (char *)house; } a[na] = NULL; execv(act, a); _exit(127); }
            int st = 0; waitpid(c, &st, 0); int rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1; done++; if (rc == 0) pass++; else if (rc == 1) fail++; else ref++; printf("step %d %s %d rc=%d\n", done, f[1], i + 1, rc); if (rc != 0 && !keep) stop = 1; } }
    fclose(p); ac_ledger(user, "PLAN | %ld | %s | done %d passed %d failed %d refused %d", (long)time(NULL), plan, done, pass, fail, ref); printf("PLAN | %d | %d | %d | %d\n", done, pass, fail, ref); return (fail || ref) ? 1 : 0;
}
