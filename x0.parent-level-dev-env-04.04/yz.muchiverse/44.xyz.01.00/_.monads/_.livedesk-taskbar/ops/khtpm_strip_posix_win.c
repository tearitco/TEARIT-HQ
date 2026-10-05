/* Win32 implementation of the small POSIX surface khtpm_strip_parser.c
 * needs. See khtpm_strip_posix_win.h for why the two groups are split. */
#include "khtpm_strip_posix_win.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- group 1: directory iteration over FindFirstFileW ------------------ */

/* REAL, NEW 2026-09-26 - one documented deviation from POSIX readdir: the
 * "." and ".." entries are not synthesized. Win32's FindFirstFileW never
 * reports them and every caller in the tree already skips names starting
 * with '.', so synthesizing them would buy nothing but the chance to
 * differ from the real thing. All real entries are returned verbatim. */
DIR *opendir(const char *path) {
    wchar_t wpath[MAX_PATH * 2];
    wchar_t wpat[MAX_PATH * 2 + 4];
    DIR *d;
    size_t n;

    if (!path || !*path) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH * 2) == 0)
        return NULL;
    n = wcslen(wpath);
    /* tolerate a caller that already appended a separator */
    if (n > 0 && (wpath[n - 1] == L'\\' || wpath[n - 1] == L'/'))
        wpat[0] = 0;
    else
        wcscpy(wpat, wpath);
    wcscat(wpat, L"\\*");

    d = (DIR *)calloc(1, sizeof(DIR));
    if (!d) return NULL;
    d->h = FindFirstFileW(wpat, &d->data);
    if (d->h == INVALID_HANDLE_VALUE) {
        /* REAL, NEW 2026-09-26 - ENOENT/ENOTDIR matters: ktb_toggle_zorder_
         * respawn() gates its whole /proc scan on `if (!pd) return;`, and
         * this is the line that makes that gate fire on Windows. */
        errno = ENOENT;
        free(d);
        return NULL;
    }
    d->first = 1;
    d->pending = 0;
    return d;
}

struct dirent *readdir(DIR *d) {
    static struct dirent ent;   /* POSIX readdir semantics: reused buffer */
    if (!d || d->h == INVALID_HANDLE_VALUE) return NULL;
    if (!d->pending) {
        if (d->first) {
            d->first = 0;       /* FindFirstFileW already filled `data` */
        } else if (!FindNextFileW(d->h, &d->data)) {
            return NULL;
        }
        d->pending = 0;
    }
    d->pending = 0;
    ent.d_ino = 0;
    if (WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1,
                            ent.d_name, MAX_PATH, NULL, NULL) == 0) {
        ent.d_name[0] = '\0';
    }
    return &ent;
}

int closedir(DIR *d) {
    if (!d) return -1;
    if (d->h != INVALID_HANDLE_VALUE) FindClose(d->h);
    d->h = INVALID_HANDLE_VALUE;
    free(d);
    return 0;
}

/* --- group 2: link-only stubs ----------------------------------------- */

/* All three are unreachable on Windows by construction (the /proc gate in
 * ktb_toggle_zorder_respawn returns first). They fail loudly rather than
 * pretending to work: a -1 from fork() means the `if (pid == 0)` child arm
 * is never entered, so no partially-initialised child can exist. */
int fork(void) {
    return -1;
}

int setsid(void) {
    return -1;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    (void)path; (void)argv; (void)envp;
    errno = ENOENT;
    return -1;
}

/* --- group 3: real module spawn over CreateProcessW ------------------- */

/* Append arg to dst as one quoted Windows command-line token. The child
 * argv is already split, so this is only re-quoting - not shell parsing.
 * Backslash-then-quote is escaped the way CommandLineToArgvW expects. */
static void khtpm_win_quote_arg(char *dst, size_t dstsz, const char *arg) {
    size_t o = strlen(dst);
    if (o + 1 < dstsz) dst[o++] = '"';
    for (const char *p = arg; *p; p++) {
        if (*p == '"' && o + 1 < dstsz) dst[o++] = '\\';
        if (o + 1 < dstsz) dst[o++] = *p;
    }
    if (o + 1 < dstsz) dst[o++] = '"';
    dst[o] = '\0';
}

long khtpm_win_spawn_module(const char *path, char *const argv[]) {
    char exe[MAX_PATH * 2];
    char cmd[MAX_PATH * 8] = "";
    char dir[MAX_PATH * 2];
    wchar_t wexe[MAX_PATH * 2], wcmd[MAX_PATH * 8], wdir[MAX_PATH * 2];
    size_t n, sep;
    int i;

    if (!path || !path[0] || !argv || !argv[0]) { errno = EINVAL; return -1; }

    /* The canonical tree names managers *.+x; this platform builds them
     * *.exe. Apply the same rewrite khtpm_taskbar_manager.c's
     * win_star_alias() applies, then fall back to "append .exe". */
    snprintf(exe, sizeof(exe), "%s", path);
    n = strlen(exe);
    if (n > 3 && strcmp(exe + n - 3, ".+x") == 0) {
        snprintf(exe, sizeof(exe), "%.*s.exe", (int)(n - 3), path);
    } else if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) {
        char alt[MAX_PATH * 2];
        snprintf(alt, sizeof(alt), "%s.exe", exe);
        if (GetFileAttributesA(alt) != INVALID_FILE_ATTRIBUTES)
            snprintf(exe, sizeof(exe), "%s", alt);
    }
    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) {
        errno = ENOENT;
        return -1;
    }

    for (i = 0; argv[i]; i++) {
        if (i) strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
        khtpm_win_quote_arg(cmd, sizeof(cmd), argv[i]);
    }

    /* cwd = the executable's own directory, so any relative state files a
     * manager writes land beside it (the Linux launch runs from there). */
    snprintf(dir, sizeof(dir), "%s", exe);
    sep = (size_t)-1;
    for (size_t k = 0; dir[k]; k++)
        if (dir[k] == '\\' || dir[k] == '/') sep = k;
    if (sep != (size_t)-1) dir[sep] = '\0';

    if (MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, MAX_PATH * 2) == 0)
        MultiByteToWideChar(CP_ACP, 0, exe, -1, wexe, MAX_PATH * 2);
    if (MultiByteToWideChar(CP_UTF8, 0, cmd, -1, wcmd, MAX_PATH * 8) == 0)
        MultiByteToWideChar(CP_ACP, 0, cmd, -1, wcmd, MAX_PATH * 8);
    if (MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH * 2) == 0)
        MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, MAX_PATH * 2);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB;
    BOOL ok = CreateProcessW(wexe, wcmd, NULL, NULL, FALSE, flags, NULL,
                             wdir[0] ? wdir : NULL, &si, &pi);
    if (!ok) {
        flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW;
        ok = CreateProcessW(wexe, wcmd, NULL, NULL, FALSE, flags, NULL,
                            wdir[0] ? wdir : NULL, &si, &pi);
    }
    if (!ok) { errno = ENOENT; return -1; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (long)pi.dwProcessId;
}
