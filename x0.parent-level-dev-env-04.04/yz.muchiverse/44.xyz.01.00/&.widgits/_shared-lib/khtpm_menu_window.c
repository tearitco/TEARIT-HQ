/* khtpm_menu_window.c - windowing math for a menu taller than the room it has (text-include, prefix mw_, pure: no X11, no globals).
 *
 * One answer for every dropdown shape in the house (the livedesk dock menus book:/page:/pals, the in-window dropdown-child
 * pass used by pc-hq's File/Desk menus, later the entity context menus): given N rows and CAP slots that fit, show a window of
 * content rows and pin the LAST row (the "- cancel -" convention) in the final slot so it is always reachable. Owner
 * 2026-10-08: 98 map pages in a dropdown, "a thumb scroll ... to reach cancel".
 *
 *   mw_plan(&m, total, cap, &scroll)    fills m, clamps *scroll to [0, m.max_sc]; scrolling=0 when it all fits (or cap < 3)
 *   mw_slot(&m, idx)                    slot 0..m.vis for row idx (idx == total-1 -> slot m.vis), or -1 = hidden (clip it)
 *   mw_thumb(&m, track_h, &y, &h)       thumb box inside a track of track_h px (min 14 px)
 *   mw_scroll_from_y(&m, track_h, y)    scroll value for a pointer at y in the track (thumb centred on the pointer)
 * Not scrolling: vis = total and every row's slot is its index, so callers can use one code path.
 * Tested with scratch cases in the commit that added it (see git log). */
#ifndef KHTPM_MENU_WINDOW_C
#define KHTPM_MENU_WINDOW_C

typedef struct { int scrolling, vis, total, max_sc; } MwPlan;

static void mw_plan(MwPlan *m, int total, int cap, int *scroll) {
    m->total = total; m->scrolling = 0; m->vis = total; m->max_sc = 0;
    if (total > cap && cap >= 3) {
        m->scrolling = 1;
        m->vis = cap - 1;                       /* content rows shown; one slot is the pinned last row */
        m->max_sc = (total - 1) - m->vis;
    }
    if (scroll) {
        if (*scroll > m->max_sc) *scroll = m->max_sc;
        if (*scroll < 0) *scroll = 0;
    }
}

static int mw_slot(const MwPlan *m, int idx, int scroll) {
    if (!m->scrolling) return idx;
    if (idx == m->total - 1) return m->vis;
    if (idx >= scroll && idx < scroll + m->vis) return idx - scroll;
    return -1;
}

static void mw_thumb(const MwPlan *m, int track_h, int scroll, int *ty, int *th) {
    int content = m->total - 1, h, y;
    if (content < 1 || track_h < 1) { *ty = 0; *th = track_h; return; }
    h = (track_h * m->vis) / content;
    if (h < 14) h = 14;
    if (h > track_h) h = track_h;
    y = (m->max_sc > 0) ? ((track_h - h) * scroll) / m->max_sc : 0;
    *ty = y; *th = h;
}

static int mw_scroll_from_y(const MwPlan *m, int track_h, int y) {
    int content = m->total - 1, h, span, sc;
    if (!m->scrolling || m->max_sc < 1 || track_h < 1 || content < 1) return 0;
    h = (track_h * m->vis) / content;
    if (h < 14) h = 14;
    if (h > track_h) h = track_h;
    span = track_h - h;
    if (span < 1) return 0;
    sc = ((y - h / 2) * m->max_sc + span / 2) / span;
    if (sc < 0) sc = 0;
    if (sc > m->max_sc) sc = m->max_sc;
    return sc;
}
#endif
