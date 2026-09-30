/* khtpm_strip_x11_win.h — thin Win32 stand-in for the Xlib/Xft surface
 * khtpm_strip_parser.c actually calls. Linux still includes real X11.
 * Design logic stays in the parser + layout engine. */
#ifndef KHTPM_STRIP_X11_WIN_H
#define KHTPM_STRIP_X11_WIN_H

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define True  1
#define False 0

#define ButtonPress      4
#define ButtonRelease    5
#define MotionNotify     6
#define KeyPress         2
#define FocusIn          9
#define FocusOut        10
#define Expose          12

#define Button1 1
#define Button2 2
#define Button3 3

#define ExposureMask      1
#define ButtonPressMask   2
#define KeyPressMask      4
#define FocusChangeMask   8
#define ButtonReleaseMask 16
#define ButtonMotionMask  32
#define StructureNotifyMask 64
#define ClientMessage      33

#define CWOverrideRedirect 1
#define CWBackPixel        2
#define CWEventMask        4
#define CWColormap         8
#define AllocNone          0
#define GrabModeAsync      1
#define GrabSuccess        0
#define ShapeBounding      0
#define ShapeSet           0

#define CopyFromParent 0
#define InputOutput    1
#define ZPixmap        2
#define AllPlanes      (~0UL)
#define LSBFirst       0
#define MSBFirst       1
#define PropModeReplace 0
#define XA_CARDINAL    6
#define GCForeground   1
#define GCBackground   2
#define GCFont         4

#define XK_Left      0xff51
#define XK_Right     0xff53
#define XK_Up        0xff52
#define XK_Down      0xff54
#define XK_Return    0xff0d
#define XK_KP_Enter  0xff8d
#define XK_Escape    0xff1b
#define XK_BackSpace 0xff08
#define XK_Tab       0xff09
#define RevertToParent 2
#define CurrentTime    0UL
#define None           0L

typedef unsigned long KeySym;
typedef unsigned long Atom;
typedef unsigned long Colormap;
typedef int Status;
typedef unsigned char FcChar8;

typedef struct {
    unsigned long red_mask, green_mask, blue_mask;
} Visual;

typedef struct Xd Xd;
typedef Xd *Window;
typedef Xd *Pixmap;
typedef Xd *Drawable;

typedef struct {
    unsigned long foreground;
    unsigned long background;
} XGCValues;

typedef struct _GC {
    unsigned long foreground;
    unsigned long background;
} *GC;

typedef struct {
    unsigned short red, green, blue;
    unsigned long pixel;
} XColor;

typedef struct {
    unsigned short red, green, blue, alpha;
} XRenderColor;

typedef struct {
    int override_redirect;
    unsigned long background_pixel;
    long event_mask;
    Colormap colormap;
} XSetWindowAttributes;

typedef unsigned long Font;
typedef struct {
    Font fid;
    int ascent, descent;
} XFontStruct;

typedef struct {
    short x, y;
    unsigned short width, height;
} XRectangle;

typedef void *XFontSet;

typedef struct {
    char *res_name;
    char *res_class;
} XClassHint;

typedef struct {
    int width, height, xoffset, format, byte_order, bitmap_pad, depth, bytes_per_line, bits_per_pixel;
    char *data;
} XImage;

typedef struct {
    Window window;
    unsigned int state;
    unsigned int keycode;
} XKeyEvent;

typedef struct {
    int type;
    struct { Window window; } xany;
    struct { Window window; int x, y, button, x_root, y_root; } xbutton;
    struct { Window window; int x, y, x_root, y_root; unsigned state; } xmotion;
    XKeyEvent xkey;
    struct { Window window; Atom message_type; struct { long l[5]; } data; } xclient;
} XEvent;

typedef struct Display Display;

typedef struct {
    unsigned long pixel;
    unsigned short color;
} XftColor;

typedef struct {
    HFONT hf;
    int px;
    int ascent, descent;
} XftFont;

