/* house_wait.h — the one wait a poll loop is allowed to use.
 *
 * A sleep that lives in an "idle" else is not a wait. The pass that
 * sees new work skips it and the core pegs. Call house_wait_us at the
 * bottom of every pass, after the work, and do not put it in an else.
 * Zero is not a wait: a non-positive value sleeps the floor (30ms),
 * the same floor as the board daemon. Do not pass a shorter delay
 * than the sleeps already next to the loop you are editing.
 */
#ifndef HOUSE_WAIT_H
#define HOUSE_WAIT_H

#include <errno.h>
#include <time.h>

#define HOUSE_WAIT_FLOOR_US 30000

static inline void house_wait_us(int usec) {
    if (usec < HOUSE_WAIT_FLOOR_US) usec = HOUSE_WAIT_FLOOR_US;
    struct timespec ts;
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (long)(usec % 1000000) * 1000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { }
}

#endif
