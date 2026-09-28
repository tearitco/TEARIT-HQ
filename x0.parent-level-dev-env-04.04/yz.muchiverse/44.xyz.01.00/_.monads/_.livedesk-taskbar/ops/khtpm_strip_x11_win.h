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
#include "khtpm_strip_posix_win.h"

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
/* REAL, NEW 2026-09-26 - the SHAPE-EXTENSION OPERATOR set (the last argument
 * to XShapeCombineMask), which is a different enum from dest_kind above and
 * so needs its own real X11 numbers. ShapeBounding/ShapeClip/ShapeInput (the
 * dest_kind values) are 0/1/2; the op values are 0..4, so ShapeSet==0 above
 * is the correct op spelling and does not collide with ShapeBounding==0
 * because they never appear in the same enum position.
 * khtpm_entity.c:2030 passes ShapeUnion when merging cursword's per-pixel
 * silhouette disc into the mask it already built. */
#define ShapeUnion         1
#define ShapeIntersect     2
#define ShapeSubtract      3
#define ShapeInvert        4
/* REAL, NEW 2026-09-26 - XVisualInfo.class for XMatchVisualInfo(). Real
 * Xlib numbering (StaticGray 0, GrayScale 1, StaticColor 2, PseudoColor 3,
 * TrueColor 4, DirectColor 5). The entity asks for a 32-bit TrueColor
 * visual so its per-pixel alpha silhouette composites; see
 * XMatchVisualInfo() in the .c for what the shim actually answers. */
#define TrueColor          4

#define CopyFromParent 0
#define InputOutput    1
#define ZPixmap        2
#define AllPlanes      (~0UL)
#define LSBFirst       0
#define MSBFirst       1
#define PropModeReplace 0
/* REAL, NEW 2026-09-26 - XA_ATOM was missing, so taskbar_make_wm_managed_
 * dock()'s two _NET_WM_STATE/_NET_WM_WINDOW_TYPE property writes would not
 * compile. Values are the real Xlib predefined-atom numbers, same as the
 * XA_CARDINAL 6 that was already here. */
#define XA_PRIMARY    1
#define XA_ATOM        4
#define XA_CARDINAL    6
#define XA_STRING     31
#define XA_WINDOW     33
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

/* REAL, NEW 2026-09-26 - remaining keysyms the dock/grid-jump key handling
 * tests. Real Xlib keysym values, so a keycode mapped through
 * XLookupKeysym() keeps comparing equal to these. */
#define XK_space     0x0020
#define XK_ISO_Left_Tab 0xfe20
#define XK_Home      0xff50
#define XK_Page_Up   0xff55
#define XK_Page_Down 0xff56
#define XK_End       0xff57
#define XK_Delete    0xffff

/* REAL, NEW 2026-09-26 - the Latin-1 keysyms khtpm_entity.c's cursword
 * camera keys test. Real X11 derives these by arithmetic, and so does this:
 * digits are 0x030+(c-'0') and lowercase letters are 0x061+(c-'a'), which is
 * exactly the rule that produced the XK_space 0x0020 already above.
 *
 * These compare EQUAL to what the shim's own XLookupString() hands back for
 * a typed character, because WM_CHAR is delivered with keycode 0x10000|ch
 * and XLookupString's non-special path returns ks = keycode & 0xff, i.e. the
 * character's own ASCII value - which is the same number as the keysym for
 * every character in this group. So a real keypress really does reach
 * cursword_handle_camera_key() as a matching XK_*, not as a miss. */
#define XK_0 0x030
#define XK_a 0x061
#define XK_c 0x063
#define XK_d 0x064
#define XK_e 0x065
#define XK_f 0x066
#define XK_q 0x071
#define XK_r 0x072
#define XK_s 0x073
#define XK_t 0x074
#define XK_v 0x076
#define XK_w 0x077

