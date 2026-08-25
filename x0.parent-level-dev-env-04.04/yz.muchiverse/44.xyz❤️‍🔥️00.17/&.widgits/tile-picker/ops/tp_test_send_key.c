/* tp_test_send_key - real key injection for automated testing, since
 * xdotool is not installed on this machine (confirmed 2026-08-04).
 * Usage: tp_test_send_key.+x <window_name_substring> <keysym_name>
 * e.g. tp_test_send_key.+x "tile-picker" 3
 *      tp_test_send_key.+x "tile-picker" Return
 * Uses XTest (libXtst) to synthesize a real key press+release, focused
 * on the target window first via XSetInputFocus - a real X11 event, not
 * a fake file-based shortcut. */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Window find_by_name(Display *d, Window start, const char *target) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strstr(name, target)) found = children[i];
            XFree(name);
        }
        if (!found) found = find_by_name(d, children[i], target);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <window_name_substring> <keysym_name>\n", argv[0]);
        return 1;
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }

    Window w = find_by_name(dpy, RootWindow(dpy, DefaultScreen(dpy)), argv[1]);
    if (!w) { fprintf(stderr, "window '%s' not found\n", argv[1]); return 1; }
    XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    XSync(dpy, False);

    KeySym ks = XStringToKeysym(argv[2]);
    if (ks == NoSymbol) { fprintf(stderr, "bad keysym '%s'\n", argv[2]); return 1; }
    KeyCode kc = XKeysymToKeycode(dpy, ks);
    if (!kc) { fprintf(stderr, "no keycode for keysym\n"); return 1; }

    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    printf("SENT %s to window matching '%s'\n", argv[2], argv[1]);
    return 0;
}
