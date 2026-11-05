#define _GNU_SOURCE
/* py_lint - the ratchet against new Python in the repo (owner 2026-11-04: "we really dont allow any python"). Harnesses, ops and tools here are pal + cases + C, never Python.
 *   py_lint <repo_root> <baseline_file>
 * Lists every TRACKED *.py (git ls-files, fork/exec, no shell) and compares it with the baseline file. Baseline rows: `<path>` = grandfathered (tolerated, may only shrink: never add a row to cover a new file),
 * `OWED <path>` = Python that must be rewritten as pal/C (tolerated for now, listed on every run), `#` comment. Output, one line each:
 *   NEW_PY <path>     a tracked .py that is in no baseline row            -> exit 1
 *   OWED_PY <path>    listed as owed                                       -> exit 0, but listed so the debt stays visible
 *   GONE <path>       a baseline row whose file is no longer tracked       -> exit 0 (delete the row: the list only shrinks)
 *   py_files=N grandfathered=G owed=O new=W
 * Exit 0 = no new Python, 1 = new Python, 2 = usage / git failed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#define MAXF 4096
static char *tracked[MAXF], *base[MAXF]; static int bowed[MAXF], bseen[MAXF], nt, nb;
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: py_lint <repo_root> <baseline_file>\n"); return 2; }
    int pfd[2]; if (pipe(pfd)) return 2; pid_t pid = fork(); if (pid < 0) return 2;
    if (pid == 0) { dup2(pfd[1], 1); close(pfd[0]); close(pfd[1]); execlp("git", "git", "-C", argv[1], "ls-files", "-z", "--", "*.py", (char *)NULL); _exit(127); }
    close(pfd[1]); static char buf[1 << 20]; size_t n = 0; ssize_t r; while ((r = read(pfd[0], buf + n, sizeof buf - 1 - n)) > 0) n += (size_t)r; buf[n] = 0; close(pfd[0]);
    int st = 0; waitpid(pid, &st, 0); if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { fprintf(stderr, "py_lint: git ls-files failed\n"); return 2; }
    for (size_t i = 0; i < n && nt < MAXF; ) { tracked[nt++] = buf + i; i += strlen(buf + i) + 1; }
    FILE *f = fopen(argv[2], "r"); if (!f) { fprintf(stderr, "py_lint: no baseline file %s\n", argv[2]); return 2; }
    char ln[PATH_MAX + 16]; while (fgets(ln, sizeof ln, f) && nb < MAXF) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0] || ln[0] == '#') continue; int owed = 0; char *p = ln; if (!strncmp(p, "OWED ", 5)) { owed = 1; p += 5; } base[nb] = strdup(p); bowed[nb] = owed; nb++; }
    fclose(f);
    int neu = 0, owed = 0, grand = 0;
    for (int i = 0; i < nt; i++) { int hit = -1; for (int j = 0; j < nb; j++) if (!strcmp(tracked[i], base[j])) { hit = j; break; }
        if (hit < 0) { printf("NEW_PY %s\n", tracked[i]); neu++; } else { bseen[hit] = 1; if (bowed[hit]) { printf("OWED_PY %s\n", tracked[i]); owed++; } else grand++; } }
    for (int j = 0; j < nb; j++) if (!bseen[j]) printf("GONE %s\n", base[j]);
    printf("py_files=%d grandfathered=%d owed=%d new=%d\n", nt, grand, owed, neu);
    return neu ? 1 : 0;
}
