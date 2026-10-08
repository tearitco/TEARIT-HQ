/* desk_restart - restart a house desktop (button.sh reset) so that it can never be left DOWN silently.
 * Origin: 2026-10-07 incident. A remote `button.sh reset` over a NON-login ssh shell killed the desktop, then the rebuild failed (pkg-config lives in
 * /usr/local/bin, not on the non-login PATH) and button.sh deliberately left the desktop down - nobody was told for minutes.
 * This op (1) PREFLIGHTS before anything is killed (house, button.sh, build tools resolvable in the LOGIN shell the child will use, DISPLAY),
 * (2) runs `<shell> -lc 'bash button.sh reset'` detached with a log, (3) WAITS until the child exited AND a taskbar manager of THIS house runs,
 * and always prints one verdict line: RESTART|UP / RESTART|DOWN / RESTART|TIMEOUT / RESTART|REFUSED.
 * Self-contained POSIX (Linux + macOS): fork/exec/waitpid only, no system()/popen, no shared headers. See ../README.md.
 * Exit codes: 0 up (or dry-run ok), 2 usage, 3 refused (nothing killed), 4 down, 5 timeout (child still running).
 * Build: ops/build_desk_restart.sh   Verify: pal harness _shared-lib/harness/desk_restart.pal */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* path buffers are 4096 and every snprintf into them is bounded; a truncated path simply fails to open, so the build uses -Wno-format-truncation */
#define P 4096
#define MAXREQ 16

static const char *MGR = "khtpm_taskbar_manager";

/* ---- small helpers ---- */
static void mkdirs(const char *path) {
    char b[P]; snprintf(b, sizeof b, "%s", path);
    for (char *p = b + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; }
    mkdir(b, 0755);
}

/* read a whole fd into a malloc'd NUL-terminated buffer */
static char *slurp_fd(int fd, size_t *lenp) {
    size_t cap = 8192, n = 0; char *b = malloc(cap); ssize_t r;
    if (!b) return NULL;
    while ((r = read(fd, b + n, cap - n - 1)) > 0) {
        n += (size_t)r;
        if (cap - n < 2) { cap *= 2; char *nb = realloc(b, cap); if (!nb) { free(b); return NULL; } b = nb; }
    }
    b[n] = 0; if (lenp) *lenp = n; return b;
}

/* run argv (no shell), capture stdout (stderr -> /dev/null unless keep_err); return exit code (or 128+sig), -1 on fork failure. *out is malloc'd. */
static int run_capture(char *const argv[], const char *chdir_to, char **out) {
    int pfd[2]; pid_t pid; int st = 0;
    if (out) *out = NULL;
    if (pipe(pfd) != 0) return -1;
    fflush(stdout); fflush(stderr);
    pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); return -1; }
    if (pid == 0) {
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 2); if (dn > 2) close(dn); }
        dup2(pfd[1], 1); close(pfd[0]); close(pfd[1]);
        if (chdir_to && chdir(chdir_to)) {}
        execvp(argv[0], argv); _exit(127);
    }
    close(pfd[1]);
    { char *b = slurp_fd(pfd[0], NULL); if (out) *out = b; else free(b); }
    close(pfd[0]);
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

/* ---- quoting for the shell command string: wrap in single quotes ---- */
static void sq(char *dst, size_t n, const char *s) {
    size_t k = 0; if (k < n) dst[k++] = '\'';
    for (; *s && k + 5 < n; s++) { if (*s == '\'') { memcpy(dst + k, "'\\''", 4); k += 4; } else dst[k++] = *s; }
    if (k < n) dst[k++] = '\'';
    dst[k < n ? k : n - 1] = 0;
}

/* ---- taskbar manager of THIS house: scan `ps -axo pid=,command=` ---- */
static int find_manager(const char *house1, const char *house2) {
    char *av[] = { "ps", "-axo", "pid=,command=", NULL }; char *out = NULL; int found = 0;
    if (run_capture(av, NULL, &out) != 0 || !out) { free(out); return 0; }
    for (char *line = strtok(out, "\n"); line; line = strtok(NULL, "\n")) {
        long pid = strtol(line, NULL, 10);
        if (pid <= 0 || pid == (long)getpid()) continue;
        if (strstr(line, "<defunct>")) continue;
        if (!strstr(line, MGR)) continue;
        if (strstr(line, house1) || (house2[0] && strstr(line, house2))) { found = (int)pid; break; }
    }
    free(out); return found;
}

