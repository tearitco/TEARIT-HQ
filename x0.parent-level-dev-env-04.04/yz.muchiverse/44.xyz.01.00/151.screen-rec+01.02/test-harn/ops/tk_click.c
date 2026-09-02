/* tk_click - simulate a single left-click at a window-relative (x, y) point
 * on a window found by title. Same XWarpPointer + XSendEvent technique as
 * 150.gl-canvas/test-harn/ops/tk_drag_sim.c, just a click instead of a
 * drag: exercises the real glutMouseFunc code path in screen_rec_gui.c
 * rather than bypassing it by writing the control file directly. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

static Window find_window_by_title(Display *d, Window start, const char *title) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren))
        return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strstr(name, title) != NULL) found = children[i];
            XFree(name);
        }
        if (!found) found = find_window_by_title(d, children[i], title);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: tk_click <window_title_substring> <rel_x> <rel_y>\n");
        return 1;
    }
    const char *title = argv[1];
    int rel_x = atoi(argv[2]);
    int rel_y = atoi(argv[3]);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "tk_click: cannot open display\n"); return 1; }

    Window win = find_window_by_title(dpy, RootWindow(dpy, DefaultScreen(dpy)), title);
    if (!win) {
        fprintf(stderr, "tk_click: window matching '%s' not found\n", title);
        XCloseDisplay(dpy);
        return 1;
    }

    Window root_win = RootWindow(dpy, DefaultScreen(dpy));
    int abs_x, abs_y;
    Window child_ret;
    XTranslateCoordinates(dpy, win, root_win, rel_x, rel_y, &abs_x, &abs_y, &child_ret);

    XWarpPointer(dpy, None, root_win, 0, 0, 0, 0, abs_x, abs_y);
    XSync(dpy, False);
    usleep(50000);

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xbutton.type = ButtonPress;
    ev.xbutton.send_event = True;
    ev.xbutton.display = dpy;
    ev.xbutton.window = win;
    ev.xbutton.root = root_win;
    ev.xbutton.x = rel_x;
    ev.xbutton.y = rel_y;
    ev.xbutton.x_root = abs_x;
    ev.xbutton.y_root = abs_y;
    ev.xbutton.button = Button1;
    ev.xbutton.state = 0;
    ev.xbutton.time = CurrentTime;
    XSendEvent(dpy, win, False, ButtonPressMask, &ev);
    XSync(dpy, False);
    usleep(50000);

    ev.xbutton.type = ButtonRelease;
    XSendEvent(dpy, win, False, ButtonReleaseMask, &ev);
    XSync(dpy, False);

    fprintf(stderr, "tk_click: clicked '%s' at window-relative (%d,%d) [screen (%d,%d)]\n",
            title, rel_x, rel_y, abs_x, abs_y);
    XCloseDisplay(dpy);
    return 0;
}
