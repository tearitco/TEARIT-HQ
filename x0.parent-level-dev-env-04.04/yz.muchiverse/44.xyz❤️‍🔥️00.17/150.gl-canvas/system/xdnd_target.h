/* xdnd_target.h - reusable Xdnd DROP TARGET protocol module.
 *
 * Extracted out of gl_canvas.c. NOT currently compiled/linked into
 * gl_canvas or any other gl-canvas binary - see button.sh, which does
 * not build this file. See xdnd_source.h for why: real Xdnd was never
 * confirmed working end-to-end here (WM reparenting + cross-connection
 * ClientMessage delivery), so gl_canvas now uses a simpler coordinate+
 * file handoff instead (see gl_canvas.c's own check_for_drop()).
 *
 * Kept here, compiling standalone, for reference or future reuse.
 */
#ifndef XDND_TARGET_H
#define XDND_TARGET_H

#include <X11/Xlib.h>

typedef void (*xdnd_target_drop_cb)(const char *pet_id);

/* Finds a window named win_title anywhere under root (recursing through
 * WM decoration frames, not just direct children) and marks it Xdnd-
 * aware. Returns the found window, or 0 if not found. */
Window xdnd_target_init(Display *d, const char *win_title, xdnd_target_drop_cb on_drop);

/* Call from your event loop / idle func for every pending X event on
 * the display returned by xdnd_target_init. */
void xdnd_target_handle_event(XEvent *xev);

/* True while a drag is currently hovering over the target window. */
int xdnd_target_drag_over(void);

#endif
