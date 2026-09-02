/* xdnd_source.h - reusable Xdnd DRAG SOURCE protocol module.
 *
 * Extracted out of egg_window.c per 0.a-z-pets-plan/a-z-fix.txt. NOT
 * currently compiled/linked into egg_window or any other muchi-pals
 * binary - button.sh does not build this file. egg_window now uses a
 * much simpler coordinate+file handoff instead (see egg_window.c's own
 * check_drop_on_release()), because this real Xdnd path - even after
 * drag-drop-bugs.txt's 6 documented protocol fixes were all applied -
 * still never worked end-to-end: WM reparenting (mutter wraps
 * gl_mirror's window in a decoration frame, so the direct-child-of-root
 * target search never found it) and, on gl_mirror's side, a CPU-spin
 * bug in its Xdnd idle poll (no throttle at all) that was one keystroke
 * away from crashing the machine the same way it already did once in
 * the gl-canvas/ prototype this was ported from.
 *
 * Kept here, compiling standalone, for reference or future reuse if a
 * real cross-application (not just our own two sibling projects) Xdnd
 * source is ever needed again.
 */
#ifndef XDND_SOURCE_H
#define XDND_SOURCE_H

#include <X11/Xlib.h>

/* Call once after creating the source window, before the event loop. */
void xdnd_source_init(Display *d, Window w);

/* Call from your event loop for the matching X event types. win must be
 * the same window passed to xdnd_source_init. */
void xdnd_source_on_button_press(Display *dpy, Window win);
void xdnd_source_on_motion(Display *dpy, Window win, int root_x, int root_y);
void xdnd_source_on_button_release(Display *dpy, Window win);
void xdnd_source_on_client_message(XClientMessageEvent *xev);

/* pet_id is whatever payload string this drag is carrying - sent to the
 * target via text/plain selection. */
void xdnd_source_on_selection_request(Display *dpy, XSelectionRequestEvent *xev, const char *pet_id);

#endif