/* --- GC value masks -----------------------------------------------------
 * The three already above (GCForeground 1, GCBackground 2, GCFont 4) are
 * this shim's own invented small values from the strip-parser era and are
 * load-bearing for khtpm_strip_x11_win.c's XCreateGC/XCopyGC/XGetGCValues,
 * which switch on exactly those numbers. They are therefore deliberately
 * NOT corrected to the real Xlib values (4/8/16384) here - doing so would
 * silently break every existing call. The masks below are new, so they get
 * the real Xlib bit positions, which are all clear of 1/2/4 and so cannot
 * collide with the invented trio. */
#define GCFillStyle         (1L<<8)
#define GCTile              (1L<<10)
#define GCTileStipXOrigin   (1L<<12)
#define GCTileStipYOrigin   (1L<<13)
#define GCGraphicsExposures (1L<<16)

/* Line/join/cap/fill styles and the GC fields they live in. */
#define LineSolid       0
#define LineOnOffDash   1
#define CapButt         1
#define JoinMiter       0
#define FillSolid       0
#define FillTiled       1

/* --- Event masks --------------------------------------------------------
 * Same caveat as the GC masks: the strip-era masks above (ExposureMask 1,
 * ButtonPressMask 2, KeyPressMask 4, FocusChangeMask 8, ButtonReleaseMask
 * 16, ButtonMotionMask 32, StructureNotifyMask 64) are this shim's invented
 * values and are what its WndProc/XSelectInput already agree on. The masks
 * below are new and use values clear of 1..64, except ShiftMask/ControlMask
 * which the old parser never used and so are free. */
#define NoEventMask             0L
#define ShiftMask               128L
#define ControlMask             1024L
#define Button1Mask             256L
#define SubstructureNotifyMask  4096L
#define SubstructureRedirectMask 8192L
/* REAL, NEW 2026-09-26 - PointerMotionMask. Real Xlib spells this 1<<6 == 64,
 * but 64 is already the strip-era invented StructureNotifyMask above and the
 * WndProc/XSelectInput agree on 64, so the real value cannot be reused
 * without breaking that. It takes the next free bit instead, which is safe
 * because this constant is only ever read as an argument to
 * XGrabPointer(), which the shim implements without consulting the mask at
 * all (there is no real X server to select events on). */
#define PointerMotionMask       2048L

/* Event type codes (these match real Xlib and the ones already above). */
#define KeyRelease        3
#define SelectionClear    29
#define SelectionRequest  30
#define SelectionNotify   31

/* Notify* modes (XSetIC/Xinerama style owner modes). */
#define NotifyNormal        0
#define NotifyGrab          1
#define NotifyUngrab        2
#define NotifyWhileGrabbed  3
#define NotifyInferior      4
#define NotifyPointer       5
#define NotifyPointerRoot   6
#define NormalState         1

/* WM_STATE / XSizeHints flag. */
#define StateHint 1

/* XGetWindowProperty's "any type" request, and the four real X11
 * stack-mode values for XWindowChanges.stack_mode. render_managed_sink_below()
 * (core_render.c:312) restacks the managed taskbar window with Below. */
#define AnyPropertyType 0L
#define Below            0
#define Above            1
#define TopIf            2
#define BottomIf         3
#define Opposite         4

/* CWStackMode. The shim's existing CW* values are the invented 1/2/4/8, so
 * this takes the next free bit rather than the real Xlib (1<<15). */
#define CWStackMode 16
/* NEW 2026-09-26 - the geometry bits, same reasoning: real Xlib uses
 * 1/2/4/8, all taken above, so these continue the sequence. Only
 * render_managed_sink_below() actually calls XConfigureWindow today, with
 * CWStackMode alone; the rest exist so win_apply() in the .c is complete and
 * a future caller can pass them. */
#define CWX      32
#define CWY      64
#define CWWidth  128
#define CWHeight 256
/* REAL, NEW 2026-09-26 - CWBorderPixel, next in the same invented sequence
 * (real Xlib uses 1<<3 == 8, which CWColormap above already took).
 * khtpm_entity.c:4141 puts it in the same XCreateWindow valuemask as
 * CWColormap/CWEventMask/CWOverrideRedirect/CWBackPixel when it creates its
 * ARGB window. The shim creates every window as a WS_POPUP, which has no
 * border at all, so there is nothing for this bit to do - it is declared so
 * the mask compiles and the caller's intent stays legible, and win_apply()
 * in the .c deliberately ignores it rather than pretending to honour it. */
