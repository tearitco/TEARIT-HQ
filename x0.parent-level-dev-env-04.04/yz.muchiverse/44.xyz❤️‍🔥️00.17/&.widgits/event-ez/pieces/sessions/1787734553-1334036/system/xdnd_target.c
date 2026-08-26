/* xdnd_target.c - see xdnd_target.h for why this is not currently used. */
#include <string.h>
#include <X11/Xatom.h>
#include "xdnd_target.h"

static Atom XA_XdndAware, XA_XdndEnter, XA_XdndPosition, XA_XdndLeave,
           XA_XdndDrop, XA_XdndFinished, XA_XdndStatus, XA_XdndSelection,
           XA_text_plain, XA_XdndActionCopy, XA_targets;
static int xdnd_version = 5;
static Window xdnd_source = 0;
static int xdnd_have_source = 0;
static Display *g_dpy = NULL;
static Window g_win = 0;
static int g_drag_over = 0;
static xdnd_target_drop_cb g_on_drop = NULL;

/* WM reparenting means a titled window is often not a direct child of
 * root - search recursively, not just one level deep. */
static Window find_window_by_name(Display *d, Window start, const char *target) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strcmp(name, target) == 0) found = children[i];
            XFree(name);
        }
        if (!found) found = find_window_by_name(d, children[i], target);
    }
    if (children) XFree(children);
    return found;
}

Window xdnd_target_init(Display *d, const char *win_title, xdnd_target_drop_cb on_drop) {
    Window root = RootWindow(d, DefaultScreen(d));
    Window found = find_window_by_name(d, root, win_title);
    if (!found) return 0;

    g_dpy = d;
    g_win = found;
    g_on_drop = on_drop;

    XA_XdndAware = XInternAtom(d, "XdndAware", False);
    XA_XdndEnter = XInternAtom(d, "XdndEnter", False);
    XA_XdndPosition = XInternAtom(d, "XdndPosition", False);
    XA_XdndLeave = XInternAtom(d, "XdndLeave", False);
    XA_XdndDrop = XInternAtom(d, "XdndDrop", False);
    XA_XdndFinished = XInternAtom(d, "XdndFinished", False);
    XA_XdndStatus = XInternAtom(d, "XdndStatus", False);
    XA_XdndSelection = XInternAtom(d, "XdndSelection", False);
    XA_text_plain = XInternAtom(d, "text/plain", False);
    XA_XdndActionCopy = XInternAtom(d, "XdndActionCopy", False);
    XA_targets = XInternAtom(d, "TARGETS", False);
    long xdnd_aware = xdnd_version;
    XChangeProperty(d, found, XA_XdndAware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&xdnd_aware, 1);
    XSync(d, False);
    return found;
}

int xdnd_target_drag_over(void) { return g_drag_over; }

void xdnd_target_handle_event(XEvent *xev) {
    if (!g_dpy || !g_win) return;

    if (xev->type == SelectionNotify) {
        if (xev->xselection.property == None) {
            g_drag_over = 0;
            return;
        }
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        if (XGetWindowProperty(g_dpy, g_win, XA_XdndSelection, 0, 1024, False,
                               XA_text_plain, &actual_type, &actual_format,
                               &nitems, &bytes_after, &data) == Success && data && nitems > 0) {
            char pet_id[256] = {0};
            memcpy(pet_id, data, nitems < sizeof(pet_id) - 1 ? nitems : sizeof(pet_id) - 1);
            XFree(data);

            if (g_on_drop) g_on_drop(pet_id);

            XEvent finished;
            memset(&finished, 0, sizeof(finished));
            finished.xclient.type = ClientMessage;
            finished.xclient.display = g_dpy;
            finished.xclient.window = xdnd_source;
            finished.xclient.message_type = XA_XdndFinished;
            finished.xclient.format = 32;
            finished.xclient.data.l[0] = g_win;
            finished.xclient.data.l[1] = 1;
            XSendEvent(g_dpy, xdnd_source, False, NoEventMask, &finished);
            XSync(g_dpy, False);
        }
        xdnd_source = 0;
        xdnd_have_source = 0;
        g_drag_over = 0;
    }
    else if (xev->type == ClientMessage) {
        if (xev->xclient.message_type == XA_XdndEnter) {
            xdnd_source = xev->xclient.data.l[0];
            xdnd_have_source = 1;
            g_drag_over = 1;
        }
        else if (xev->xclient.message_type == XA_XdndPosition) {
            XEvent status;
            memset(&status, 0, sizeof(status));
            status.xclient.type = ClientMessage;
            status.xclient.display = g_dpy;
            status.xclient.window = xdnd_source;
            status.xclient.message_type = XA_XdndStatus;
            status.xclient.format = 32;
            status.xclient.data.l[0] = g_win;
            status.xclient.data.l[1] = 1;
            status.xclient.data.l[2] = 0;
            status.xclient.data.l[3] = 0;
            status.xclient.data.l[4] = XA_XdndActionCopy;
            XSendEvent(g_dpy, xdnd_source, False, NoEventMask, &status);
        }
        else if (xev->xclient.message_type == XA_XdndLeave) {
            xdnd_source = 0;
            xdnd_have_source = 0;
            g_drag_over = 0;
        }
        else if (xev->xclient.message_type == XA_XdndDrop) {
            XConvertSelection(g_dpy, XA_XdndSelection, XA_text_plain,
                              XA_XdndSelection, g_win, CurrentTime);
        }
    }
}
