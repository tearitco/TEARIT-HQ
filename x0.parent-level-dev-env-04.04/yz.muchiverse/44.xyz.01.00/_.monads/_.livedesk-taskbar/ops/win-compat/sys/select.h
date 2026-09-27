/* win-compat/sys/select.h — MinGW has no <sys/select.h>.
 *
 * khtpm_core_render.c's main loop does exactly one thing with select():
 *
 *     fd_set fds; FD_ZERO(&fds);
 *     int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
 *     select(xfd + 1, &fds, NULL, NULL, &tv);
 *
 * i.e. it waits on the X connection purely to pace its ~60fps redraw and to
 * be interruptible by the SIGTERM/SIGINT hook. On Windows there is no X
 * connection to wait on — ConnectionNumber() is a synthetic handle from
 * khtpm_strip_x11_win.h, and real events are drained through XPending()/
 * XNextEvent(). So the only honest behaviour is to honour the timeout and
 * report "nothing became readable", which is exactly what the X fd would
 * have done between frames. khtpm_win_compat.c implements that.
 *
 * fd_set is defined here rather than pulled from winsock2.h on purpose: the
 * shim header defines WIN32_LEAN_AND_MEAN before <windows.h>, so nothing else
 * in the translation unit has pulled in the Winsock version, and two
 * incompatible fd_set definitions in one TU is the classic MinGW port bug.
 */
#ifndef KHTPM_WINCOMPAT_SYS_SELECT_H
#define KHTPM_WINCOMPAT_SYS_SELECT_H

#include <sys/time.h>

#ifndef FD_SETSIZE
#define FD_SETSIZE 64
#endif

typedef struct {
    unsigned long __fds_bits[(FD_SETSIZE + (8 * sizeof(unsigned long)) - 1) / (8 * sizeof(unsigned long))];
} fd_set;

#define FD_ZERO(set)                                                        \
    do {                                                                     \
        int __i;                                                             \
        for (__i = 0; __i < (int)(sizeof((set)->__fds_bits) /                \
                                  sizeof((set)->__fds_bits[0])); __i++)      \
            (set)->__fds_bits[__i] = 0;                                      \
    } while (0)

#define FD_SET(fd, set)                                                     \
    do {                                                                     \
        int __fd = (fd);                                                     \
        if (__fd >= 0 && __fd < FD_SETSIZE)                                  \
            (set)->__fds_bits[__fd / (8 * sizeof(unsigned long))] |=         \
                (1UL << (__fd % (8 * sizeof(unsigned long))));                \
    } while (0)

#define FD_CLR(fd, set)                                                     \
    do {                                                                     \
        int __fd = (fd);                                                     \
        if (__fd >= 0 && __fd < FD_SETSIZE)                                  \
            (set)->__fds_bits[__fd / (8 * sizeof(unsigned long))] &=         \
                ~(1UL << (__fd % (8 * sizeof(unsigned long))));               \
    } while (0)

#define FD_ISSET(fd, set)                                                   \
    (((fd) >= 0 && (fd) < FD_SETSIZE) ?                                     \
        (((set)->__fds_bits[(fd) / (8 * sizeof(unsigned long))] >>            \
          ((fd) % (8 * sizeof(unsigned long)))) & 1UL) : 0UL)

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout);

#endif /* KHTPM_WINCOMPAT_SYS_SELECT_H */
