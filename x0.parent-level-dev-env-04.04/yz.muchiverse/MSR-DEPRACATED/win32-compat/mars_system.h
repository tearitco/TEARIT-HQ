/* mars_system.h - Windows translation layer for the project's system() calls.
 *
 * INJECTED VIA `gcc -include`, NOT #included by any .c file, so the Linux
 * build is completely unaffected and no source file is edited.
 *
 * WHY THIS IS NEEDED
 *   60 system() calls across 14 files. Enumerated from the sources, the
 *   distinct shapes are:
 *     1. ./+x/NAME.+x                   self-launching sibling binaries
 *     2. cls / clear                         clear the screen
 *     3. kill $(cat data/X.pid) 2>/dev/null  stop the clock loop
 *     4. rm [-r|-f|-rf] DIR|DIR/*            clear generated state
 *     5. mkdir -p DIR                        create a directory
 *     6. >DIR/file                            create/truncate a file
 *     7. cp -r SRC DST/                       seed presets into data/
 *     8. ./analysis_loop                      a bare ./ sibling call
 *     9. a && b                               sequencing
 *
 *   Patterns 3, 4, 6, 7 and 9 cannot be shimmed by putting something on
 *   PATH: cmd.exe does not parse $(...) command substitution, has no `rm`
 *   or `cp`, and does not accept a forward slash in a redirection target
 *   (`>data/x.txt` -> "The syntax of the command is incorrect."). So the
 *   string has to be understood before it ever reaches a shell. Doing
 *   that here means one implementation instead of ~22 edits over 5 files.
 *
 * FIDELITY NOTE ON PATTERN 8
 *   setup_corporations.c:468 calls system("./analysis_loop"), but the
 *   binary this project actually builds is `analysis_loop.+x`. That call
 *   is therefore already dead code on Linux - bash reports
 *   "No such file or directory" and the non-zero result is the value the
 *   C code sees. It is translated faithfully to ".\analysis_loop" so
 *   Windows fails the same way rather than quietly succeeding where Linux
 *   does not. Fixing it is a change to the game, not to this port.
 *
 * WHAT IS DELIBERATELY NOT EMULATED
 *   The `&` at the end of "./+x/wsr_clock.+x &" is shell backgrounding.
 *   Those are turned into a detached start, but the C code still blocks
 *   for them, so the program's own sequencing is unchanged from Linux.
 */
#ifndef MARS_WIN32_SYSTEM_H
#define MARS_WIN32_SYSTEM_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* STDOIO MUST NOT BE BLOCK-BUFFERED. THIS IS NOT A NICETY.
 *
 * The Linux orchestrator prepends "stdbuf -oL " to every command
 * (l.button.<emoji>.g11.c:116, and the macOS twin at :120) precisely to
 * force line buffering. Drop that on Windows and the game appears frozen
 * on an empty screen: every one of these programs writes its UI with
 * ordinary printf, and when stdout is a file rather than a console the C
 * runtime block-buffers it. 4 KB of menu never reaches the orchestrator's
 * log, so nothing is echoed, so the screen stays blank FOREVER - the
 * process is alive and sitting at its first prompt the whole time, holding
 * the menu in a buffer it will only flush on exit.
 *
 * That is a genuinely confusing failure: it looks exactly like a hang or a
 * crash. It also means the "it works" evidence gathered before this line
 * existed was misleading - the menu had been visible only in runs where
 * the process was subsequently killed or fed 'n', and killing it is what
 * flushed the buffer.
 *
 * _IONBF is used rather than _IOLBF on purpose: the Microsoft C runtime
 * does not honour _IOLBF and quietly treats it as full buffering, which
 * would reproduce the identical bug. Unbuffered is slower per write, but
 * these are line-oriented console programs and the alternative is a
 * permanently blank screen.
 *
 * A GCC constructor runs before main and therefore before anything has
 * been written to stdout, which is exactly the window setvbuf needs. This
 * reaches all 31 binaries because the build force-includes this header
 * into every translation unit - no .c is edited.
 */
#if defined(__GNUC__)
__attribute__((constructor))
static void mars_unbuffer_stdout(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
}
#endif

