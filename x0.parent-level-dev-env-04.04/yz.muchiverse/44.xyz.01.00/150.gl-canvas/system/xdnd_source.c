/* xdnd_source.c - see xdnd_source.h for why this is not currently used. */
#include <string.h>
#include <X11/Xatom.h>
#include "xdnd_source.h"

static Atom XA_XdndAware, XA_XdndEnter, XA_XdndPosition, XA_XdndLeave,
           XA_XdndDrop, XA_XdndFinished, XA_XdndStatus, XA_XdndSelection,
           XA_text_plain, XA_XdndActionCopy, XA_targets;
static int xdnd_version = 5;
static Window xdnd_target = 0;
static int xdnd_have_target = 0;

/* WM reparenting means the real Xdnd-aware client window is often nested
 * one or more levels below the direct root child under the cursor (a
 * decoration frame). XTranslateCoordinates walks down one level at a time
 * for us; descend to the deepest window under the point, then walk back
 * up looking for the first one that actually has XdndAware set. */
static Window find_xdnd_target(Display *dpy, Window self, int root_x, int root_y) {
    Window root = RootWindow(dpy, DefaultScreen(dpy));
    Window stack[64];
    int depth = 0;
    Window cur = root;
    while (depth < 64) {
        Window child = None;
        int dx, dy;
        if (!XTranslateCoordinates(dpy, root, cur, root_x, root_y, &dx, &dy, &child)) break;
        if (child == None || child == self) break;
        cur = child;
        stack[depth++] = cur;
    }
    for (int i = depth - 1; i >= 0; i--) {
        Atom actual_type; int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        if (XGetWindowProperty(dpy, stack[i], XA_XdndAware, 0, 1, False, AnyPropertyType,
                                &actual_type, &actual_format, &nitems, &bytes_after, &data) == Success) {
            int has = (data != NULL && nitems > 0);
            if (data) XFree(data);
            if (has) return stack[i];
        }
    }
    return depth > 0 ? stack[0] : 0;
}

void xdnd_source_init(Display *d, Window w) {
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
    XChangeProperty(d, w, XA_XdndAware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&xdnd_aware, 1);
    XSync(d, False);
}

void xdnd_source_on_button_press(Display *dpy, Window win) {
    xdnd_have_target = 0;
    xdnd_target = 0;
    XSetSelectionOwner(dpy, XA_XdndSelection, win, CurrentTime);
}

void xdnd_source_on_motion(Display *dpy, Window win, int root_x, int root_y) {
    Window target = find_xdnd_target(dpy, win, root_x, root_y);

    if (target != xdnd_target) {
        if (xdnd_have_target && xdnd_target) {
            XEvent leave;
            memset(&leave, 0, sizeof(leave));
            leave.xclient.type = ClientMessage;
            leave.xclient.window = xdnd_target;
            leave.xclient.message_type = XA_XdndLeave;
            leave.xclient.format = 32;
            leave.xclient.data.l[0] = win;
            XSendEvent(dpy, xdnd_target, False, NoEventMask, &leave);
        }
        xdnd_target = target;
        xdnd_have_target = (target != None && target != win);
        if (xdnd_have_target) {
            XEvent enter;
            memset(&enter, 0, sizeof(enter));
            enter.xclient.type = ClientMessage;
            enter.xclient.window = xdnd_target;
            enter.xclient.message_type = XA_XdndEnter;
            enter.xclient.format = 32;
            enter.xclient.data.l[0] = win;
            enter.xclient.data.l[1] = (xdnd_version << 24);
            enter.xclient.data.l[2] = XA_text_plain;
            XSendEvent(dpy, xdnd_target, False, NoEventMask, &enter);
        }
    }

    if (xdnd_have_target && xdnd_target) {
        XEvent pos;
        memset(&pos, 0, sizeof(pos));
        pos.xclient.type = ClientMessage;
        pos.xclient.window = xdnd_target;
        pos.xclient.message_type = XA_XdndPosition;
        pos.xclient.format = 32;
        pos.xclient.data.l[0] = win;
        pos.xclient.data.l[2] = ((long)root_x << 16) | (root_y & 0xFFFF);
        pos.xclient.data.l[3] = CurrentTime;
        pos.xclient.data.l[4] = XA_XdndActionCopy;
        XSendEvent(dpy, xdnd_target, False, NoEventMask, &pos);
    }
}

void xdnd_source_on_button_release(Display *dpy, Window win) {
    if (xdnd_have_target && xdnd_target) {
        XEvent drop;
        memset(&drop, 0, sizeof(drop));
        drop.xclient.type = ClientMessage;
        drop.xclient.window = xdnd_target;
        drop.xclient.message_type = XA_XdndDrop;
        drop.xclient.format = 32;
        drop.xclient.data.l[0] = win;
        drop.xclient.data.l[2] = CurrentTime;
        XSendEvent(dpy, xdnd_target, False, NoEventMask, &drop);
        XSync(dpy, False);
    }
    xdnd_have_target = 0;
    xdnd_target = 0;
}

void xdnd_source_on_client_message(XClientMessageEvent *xev) {
    if (xev->message_type == XA_XdndStatus) {
        int accepted = xev->data.l[1] & 1;
        if (!accepted) { xdnd_have_target = 0; xdnd_target = 0; }
    }
    else if (xev->message_type == XA_XdndFinished) {
        xdnd_have_target = 0;
        xdnd_target = 0;
    }
}

void xdnd_source_on_selection_request(Display *dpy, XSelectionRequestEvent *xev, const char *pet_id) {
    XEvent sel;
    memset(&sel, 0, sizeof(sel));
    sel.xselection.type = SelectionNotify;
    sel.xselection.display = xev->display;
    sel.xselection.requestor = xev->requestor;
    sel.xselection.selection = xev->selection;
    sel.xselection.target = xev->target;
    sel.xselection.property = xev->property;
    sel.xselection.time = xev->time;

    if (xev->target == XA_targets) {
        Atom supported[] = { XA_text_plain };
        XChangeProperty(dpy, xev->requestor, xev->property,
                        XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)supported, 1);
    } else if (xev->target == XA_text_plain && pet_id) {
        XChangeProperty(dpy, xev->requestor, xev->property,
                        XA_text_plain, 8, PropModeReplace,
                        (unsigned char *)pet_id, strlen(pet_id));
    } else {
        sel.xselection.property = None;
    }
    XSendEvent(dpy, xev->requestor, False, NoEventMask, &sel);
    XSync(dpy, False);
}
