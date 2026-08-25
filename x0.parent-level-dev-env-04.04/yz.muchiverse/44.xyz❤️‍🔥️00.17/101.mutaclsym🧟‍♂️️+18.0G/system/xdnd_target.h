/* xdnd_target.h - reusable Xdnd DROP TARGET protocol module.
 *
 * Extracted out of gl_mirror.c per 0.a-z-pets-plan/a-z-fix.txt. NOT
 * currently compiled/linked into gl_mirror or any other mutaclsym
 * binary - button.sh does not build this file. See xdnd_target.h in
 * 01.muchi-pals-🥚️-13.01/system/ (the source-side counterpart) for the
 * full reasoning: even after drag-drop-bugs.txt's 6 documented protocol
 * fixes were applied, real Xdnd here still never worked end-to-end -
 * WM reparenting broke this file's own fgDisplay-based window self-
 * lookup (same bug gl-canvas/system/gl_canvas.c had before its fix),
 * and gl_mirror's own check_xdnd_events() idle poll had NO throttle at
 * all, the exact CPU-spin bug that crashed the machine once already in
 * the gl-canvas/ prototype this port is based on. gl_mirror now uses a
 * simpler coordinate+file handoff instead (see gl_mirror.c's own
 * check_for_drop()).
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