/* Runs a command line detached, returning immediately. */
static void mars_run_detached(const char *cmd) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char buf[2048];

    snprintf(buf, sizeof(buf), "cmd.exe /c %s", cmd);
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    /* DETACHED_PROCESS so the child survives this one exiting. */
    if (CreateProcessA(NULL, buf, NULL, NULL, FALSE,
                       DETACHED_PROCESS | CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
/* Duplicate a std handle as an inheritable one, so it can be handed to a
 * child through STARTF_USESTDHANDLES. Returns the ORIGINAL handle unchanged
 * (and a marker of NULL) if the source is unusable - a missing or already
 * closed std handle must not be fatal, it just degrades to the child's
 * default. `*was_dup` lets the caller close only what it owns. */
static HANDLE mars_inherit(HANDLE src, int *was_dup) {
    HANDLE copy = NULL;
    if (was_dup) *was_dup = 0;
    if (src == NULL || src == INVALID_HANDLE_VALUE) return NULL;
    if (!DuplicateHandle(GetCurrentProcess(), src, GetCurrentProcess(),
                         &copy, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return src;                  /* duplication failed: use as-is */
    }
    if (was_dup) *was_dup = 1;
    return copy;
}

/* Closes ONLY handles this shim actually created. Closing an original std
 * handle would break the parent's own stdio, so ownership is tracked rather
 * than assumed. */
static void mars_close_dup(HANDLE h, int was_dup) {
    if (was_dup && h) CloseHandle(h);
}

/* Runs a command line, waiting, with its console output inherited.
 *
 * The child's exit code is RETURNED, which is the whole point: POSIX
 * system() gives the caller the command's status, and setup_corporations.c
 * branches on it. An earlier version of this shim always returned 0,
 * which made setup_corporations.c:468 take the success branch and print
 * "Analysis completed successfully" even though the command had failed -
 * i.e. the port lied to the game. Do not "simplify" this back to 0.
 *
 * The std handles are passed EXPLICITLY, and that is not incidental. This
 * shim sits three levels deep: the orchestrator redirects main.+x's stdout
 * to a log, main.+x calls system("game_setup"), and game_setup calls
 * system("./+x/game.+x"). With plain CreateProcess and no
 * STARTF_USESTDHANDLES, the grandchild is handed the console's own handles
 * rather than the ones its parent was given, so game.+x paints straight to
 * the terminal and the orchestrator's tee never sees a byte of the menu.
 * Under Linux the same chain inherits the pipe all the way down and `tee`
 * captures everything. Duplicating the handles as inheritable is what
 * restores that behaviour.
 */
static int mars_run_console(const char *cmd) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char buf[2048];
    DWORD code = 0;
    HANDLE hIn, hOut, hErr;
    int dIn = 0, dOut = 0, dErr = 0;

    snprintf(buf, sizeof(buf), "cmd.exe /c %s", cmd);
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    /* Duplicate rather than reuse: GetStdHandle returns handles that are
     * typically NOT inheritable, and CreateProcess ignores
     * STARTF_USESTDHANDLES entries that cannot survive inheritance. */
    hIn  = mars_inherit(GetStdHandle(STD_INPUT_HANDLE),  &dIn);
    hOut = mars_inherit(GetStdHandle(STD_OUTPUT_HANDLE), &dOut);
    hErr = mars_inherit(GetStdHandle(STD_ERROR_HANDLE),  &dErr);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = hIn;
    si.hStdOutput = hOut;
    si.hStdError  = hErr;

    if (!CreateProcessA(NULL, buf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        mars_close_dup(hIn, dIn);
        mars_close_dup(hOut, dOut);
        mars_close_dup(hErr, dErr);
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code)) code = 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    mars_close_dup(hIn, dIn);
    mars_close_dup(hOut, dOut);
    mars_close_dup(hErr, dErr);
    return (int)code;
}

/* Forward slashes to backslashes, in place. cmd.exe accepts them in most
 * positions but not in a redirection target, so paths are normalised
 * before being handed over rather than after failing. */
static void mars_fix_slashes(char *s) {
    for (; *s; s++) if (*s == '/') *s = '\\';
}

/* kill $(cat FILE) 2>/dev/null  ->  taskkill /PID <n> /F
 * A missing or empty pid file is NOT an error on Linux either: the shell
 * simply produces no pid and `kill` complains into /dev/null. So this
 * returns success without doing anything in that case, which is what the
 * original call sites rely on to avoid aborting shutdown. */
static int mars_kill_from_pidfile(const char *path) {
    FILE *f = fopen(path, "r");
    long pid = 0;
    char cmd[64];

    if (!f) return 0;               /* nothing to kill; matches the && */
    if (fscanf(f, "%ld", &pid) != 1) { fclose(f); return 0; }
    fclose(f);
    if (pid <= 0) return 0;

    snprintf(cmd, sizeof(cmd), "taskkill /PID %ld /F", pid);
    mars_run_console(cmd);
    return 0;
}

/* rm [-r] TARGET
 * The `/*` suffix is the meaningful part: "rm -r DIR/*" clears DIR's
 * contents and LEAVES DIR, while "rm -rf DIR" removes DIR as well. Both
 * occur in the sources (file_submenu.c vs game_setup.c) and they are not
 * interchangeable, so the suffix is honoured rather than flattened. */
static int mars_rm_one(const char *target) {
    char path[1024];
    char cmd[1200];
    size_t len = strlen(target);
    int contents_only;
    DWORD attr;

    while (len > 1 && (target[len - 1] == '/' || target[len - 1] == '\\')) len--;
    if (len == 0) return 0;
    if (len >= sizeof(path)) len = sizeof(path) - 1;
    memcpy(path, target, len);
    path[len] = '\0';
    mars_fix_slashes(path);

    contents_only = (len >= 2 && path[len - 1] == '*' &&
                     (path[len - 2] == '\\' || path[len - 2] == '/'));
    if (contents_only) {
        len -= 2;                    /* drop the "/*" */
        path[len] = '\0';
    }
    if (len == 0) return 0;

    /* A plain file target is not a tree: `del /s` on "file\*" matches
     * nothing and would silently do nothing. */
    attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        snprintf(cmd, sizeof(cmd), "del /f /q \"%s\" >NUL 2>NUL", path);
        return mars_run_console(cmd);
    }

    snprintf(cmd, sizeof(cmd), "del /f /s /q \"%s\\*\" >NUL 2>NUL", path);
    mars_run_console(cmd);

    if (!contents_only) {
        snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" >NUL 2>NUL", path);
        mars_run_console(cmd);
    }
    return 0;
}

