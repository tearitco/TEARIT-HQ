#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xatom.h>

/* WM reparenting (mutter/gnome-shell wraps client windows in a decoration
 * frame) means the titled window is often NOT a direct child of root -
 * search recursively, not just one level deep. */
static Window find_window_by_title(Display *d, Window start, const char *title) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren))
        return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strcmp(name, title) == 0) found = children[i];
            XFree(name);
        }
        if (!found) found = find_window_by_title(d, children[i], title);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tk_drag_sim <source_title> <target_title> [step_delay_ms]\n");
        return 1;
    }
    const char *src_title = argv[1];
    const char *tgt_title = argv[2];
    int step_delay = (argc >= 4) ? atoi(argv[3]) : 10;
    if (step_delay < 1) step_delay = 1;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "tk_drag_sim: cannot open display\n"); return 1; }

    Window root_search = RootWindow(dpy, DefaultScreen(dpy));
    Window src = find_window_by_title(dpy, root_search, src_title);
    Window tgt = find_window_by_title(dpy, root_search, tgt_title);
    if (!src) { fprintf(stderr, "tk_drag_sim: source window '%s' not found\n", src_title); XCloseDisplay(dpy); return 1; }
    if (!tgt) { fprintf(stderr, "tk_drag_sim: target window '%s' not found\n", tgt_title); XCloseDisplay(dpy); return 1; }

    Window root_win = RootWindow(dpy, DefaultScreen(dpy));
    Window junk;
    int src_x, src_y;
    unsigned int src_w, src_h, b, d;
    XGetGeometry(dpy, src, &junk, &src_x, &src_y, &src_w, &src_h, &b, &d);

    int tgt_x, tgt_y;
    unsigned int tgt_w, tgt_h;
    XGetGeometry(dpy, tgt, &junk, &tgt_x, &tgt_y, &tgt_w, &tgt_h, &b, &d);

    /* XGetGeometry's x/y are relative to the window's PARENT, not the
     * screen. src (override_redirect, unmanaged) has root as its parent
     * so this is a no-op for it, but tgt is reparented into a WM
     * decoration frame - its raw x/y would be relative to that frame,
     * not the screen. Translate both into true root-relative (absolute
     * screen) coordinates before computing drag centers/path. */
    int src_abs_x, src_abs_y, tgt_abs_x, tgt_abs_y;
    Window child_ret;
    XTranslateCoordinates(dpy, src, root_win, 0, 0, &src_abs_x, &src_abs_y, &child_ret);
    XTranslateCoordinates(dpy, tgt, root_win, 0, 0, &tgt_abs_x, &tgt_abs_y, &child_ret);

    int src_cx = src_abs_x + src_w / 2;
    int src_cy = src_abs_y + src_h / 2;
    int tgt_cx = tgt_abs_x + tgt_w / 2;
    int tgt_cy = tgt_abs_y + tgt_h / 2;

    fprintf(stderr, "tk_drag_sim: source='%s' (%d,%d %ux%u) target='%s' (%d,%d %ux%u)\n",
            src_title, src_x, src_y, src_w, src_h, tgt_title, tgt_x, tgt_y, tgt_w, tgt_h);

    XEvent ev;
    int STEPS = 40;

    /* Warp to source center */
    XWarpPointer(dpy, None, root_win, 0, 0, 0, 0, src_cx, src_cy);
    XSync(dpy, False);
    usleep(50000);

    /* ButtonPress on source */
    memset(&ev, 0, sizeof(ev));
    ev.xbutton.type = ButtonPress;
    ev.xbutton.send_event = True;
    ev.xbutton.display = dpy;
    ev.xbutton.window = src;
    ev.xbutton.root = root_win;
    ev.xbutton.x_root = src_cx;
    ev.xbutton.y_root = src_cy;
    ev.xbutton.button = Button1;
    ev.xbutton.state = 0;
    ev.xbutton.time = CurrentTime;
    XSendEvent(dpy, src, False, ButtonPressMask, &ev);
    XSync(dpy, False);
    usleep(30000);

    /* Drag: warp + synthetic MotionNotify at each step */
    int cur_x = src_cx, cur_y = src_cy;
    for (int i = 1; i <= STEPS; i++) {
        cur_x = src_cx + (tgt_cx - src_cx) * i / STEPS;
        cur_y = src_cy + (tgt_cy - src_cy) * i / STEPS;

        XWarpPointer(dpy, None, root_win, 0, 0, 0, 0, cur_x, cur_y);

        memset(&ev, 0, sizeof(ev));
        ev.xmotion.type = MotionNotify;
        ev.xmotion.send_event = True;
        ev.xmotion.display = dpy;
        ev.xmotion.window = src;
        ev.xmotion.root = root_win;
        ev.xmotion.x_root = cur_x;
        ev.xmotion.y_root = cur_y;
        ev.xmotion.state = Button1Mask;
        ev.xmotion.time = CurrentTime;
        XSendEvent(dpy, src, False, ButtonMotionMask, &ev);
        XSync(dpy, False);
        usleep(step_delay * 1000);
    }

    usleep(50000);

    /* ButtonRelease on source */
    memset(&ev, 0, sizeof(ev));
    ev.xbutton.type = ButtonRelease;
    ev.xbutton.send_event = True;
    ev.xbutton.display = dpy;
    ev.xbutton.window = src;
    ev.xbutton.root = root_win;
    ev.xbutton.x_root = tgt_cx;
    ev.xbutton.y_root = tgt_cy;
    ev.xbutton.button = Button1;
    ev.xbutton.state = 0;
    ev.xbutton.time = CurrentTime;
    XSendEvent(dpy, src, False, ButtonReleaseMask, &ev);
    XSync(dpy, False);

    fprintf(stderr, "tk_drag_sim: drop simulated: '%s' -> '%s'\n", src_title, tgt_title);
    XCloseDisplay(dpy);
    return 0;
}