#define CWBorderPixel 512

typedef unsigned long KeySym;
typedef unsigned long Atom;
typedef unsigned long Colormap;
typedef int Status;
typedef unsigned char FcChar8;

/* REAL, NEW 2026-09-26 - khtpm_core_render.c (the renderer that actually draws
 * the dock, text-including khtpm_draw_core.c) needs a much wider slice of
 * Xlib than the old strip parser did. Types first: the Display forward
 * declaration has to move ABOVE XEvent now, because the selection-event
 * members below carry a Display* and the old XErrorEvent comment already
 * admitted it had to spell "struct Display *" out to work around that. */
typedef struct Display Display;
typedef int Bool;
typedef unsigned long Time;

#define Success 0

typedef struct {
    unsigned long red_mask, green_mask, blue_mask;
} Visual;

typedef struct Xd Xd;
typedef Xd *Window;
typedef Xd *Pixmap;
typedef Xd *Drawable;

/* REAL, NEW 2026-09-26 - foreground/background are the only fields the
 * strip-era XCreateGC/XCopyGC/XGetGCValues/XChangeGC actually read, and they
 * keep their original names and leading position. khtpm_draw_core.c sets
 * fill_style/tile/ts_x_origin/ts_y_origin/graphics_exposures on the same
 * struct, so those are declared too. The shim ignores the new ones (no
 * dash or tile support is needed for a solid-fill dock), but they must exist
 * for the caller's XChangeGC mask to compile. Declared in the same order as
 * real Xlib's XGCValues for readability, minus the ones nothing references. */
typedef struct {
    unsigned long function;
    unsigned long plane_mask;
    unsigned long foreground;
    unsigned long background;
    unsigned long line_width;
    unsigned long line_style;
    unsigned long cap_style;
    unsigned long join_style;
    unsigned long fill_style;
    unsigned long fill_rule;
    Pixmap        tile;
    Pixmap        stipple;
    long          ts_x_origin;
    long          ts_y_origin;
    unsigned long font;
    unsigned long subwindow_mode;
    Bool          graphics_exposures;
    long          clip_x_origin;
    long          clip_y_origin;
    unsigned long dash_offset;
    char          *dashes;
} XGCValues;

typedef struct _GC {
    unsigned long foreground;
    unsigned long background;
    /* REAL, NEW 2026-09-26 - the tiled-fill state. khtpm_draw_core.c stamps
     * a 12x12 checkerboard Pixmap through a FillTiled GC (draw_core.c:820-840)
     * and has done since the earlier per-6px-square alloc_pixel() loop was
     * removed as a performance fix. foreground/background above keep their
     * original names and leading position; the shim previously had no place
     * to record the fill style, so the checkerboard would have degraded to a
     * solid block. XFillRectangle() now honours these. */
    unsigned long fill_style;      /* FillSolid or FillTiled */
    Pixmap        tile;            /* source pattern for FillTiled */
    long          ts_x_origin;     /* tile origin, GCTileStipXOrigin */
    long          ts_y_origin;     /* tile origin, GCTileStipYOrigin */
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
    /* REAL, NEW 2026-09-26 - khtpm_core_render.c reads ev.xkey.time when
     * matching KeyPress/KeyRelease for grid-jump and shortcut handling. It
     * sits after keycode so the three fields the strip-era WndProc already
     * filled keep their existing offsets. */
    unsigned int state;
    unsigned int keycode;
    Time time;
} XKeyEvent;

/* REAL, NEW 2026-09-26 - xexpose/xfocus were missing from this union, which
 * is the whole reason the bars stayed blank. The parser's event loop reads
 * ev.xexpose.count/.window and ev.xfocus.window; with the members absent
 * those reads were compile errors, and once the shim "fixed" them by
 * writing the event into xany/xbutton instead (see the WndProc and
 * XMapRaised in the .c) every Expose compared against a NULL window and no
 * redraw ever matched. count==0 is the X11 "last Expose in this batch"
 * convention the parser deliberately keys its redraw off, so it must be a
 * real field here and must be set to 0, not left to chance. */