/* rm with flag parsing. -r and -f may appear in either order, alone, or
 * combined; all of the call sites use one of those four forms. */
static int mars_rm(const char *args) {
    char target[1024];
    size_t t = 0;
    int force = 0;
    (void)force;                     /* -f only means "ignore errors", and
                                     * every command here already sends
                                     * stderr to NUL. */

    while (*args == ' ' || *args == '\t') args++;
    while (*args == '-') {
        args++;
        while (*args && *args != ' ' && *args != '\t') {
            if (*args == 'f' || *args == 'F') force = 1;
            args++;
        }
        while (*args == ' ' || *args == '\t') args++;
    }

    while (*args && *args != ' ' && *args != '\t' && t < sizeof(target) - 1)
        target[t++] = *args++;
    target[t] = '\0';
    if (t == 0) return 0;
    return mars_rm_one(target);
}

/* cp -r SRC DST/  ->  robocopy
 * robocopy is used rather than xcopy because its exit code is a bitmask
 * (<8 = success) and because it does not prompt or spew a per-file
 * banner, which matters when this is called during game setup. */
static int mars_cp(const char *args) {
    char src[1024], dst[1024], cmd[2400];
    size_t n = 0;

    while (*args == ' ' || *args == '\t') args++;
    while (*args == '-') {
        args++;
        while (*args && *args != ' ' && *args != '\t') args++;
        while (*args == ' ' || *args == '\t') args++;
    }
    while (*args && *args != ' ' && *args != '\t' && n < sizeof(src) - 1) src[n++] = *args++;
    src[n] = '\0';
    while (*args == ' ' || *args == '\t') args++;
    n = 0;
    while (*args && *args != ' ' && *args != '\t' && n < sizeof(dst) - 1) dst[n++] = *args++;
    dst[n] = '\0';
    if (!src[0] || !dst[0]) return 0;

    mars_fix_slashes(src);
    mars_fix_slashes(dst);

    /* The trailing slash MUST be stripped. `cp -r SRC data/` becomes
     * "data\", and robocopy treats a destination that ends in a separator
     * as malformed: it exits 16 and copies nothing at all, so data/ was
     * silently never created. robocopy already copies the CONTENTS of SRC
     * into DST and creates DST if absent, which is exactly what the
     * trailing slash meant, so dropping it changes no semantics. */
    {
        size_t dl = strlen(dst);
        while (dl > 1 && (dst[dl - 1] == '\\' || dst[dl - 1] == '/')) dst[--dl] = '\0';
    }

    snprintf(cmd, sizeof(cmd),
             "robocopy \"%s\" \"%s\" /E /NFL /NDL /NJH /NJS /NC /NS /NP >NUL 2>NUL",
             src, dst);
    return mars_run_console(cmd);
}

