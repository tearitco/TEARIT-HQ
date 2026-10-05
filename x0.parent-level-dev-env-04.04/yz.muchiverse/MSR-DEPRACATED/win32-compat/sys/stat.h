/* sys/stat.h - Windows shim for the Mars StreetRace CLI (MarS).
 *
 * WHY THIS EXISTS
 *   10 call sites in this project use the POSIX 2-arg mkdir(path, mode).
 *   mingw-w64 declares 1-arg mkdir, so all 10 fail with "too many
 *   arguments to function 'mkdir'; expected 1, have 2".
 *
 * WHY #include_next
 *   The rest of <sys/stat.h> (stat, fstat, S_ISDIR, S_IFMT, ...) is
 *   needed and is correct as shipped. Rather than reimplement the header
 *   or edit 10 call sites, this pulls in the real one and then fixes the
 *   single broken signature. #include_next is a GCC extension, which is
 *   fine: this whole shim directory is only ever passed to gcc, and only
 *   on Windows, so Linux never sees it.
 *
 *   The mode argument is dropped, not emulated. Windows has no file
 *   permission bits; nothing in this project uses the mode anyway.
 */
#ifndef MARS_WIN32_SYS_STAT_H
#define MARS_WIN32_SYS_STAT_H

#include <windows.h>
#include <errno.h>
#include <sys/types.h>

#include_next <sys/stat.h>

/* Implemented with CreateDirectoryA rather than _mkdir(): mingw's
 * <sys/stat.h> does not declare _mkdir at all (it lives in <direct.h>),
 * so redirecting to it just trades one implicit-declaration error for
 * another. The mode argument is dropped, not emulated - Windows has no
 * permission bits and nothing here uses it. */
static int mars_mkdir_1(const char *path) {
    if (CreateDirectoryA(path, NULL)) return 0;
    /* Already-exists is a failure to POSIX mkdir too, but report it the
     * same way (non-zero return) so caller error-checks still work. */
    return -1;
}

#undef mkdir
#define mkdir(path, mode) mars_mkdir_1(path)

#endif /* MARS_WIN32_SYS_STAT_H */
