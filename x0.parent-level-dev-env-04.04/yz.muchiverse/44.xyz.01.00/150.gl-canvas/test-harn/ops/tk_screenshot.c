#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

int main(int argc, char **argv) {
    const char *outpath = (argc >= 2) ? argv[1] : "screenshot.ppm";

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "tk_screenshot: cannot open display\n"); return 1; }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    int w = DisplayWidth(dpy, screen);
    int h = DisplayHeight(dpy, screen);

    GC gc = XCreateGC(dpy, root, 0, NULL);
    Pixmap pm = XCreatePixmap(dpy, root, w, h, depth);
    XCopyArea(dpy, root, pm, gc, 0, 0, w, h, 0, 0);

    XImage *img = XGetImage(dpy, pm, 0, 0, w, h, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "tk_screenshot: XGetImage on pixmap failed\n");
        XFreePixmap(dpy, pm); XFreeGC(dpy, gc); XCloseDisplay(dpy);
        return 1;
    }

    FILE *f = fopen(outpath, "wb");
    if (!f) { fprintf(stderr, "tk_screenshot: cannot write %s\n", outpath); XDestroyImage(img); XFreePixmap(dpy, pm); XFreeGC(dpy, gc); XCloseDisplay(dpy); return 1; }

    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long pixel = XGetPixel(img, x, y);
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
    XFreePixmap(dpy, pm);
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);

    fprintf(stderr, "tk_screenshot: saved %dx%d to %s\n", w, h, outpath);
    return 0;
}
