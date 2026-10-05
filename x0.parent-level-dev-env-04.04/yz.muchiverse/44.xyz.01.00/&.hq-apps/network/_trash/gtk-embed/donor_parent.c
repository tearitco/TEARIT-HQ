/* donor_parent.c - minimal Xlib host window to prove nb_webkit_view's
 * --xembed path embeds into a parent X window. Creates a main toplevel,
 * a child container at a fixed rect, prints the container's xid for the
 * child process to use, maps, and idles on X events. No visual chrome -
 * that exists to validate the embed, not to be the final house shape. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes a;
    Window top = XCreateSimpleWindow(dpy, root, 100, 100, 900, 700, 1,
                                     WhitePixel(dpy, scr), BlackPixel(dpy, scr));
    a.background_pixel = 0x222222;
    XChangeWindowAttributes(dpy, top, CWBackPixel, &a);
    XSelectInput(dpy, top, StructureNotifyMask | ExposureMask);
    XMapWindow(dpy, top);
    Window cont = XCreateSimpleWindow(dpy, top, 20, 20, 800, 600, 1,
                                      0, 0x111111);
    XMapWindow(dpy, cont);
    XStoreName(dpy, top, "donor-parent (xembed spike)");
    /* print container xid in all three notations the child parses */
    printf("CONTAINER_XID=%lu\n", (unsigned long)cont);
    fflush(stdout);
    XEvent e;
    for (;;) {
        XNextEvent(dpy, &e);
        if (e.type == DestroyNotify) return 0;
    }
}
