/* case_run - tiny generic case runner for ops (not windows).   case_run <cases.pdl>   rows:  CASE | name | expected exit code | stdout+stderr must contain (or - for nothing) | must NOT contain (or -) | shell command   (in the two text fields write ~ for | )
 * The command runs with `sh -c` in the current environment (so a case file can use $AUTO_USER, $HOUSE...). PASS/FAIL per case, exit 1 on any FAIL. Used as the proof harness for the auto ops (negative cases are ordinary rows). */
#define _GNU_SOURCE
#include "auto_common.c"
#include <fcntl.h>
#include <sys/wait.h>
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: case_run cases.pdl\n"); return 2; } FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; } char ln[8192]; int pass = 0, fail = 0;
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; char *fd[6]; int n = ac_split(ln, fd, 6); if (n < 6 || strcmp(fd[0], "CASE")) continue; int want = atoi(fd[2]); for (char *q = fd[3]; *q; q++) if (*q == '~') *q = '|'; for (char *q = fd[4]; *q; q++) if (*q == '~') *q = '|'; char tmp[64]; snprintf(tmp, sizeof tmp, "/tmp/case_run_%d.out", (int)getpid());
        pid_t c = fork(); if (c == 0) { int o = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600); dup2(o, 1); dup2(o, 2); int nul = open("/dev/null", O_RDONLY); dup2(nul, 0); execl("/bin/sh", "sh", "-c", fd[5], (char *)NULL); _exit(127); }
        int st = 0; waitpid(c, &st, 0); int rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1; char out[16384]; size_t k = 0; FILE *o = fopen(tmp, "r"); if (o) { k = fread(out, 1, sizeof out - 1, o); fclose(o); } out[k] = 0; unlink(tmp);
        int ok = (rc == want); const char *why = ""; if (ok && strcmp(fd[3], "-") && !strstr(out, fd[3])) { ok = 0; why = " (missing expected text)"; } if (ok && strcmp(fd[4], "-") && strstr(out, fd[4])) { ok = 0; why = " (forbidden text present)"; } if (!ok && !*why) why = " (exit code)";
        if (ok) pass++; else { fail++; } printf("%s %s (exit %d, wanted %d)%s\n", ok ? "PASS" : "FAIL", fd[1], rc, want, why); if (!ok) { char *e = out + (strlen(out) > 300 ? strlen(out) - 300 : 0); printf("   output tail: %s\n", e); } }
    fclose(f); printf("%s: %d pass, %d fail\n", fail ? "FAILED" : "ALL PASS", pass, fail); return fail ? 1 : 0;
}
