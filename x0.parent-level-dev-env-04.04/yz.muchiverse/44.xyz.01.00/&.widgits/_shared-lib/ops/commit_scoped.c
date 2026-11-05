/* commit_scoped - the house's scoped commit, as one op (owner 2026-11-05, audit decision D6 in HQ-IQ-BOOK 02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md).
 *   commit_scoped <repo> --branch <own branch> --message-file <file> [--trailer <line>] [--push] -- <path> [<path> ...]
 * What AGENTS.md asks for on every commit, done the same way every time: only YOUR paths, by explicit pathspec, on YOUR branch, nothing swept in from a shared index.
 *   1. refuses unless HEAD is <own branch> (never commits on another tool's branch or main by accident);
 *   2. refuses an empty path list, ".", "-A"/"--all", a path starting with "-", and anything under xyzfs/users (live user data, never added);
 *   3. git add -- <paths>, prints `git diff --cached --name-only`, and says which staged files are NOT yours (they are left staged and are NOT committed: the commit uses the pathspec form
 *      `git commit -F <msg> -- <paths>`, the primitive AGENTS.md names as the safe one);
 *   4. the message comes from a file (long messages, no shell quoting); --trailer <line> is appended once when the message does not already end with it;
 *   5. --push: git push origin <own branch>, never forced, never another branch.
 * Plain fork/exec of git, no shell. Exit 0 committed (and pushed when asked), 1 refused or git failed (the reason on stderr, nothing committed for a refusal), 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#define OUTMAX 262144
static char OUT[OUTMAX]; static const char *REPO;
static int git(char *const *args, int capture) {      /* run `git -C <repo> args...`; stdout into OUT when capture; stderr passes through */
    int pf[2] = {-1, -1}; if (capture && pipe(pf)) return -1; OUT[0] = 0; fflush(stdout);
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { if (capture) { dup2(pf[1], 1); close(pf[0]); close(pf[1]); } char *av[256]; int n = 0; av[n++] = "git"; av[n++] = "-C"; av[n++] = (char *)REPO; for (int i = 0; args[i] && n < 254; i++) av[n++] = args[i]; av[n] = NULL; execvp("git", av); _exit(127); }
    if (capture) { close(pf[1]); size_t used = 0; ssize_t k; while (used < OUTMAX - 1 && (k = read(pf[0], OUT + used, OUTMAX - 1 - used)) > 0) used += (size_t)k; OUT[used] = 0; close(pf[0]); }
    int st = 0; waitpid(p, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static int is_mine(const char *file, char **paths, int np) { for (int i = 0; i < np; i++) { size_t l = strlen(paths[i]); while (l > 1 && paths[i][l - 1] == '/') l--; if (!strncmp(file, paths[i], l) && (file[l] == 0 || file[l] == '/' || paths[i][l - 1] == '/')) return 1; } return 0; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: commit_scoped <repo> --branch <own> --message-file <file> [--trailer <line>] [--push] -- <path>...\n"); return 2; }
    REPO = argv[1]; const char *branch = NULL, *mf = NULL, *trailer = NULL; int push = 0, pi = -1;
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--")) { pi = i + 1; break; } else if (!strcmp(argv[i], "--branch") && i + 1 < argc) branch = argv[++i]; else if (!strcmp(argv[i], "--message-file") && i + 1 < argc) mf = argv[++i]; else if (!strcmp(argv[i], "--trailer") && i + 1 < argc) trailer = argv[++i]; else if (!strcmp(argv[i], "--push")) push = 1; else { fprintf(stderr, "commit_scoped: unknown argument %s\n", argv[i]); return 2; } }
    if (!branch || !mf || pi < 0) { fprintf(stderr, "commit_scoped: --branch, --message-file and -- <paths> are required\n"); return 2; }
    int np = argc - pi; char **paths = &argv[pi]; if (np < 1) { fprintf(stderr, "commit_scoped: REFUSED: no paths (a scoped commit names its files)\n"); return 1; }
    for (int i = 0; i < np; i++) { const char *p = paths[i];
        if (!strcmp(p, ".") || !strcmp(p, "..") || !strcmp(p, "-A") || !strcmp(p, "--all") || p[0] == '-' || !p[0]) { fprintf(stderr, "commit_scoped: REFUSED: path '%s' is not a specific file or folder of yours\n", p); return 1; }
        if (strstr(p, "xyzfs/users")) { fprintf(stderr, "commit_scoped: REFUSED: '%s' is under xyzfs/users (live user data, never committed)\n", p); return 1; } }
    { char *a[] = {"rev-parse", "--abbrev-ref", "HEAD", NULL}; if (git(a, 1) != 0) { fprintf(stderr, "commit_scoped: not a git repo: %s\n", REPO); return 1; } OUT[strcspn(OUT, "\r\n")] = 0;
      if (strcmp(OUT, branch)) { fprintf(stderr, "commit_scoped: REFUSED: HEAD is '%s', your branch is '%s' (never commit on another tool's branch)\n", OUT, branch); return 1; } }
    char msg[OUTMAX / 2]; { FILE *f = fopen(mf, "r"); if (!f) { fprintf(stderr, "commit_scoped: cannot read the message file %s\n", mf); return 1; } size_t k = fread(msg, 1, sizeof msg - 400, f); msg[k] = 0; fclose(f); }
    if (!msg[0]) { fprintf(stderr, "commit_scoped: REFUSED: empty message\n"); return 1; }
    if (trailer && trailer[0]) { size_t l = strlen(msg); while (l && (msg[l - 1] == '\n' || msg[l - 1] == ' ')) msg[--l] = 0; if (!strstr(msg, trailer)) { size_t n = strlen(msg); snprintf(msg + n, sizeof msg - n, "\n\n%s\n", trailer); } else { size_t n = strlen(msg); snprintf(msg + n, sizeof msg - n, "\n"); } }
    char tmpf[] = "/tmp/commit_scoped_XXXXXX"; int fd = mkstemp(tmpf); if (fd < 0) return 1; if (write(fd, msg, strlen(msg)) < 0) { close(fd); unlink(tmpf); return 1; } close(fd);
    char *add[260]; int n = 0; add[n++] = "add"; add[n++] = "--"; for (int i = 0; i < np && n < 258; i++) add[n++] = paths[i]; add[n] = NULL;
    if (git(add, 0) != 0) { fprintf(stderr, "commit_scoped: git add failed\n"); unlink(tmpf); return 1; }
    { char *a[] = {"-c", "core.quotepath=false", "diff", "--cached", "--name-only", NULL}; /* quotepath off: git prints non-ASCII names C-quoted otherwise and they would never match the given paths */ git(a, 1); int mine = 0, foreign = 0; char *save = NULL; char *cp = strdup(OUT);
      for (char *l = strtok_r(cp, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) { if (is_mine(l, paths, np)) mine++; else { foreign++; fprintf(stderr, "commit_scoped: staged but NOT yours (left staged, not committed): %s\n", l); } }
      free(cp); printf("staged for you: %d file(s); not yours and left alone: %d\n", mine, foreign); if (!mine) { fprintf(stderr, "commit_scoped: nothing of yours to commit\n"); unlink(tmpf); return 1; } }
    char *com[270]; n = 0; com[n++] = "commit"; com[n++] = "-q"; com[n++] = "-F"; com[n++] = tmpf; com[n++] = "--"; for (int i = 0; i < np && n < 268; i++) com[n++] = paths[i]; com[n] = NULL;
    int rc = git(com, 0); unlink(tmpf); if (rc != 0) { fprintf(stderr, "commit_scoped: git commit failed (rc=%d)\n", rc); return 1; }
    { char *a[] = {"log", "--oneline", "-1", NULL}; git(a, 1); printf("committed: %s", OUT); }
    if (push) { char *a[] = {"push", "-q", "origin", (char *)branch, NULL}; if (git(a, 0) != 0) { fprintf(stderr, "commit_scoped: push failed (the commit is made)\n"); return 1; } printf("pushed origin %s\n", branch); }
    return 0;
}
