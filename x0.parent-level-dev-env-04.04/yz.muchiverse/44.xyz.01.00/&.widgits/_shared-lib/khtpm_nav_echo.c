/* khtpm_nav_echo.c - the nav box's text: focus mark + the digits typed so far.
 *
 * Text-included canonical helper (pure, no I/O, no drawing), same family as
 * khtpm_grid_jump.c. The nav box is the small cell at the left edge of every
 * taskbar bar (and of pc-hq windows): it shows "^" when that window really has
 * keyboard focus, "." when it does not, and - the part that was missing - the
 * digits the user is typing to jump to a nav index, accumulating right after
 * the mark, in the same spot:
 *
 *     .        idle, no focus          ^        idle, has focus
 *     ^1       typed "1"               ^12      typed "12"
 *
 * ONE buffer feeds every surface. The taskbar manager owns it (digit_buf in
 * strip_state.txt) and publishes it as the variable nav_digits in strip_ui.txt,
 * so the top bar, the bottom bar and any pc-hq window show the same digits at
 * the same time. This file only turns (mark, digits) into the string to draw,
 * so the surfaces cannot drift apart.
 *
 * Layout note (2026-10-05, owner): the nav box is meant to become a layout
 * element bound to ${nav_digits} once the dock moves to the generic layout
 * (DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md); the string this builds is exactly what
 * that binding would show, so nothing is rewritten when it does.
 *
 * Prefix: nve_. Static + unused-tolerant. */
#ifndef KHTPM_NAV_ECHO_C
#define KHTPM_NAV_ECHO_C

#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
#define NVE_UNUSED __attribute__((unused))
#else
#define NVE_UNUSED
#endif

/* The box is a fixed 64 px wide; mark + this many digits fit at 1.25x scale. */
#define NVE_MAX_DIGITS 3

/* out = mark, then the last NVE_MAX_DIGITS digits of `digits` (non-digit
 * characters are skipped). `mark` is "^" or "." (any short string is fine).
 * Returns strlen(out). */
NVE_UNUSED static int nve_text(const char *mark, const char *digits, char *out, size_t n) {
    char d[NVE_MAX_DIGITS + 1];
    int nd = 0, i, total = 0;
    size_t len;
    if (!out || n == 0) return 0;
    if (!mark) mark = "";
    if (digits) {
        for (i = 0; digits[i]; i++)
            if (digits[i] >= '0' && digits[i] <= '9') total++;
        {
            int skip = total > NVE_MAX_DIGITS ? total - NVE_MAX_DIGITS : 0, seen = 0;
            for (i = 0; digits[i] && nd < NVE_MAX_DIGITS; i++) {
                if (digits[i] < '0' || digits[i] > '9') continue;
                if (seen++ < skip) continue;
                d[nd++] = digits[i];
            }
        }
    }
    d[nd] = '\0';
    len = (size_t)snprintf(out, n, "%s%s", mark, d);
    return (int)(len < n ? len : n - 1);
}

#endif
