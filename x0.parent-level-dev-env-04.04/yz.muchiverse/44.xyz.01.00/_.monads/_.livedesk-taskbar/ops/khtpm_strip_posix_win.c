/* Win32 implementation of the small POSIX surface khtpm_strip_parser.c
 * needs. See khtpm_strip_posix_win.h for why the two groups are split. */
#include "khtpm_strip_posix_win.h"
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
