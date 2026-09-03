#define _POSIX_C_SOURCE 200809L
/* nb_embed_demo.c - PROOF OF CONCEPT for "our x11-hq chrome + real
 * engine page content, in ONE window".
 *
 * Stands in for what khtpm_core_render would do:
 *   - create a top-level X window
 *   - draw a chrome strip (toolbar / tabs / address) ourselves
 *   - create a bare child X window for the CONTENT rect
 *   - spawn  nb_webkit_view.+x --xembed <content_xid> <url>
 *     which reparents a real WebKitGTK page into that child window
 *   - on resize, move/resize the content child; the webkit view tracks it
 *
 * No GL. The chrome is plain Xlib fills+text; the page is WebKitGTK's
 * normal X11/cairo paint. Two processes, one visible window.
 *
 * usage: nb_embed_demo <url>
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define CHROME_H 84

int main(int argc, char **argv) {
    const char *url = argc > 1 ? argv[1] : "https://example.com";

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);

    int W = 1200, H = 850;
    unsigned long bg = 0x1c1c1c, chrome = 0x141414, accent = 0xff8c00, ink = 0xdddddd;

    Window top = XCreateSimpleWindow(dpy, root, 60, 60, W, H, 0, 0, bg);
    XStoreName(dpy, top, "network  (x11-hq chrome + real engine)");
    XSelectInput(dpy, top, ExposureMask | StructureNotifyMask | KeyPressMask);

    /* the content sub-window the engine reparents into */
    Window content = XCreateSimpleWindow(dpy, top, 0, CHROME_H, W, H - CHROME_H, 0, 0, 0x000000);
    XSelectInput(dpy, content, StructureNotifyMask);
    XMapWindow(dpy, content);
    XMapWindow(dpy, top);
    XFlush(dpy);

    GC gc = XCreateGC(dpy, top, 0, NULL);
    XFontStruct *fs = XLoadQueryFont(dpy, "-*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-*");
    if (!fs) fs = XLoadQueryFont(dpy, "fixed");
    if (fs) XSetFont(dpy, gc, fs->fid);

    /* spawn the embedded engine view */
    char xid[32]; snprintf(xid, sizeof(xid), "0x%lx", content);
    pid_t kid = fork();
    if (kid == 0) {
        char self[4096]; ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n <= 0) _exit(127);
        self[n] = 0;
        char *slash = strrchr(self, '/'); if (slash) *slash = 0;
        char view[4200]; snprintf(view, sizeof(view), "%s/+x/nb_webkit_view.+x", self);
        execl(view, view, "--xembed", xid, url, (char *)NULL);
        _exit(127);
    }

    for (;;) {
        XEvent e; XNextEvent(dpy, &e);
        if (e.type == Expose && e.xexpose.window == top) {
            XSetForeground(dpy, gc, chrome);
            XFillRectangle(dpy, top, gc, 0, 0, (unsigned)W, CHROME_H);
            XSetForeground(dpy, gc, ink);
            XDrawString(dpy, top, gc, 12, 20, "[ ] Back   [ ] Forward   [ ] Reload   [ ] Home   [ ] Bookmark", 58);
            XDrawString(dpy, top, gc, 12, 44, "[*] tab: this page      [ ] + New tab", 37);
            XSetForeground(dpy, gc, 0x2a2a2a);
            XFillRectangle(dpy, top, gc, 10, 54, (unsigned)(W - 20), 22);
            XSetForeground(dpy, gc, accent);
            XDrawString(dpy, top, gc, 16, 69, url, (int)strlen(url));
        } else if (e.type == ConfigureNotify && e.xconfigure.window == top) {
            W = e.xconfigure.width; H = e.xconfigure.height;
            XMoveResizeWindow(dpy, content, 0, CHROME_H, (unsigned)W, (unsigned)(H - CHROME_H));
        } else if (e.type == KeyPress) {
            break;
        } else if (e.type == DestroyNotify && e.xdestroywindow.window == top) {
            break;
        }
    }
    if (kid > 0) kill(kid, 15);
    XCloseDisplay(dpy);
    return 0;
}