typedef struct {
    Window window;
    int x, y, width, height, count;
} XExposeEvent;

typedef struct {
    Window window;
    int mode;    /* NotifyNormal / NotifyGrab / NotifyUngrab */
    int detail;
} XFocusChangeEvent;

/* REAL, NEW 2026-09-26 - selection events. XDND drop support in
 * khtpm_core_render.c converts a selection (XConvertSelection) and then reads
 * ev.xselection.{selection,property,requestor,time,x,y,display,type} to
 * consume it; drag-source code fills ev.xselectionrequest.{target,property,
 * requestor,time}. Without these two union arms every one of those reads was
 * a compile error, and the accessors are what the drop path hangs on. */
typedef struct {
    int            type;
    unsigned long  serial;
    Bool           send_event;
    Display       *display;
    Window         window;
    Atom           selection;
    Time           time;
    int            x, y;
    Window         requestor;
    Atom           property;
    int            state;
    /* NOT a real Xlib member. core_render.c:10538 builds a SelectionNotify
     * reply in an XSelectionEvent and copies .target across from the
     * XSelectionRequestEvent it is answering. Real Xlib declares target only
     * on the request event, and a real server ignores the field on the way
     * out - but the code must still compile unchanged, so the shim carries
     * it. Harmless: nothing on the receive path reads it. */
    Atom           target;
} XSelectionEvent;

typedef struct {
    int            type;
    unsigned long  serial;
    Bool           send_event;
    Display       *display;
    Window         owner;
    Window         requestor;
    Atom           selection;
    Atom           target;
    Atom           property;
    Time           time;
} XSelectionRequestEvent;

typedef struct {
    int type;
    struct { Window window; } xany;
    struct { Window window; int x, y, button, x_root, y_root; } xbutton;
    struct { Window window; int x, y, x_root, y_root; unsigned state; } xmotion;
    XExposeEvent xexpose;
    XFocusChangeEvent xfocus;
    XKeyEvent xkey;
    /* REAL, NEW 2026-09-26 - .type and .format added: the XDND completion
     * message (core_render.c:9497/9500) fills a ClientMessage by hand, and
     * the strip-era arm had neither field, so both writes were errors. */
    struct { int type; Window window; Atom message_type; int format;
             struct { long l[5]; } data; } xclient;
    XSelectionEvent       xselection;
    XSelectionRequestEvent xselectionrequest;
} XEvent;

/* REAL, NEW 2026-09-26 - needed by the parser's own non-fatal X error
 * handler, which reads error_code/request_code/minor_code. Only those three
 * are ever touched; the shim never generates a protocol error, so this type
 * exists to keep that handler compiling and honest, not to carry state. */
typedef struct {
    int type;
    struct Display *display;   /* spelled out: the Display typedef sits
                                * below XEvent in this header */
    unsigned long resourceid;
    unsigned long serial;
    unsigned char error_code;
    unsigned char request_code;
    unsigned char minor_code;
} XErrorEvent;

/* REAL, NEW 2026-09-26 - XErrorHandler, and the reason XSetErrorHandler()'s
 * signature had to change. Real Xlib declares
 *     typedef int (*XErrorHandler)(Display *, XErrorEvent *);
 *     XErrorHandler XSetErrorHandler(XErrorHandler);
 * i.e. it returns the PREVIOUS handler so a caller can save/restore around a
 * risky section. The shim's original stub returned int ("had a handler
 * already"), which is fine for the two callers that ignore the result
 * (core_render.c:11715, strip_parser.c:2727) but not for
 * khtpm_entity.c:205-231, which does exactly the real save/restore dance
 * around XReparentWindow() - saving into an XErrorHandler, then handing it
 * back to restore. Under the old int-returning stub that value came back as
 * 0 and the restore installed a NULL handler, so this typedef and the
 * matching return type in the .c are load-bearing, not cosmetic.
 * It must be declared AFTER XErrorEvent above, hence its position here. */
