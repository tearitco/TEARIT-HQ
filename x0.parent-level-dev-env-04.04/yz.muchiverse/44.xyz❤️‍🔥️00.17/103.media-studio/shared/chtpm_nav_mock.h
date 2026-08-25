/* chtpm_nav_mock.h — visual CHTPM continuous-nav overlay for freeglut media apps.
 *
 * Mirrors ee_gl_mock / house CHTPM language:
 *   [>] n. label   = focused nav target
 *   [ ] n. label   = other targets
 *
 * Layout:
 *   Top bar (chtpm_nav_bar_h() px) holds the continuous nav chips.
 *   Hosts MUST shift content + mouse Y by that height so nothing sits under the bar.
 *
 * Safe keys: Tab / Shift+Tab cycle; ` digit mode; 0-9 only in digit mode.
 */
#ifndef CHTPM_NAV_MOCK_H
#define CHTPM_NAV_MOCK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHTPM_NAV_MAX 32
#define CHTPM_NAV_BAR_H 28  /* reserved top strip — hosts offset by this */

void chtpm_nav_set_window(int win_w, int win_h);
int  chtpm_nav_bar_h(void); /* always CHTPM_NAV_BAR_H for layout */

void chtpm_nav_begin(void);
/* Region rects are in *content* coords (y=0 = just below the mock bar). */
void chtpm_nav_add(const char *label, float x, float y, float w, float h, int zone);
int  chtpm_nav_count(void);
int  chtpm_nav_focus(void);
const char *chtpm_nav_focus_label(void);
int  chtpm_nav_digit_mode(void);

int  chtpm_nav_on_key(unsigned char key, int shift);
int  chtpm_nav_on_special(int key);

/* Draw top bar + focus outline on focused region (content space, after host translate). */
void chtpm_nav_draw(void);

void chtpm_nav_status(char *buf, size_t n);

/* Convert window mouse Y (top-left origin) → content Y (below bar). */
static inline int chtpm_nav_mouse_y(int my) {
    return my - CHTPM_NAV_BAR_H;
}

#ifdef __cplusplus
}
#endif
#endif
