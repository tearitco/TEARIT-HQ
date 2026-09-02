/* tk_screenshot - grab a SPECIFIC window's own content, not the whole
 * screen. Ported from 150.gl-canvas/test-harn/ops/tk_screenshot.c, but that
 * version XCopyArea's from the root window across the whole display --
 * exactly the operation Wayland/mutter blocks for security (the reason
 * this whole project exists instead of just calling ffmpeg's x11grab).
 * Grabbing an individual XWayland client window's own drawable directly
 * (what this file does) is a different, unrestricted operation: it's the
 * client's own buffer, not a read of the compositor's protected output. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    if (argc < 2) {
        fprintf(stderr, "Usage: tk_screenshot <window_title_substring> [outpath.ppm]\n");
        return 1;
    }
    const char *title = argv[1];
    const char *outpath = (argc >= 3) ? argv[2] : "screenshot.ppm";

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "tk_screenshot: cannot open display\n"); return 1; }

    Window win = find_window_by_title(dpy, RootWindow(dpy, DefaultScreen(dpy)), title);
    if (!win) {
        fprintf(stderr, "tk_screenshot: window matching '%s' not found\n", title);
        XCloseDisplay(dpy);
        return 1;
    }

    Window junk;
    int x, y;
    unsigned int w, h, border, depth;
    XGetGeometry(dpy, win, &junk, &x, &y, &w, &h, &border, &depth);

    XImage *img = XGetImage(dpy, win, 0, 0, w, h, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "tk_screenshot: XGetImage on window '%s' (%ux%u) failed\n", title, w, h);
        XCloseDisplay(dpy);
        return 1;
    }

    FILE *f = fopen(outpath, "wb");
    if (!f) {
        fprintf(stderr, "tk_screenshot: cannot write %s\n", outpath);
        XDestroyImage(img);
        XCloseDisplay(dpy);
        return 1;
    }

    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (unsigned int py = 0; py < h; py++) {
        for (unsigned int px = 0; px < w; px++) {
            unsigned long pixel = XGetPixel(img, px, py);
            unsigned char rgb[3] = {
                (unsigned char)((pixel >> 16) & 0xFF),
                (unsigned char)((pixel >> 8) & 0xFF),
                (unsigned char)(pixel & 0xFF)
            };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    XDestroyImage(img);
    XCloseDisplay(dpy);

    fprintf(stderr, "tk_screenshot: saved '%s' (%ux%u) to %s\n", title, w, h, outpath);
    return 0;
}