/* One command, after `&&` splitting. */
static int mars_exec_one(const char *raw) {
    char cmd[2048];
    const char *p = raw;
    size_t n;
    int detached = 0;

    if (!raw) return -1;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return 0;

    /* --- clear-screen ------------------------------------------------- */
    if (strncmp(p, "clear", 5) == 0) { mars_run_console("cls"); return 0; }
    if (strncmp(p, "cls", 3) == 0)   { mars_run_console("cls"); return 0; }

    /* --- >FILE / >>FILE  (create or truncate) -------------------------- */
    /* Done in C rather than via a shell. `>data/x.txt` is the exact call
     * that produced "The syntax of the command is incorrect." on cmd,
     * which will not take a forward slash in a redirection target. */
    if (*p == '>') {
        int append = 0;
        FILE *f;
        p++;
        if (*p == '>') { append = 1; p++; }
        while (*p == ' ' || *p == '\t') p++;
        f = fopen(p, append ? "ab" : "wb");
        if (f) fclose(f);
        return 0;
    }

    /* --- kill $(cat FILE) 2>/dev/null ---------------------------------- */
    if (strncmp(p, "kill $(cat ", 11) == 0) {
        const char *q = p + 11;
        char path[512];
        n = 0;
        while (*q && *q != ')' && *q != ' ' && n < sizeof(path) - 1) path[n++] = *q++;
        path[n] = '\0';
        return mars_kill_from_pidfile(path);
    }

    /* --- rm ------------------------------------------------------------- */
    if (p[0] == 'r' && p[1] == 'm' && (p[2] == ' ' || p[2] == '-'))
        return mars_rm(p + 2);

    /* --- cp ------------------------------------------------------------- */
    if (p[0] == 'c' && p[1] == 'p' && (p[2] == ' ' || p[2] == '-'))
        return mars_cp(p + 2);

    /* --- mkdir -p X ---------------------------------------------------- */
    if (strncmp(p, "mkdir -p ", 10) == 0) {
        snprintf(cmd, sizeof(cmd), "mkdir \"%s\"", p + 10);
        return mars_run_console(cmd);
    }

    /* --- everything else: copy and normalise for cmd ------------------- */
    n = 0;
    while (p[n] && n < sizeof(cmd) - 1) {
        if (strncmp(p + n, "2>/dev/null", 10) == 0) {
            memcpy(cmd + n, "2>NUL", 5); n += 5; continue;
        }
        cmd[n] = p[n];
        n++;
    }
    cmd[n] = '\0';

    /* A trailing " &" is shell backgrounding. */
    {
        char *amp = strrchr(cmd, '&');
        if (amp && *(amp + 1) == '\0') {
            *amp = '\0';
            detached = 1;
        }
    }

    /* "./+x/name.+x" -> "\"+x\\name.+x\"". cmd needs backslashes, and the
     * house's '+' in +x is a cmd metacharacter, so the path is quoted. */
    if (strncmp(cmd, "./+x/", 5) == 0) {
        char fixed[2048];
        size_t i, k = 0;
        fixed[k++] = '"';
        fixed[k++] = '+'; fixed[k++] = 'x'; fixed[k++] = '\\';
        for (i = 5; cmd[i] && k < sizeof(fixed) - 2; i++) {
            if (cmd[i] == '/') fixed[k++] = '\\';
            else fixed[k++] = cmd[i];
        }
        fixed[k++] = '"';
        fixed[k] = '\0';
        snprintf(cmd, sizeof(cmd), "%s", fixed);
    } else if (strncmp(cmd, "./", 2) == 0) {
        /* Any other bare ./ sibling. Left unquoted because the name may
         * legitimately carry spaces, and '.' is not a cmd metacharacter
         * the way '+' is. */
        char fixed[2048];
        size_t i, k = 0;
        fixed[k++] = '.';
        fixed[k++] = '\\';
        for (i = 2; cmd[i] && k < sizeof(fixed) - 1; i++) {
            if (cmd[i] == '/') fixed[k++] = '\\';
            else fixed[k++] = cmd[i];
        }
        fixed[k] = '\0';
        snprintf(cmd, sizeof(cmd), "%s", fixed);
    }

    if (detached) { mars_run_detached(cmd); return 0; }
    return mars_run_console(cmd);
}

/* `&&` sequencing. cmd.exe does understand `&&`, but only if every part
 * is a command it can run - and the `rm -rf players && mkdir players` in
 * game_setup.c:117 died on the first half, which silently skipped the
 * mkdir. So the chain is split and each part goes through the same
 * translation as a standalone command. The result of the LAST part is
 * returned, which is what a shell's exit status means here. */
static int mars_system_impl(const char *raw) {
    char seg[2048];
    const char *p = raw;
    int rc = 0;

    if (!raw) return -1;

    while (*p) {
        const char *amp = strstr(p, "&&");
        size_t len = amp ? (size_t)(amp - p) : strlen(p);

        while (len && (*p == ' ' || *p == '\t')) { p++; len--; }
        while (len && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;

        if (len) {
            if (len >= sizeof(seg)) len = sizeof(seg) - 1;
            memcpy(seg, p, len);
            seg[len] = '\0';
            rc = mars_exec_one(seg);
        }
        if (!amp) break;
        p = amp + 2;
    }
    return rc;
}

#define system(cmd) mars_system_impl(cmd)

#endif /* MARS_WIN32_SYSTEM_H */
