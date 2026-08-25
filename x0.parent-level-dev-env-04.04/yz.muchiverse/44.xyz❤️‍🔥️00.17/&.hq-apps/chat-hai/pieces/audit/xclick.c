#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Window find_by_name(Display *dpy, Window root, const char *needle) {
    Window root_ret, parent_ret, *children = NULL;
    unsigned int n = 0;
    if (!XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &n)) return 0;
    Window found = 0;
    for (unsigned int i = 0; i < n && !found; i++) {
        char *name = NULL;
        XFetchName(dpy, children[i], &name);
        if (name) {
            if (strstr(name, needle)) found = children[i];
            XFree(name);
        }
        if (!found) found = find_by_name(dpy, children[i], needle);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <x> <y> [window-id]\n", argv[0]); return 1; }
    int x = atoi(argv[1]), y = atoi(argv[2]);
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    Window root = DefaultRootWindow(dpy);
    Window win = 0;
    if (argc >= 4) win = (Window)strtoul(argv[3], NULL, 0);
    if (!win) win = find_by_name(dpy, root, "chat");
    if (!win) { fprintf(stderr, "window not found\n"); return 1; }
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, win, &wa);
    fprintf(stderr, "win=0x%lx at %d,%d size %dx%d\n", win, wa.x, wa.y, wa.width, wa.height);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xbutton.type = ButtonPress;
    ev.xbutton.display = dpy;
    ev.xbutton.window = win;
    ev.xbutton.root = root;
    ev.xbutton.subwindow = None;
    ev.xbutton.time = CurrentTime;
    ev.xbutton.x = x; ev.xbutton.y = y;
    ev.xbutton.x_root = wa.x + x; ev.xbutton.y_root = wa.y + y;
    ev.xbutton.state = 0; ev.xbutton.button = 1;
    ev.xbutton.same_screen = True;
    XSendEvent(dpy, win, True, ButtonPressMask, &ev);
    XFlush(dpy);
    usleep(20000);
    ev.xbutton.type = ButtonRelease;
    XSendEvent(dpy, win, True, ButtonReleaseMask, &ev);
    XFlush(dpy);
    XCloseDisplay(dpy);
    fprintf(stderr, "click sent at %d,%d\n", x, y);
    return 0;
}
