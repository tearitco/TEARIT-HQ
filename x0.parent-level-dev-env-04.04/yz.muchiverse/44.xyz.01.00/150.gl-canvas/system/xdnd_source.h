/* xdnd_source.h - reusable Xdnd DRAG SOURCE protocol module.
 *
 * Extracted out of pet_purely.c. NOT currently compiled/linked into
 * pet_purely or any other gl-canvas binary - see button.sh, which does
 * not build this file. The real pet-drop test now uses a much simpler
 * coordinate+file handoff (pet_purely.c's own check_drop_on_release()),
 * because this real Xdnd path was never confirmed working end-to-end:
 * WM reparenting (mutter wraps target windows in a decoration frame)
 * and the fact that Xdnd ClientMessage/Selection events only deliver to
 * the X11 connection that created the target window made this fragile
 * to debug and, combined with an unrelated CPU-throttle bug on the
 * target side, contributed to a real system crash during testing.
 *
 * Kept here, compiling standalone, for reference or future reuse if a
 * real cross-application (not just our own two test programs) Xdnd
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

/* pet_id is whatever payload string this drag is carrying (e.g. the
 * pet's identifier) - sent to the target via text/plain selection. */
void xdnd_source_on_selection_request(Display *dpy, XSelectionRequestEvent *xev, const char *pet_id);

#endif