typedef int (*XErrorHandler)(Display *, XErrorEvent *);

/* REAL, NEW 2026-09-26 - XVisualInfo, for XMatchVisualInfo(). Field order and
 * types mirror real Xlib exactly (including VisualID, which this shim has no
 * use for but which sits in the middle of the struct there, so any
 * positional initialiser a future caller writes keeps working).
 * khtpm_entity.c only ever reads .visual and .depth out of it - once in
 * popup_draw_text() for a 32-bit visual when the popup font needs one, and
 * once in tp_main() to ask for an ARGB visual so the per-pixel silhouette
 * composites. */
typedef struct {
    Visual       *visual;
    unsigned long visualid;
    int           screen;
    int           depth;
    int           class;
    unsigned long red_mask, green_mask, blue_mask;
    int           colormap;
    int           bits_per_rgb;
} XVisualInfo;

/* REAL, NEW 2026-09-26 - XConfigureWindow's value struct. khtpm_core_render.c
 * moves/resizes the dock and popup windows through it (chained onto
 * XWindowChanges, exactly as real Xlib does). */
typedef struct {
    long flags;
    int  x, y;
    int  width, height;
    int  border_width;
    int  sibling;
    int  stack_mode;
} XWindowChanges;

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
/* REAL, NEW 2026-09-26 - DefaultRootWindow was missing outright, so every
 * XCreatePixmap/XTranslateCoordinates call in khtpm_draw_core.c passed an
 * implicitly-declared int where a Drawable belongs. It is the DefaultScreen
 * flavour of the RootWindow macro above, so it resolves to the same value. */
#define DefaultRootWindow(dpy) ((Window)0)
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
/* REAL, NEW 2026-09-26 - the five below are what the recovered
 * khtpm_strip_parser.c (git 19774224^) actually calls that this shim did
 * not declare, i.e. the complete remaining X11 gap for the bar build. */
void XLowerWindow(Display *dpy, Window w);
int XQueryTree(Display *dpy, Window w, Window *root_ret, Window *parent_ret,
               Window **children, unsigned int *nchildren);
int XFetchName(Display *dpy, Window w, char **name_out);
/* REAL, NEW 2026-09-26 - returns the PREVIOUS handler (real Xlib's
 * signature), not a "one already existed" flag. See the XErrorHandler
 * typedef above for the call site that depends on it. */
XErrorHandler XSetErrorHandler(XErrorHandler handler);
int XGetErrorText(Display *dpy, int code, char *buf, int len);
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
int x11_spawn_cwd2(const char *exe, const char *arg1, const char *arg2);


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

/* --- REAL, NEW 2026-09-26 -----------------------------------------------
 * The Xlib calls khtpm_core_render.c makes that this shim did not yet
 * declare. All ten were implicit-declaration errors in the first
 * khtpm_core_render.c probe, which on Win64 is not a warning but a hard
 * error (no int-returning assumption is safe when the real return is a
 * 64-bit pointer). Grouped by what the renderer needs them for:
 *
 *  - window plumbing: XConfigureWindow / XResizeWindow (dock + popup
 *    geometry, plus the zorder/corrective-move paths)
 *  - drawing state:  XChangeGC / XSetLineAttributes (per-run colours,
 *    dash styles for the focus ring and drop-target outlines)
 *  - XDND drop:     XConvertSelection / XSetSelectionOwner /
 *                   XGetWindowProperty / XSendEvent (the drag-and-drop
 *                   path is entirely built on these four)
 *  - event pump:    XCheckTypedWindowEvent (non-blocking drain of a single
 *                   event type, used to peek ClientMessage without
 *                   blocking the render loop)
 */
void XChangeGC(Display *dpy, GC gc, unsigned long mask, XGCValues *v);
void XSetLineAttributes(Display *dpy, GC gc, unsigned int width,
                        int line_style, int cap_style, int join_style);
