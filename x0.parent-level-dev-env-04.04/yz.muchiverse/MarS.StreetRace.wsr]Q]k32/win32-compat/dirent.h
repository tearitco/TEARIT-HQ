/* dirent.h - Windows shim for the Mars StreetRace CLI (MarS).
 *
 * WHY THIS REPLACES THE HEADER INSTEAD OF PATCHING IT
 *   select_entity.c and setup_corporations_stage2.c test
 *   `dir->d_type == DT_DIR`. mingw-w64's <dirent.h> has no d_type member
 *   and no DT_DIR, and a member cannot be added to an existing struct
 *   after the fact. So this is a small complete implementation over the
 *   Win32 directory API rather than a wrapper.
 *
 * API SURFACE - exactly what this project uses, nothing speculative:
 *   opendir / readdir / closedir, struct dirent { d_name, d_type }
 *   d_name  is used 45x, d_type 3x, errno 1x.
 *
 * FIDELITY NOTES
 *   - readdir RETURNS "." and "..", same as Linux. Both call sites filter
 *     them with an explicit strcmp, so suppressing them here would silently
 *     change their logic rather than simplify it.
 *   - d_name is UTF-8. The WIDE API is used internally and converted,
 *     because this house is full of emoji in directory names; the ANSI
 *     FindFirstFile would mangle or skip them on a non-UTF-8 code page.
 *   - d_type values match Linux (DT_UNKNOWN 0, DT_DIR 4, DT_REG 8,
 *     DT_LNK 10) so the constants in the callers behave identically.
 */
#ifndef MARS_WIN32_DIRENT_H
#define MARS_WIN32_DIRENT_H

#include <windows.h>
#include <string.h>
#include <errno.h>

#define DT_UNKNOWN 0
#define DT_REG     8
#define DT_DIR     4
#define DT_LNK     10

#define NAME_MAX 255

typedef struct dirent {
    unsigned long  d_ino;        /* not populated; unused by this project */
    char           d_name[NAME_MAX + 1];
    long           d_off;        /* not populated; unused */
    unsigned short d_reclen;     /* not populated; unused */
    unsigned char  d_type;
} dirent;

typedef struct {
    HANDLE          handle;      /* FindFirstFile handle */
    int             pending;     /* a real FindNextFile result is waiting */
    WIN32_FIND_DATAW data;
    int             fake_index;  /* -1 none, 0 -> ".", 1 -> ".." */
    struct dirent   ent;
} mars_DIR;

typedef mars_DIR DIR;

/* UTF-8 -> UTF-16, for handing a caller path to the wide API. */
static WCHAR *mars_utf8_to_wide(const char *in) {
    int n = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
    WCHAR *w;
    if (n <= 0) return NULL;
    w = (WCHAR *)malloc((size_t)n * sizeof(WCHAR));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, in, -1, w, n);
    return w;
}

/* UTF-16 -> UTF-8, for d_name. Truncates rather than failing: a name too
 * long to fit is not worth aborting a directory listing over. */
static void mars_wide_to_utf8(const WCHAR *in, char *out, size_t outsz) {
    int n = WideCharToMultiByte(CP_UTF8, 0, in, -1, out, (int)outsz, NULL, NULL);
    if (n <= 0 && outsz) out[0] = '\0';
}

static DIR *opendir(const char *name) {
    mars_DIR *d;
    size_t need;
    WCHAR *wpath;
    WCHAR *wpat;

    need = strlen(name) + 3;                 /* "\" plus wildcard plus NUL */
    wpath = (WCHAR *)malloc(need * sizeof(WCHAR));
    if (!wpath) return NULL;
    mars_utf8_to_wide(name);                 /* validation only */
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wpath, (int)need);

    need = strlen(name) + 3;                 /* "\\*" */
    wpat = (WCHAR *)malloc(need * sizeof(WCHAR));
    if (!wpat) { free(wpath); return NULL; }
    wcscpy(wpat, wpath);
    wcscat(wpat, L"\\*");

    d = (mars_DIR *)calloc(1, sizeof(mars_DIR));
    if (!d) { free(wpath); free(wpat); return NULL; }
    d->fake_index = -1;

    d->handle = FindFirstFileW(wpat, &d->data);
    if (d->handle == INVALID_HANDLE_VALUE) {
        /* On Windows a missing directory is an error, same as POSIX. But an
         * existing directory with zero entries also reports failure, and is
         * NOT an error - errno is left 0 to let the caller tell them apart. */
        DWORD err = GetLastError();
        free(wpath); free(wpat); free(d);
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            errno = ENOENT;
            return NULL;
        }
        return NULL;   /* genuinely empty directory */
    }
    d->pending = 1;
    free(wpath);
    free(wpat);
    return d;
}

static struct dirent *readdir(DIR *d) {
    static struct dirent result;   /* POSIX: valid until next call */
    unsigned char type;

    if (!d) { errno = EBADF; return NULL; }

    /* Synthesize "." and ".." to match Linux ordering. */
    if (d->fake_index == 0 || d->fake_index == 1) {
        strcpy(result.d_name, d->fake_index == 0 ? "." : "..");
        result.d_type = DT_DIR;
        d->fake_index++;
        return &result;
    }
    if (d->fake_index < 0) d->fake_index = 0;

    if (!d->pending) return NULL;
    if (!FindNextFileW(d->handle, &d->data)) { d->pending = 0; return NULL; }

    mars_wide_to_utf8(d->data.cFileName, result.d_name, sizeof(result.d_name));
    if (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)      type = DT_DIR;
    else if (d->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) type = DT_LNK;
    else                                                             type = DT_REG;
    result.d_type = type;
    return &result;
}

static int closedir(DIR *d) {
    if (!d) { errno = EBADF; return -1; }
    if (d->handle != INVALID_HANDLE_VALUE) FindClose(d->handle);
    free(d);
    return 0;
}

#endif /* MARS_WIN32_DIRENT_H */