/* ---- tools the build scripts call ---- */
static int word_in_file(const char *text, const char *w) {
    size_t wl = strlen(w); const char *p = text; int bol = 1;
    for (; *p; p++) {
        if (bol) { const char *q = p; while (*q == ' ' || *q == '\t') q++; if (*q == '#') { while (*p && *p != '\n') p++; if (!*p) break; continue; } bol = 0; }
        if (*p == '\n') { bol = 1; continue; }
        if (strncmp(p, w, wl) != 0) continue;
        char before = p > text ? p[-1] : ' ', after = p[wl];
        int bok = before == ' ' || before == '\t' || before == '\n' || before == '(' || before == '`' || before == ';' || before == '|' || before == '&' || before == '"' || before == '\''
                  || (before == '-' && p - text >= 2 && p[-2] == ':');   /* ${CC:-gcc} */
        int aok = after == 0 || after == ' ' || after == '\t' || after == '\n' || after == ')' || after == '"' || after == '\'' || after == '}' || after == ';';
        if (bok && aok) return 1;
    }
    return 0;
}

static void add_tool(char tools[][64], int *nt, const char *t) {
    for (int i = 0; i < *nt; i++) if (!strcmp(tools[i], t)) return;
    if (*nt < 40) snprintf(tools[(*nt)++], 64, "%s", t);
}

/* is `tool` resolvable in the shell the child will use (login shell, PATH prefix applied)? */
static int tool_ok(const char *shell, const char *prefix, const char *tool) {
    char cmd[P * 2]; char *av[] = { (char *)shell, "-lc", cmd, NULL };
    snprintf(cmd, sizeof cmd, "%scommand -v %s >/dev/null 2>&1", prefix, tool);
    return run_capture(av, NULL, NULL) == 0;
}

static void usage(void) {
    fprintf(stderr, "usage: desk_restart --house <root> [--button <button.sh>] [--display <D>] [--xauthority <F>] [--path-extra <dir[:dir]>]\n"
                    "                    [--wait <sec, default 90>] [--dry-run] [--shell <path, default /bin/bash>] [--require <tool>]...\n");
}

static void echo_tail(const char *path, long from, int nlines, const char *prefix) {
    int fd = open(path, O_RDONLY); if (fd < 0) return;
    if (from > 0) lseek(fd, from, SEEK_SET);
    char *b = slurp_fd(fd, NULL); close(fd); if (!b) return;
    size_t l = strlen(b); while (l && (b[l - 1] == '\n' || b[l - 1] == '\r')) b[--l] = 0;
    char *s = b + l; int seen = 0;
    while (s > b) { s--; if (*s == '\n' && ++seen == nlines) { s++; break; } }
    printf("%s%s\n", prefix, s[0] ? s : "(empty)");
    free(b);
}