void XConfigureWindow(Display *dpy, Window w, unsigned int mask, XWindowChanges *wc);
void XResizeWindow(Display *dpy, Window w, unsigned int width, unsigned int height);
int  XConvertSelection(Display *dpy, Atom selection, Atom target, Atom property,
                       Window requestor, Time time);
int  XSetSelectionOwner(Display *dpy, Atom selection, Window owner, Time time);
int  XGetWindowProperty(Display *dpy, Window w, Atom property,
                        long long_offset, long long_length, Bool delete,
                        Atom req_type, Atom *actual_type_return,
                        int *actual_format_return,
                        unsigned long *nitems_return,
                        unsigned long *bytes_after_return,
                        unsigned char **prop_return);
Status XSendEvent(Display *dpy, Window w, Bool propagate, long event_mask, XEvent *ev);
int  XCheckTypedWindowEvent(Display *dpy, Window w, long mask, XEvent *ev);
/* REAL, NEW 2026-09-26 - pointer-position hit testing (core_render.c:8646,
 * the popup auto-hide screen-edge checks). */
Bool XTranslateCoordinates(Display *dpy, Window src_w, Window dest_w,
                           int src_x, int src_y, int *dest_x_return,
                           int *dest_y_return, Window *child_return);
/* REAL, NEW 2026-09-26 - the last two gaps in khtpm_draw_core.c's image
 * path. XPutPixel writes individual pixels while kh_draw_canvas() blits a
 * canvas box into an XImage (draw_core.c:668); the shim had only the
 * matching XGetPixel, so this was an implicit declaration and GCC helpfully
 * suggested the read-only one. XSetFillStyle is the setter form of the
 * GCFillStyle mask: draw_core.c:828/840 flips the checkerboard GC between
 * FillSolid and FillTiled around the XFillRectangle that stamps it. */
int  XPutPixel(XImage *img, int x, int y, unsigned long pixel);
void XSetFillStyle(Display *dpy, GC gc, int fill_style);

/* --- REAL, NEW 2026-09-26: the Xlib surface khtpm_entity.c needs ----------
 * The pal/tile entity is a much bigger Xlib consumer than the strip parser
 * was - it draws its own per-pixel-alpha sprite, has popups, and drags
 * itself between desktop windows. These are the five entry points it calls
 * that the shim did not declare, grouped by what each is for:
 *
 *  - drag-into-another-pal: XReparentWindow (the pal becomes a child of the
 *    window it is dropped on, and goes back to root when it is dropped on
 *    nothing - kh_drag_stack_above(), entity.c:214/223)
 *  - ARGB window selection: XMatchVisualInfo (ask the server for a 32-bit
 *    TrueColor visual, with the same graceful fallback to DefaultVisual the
 *    entity already writes - entity.c:1644 and :4002)
 *  - sprite silhouette:   XCreateBitmapFromData (1-bit-per-pixel mask ->
 *    Pixmap; the entity then XCopyArea's it into the window's shape mask)
 *  - cursword's round     XDrawArc (a 360-degree outline, stroked not
 *    placement reticle:    filled, onto the same shape mask)
 *  - drag tracking:       XQueryPointer (root-relative pointer position
 *    while a drag is live: while the X loop is sleeping on select())
 */
void XReparentWindow(Display *dpy, Window w, Window parent, int x, int y);
Status XMatchVisualInfo(Display *dpy, int screen, int depth, int class,
                        XVisualInfo *vinfo_return);
Pixmap XCreateBitmapFromData(Display *dpy, Drawable d, const char *data,
                             unsigned width, unsigned height);
void XDrawArc(Display *dpy, Drawable d, GC gc, int x, int y,
              unsigned width, unsigned height, int angle1, int angle2);
Bool XQueryPointer(Display *dpy, Window w, Window *root_return,
                   Window *child_return, int *root_x_return, int *root_y_return,
                   int *win_x_return, int *win_y_return,
                   unsigned int *mask_return);

#ifdef __cplusplus
}
#endif
#endif