typedef struct {
    Drawable d;
} XftDraw;

Display *XOpenDisplay(const char *name);
void XCloseDisplay(Display *dpy);
#define RootWindow(dpy, scr) ((Window)0)
int DefaultScreen(Display *dpy);
int DisplayWidth(Display *dpy, int scr);
int DisplayHeight(Display *dpy, int scr);
unsigned long BlackPixel(Display *dpy, int scr);
unsigned long WhitePixel(Display *dpy, int scr);
Colormap DefaultColormap(Display *dpy, int scr);
Visual *DefaultVisual(Display *dpy, int scr);
int DefaultDepth(Display *dpy, int scr);
int ConnectionNumber(Display *dpy);

Window XCreateWindow(Display *dpy, Window parent, int x, int y,
                     unsigned w, unsigned h, unsigned border, int depth, unsigned cls,
                     Visual *vis, unsigned long valuemask, XSetWindowAttributes *swa);
void XDestroyWindow(Display *dpy, Window w);
void XMapRaised(Display *dpy, Window w);
void XUnmapWindow(Display *dpy, Window w);
void XRaiseWindow(Display *dpy, Window w);
void XMoveResizeWindow(Display *dpy, Window w, int x, int y, unsigned width, unsigned height);
void XSetWindowBackground(Display *dpy, Window w, unsigned long pixel);
void XSetInputFocus(Display *dpy, Window w, int revert, unsigned long time);
void XFlush(Display *dpy);
void XSync(Display *dpy, int discard);
int XPending(Display *dpy);
int XNextEvent(Display *dpy, XEvent *ev);

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *v);
void XFreeGC(Display *dpy, GC gc);
void XSetForeground(Display *dpy, GC gc, unsigned long pixel);
void XSetBackground(Display *dpy, GC gc, unsigned long pixel);
void XCopyGC(Display *dpy, GC src, unsigned long mask, GC dst);
int XGetGCValues(Display *dpy, GC gc, unsigned long mask, XGCValues *v);

void XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y, unsigned w, unsigned h);
void XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2);
void XDrawRectangle(Display *dpy, Drawable d, GC gc, int x, int y, unsigned w, unsigned h);
int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y, const char *s, int len);

Pixmap XCreatePixmap(Display *dpy, Drawable d, unsigned w, unsigned h, unsigned depth);
void XFreePixmap(Display *dpy, Pixmap p);
void XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc,
               int sx, int sy, unsigned w, unsigned h, int dx, int dy);

XImage *XCreateImage(Display *dpy, Visual *v, unsigned depth, int format, int offset,
                     char *data, unsigned w, unsigned h, int pad, int bpl);
XImage *XGetImage(Display *dpy, Drawable d, int x, int y, unsigned w, unsigned h,
                  unsigned long plane, int format);
void XPutImage(Display *dpy, Drawable d, GC gc, XImage *img,
               int sx, int sy, int dx, int dy, unsigned w, unsigned h);
void XDestroyImage(XImage *img);

int XAllocNamedColor(Display *dpy, Colormap cmap, const char *name, XColor *sc, XColor *ec);
int XQueryColor(Display *dpy, Colormap cmap, XColor *c);

Atom XInternAtom(Display *dpy, const char *name, int only_if_exists);
int XChangeProperty(Display *dpy, Window w, Atom prop, Atom type, int format,
                    int mode, const unsigned char *data, int nelements);

XClassHint *XAllocClassHint(void);
void XSetClassHint(Display *dpy, Window w, XClassHint *ch);
void XFree(void *p);

KeySym XLookupKeysym(XKeyEvent *ev, int idx);
int XLookupString(XKeyEvent *ev, char *buf, int n, KeySym *ks, void *compose);