int main(int argc, char **argv) {
    const char *house = NULL, *button = NULL, *display = NULL, *xauth = NULL, *pextra = NULL, *shell = "/bin/bash";
    const char *req[MAXREQ]; int nreq = 0, wait_s = 90, dry = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i]; int hasv = i + 1 < argc;
        if (!strcmp(a, "--dry-run")) dry = 1;
        else if (!strcmp(a, "--house") && hasv) house = argv[++i];
        else if (!strcmp(a, "--button") && hasv) button = argv[++i];
        else if (!strcmp(a, "--display") && hasv) display = argv[++i];
        else if (!strcmp(a, "--xauthority") && hasv) xauth = argv[++i];
        else if (!strcmp(a, "--path-extra") && hasv) pextra = argv[++i];
        else if (!strcmp(a, "--shell") && hasv) shell = argv[++i];
        else if (!strcmp(a, "--wait") && hasv) { wait_s = atoi(argv[++i]); if (wait_s < 1) wait_s = 1; }
        else if (!strcmp(a, "--require") && hasv && nreq < MAXREQ) req[nreq++] = argv[++i];
        else { fprintf(stderr, "desk_restart: bad or incomplete argument '%s'\n", a); usage(); return 2; }
    }
    if (!house || !house[0]) { fprintf(stderr, "desk_restart: --house is required\n"); usage(); return 2; }

    char hroot[P], hreal[P] = "", bpath[P], tbdir[P], berr[P], dlog[P];
    snprintf(hroot, sizeof hroot, "%s", house);
    for (size_t l = strlen(hroot); l > 1 && hroot[l - 1] == '/'; ) hroot[--l] = 0;
    if (!realpath(hroot, hreal)) hreal[0] = 0;
    if (!strcmp(hreal, hroot)) hreal[0] = 0;
    if (button) snprintf(bpath, sizeof bpath, "%s", button); else snprintf(bpath, sizeof bpath, "%s/$.crypts/button.sh", hroot);
    snprintf(tbdir, sizeof tbdir, "%s/_.monads/_.livedesk-taskbar/ops", hroot);
    snprintf(berr, sizeof berr, "%s/+x/build_error.log", tbdir);
    snprintf(dlog, sizeof dlog, "%s/#.desktop/desk_restart.log", hroot);

    /* ---- (1) PREFLIGHT: nothing has been killed yet ---- */
    struct stat sb; char reason[P] = "";
    if (stat(hroot, &sb) != 0 || !S_ISDIR(sb.st_mode)) snprintf(reason, sizeof reason, "house-root-missing:%s", hroot);
    else if (stat(bpath, &sb) != 0 || !S_ISREG(sb.st_mode)) snprintf(reason, sizeof reason, "button-sh-missing:%s", bpath);
    else if (access(shell, X_OK) != 0) snprintf(reason, sizeof reason, "shell-not-executable:%s", shell);

    const char *envdisp = getenv("DISPLAY");
    const char *disp = (display && display[0]) ? display : ((envdisp && envdisp[0]) ? envdisp : NULL);
    int has_x = (stat(tbdir, &sb) == 0 && S_ISDIR(sb.st_mode));   /* house with a taskbar = X windows */
    if (!reason[0] && has_x && !disp) snprintf(reason, sizeof reason, "no-display (house has X windows; pass --display or set DISPLAY)");

    /* child shell prefix: login shell may reset PATH, so --path-extra / DISPLAY / XAUTHORITY are re-applied inside the -lc string */
    char prefix[P * 2] = "", q[P];
    if (pextra && pextra[0]) { sq(q, sizeof q, pextra); size_t l = strlen(prefix); snprintf(prefix + l, sizeof prefix - l, "PATH=%s:\"$PATH\"; export PATH; ", q); }
    if (disp) { sq(q, sizeof q, disp); size_t l = strlen(prefix); snprintf(prefix + l, sizeof prefix - l, "DISPLAY=%s; export DISPLAY; ", q); }
    if (xauth && xauth[0]) { sq(q, sizeof q, xauth); size_t l = strlen(prefix); snprintf(prefix + l, sizeof prefix - l, "XAUTHORITY=%s; export XAUTHORITY; ", q); }

    char tools[64][64]; int nt = 0; char missing[P] = ""; char toollist[P] = "";
    if (!reason[0]) {
        static const char *scripts[] = { "build_khtpm_strip.sh", "build_core_render.sh" };
        int want_cc = 0;
        for (int s = 0; s < 2; s++) {
            char sp[P]; snprintf(sp, sizeof sp, "%s/%s", tbdir, scripts[s]);
            int fd = open(sp, O_RDONLY); if (fd < 0) continue;
            char *txt = slurp_fd(fd, NULL); close(fd); if (!txt) continue;
            if (word_in_file(txt, "pkg-config")) add_tool(tools, &nt, "pkg-config");
            if (word_in_file(txt, "make")) add_tool(tools, &nt, "make");
            if (word_in_file(txt, "gcc") || word_in_file(txt, "cc") || word_in_file(txt, "clang")) want_cc = 1;
            free(txt);
        }
        for (int i = 0; i < nreq; i++) add_tool(tools, &nt, req[i]);
        if (want_cc) {   /* any one C compiler will do */
            if (tool_ok(shell, prefix, "gcc")) add_tool(tools, &nt, "gcc");
            else if (tool_ok(shell, prefix, "cc")) add_tool(tools, &nt, "cc");
            else if (tool_ok(shell, prefix, "clang")) add_tool(tools, &nt, "clang");
            else add_tool(tools, &nt, "gcc");   /* will be reported missing */
        }
        for (int i = 0; i < nt; i++) {
            size_t l = strlen(toollist); snprintf(toollist + l, sizeof toollist - l, "%s%s", l ? "," : "", tools[i]);
            if (!tool_ok(shell, prefix, tools[i])) { size_t m = strlen(missing); snprintf(missing + m, sizeof missing - m, "%s%s", m ? "," : "", tools[i]); }
        }
        if (missing[0]) snprintf(reason, sizeof reason, "tool-not-on-PATH:%s (login-shell PATH; use --path-extra)", missing);
    }
    if (reason[0]) { printf("RESTART|REFUSED|%s\n", reason); return 3; }

    int pre = find_manager(hroot, hreal);
    if (dry) {
        printf("RESTART|PREFLIGHT|ok|pre-manager=%d|tools=%s|button=%s\n", pre, toollist[0] ? toollist : "(none)", bpath);
        return 0;
    }

    /* ---- (2) RUN: detached child, login shell, log appended ---- */
    { char d[P]; snprintf(d, sizeof d, "%s/#.desktop", hroot); mkdirs(d); }
    long log_from = 0; { struct stat ls; if (stat(dlog, &ls) == 0) log_from = (long)ls.st_size; }
    long berr_from = 0; { struct stat bs; if (stat(berr, &bs) == 0) berr_from = (long)bs.st_size; }   /* marker rule: judge only bytes that appear after this offset (never mtime) */
    time_t t_start = time(NULL);
    { FILE *lf = fopen(dlog, "a"); if (lf) { fprintf(lf, "=== desk_restart %ld house=%s pre-manager=%d ===\n", (long)t_start, hroot, pre); fclose(lf); } }
    { struct stat ls; if (stat(dlog, &ls) == 0) log_from = (long)ls.st_size; }   /* tail only what this run adds */

    char cmd[P * 3], bq[P]; sq(bq, sizeof bq, bpath);
    snprintf(cmd, sizeof cmd, "%sbash %s reset", prefix, bq);
    fflush(stdout);
    pid_t child = fork();
    if (child < 0) { printf("RESTART|DOWN|fork-failed|log=%s\n", dlog); return 4; }
    if (child == 0) {
        setsid();
        int dn = open("/dev/null", O_RDONLY); if (dn >= 0) { dup2(dn, 0); if (dn > 2) close(dn); }
        int lf = open(dlog, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); if (lf > 2) close(lf); }
        if (pextra && pextra[0]) {   /* also in the env, for shells whose profile does not reset it */
            const char *op = getenv("PATH"); char np[P * 2]; snprintf(np, sizeof np, "%s%s%s", pextra, op && op[0] ? ":" : "", op ? op : ""); setenv("PATH", np, 1);
        }
        if (disp) setenv("DISPLAY", disp, 1);
        if (xauth && xauth[0]) setenv("XAUTHORITY", xauth, 1);
        execl(shell, shell, "-lc", cmd, (char *)NULL); _exit(127);
    }

    /* ---- (3) WAIT ---- */
    int exited = 0, code = 0, mgr = 0; long waited = 0;
    for (;;) {
        int st;
        if (!exited && waitpid(child, &st, WNOHANG) == child) { exited = 1; code = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st); }
        if (exited && code != 0) break;   /* failed: no point waiting */
        if (exited) { mgr = find_manager(hroot, hreal); if (mgr) break; }
        if (waited >= wait_s) break;
        sleep(1); waited++;
    }
    if (exited && code == 0 && mgr) { printf("RESTART|UP|pid=%d|seconds=%ld\n", mgr, waited); return 0; }
    if (!exited) { printf("RESTART|TIMEOUT|child-pid=%d-still-running-after-%ds|log=%s\n", (int)child, wait_s, dlog); echo_tail(dlog, log_from, 5, "  log> "); return 5; }

    /* DOWN: derive the reason */
    char why[P]; int blog = 0;
    {   int fd = open(dlog, O_RDONLY); char *b = NULL;
        if (fd >= 0) { if (log_from > 0) lseek(fd, log_from, SEEK_SET); b = slurp_fd(fd, NULL); close(fd); }
        if (b && (strstr(b, "BUILD FAILED") || strstr(b, "MISSING "))) blog = 1;
        free(b);
        /* build_error.log: only bytes after the pre-run offset count (tee truncates it, so a smaller file means read from 0) */
        fd = open(berr, O_RDONLY);
        if (fd >= 0) {
            struct stat es; long from = (fstat(fd, &es) == 0 && (long)es.st_size >= berr_from) ? berr_from : 0;
            if (from > 0) lseek(fd, from, SEEK_SET);
            b = slurp_fd(fd, NULL); close(fd);
            if (b && (strstr(b, "BUILD FAILED") || strstr(b, "MISSING ") || (code != 0 && b[0]))) blog = 1;
            free(b);
        }
    }
    if (blog) snprintf(why, sizeof why, "build-failed");
    else if (code != 0) snprintf(why, sizeof why, "child-exit-%d", code);
    else snprintf(why, sizeof why, "no-taskbar-after-wait");
    printf("RESTART|DOWN|%s|log=%s\n", why, dlog);
    printf("  !!! THE DESKTOP IS DOWN. Last log lines:\n");
    echo_tail(dlog, log_from, 5, "  log> ");
    if (blog) { struct stat es; if (stat(berr, &es) == 0) echo_tail(berr, 0, 5, "  build_error.log> "); }
    return 4;
}
