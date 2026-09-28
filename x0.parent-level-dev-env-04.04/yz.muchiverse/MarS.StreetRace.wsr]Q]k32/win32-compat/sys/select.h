/* sys/select.h - Windows shim for the Mars StreetRace CLI (MarS).
 *
 * WHY THIS EXISTS
 *   8 .c files poll stdin with the classic non-blocking idiom:
 *
 *       struct timeval tv = { 0L, 0L };
 *       fd_set fds;
 *       FD_ZERO(&fds);
 *       FD_SET(STDIN_FILENO, &fds);
 *       return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
 *
 *   which is just "is there a key waiting?". mingw-w64 does provide a
 *   <sys/select.h>, but its select() works on SOCKETS only and returns
 *   WSAENOTSOCK on a console handle - so the shim replaces it outright
 *   rather than letting the real header win on some builds.
 *
 * SCOPE, STATED PLAINLY
 *   Only the console-stdin read case is implemented, because that is the
 *   only way this project uses it. Socket and write-set support is
 *   deliberately absent: it would be untested code pretending to be
 *   general. If this file is ever needed for real sockets, use
 *   <winsock2.h> and delete the shim.
 */
#ifndef MARS_WIN32_SYS_SELECT_H
#define MARS_WIN32_SYS_SELECT_H

/* winsock2.h FIRST, because <windows.h> (needed for the console handles)
 * transitively includes it and would otherwise clash with a later direct
 * include. winsock2's fd_set is reused rather than redeclared - defining a
 * second `struct fd_set` here produced "redefinition of struct or union"
 * in all 8 files that use this. A HANDLE and a SOCKET are both
 * pointer-sized on this target, so one is safely stored in the other.
 */
#include <winsock2.h>
#include <windows.h>

#define MARS_FD_SETSIZE 64

/* Winsock2 already defines FD_ZERO/FD_SET/FD_CLR/FD_ISSET as macros over
 * SOCKET values. Ours are POSIX (int fds) and must go through a console
 * handle, so the old macros are dropped and the new ones are MACROS over
 * uniquely-named static functions.
 *
 * Macros, not plain functions, and unique names, for one specific reason:
 * winsock2 declares `select` as an extern function. Defining a `static
 * int select(...)` of my own is a linkage conflict - "conflicting types
 * for 'select'" - and renaming the function to something unique while
 * leaving the CALL SITE spelled `select` avoids that entirely, because the
 * preprocessor rewrites the call before the compiler ever sees it.
 *
 * Consequence, stated plainly: `select` on the console is intercepted for
 * this whole program, so a real socket select would break. This project
 * never opens a socket, and the shim is Windows-only. If that ever
 * changes, drop the shim and use <winsock2.h> properly. */
#undef FD_ZERO
#undef FD_SET
#undef FD_CLR
#undef FD_ISSET

static HANDLE mars_fd_to_handle(int fd) {
    if (fd == 0) return GetStdHandle(STD_INPUT_HANDLE);
    if (fd == 1) return GetStdHandle(STD_OUTPUT_HANDLE);
    if (fd == 2) return GetStdHandle(STD_ERROR_HANDLE);
    return (HANDLE)(intptr_t)fd;
}

static void mars_fd_zero(fd_set *s) { s->fd_count = 0; }

static void mars_fd_set(int fd, fd_set *s) {
    if (s->fd_count < MARS_FD_SETSIZE) s->fd_array[s->fd_count++] = (SOCKET)mars_fd_to_handle(fd);
}

static void mars_fd_clr(int fd, fd_set *s) {
    SOCKET h = (SOCKET)mars_fd_to_handle(fd);
    int i;
    for (i = 0; i < s->fd_count; i++) {
        if (s->fd_array[i] == h) {
            s->fd_array[i] = s->fd_array[--s->fd_count];
            return;
        }
    }
}

static int mars_fd_isset(int fd, fd_set *s) {
    SOCKET h = (SOCKET)mars_fd_to_handle(fd);
    int i;
    for (i = 0; i < s->fd_count; i++) {
        if (s->fd_array[i] == h) return 1;
    }
    return 0;
}

#define FD_ZERO(s)          mars_fd_zero(s)
#define FD_SET(fd, s)       mars_fd_set(fd, s)
#define FD_CLR(fd, s)       mars_fd_clr(fd, s)
#define FD_ISSET(fd, s)     mars_fd_isset(fd, s)

/* Macro over a uniquely-named static, for the same linkage reason as the
 * FD_* family above. */
static int mars_select(int nfds, fd_set *readfds, fd_set *writefds,
                       fd_set *exceptfds, struct timeval *tv) {
    DWORD timeout_ms;
    HANDLE first;
    (void)nfds; (void)writefds; (void)exceptfds;

    timeout_ms = tv ? (DWORD)(tv->tv_sec * 1000L + tv->tv_usec / 1000L) : INFINITE;

    /* No read set: this idiom doubles as a plain sleep in places. */
    if (!readfds || readfds->fd_count == 0) {
        if (timeout_ms != 0) Sleep(timeout_ms);
        return 0;
    }
    /* fd_array[] holds SOCKET (an integer type) but a console handle is a
     * real pointer, so the cast has to be explicit in both directions. */
    first = (HANDLE)(intptr_t)readfds->fd_array[0];
    if (first == NULL || first == INVALID_HANDLE_VALUE) return 0;

    /* CONSOLE stdin: the handle becomes signalled when a key event is
     * waiting, which is exactly "is there input?". */
    {
        DWORD mode;
        if (GetConsoleMode(first, &mode)) {
            switch (WaitForSingleObject(first, timeout_ms)) {
                case WAIT_OBJECT_0:  return 1;
                case WAIT_TIMEOUT:   return 0;
                default:             return 0;
            }
        }
    }

    /* PIPE / redirected stdin: the read end of a pipe is never signalled,
     * so WaitForSingleObject is the wrong tool - it returns WAIT_FAILED.
     * Returning -1 there produced a hot spin that printed
     * "Input received: -1" forever whenever the program was run with
     * redirected stdin. PeekNamedPipe is the correct probe, and a byte
     * count of 0 is the correct answer for "no input yet". */
    {
        DWORD avail = 0;
        if (PeekNamedPipe(first, NULL, 0, NULL, &avail, NULL)) {
            if (avail > 0) return 1;
            /* Nothing buffered. Sleep out the requested timeout so a
             * non-zero tv still behaves as a wait rather than a spin. */
            if (timeout_ms != 0 && timeout_ms != INFINITE) Sleep(timeout_ms);
            return 0;
        }
    }

    /* DISK FILE / redirected stdin: neither WaitForSingleObject nor
     * PeekNamedPipe applies. Size is the only available signal, and it is
     * enough: the game reads stdin strictly sequentially, so a non-empty
     * file means there is something left to read. Without this the game
     * would block forever on a file-redirected run, which also makes the
     * whole thing untestable from a script. */
    if (GetFileType(first) == FILE_TYPE_DISK) {
        LARGE_INTEGER li;
        if (GetFileSizeEx(first, &li)) {
            if (li.QuadPart > 0) return 1;
            if (timeout_ms != 0 && timeout_ms != INFINITE) Sleep(timeout_ms);
            return 0;
        }
    }

    /* Anything else (/dev/null equivalent, a closed handle): never ready. */
    if (timeout_ms != 0 && timeout_ms != INFINITE) Sleep(timeout_ms);
    return 0;
}

#define select(nfds, r, w, e, tv) mars_select((nfds), (r), (w), (e), (tv))

#endif /* MARS_WIN32_SYS_SELECT_H */