XftFont *XftFontOpenName(Display *dpy, int screen, const char *name);
XftDraw *XftDrawCreate(Display *dpy, Drawable d, Visual *v, Colormap cmap);
void XftDrawDestroy(XftDraw *dr);
int XftColorAllocValue(Display *dpy, Visual *v, Colormap cmap, const XRenderColor *c, XftColor *out);
void XftColorFree(Display *dpy, Visual *v, Colormap cmap, XftColor *c);
void XftDrawStringUtf8(XftDraw *dr, const XftColor *col, XftFont *font,
                       int x, int y, const FcChar8 *s, int len);

void x11_wait(Display *dpy, int usec);

void XMapWindow(Display *dpy, Window w);
void XMoveWindow(Display *dpy, Window w, int x, int y);
void XClearWindow(Display *dpy, Window w);
void XStoreName(Display *dpy, Window w, const char *name);
void XDrawPoint(Display *dpy, Drawable d, GC gc, int x, int y);
void XFillArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned w, unsigned h, int a1, int a2);
Colormap XCreateColormap(Display *dpy, Window w, Visual *v, int alloc);
int XGetGeometry(Display *dpy, Drawable d, Window *root, int *x, int *y,
                 unsigned *w, unsigned *h, unsigned *bw, unsigned *depth);
int XCheckWindowEvent(Display *dpy, Window w, long mask, XEvent *ev);
int XGrabPointer(Display *dpy, Window w, int owner, unsigned mask, int pmode, int kmode,
                 Window confine, int cursor, unsigned long time);
int XGrabKeyboard(Display *dpy, Window w, int owner, int pmode, int kmode, unsigned long time);
int XUngrabPointer(Display *dpy, unsigned long time);
int XUngrabKeyboard(Display *dpy, unsigned long time);
void XShapeCombineMask(Display *dpy, Window dest, int dest_kind, int x, int y, Pixmap mask, int op);
void x11_apply_alpha_shape(Window dest, const unsigned char *rgba, int res, int win_px);
XFontStruct *XLoadQueryFont(Display *dpy, const char *name);
int XSetFont(Display *dpy, GC gc, Font font);
XFontSet XCreateFontSet(Display *dpy, const char *name, char ***missing, int *nmissing, char **def);
int Xutf8DrawString(Display *dpy, Drawable d, XFontSet fs, GC gc, int x, int y, const char *s, int len);
int Xutf8TextExtents(XFontSet fs, const char *s, int len, XRectangle *ink, XRectangle *logical);
void XFreeStringList(char **list);
char *XSetLocaleModifiers(const char *mod);
int x11_process_running(const char *name);
int x11_spawn_cwd(const char *exe, const char *arg1);


typedef struct {
    int x, y, width, height;
} XWindowAttributes;
typedef struct {
    long flags;
    int x, y, width, height;
} XSizeHints;
typedef struct {
    long flags;
    int input;
    int initial_state;
} XWMHints;
typedef struct {
    short width, height, x, y, xOff, yOff;
} XGlyphInfo;
#define InputHint 1
#define USPosition 1
#define PPosition 4
#define WM_DELETE_WINDOW 1
XSizeHints *XAllocSizeHints(void);
XWMHints *XAllocWMHints(void);
void XSetWMHints(Display *dpy, Window w, XWMHints *h);
void XSetWMNormalHints(Display *dpy, Window w, XSizeHints *h);
int XSetWMProtocols(Display *dpy, Window w, Atom *protocols, int n);
int XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *wa);
int XGetInputFocus(Display *dpy, Window *w, int *revert);
unsigned long XGetPixel(XImage *img, int x, int y);
char *XKeysymToString(KeySym ks);
int XAllocColor(Display *dpy, Colormap cmap, XColor *c);
int XParseColor(Display *dpy, Colormap cmap, const char *spec, XColor *c);
void XftFontClose(Display *dpy, XftFont *font);
void XftTextExtentsUtf8(Display *dpy, XftFont *font, const FcChar8 *s, int len, XGlyphInfo *out);

#ifdef __cplusplus
}
#endif
#endif
