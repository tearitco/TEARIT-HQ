/* Win32 implementation of the Xlib/Xft subset used by khtpm_strip_parser.c */
#include "khtpm_strip_x11_win.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* REAL, NEW 2026-09-26 - for the cos()/sin() that place X11's arc end
 * angles onto the ellipse in XDrawArc. The -lm that goes with it is on both
 * the renderer and entity link lines in build_khtpm_strip_win.ps1. */
#include <math.h>
  #include <tlhelp32.h>
  
  /* TEMPORARY DIAGNOSTIC 2026-09-27: trace the detached-launch crash. */
  static void x11_trace(const char *fmt, ...) {
      char p[MAX_PATH];
      const char *t = getenv("TEMP");
      if (!t) t = "C:\\Temp";
      snprintf(p, sizeof(p), "%s\\khtpm_win_trace.log", t);
      FILE *f = fopen(p, "a");
      if (!f) return;
      /* strip the recursive prefix off %TEMP%-relative paths */
      va_list ap;
      va_start(ap, fmt);
      char buf[1024];
      vsnprintf(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      fprintf(f, "[pid=%lu] %s\n", (unsigned long)GetCurrentProcessId(), buf);
      fclose(f);
  }
  static LONG WINAPI x11_trace_seh(PEXCEPTION_POINTERS ep) {
      x11_trace("SEH code=0x%08lx addr=%p", ep ? (unsigned long)ep->ExceptionRecord->ExceptionCode : 0,
                ep ? ep->ExceptionRecord->ExceptionAddress : NULL);
      return EXCEPTION_CONTINUE_SEARCH;
  }
  
  #define KIND_WIN 1
#define KIND_PIX 2
#define EQMAX 256
#define XWMAX 64

/* REAL, NEW 2026-09-26 - a real property store.
 *
 * XChangeProperty existed but only ever handled _NET_WM_WINDOW_OPACITY and
 * discarded everything else, so XGetWindowProperty had nothing to read back.
 * khtpm_core_render.c's XDND drop path is built on exactly that round trip:
 * XConvertSelection to a target, then XGetWindowProperty(.., AnyPropertyType,
 * ..) to read what the source wrote onto its own window. With no store the
 * read always fails and every drop is silently discarded.
 *
 * Window properties are a tiny fixed set on this renderer (the ICCCM/WM hints
 * it sets on itself, plus one XDND payload per in-flight drag), so a linked
 * list keyed by atom is the right shape - no hashing needed. */
typedef struct KProp {
    Atom            atom;
    int             format;      /* 8/16/32, as X11 stores it */
    unsigned long   nitems;
    unsigned char  *data;
    struct KProp   *next;
} KProp;

struct Xd {
    int kind;
    HWND hwnd;
    HBITMAP hbmp;
    HDC hdc;
    void *bits;
    int w, h, x, y;
    int content_w, content_h; /* last presented pixmap size (for mouse scale) */
    unsigned long bg;
    BYTE opacity;
    KProp *props;             /* NEW 2026-09-26 - see KProp above */
    /* REAL, NEW 2026-09-26 - current X11 parent, NULL for a top-level
     * window. Only XReparentWindow() ever sets it, and only the entity's
     * drag-into-another-pal path ever calls that; every taskbar window keeps
     * it NULL, so their existing work-area-relative geometry below is
     * untouched. A reparented window is a WS_CHILD, so its x/y are relative
     * to this parent rather than to the work area. */
    Window parent;
};

struct Display {
    int sw, sh;
    Visual vis;
    XEvent q[EQMAX];
    int qh, qt;
    Atom next_atom;
    Atom opacity_atom;
    HFONT font;
    /* REAL, NEW 2026-09-26 - registry of the live shim windows. Two jobs.
     * XQueryTree has to hand back a real child list, and xd_valid() below
     * lets every window-taking entry point reject the bogus Window values
     * the parser builds by casting a raw integer - (Window)xid, read out
     * of the nav_tab "ord xid" registry. A real X11 XID is an integer, but
     * here a Window is an Xd*, so an unchecked cast gets dereferenced and
     * kills the taskbar the moment @ is pressed. */
    Xd *wins[XWMAX];
    int nwins;
    /* REAL, NEW 2026-09-26 - selection ownership. XSetSelectionOwner /
     * XGetSelectionOwner / XConvertSelection complete the clipboard trio the
     * dock's copy/paste and its XDND drag-source both sit on. The shim has
     * exactly one selection, which is all a single process needs; a real X
     * server would arbitrate this across clients. */
    Atom   sel_atom;
    Window sel_owner;
};

static Display *g_dpy = NULL;
static const wchar_t *kCls = L"KhtpmStripX11";

static COLORREF pix_to_cr(unsigned long p) {
    return RGB((p >> 16) & 255, (p >> 8) & 255, p & 255);
}

static unsigned long cr_to_pix(COLORREF c) {
    return ((unsigned long)GetRValue(c) << 16) |
           ((unsigned long)GetGValue(c) << 8) |
           (unsigned long)GetBValue(c);
}

static void qpush(Display *d, const XEvent *ev) {
    int n = (d->qh + 1) % EQMAX;
    if (n == d->qt) return;
    d->q[d->qh] = *ev;
    d->qh = n;
}

static Xd *hwnd_xd(HWND h) {
    return (Xd *)GetWindowLongPtrW(h, GWLP_USERDATA);
}

/* REAL, NEW 2026-09-26 - the guard every Window-taking entry point now
 * runs. On X11 a Window is a small integer XID, so passing a stale or
 * foreign one is merely a protocol error the installed error handler
 * swallows. Here a Window is an Xd*, so the same value is a wild pointer:
 * ktb_toggle_zorder_apply() reads "ord xid" out of nav_tab and casts it
 * straight to Window, and nav_tab is written by other processes, so that
 * value is routinely not one of ours. Membership in the registry is the
 * only trustworthy test - an IsWindow() probe would still read the garbage
 * pointer to get an HWND. */
static int xd_valid(Window w) {
    int i;
    if (!w) return 0;
    for (i = 0; i < g_dpy->nwins; i++)
        if (g_dpy->wins[i] == w) return 1;
    return 0;
}

static void xd_register(Xd *xd) {
    if (!xd || !g_dpy) return;
    if (g_dpy->nwins >= XWMAX) return;
    g_dpy->wins[g_dpy->nwins++] = xd;
}

static void xd_unregister(Xd *xd) {
    int i, j;
    if (!xd || !g_dpy) return;
    for (i = 0; i < g_dpy->nwins; i++) {
        if (g_dpy->wins[i] != xd) continue;
        for (j = i; j + 1 < g_dpy->nwins; j++) g_dpy->wins[j] = g_dpy->wins[j + 1];
        g_dpy->nwins--;
        return;
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Xd *xd = hwnd_xd(h);
    Display *d = g_dpy;
    if (!d) return DefWindowProcW(h, m, w, l);
    if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_LBUTTONUP || m == WM_MOUSEMOVE) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        int mx = (int)(short)LOWORD(l);
        int my = (int)(short)HIWORD(l);
        if (xd) {
            RECT rc;
            GetClientRect(h, &rc);
            if (xd->content_w > 0 && rc.right > 0 && xd->content_w > rc.right)
                mx = mx * xd->content_w / rc.right;
            if (xd->content_h > 0 && rc.bottom > 0 && xd->content_h > rc.bottom)
                my = my * xd->content_h / rc.bottom;
        }
        POINT scr = { (int)(short)LOWORD(l), (int)(short)HIWORD(l) };
        ClientToScreen(h, &scr);
        if (m == WM_MOUSEMOVE) {
            if (!(w & MK_LBUTTON)) return 0;
            ev.type = MotionNotify;
            ev.xany.window = xd;
            ev.xmotion.window = xd;
            ev.xmotion.x = mx; ev.xmotion.y = my;
            ev.xmotion.x_root = scr.x; ev.xmotion.y_root = scr.y;
        } else {
            ev.type = (m == WM_LBUTTONUP) ? ButtonRelease : ButtonPress;
            ev.xany.window = xd;
            ev.xbutton.window = xd;
            ev.xbutton.button = (m == WM_RBUTTONDOWN) ? 3 : 1;
            ev.xbutton.x = mx; ev.xbutton.y = my;
            ev.xbutton.x_root = scr.x; ev.xbutton.y_root = scr.y;
        }
        qpush(d, &ev);
        if (m == WM_LBUTTONDOWN) SetCapture(h);
        if (m == WM_LBUTTONUP) ReleaseCapture();
        return 0;
    }
    if (m == WM_KEYDOWN) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KeyPress;
        ev.xkey.window = xd;
        ev.xkey.keycode = (unsigned)w;
        qpush(d, &ev);
        return 0;
    }
    if (m == WM_CHAR) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KeyPress;
        ev.xkey.window = xd;
        ev.xkey.keycode = 0x10000u | (unsigned)(w & 0xff);
        qpush(d, &ev);
        return 0;
    }
    if (m == WM_SETFOCUS) {
        XEvent ev; memset(&ev, 0, sizeof(ev));
            /* REAL, NEW 2026-09-26 - wrote xbutton.window for a FocusIn.
             * The parser reads ev.xfocus.window, so g_focused_win came back
             * 0 and every later focus comparison failed. */
            ev.type = FocusIn; ev.xfocus.window = xd; ev.xfocus.mode = 0;
        qpush(d, &ev);
        return 0;
    }
    if (m == WM_KILLFOCUS) {
        XEvent ev; memset(&ev, 0, sizeof(ev));
            /* REAL, NEW 2026-09-26 - same wrong-member bug as FocusIn above:
             * xbutton instead of xfocus, so the FocusOut half could never
             * clear g_focused_win. */
            ev.type = FocusOut; ev.xfocus.window = xd; ev.xfocus.mode = 0;
        qpush(d, &ev);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        static int first_paint = 0;
        if (first_paint < 3) { first_paint++; x11_trace("WM_PAINT hwnd=%p", h); }
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        /* Linux MapWindow delivers Expose; Win WM_PAINT must too or
         * entity tiles never present (need_redraw stays 0). */
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
            ev.type = Expose;
            /* REAL, NEW 2026-09-26 - wrote xany.window for an Expose. The
             * parser's redraw keys off ev.xexpose.window and treats
             * ev.xexpose.count==0 as "last Expose in the batch, safe to
             * draw now"; with xany it compared against NULL every time and
             * drew nothing. This is the blank-bars bug. */
            ev.xany.window = xd;
            ev.xexpose.window = xd;
            ev.xexpose.count = 0;
            qpush(d, &ev);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void pump(Display *d) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    (void)d;
}

static void work_area(RECT *wa) {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, wa, 0);
}

/* Keep every strip window on the primary work area. Linux sizes from a
 * different display; DPI-unaware CreateWindow also inflates past the edge. */
#define STRIP_GUTTER 24

/* Linux PDL: strip_y_offset ~50 (GNOME top panel), popup at 50+36.
 * Windows work area is already below/above OS chrome — snap flush. */
static void unbias_linux_desktop_pad(int *x, int *y) {
    (void)x;
    /* Parser already sets strip y=0 on Win. Mapping 32–64→0 also yanked
     * HQ popups (y = bar height ~36) onto the top of the screen. */
    if (*y >= 48 && *y <= 56)
        *y = 0; /* leftover GNOME strip_y_offset=50 only */
    else if (*y >= 120 && *y <= 160)
        *y = 40; /* cli_io legacy y=140 */
}

static void clamp_to_work_area(int *x, int *y, unsigned *w, unsigned *h) {
    RECT wa;
    work_area(&wa);
    int aw = wa.right - wa.left;
    int ah = wa.bottom - wa.top;
    int g = STRIP_GUTTER;
    if (aw < 64) aw = 64;
    if (ah < 64) ah = 64;
    unbias_linux_desktop_pad(x, y);
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    /* If Linux offset + content is wider than this screen, slide left
     * first (recover the GNOME-era x pad) then shrink. */
    int right = *x + (int)*w;
    int limit = aw - g;
    if (right > limit) {
        int overflow = right - limit;
        if (*x > g) {
            int slide = *x - g;
            if (slide > overflow) slide = overflow;
            *x -= slide;
            overflow -= slide;
        }
        if (overflow > 0 && (int)*w > overflow)
            *w -= (unsigned)overflow;
    }
    /* Vertical: flush to work area (top of screen / just above Win taskbar). */
    int bottom = *y + (int)*h;
    if (bottom > ah) {
        int overflow = bottom - ah;
        if (*y > 0) {
            int slide = *y;
            if (slide > overflow) slide = overflow;
            *y -= slide;
            overflow -= slide;
        }
        if (overflow > 0 && (int)*h > overflow)
            *h -= (unsigned)overflow;
    }
    if (*x + (int)*w > aw - g) {
        int maxw = aw - g - *x;
        if (maxw < 32) { *x = g; maxw = aw - 2 * g; }
        if (maxw < 32) maxw = aw;
        *w = (unsigned)maxw;
    }
    if (*y + (int)*h > ah) {
        int maxh = ah - *y;
        if (maxh < 16) { *y = 0; maxh = ah; }
        *h = (unsigned)(maxh < 16 ? ah : maxh);
    }
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}

static HDC dc_of(Drawable dr) {
    if (!dr) return NULL;
    if (dr->kind == KIND_PIX) return dr->hdc;
    if (dr->kind == KIND_WIN && dr->hwnd) return GetDC(dr->hwnd);
    return NULL;
}

static void dc_done(Drawable dr, HDC hdc) {
    if (dr && dr->kind == KIND_WIN && hdc) ReleaseDC(dr->hwnd, hdc);
}

Display *XOpenDisplay(const char *name) {
    (void)name;
    SetUnhandledExceptionFilter(x11_trace_seh);
    x11_trace("XOpenDisplay()");
    /* Match physical pixels to CreateWindow, or Windows will scale a
     * "fits the work area" header off the right edge. */
    {
        HMODULE u = GetModuleHandleW(L"user32.dll");
        if (u) {
            typedef BOOL (WINAPI *SetDpiAwareFn)(void);
            SetDpiAwareFn fn = (SetDpiAwareFn)GetProcAddress(u, "SetProcessDPIAware");
            if (fn) fn();
        }
    }
    Display *d = (Display *)calloc(1, sizeof(Display));
    if (!d) return NULL;
    RECT wa;
    work_area(&wa);
    d->sw = wa.right - wa.left - STRIP_GUTTER;
    d->sh = wa.bottom - wa.top; /* full work-area height: bottom bar sits on Win taskbar */
    if (d->sw < 64) d->sw = wa.right - wa.left;
    if (d->sh < 64) d->sh = wa.bottom - wa.top;
    d->vis.red_mask = 0xFF0000;
    d->vis.green_mask = 0x00FF00;
    d->vis.blue_mask = 0x0000FF;
    d->next_atom = 1;
    d->font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                          0, 0, CLEARTYPE_QUALITY, FF_DONTCARE, L"Segoe UI");
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kCls;
    RegisterClassW(&wc);
    g_dpy = d;
    return d;
}

void XCloseDisplay(Display *dpy) {
    if (!dpy) return;
    if (dpy->font) DeleteObject(dpy->font);
    if (g_dpy == dpy) g_dpy = NULL;
    free(dpy);
}

int DefaultScreen(Display *dpy) { (void)dpy; return 0; }
int DisplayWidth(Display *dpy, int scr) { (void)scr; return dpy ? dpy->sw : 800; }
int DisplayHeight(Display *dpy, int scr) { (void)scr; return dpy ? dpy->sh : 600; }
unsigned long BlackPixel(Display *dpy, int scr) { (void)dpy; (void)scr; return 0; }
unsigned long WhitePixel(Display *dpy, int scr) { (void)dpy; (void)scr; return 0xFFFFFFul; }
Colormap DefaultColormap(Display *dpy, int scr) { (void)dpy; (void)scr; return 1; }
Visual *DefaultVisual(Display *dpy, int scr) { (void)scr; return dpy ? &dpy->vis : NULL; }
int DefaultDepth(Display *dpy, int scr) { (void)dpy; (void)scr; return 32; }
int ConnectionNumber(Display *dpy) { (void)dpy; return 1; }

static Window RootDummy(void) { return NULL; }

Window XCreateWindow(Display *dpy, Window parent, int x, int y,
                     unsigned w, unsigned h, unsigned border, int depth, unsigned cls,
                     Visual *vis, unsigned long valuemask, XSetWindowAttributes *swa) {
    (void)parent; (void)border; (void)depth; (void)cls; (void)vis; (void)valuemask;
    Xd *xd = (Xd *)calloc(1, sizeof(Xd));
    if (!xd) return NULL;
    clamp_to_work_area(&x, &y, &w, &h);
    xd->kind = KIND_WIN;
    xd->w = (int)w; xd->h = (int)h; xd->x = x; xd->y = y;
    xd->opacity = 255;
    if (swa) xd->bg = swa->background_pixel;
    RECT wa;
    work_area(&wa);
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kCls, L"khtpm",
        WS_POPUP,
        wa.left + x, wa.top + y, (int)w, (int)h,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
        x11_trace("XCreateWindow(%ux%u@%d,%d) hwnd=%p", w, h, x, y, hwnd);
        xd->hwnd = hwnd;
        if (hwnd)
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)xd);
        xd_register(xd);   /* REAL, NEW 2026-09-26 - join the registry so
                             * xd_valid() accepts this window from here on. */
        (void)dpy;
        return xd;
    }

    void XDestroyWindow(Display *dpy, Window w) {
        (void)dpy;
        /* REAL, NEW 2026-09-26 - xd_valid() before touching the pointer:
         * same wild-pointer guard as everywhere else. */
        if (!xd_valid(w)) return;
        xd_unregister(w);
        if (w->hwnd) DestroyWindow(w->hwnd);
        free(w);
    }

void XMapRaised(Display *dpy, Window w) {
    if (!xd_valid(w) || !w->hwnd) return;
    x11_trace("XMapRaised hwnd=%p (owner pid=%lu)", w->hwnd, (unsigned long)GetCurrentProcessId());
    ShowWindow(w->hwnd, SW_SHOW);
    SetWindowPos(w->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    InvalidateRect(w->hwnd, NULL, FALSE);
    if (dpy) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
            ev.type = Expose;
            /* REAL, NEW 2026-09-26 - same wrong-member fix as the WM_PAINT
             * Expose above: xexpose.window/count, not xany.window. */
            ev.xany.window = w;
            ev.xexpose.window = w;
            ev.xexpose.count = 0;
            qpush(dpy, &ev);
    }
}

void XUnmapWindow(Display *dpy, Window w) {
    (void)dpy;
    if (xd_valid(w) && w->hwnd) ShowWindow(w->hwnd, SW_HIDE);
}

    void XRaiseWindow(Display *dpy, Window w) {
        (void)dpy;
        if (xd_valid(w) && w->hwnd)
            SetWindowPos(w->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    /* REAL, NEW 2026-09-26 - the five functions below closed the last X11
     * gap in this shim (see khtpm_strip_x11_win.h). All of them are reached
     * only from the @ zorder toggle and main()'s error-handler install. */

    /* X11 stacks within the same layer; these bars are all WS_EX_TOPMOST, so
     * HWND_BOTTOM is the honest "send to the back of the topmost band"
     * equivalent of XLowerWindow. */
    void XLowerWindow(Display *dpy, Window w) {
        (void)dpy;
        if (xd_valid(w) && w->hwnd)
            SetWindowPos(w->hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    /* X11's root has no real parent and its "children" are every top-level
     * window. Here the honest answer is this process's own shim windows -
     * the registry - because a Win32 top-level window owned by some other
     * process (the live tile pieces) is not ours to restack, and pretending
     * otherwise would mean SetWindowPos on a handle we do not own. The
     * cross-process half of the toggle is ktb_toggle_zorder_apply's nav_tab
     * registry walk, which is a different code path entirely.
     * Caller frees *children with XFree, which is free(). */
    int XQueryTree(Display *dpy, Window w, Window *root_ret, Window *parent_ret,
                   Window **children, unsigned int *nchildren) {
        unsigned int n, i;
        Window *arr;
        if (!dpy || !children || !nchildren) return 0;
        n = (unsigned int)dpy->nwins;
        if (n == 0) { *children = NULL; *nchildren = 0; return 0; }
        arr = (Window *)calloc(n, sizeof(Window));
        if (!arr) { *children = NULL; *nchildren = 0; return 0; }
        for (i = 0; i < n; i++) arr[i] = dpy->wins[i];
        *children = arr;
        *nchildren = n;
        if (root_ret) *root_ret = w;
        if (parent_ret) *parent_ret = NULL;
        return 1;
    }

    /* XFetchName hands back a malloc'd UTF-8 copy the caller XFree()s - the
     * real Xlib ownership rule, and ktb_zorder_apply_tree relies on it. */
    int XFetchName(Display *dpy, Window w, char **name_out) {
        wchar_t wn[256];
        char *out;
        int n;
        (void)dpy;
        if (!name_out) return 0;
        *name_out = NULL;
        if (!xd_valid(w) || !w->hwnd) return 0;
        if (GetWindowTextW(w->hwnd, wn, 256) == 0) return 0;
        n = WideCharToMultiByte(CP_UTF8, 0, wn, -1, NULL, 0, NULL, NULL);
        if (n <= 0) return 0;
        out = (char *)malloc((size_t)n);
        if (!out) return 0;
        WideCharToMultiByte(CP_UTF8, 0, wn, -1, out, n, NULL, NULL);
        *name_out = out;
        return 1;
    }

    /* REAL, NEW 2026-09-26 - recorded, never invoked: this shim speaks no X
     * protocol, so it never raises BadWindow/BadMatch and there is nothing
     * to deliver. main() installs the parser's non-fatal handler here and
     * that install is the whole point - it stays correct if a future
     * shimmed call ever does want to report one.
     *
     * The RETURN value is the previous handler, matching real Xlib, and
     * that is the part khtpm_entity.c depends on: its
     * kh_drag_stack_above() saves the current handler into an XErrorHandler,
     * installs its own ignore-everything handler across the XReparentWindow
     * call, then passes the saved value back to restore. Returning a
     * "had one already" int flag instead - which is what this used to do -
     * would have that restore install NULL and silently disarm the process's
     * error handling for the rest of its life. */
    static XErrorHandler g_xerror_handler = NULL;
    XErrorHandler XSetErrorHandler(XErrorHandler handler) {
        XErrorHandler prev = g_xerror_handler;
        g_xerror_handler = handler;
        return prev;
    }

    int XGetErrorText(Display *dpy, int code, char *buf, int len) {
        const char *m = "unknown error (win32 shim raises no X protocol errors)";
        (void)dpy; (void)code;
        if (!buf || len <= 0) return 0;
        lstrcpynA(buf, m, (int)strlen(m) + 1 > len ? len : (int)strlen(m) + 1);
        return (int)strlen(buf);
    }

void XMoveResizeWindow(Display *dpy, Window w, int x, int y, unsigned width, unsigned height) {
    (void)dpy;
    if (!xd_valid(w) || !w->hwnd) return;
    w->x = x; w->y = y; w->w = (int)width; w->h = (int)height;
    if (w->parent) {
        /* A reparented window is a WS_CHILD (XReparentWindow), and for a child
         * all three of the things the top-level path below does are wrong:
         * x/y are relative to that parent rather than to the work area, so
         * adding wa.left/wa.top would double the offset; there is no work area
         * to clamp a child against, because clipping a child is the parent's
         * job, not the window's; and HWND_TOPMOST is meaningless for a child
         * (SetWindowPos silently ignores the z-order for WS_CHILD anyway).
         * khtpm_entity.c drags a reparented tile around with XMoveWindow, so
         * without this leg the tile would be flung off the parent's origin
         * every time the pointer moved. */
        SetWindowPos(w->hwnd, NULL, x, y, (int)width, (int)height,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        return;
    }
    clamp_to_work_area(&x, &y, &width, &height);
    RECT wa;
    work_area(&wa);
    SetWindowPos(w->hwnd, HWND_TOPMOST, wa.left + x, wa.top + y,
                 (int)width, (int)height, SWP_NOACTIVATE);
}

    void XSetWindowBackground(Display *dpy, Window w, unsigned long pixel) {
        (void)dpy;
        if (xd_valid(w)) w->bg = pixel;   /* REAL, NEW 2026-09-26 - guarded */
    }

void XSetInputFocus(Display *dpy, Window w, int revert, unsigned long time) {
    (void)dpy; (void)revert; (void)time;
    if (xd_valid(w) && w->hwnd) SetFocus(w->hwnd);
}

void XFlush(Display *dpy) { pump(dpy); GdiFlush(); }
void XSync(Display *dpy, int discard) { (void)discard; pump(dpy); GdiFlush(); }

int XPending(Display *dpy) {
    pump(dpy);
    if (!dpy) return 0;
    if (dpy->qh >= dpy->qt) return dpy->qh - dpy->qt;
    return EQMAX - (dpy->qt - dpy->qh);
}

int XNextEvent(Display *dpy, XEvent *ev) {
    pump(dpy);
    if (!dpy || dpy->qt == dpy->qh) { memset(ev, 0, sizeof(*ev)); return 0; }
    *ev = dpy->q[dpy->qt];
    dpy->qt = (dpy->qt + 1) % EQMAX;
    return 0;
}

void x11_wait(Display *dpy, int usec) {
    int ms = usec / 1000;
    if (ms < 1) ms = 1;
    MsgWaitForMultipleObjects(0, NULL, FALSE, (DWORD)ms, QS_ALLINPUT);
    pump(dpy);
}

/* NEW 2026-09-26 - shared mask application for XCreateGC/XChangeGC. The
 * shim's invented GCForeground=1/GCBackground=2/GCFont=4 are switched on by
 * value (see the header for why they are not the real Xlib numbers); the new
 * GCFillStyle/GCGraphicsExposures/GCTile* masks carry the real Xlib bit
 * positions and are distinct from them, so both sets can be tested together. */
static void gc_apply(GC gc, unsigned long mask, XGCValues *v) {
    if (!gc || !v) return;
    if (mask & GCForeground)         gc->foreground = v->foreground;
    if (mask & GCBackground)         gc->background = v->background;
    if (mask & GCFillStyle)          gc->fill_style = v->fill_style;
    if (mask & GCTile)               gc->tile = v->tile;
    if (mask & GCTileStipXOrigin)    gc->ts_x_origin = v->ts_x_origin;
    if (mask & GCTileStipYOrigin)    gc->ts_y_origin = v->ts_y_origin;
    /* GCFont, GCGraphicsExposures, GCFunction, GCStipple and the rest are
     * accepted and ignored: this renderer sets solid fills and has no font
     * or exposure dependency, so honouring them would change nothing on
     * screen. */
    (void)gc;
}

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *v) {
    (void)dpy; (void)d;
    GC gc = (GC)calloc(1, sizeof(*gc));
    if (!gc) return NULL;
    gc->foreground = 0xFFFFFFul;
    gc->background = 0;
    gc->fill_style = FillSolid;
    if (v) gc_apply(gc, mask, v);
    return gc;
}

void XChangeGC(Display *dpy, GC gc, unsigned long mask, XGCValues *v) {
    (void)dpy;
    gc_apply(gc, mask, v);
}

void XSetFillStyle(Display *dpy, GC gc, int fill_style) {
    (void)dpy;
    if (gc) gc->fill_style = (unsigned long)fill_style;
}

/* NEW 2026-09-26 - the dash/width half of GC line state. khtpm_core_render.c
 * uses dashes for the focused-cell ring and the XDND drop-target outline. The
 * Win32 shim draws with CreatePen, which has no dash pattern, so the width is
 * honoured and the pattern is recorded but not rendered - the outline reads
 * as a solid ring. Storing the values keeps XSetLineAttributes a real
 * function rather than a lie that discards its arguments. */
static unsigned long g_line_width = 1;
static int g_line_style = LineSolid;
static int g_cap_style = CapButt;
static int g_join_style = JoinMiter;

void XSetLineAttributes(Display *dpy, GC gc, unsigned int width,
                        int line_style, int cap_style, int join_style) {
    (void)dpy; (void)gc;
    g_line_width  = width ? width : 1;
    g_line_style  = line_style;
    g_cap_style   = cap_style;
    g_join_style  = join_style;
}

void XFreeGC(Display *dpy, GC gc) { (void)dpy; free(gc); }
void XSetForeground(Display *dpy, GC gc, unsigned long pixel) { (void)dpy; if (gc) gc->foreground = pixel; }
void XSetBackground(Display *dpy, GC gc, unsigned long pixel) { (void)dpy; if (gc) gc->background = pixel; }
void XCopyGC(Display *dpy, GC src, unsigned long mask, GC dst) {
    (void)dpy; (void)mask;
    if (src && dst) { dst->foreground = src->foreground; dst->background = src->background; }
}
int XGetGCValues(Display *dpy, GC gc, unsigned long mask, XGCValues *v) {
    (void)dpy; (void)mask;
    if (!gc || !v) return 0;
    v->foreground = gc->foreground;
    v->background = gc->background;
    return 1;
}

static HPEN pen_fg(GC gc) {
    return CreatePen(PS_SOLID, 1, pix_to_cr(gc ? gc->foreground : 0xFFFFFFul));
}
static HBRUSH brush_fg(GC gc) {
    return CreateSolidBrush(pix_to_cr(gc ? gc->foreground : 0xFFFFFFul));
}

void XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y, unsigned w, unsigned h) {
    (void)dpy;
    if (d && d->kind == KIND_PIX && d->bits) {
        unsigned long p = gc ? gc->foreground : 0;
        unsigned char b = (unsigned char)(p & 255), g = (unsigned char)((p >> 8) & 255),
                         r = (unsigned char)((p >> 16) & 255);
        int x1 = x + (int)w, y1 = y + (int)h;
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x1 > d->w) x1 = d->w; if (y1 > d->h) y1 = d->h;
        unsigned char *bits = (unsigned char *)d->bits;
        /* NEW 2026-09-26 - FillTiled support. khtpm_draw_core.c stamps a 12x12
         * checkerboard through a tiled GC (draw_core.c:820-840); without this
         * branch the whole cell came out as one solid block in the current
         * foreground colour. The tile is itself a shim Pixmap, so its pixels
         * are already in the same CPU-accessible BGRX layout as the
         * destination and the stamp is a straight modulo read. */
        Pixmap tp = (gc && gc->fill_style == FillTiled) ? gc->tile : NULL;
        if (tp && tp->kind == KIND_PIX && tp->bits && tp->w > 0 && tp->h > 0) {
            const unsigned char *tb = (const unsigned char *)tp->bits;
            long ox = gc->ts_x_origin, oy = gc->ts_y_origin;
            for (int yy = y; yy < y1; yy++) {
                int ty = (int)(((yy - oy) % tp->h + tp->h) % tp->h);
                for (int xx = x; xx < x1; xx++) {
                    int tx = (int)(((xx - ox) % tp->w + tp->w) % tp->w);
                    int si = (ty * tp->w + tx) * 4;
                    int di = (yy * d->w + xx) * 4;
                    bits[di + 0] = tb[si + 0];
                    bits[di + 1] = tb[si + 1];
                    bits[di + 2] = tb[si + 2];
                    bits[di + 3] = 255;
                }
            }
            return;
        }
        for (int yy = y; yy < y1; yy++) {
            for (int xx = x; xx < x1; xx++) {
                int i = (yy * d->w + xx) * 4;
                bits[i + 0] = b; bits[i + 1] = g; bits[i + 2] = r; bits[i + 3] = 255;
            }
        }
        return;
    }
    HDC hdc = dc_of(d);
    if (!hdc) return;
    RECT rc = { x, y, x + (int)w, y + (int)h };
    HBRUSH br = brush_fg(gc);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    dc_done(d, hdc);
}

void XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2) {
    (void)dpy;
    HDC hdc = dc_of(d);
    if (!hdc) return;
    HPEN pen = pen_fg(gc);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, old);
    DeleteObject(pen);
    dc_done(d, hdc);
}

void XDrawRectangle(Display *dpy, Drawable d, GC gc, int x, int y, unsigned w, unsigned h) {
    (void)dpy;
    HDC hdc = dc_of(d);
    if (!hdc) return;
    HPEN pen = pen_fg(gc);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    Rectangle(hdc, x, y, x + (int)w, y + (int)h);
    SelectObject(hdc, old);
    SelectObject(hdc, oldb);
    DeleteObject(pen);
    dc_done(d, hdc);
}

int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y, const char *s, int len) {
    XftDraw dr; dr.d = d;
    XftColor col; col.pixel = gc ? gc->foreground : 0xFFFFFFul;
    XftFont font; font.hf = dpy && dpy->font ? dpy->font : (HFONT)GetStockObject(DEFAULT_GUI_FONT); font.px = 13;
    XftDrawStringUtf8(&dr, &col, &font, x, y, (const FcChar8 *)s, len);
    return 0;
}

static int make_dib(Xd *xd, int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (xd->hdc) { DeleteDC(xd->hdc); xd->hdc = NULL; }
    if (xd->hbmp) { DeleteObject(xd->hbmp); xd->hbmp = NULL; }
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(NULL);
    xd->hdc = CreateCompatibleDC(screen);
    xd->hbmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &xd->bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!xd->hdc || !xd->hbmp) return 0;
    SelectObject(xd->hdc, xd->hbmp);
    xd->w = w; xd->h = h;
    if (xd->bits) memset(xd->bits, 0, (size_t)w * (size_t)h * 4);
    return 1;
}

Pixmap XCreatePixmap(Display *dpy, Drawable d, unsigned w, unsigned h, unsigned depth) {
    (void)dpy; (void)d; (void)depth;
    Xd *xd = (Xd *)calloc(1, sizeof(Xd));
    if (!xd) return NULL;
    xd->kind = KIND_PIX;
    if (!make_dib(xd, (int)w, (int)h)) { free(xd); return NULL; }
    return xd;
}

void XFreePixmap(Display *dpy, Pixmap p) {
    (void)dpy;
    if (!p) return;
    if (p->hdc) DeleteDC(p->hdc);
    if (p->hbmp) DeleteObject(p->hbmp);
    free(p);
}

void XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc,
               int sx, int sy, unsigned w, unsigned h, int dx, int dy) {
    (void)dpy; (void)gc;
    if (!src || !dst) return;
    HDC sdc = dc_of(src);
    HDC ddc = dc_of(dst);
    if (!sdc || !ddc) return;
    if (dst->kind == KIND_WIN && dst->hwnd) {
        RECT rc;
        GetClientRect(dst->hwnd, &rc);
        dst->content_w = (int)w;
        dst->content_h = (int)h;
        SetStretchBltMode(ddc, HALFTONE);
        StretchBlt(ddc, 0, 0, rc.right, rc.bottom, sdc, sx, sy, (int)w, (int)h, SRCCOPY);
    } else {
        BitBlt(ddc, dx, dy, (int)w, (int)h, sdc, sx, sy, SRCCOPY);
    }
    dc_done(src, sdc);
    dc_done(dst, ddc);
}

XImage *XCreateImage(Display *dpy, Visual *v, unsigned depth, int format, int offset,
                     char *data, unsigned w, unsigned h, int pad, int bpl) {
    (void)dpy; (void)v; (void)depth; (void)format; (void)offset;
    XImage *img = (XImage *)calloc(1, sizeof(XImage));
    if (!img) return NULL;
    img->width = (int)w; img->height = (int)h;
    img->data = data;
    img->byte_order = LSBFirst;
    img->bitmap_pad = pad;
    img->depth = 32;
    img->bits_per_pixel = 32;
    img->bytes_per_line = bpl ? bpl : (int)w * 4;
    return img;
}

XImage *XGetImage(Display *dpy, Drawable d, int x, int y, unsigned w, unsigned h,
                  unsigned long plane, int format) {
    (void)dpy; (void)plane; (void)format; (void)x; (void)y;
    if (!d || d->kind != KIND_PIX || !d->bits) return NULL;
    if ((int)w > d->w) w = (unsigned)d->w;
    if ((int)h > d->h) h = (unsigned)d->h;
    if (w == 0 || h == 0) return NULL;
    size_t n = (size_t)w * (size_t)h * 4;
    char *buf = (char *)malloc(n);
    if (!buf) return NULL;
    /* copy from DIB (already BGRA/BGRX top-down) */
    int dw = d->w;
    unsigned char *src = (unsigned char *)d->bits;
    unsigned char *dst = (unsigned char *)buf;
    unsigned iy, ix;
    for (iy = 0; iy < h; iy++) {
        for (ix = 0; ix < w; ix++) {
            int si = ((int)iy * dw + (int)ix) * 4;
            int di = ((int)iy * (int)w + (int)ix) * 4;
            dst[di + 0] = src[si + 0];
            dst[di + 1] = src[si + 1];
            dst[di + 2] = src[si + 2];
            dst[di + 3] = src[si + 3];
        }
    }
    return XCreateImage(dpy, NULL, 32, ZPixmap, 0, buf, w, h, 32, (int)w * 4);
}

void XPutImage(Display *dpy, Drawable d, GC gc, XImage *img,
               int sx, int sy, int dx, int dy, unsigned w, unsigned h) {
    static int first_put = 0;
    if (first_put < 120) { first_put++; x11_trace("XPutImage(%ux%u at %d,%d kind=%d) w=%p", w, h, dx, dy, d ? d->kind : -1, d); }
    (void)dpy; (void)gc; (void)sx; (void)sy;
    if (!d || !img || !img->data) return;
    if (d->kind == KIND_PIX && d->bits) {
        unsigned char *dst = (unsigned char *)d->bits;
        unsigned char *src = (unsigned char *)img->data;
        unsigned iy, ix;
        for (iy = 0; iy < h; iy++) {
            int yy = dy + (int)iy;
            if (yy < 0 || yy >= d->h) continue;
            for (ix = 0; ix < w; ix++) {
                int xx = dx + (int)ix;
                if (xx < 0 || xx >= d->w) continue;
                int si = ((int)iy * img->width + (int)ix) * 4;
                int di = (yy * d->w + xx) * 4;
                dst[di + 0] = src[si + 0];
                dst[di + 1] = src[si + 1];
                dst[di + 2] = src[si + 2];
                dst[di + 3] = src[si + 3];
            }
        }
        return;
    }
    if (d->kind == KIND_WIN && d->hwnd) {
        static int dump_done = 0;
        if (!dump_done && img && img->data && w >= 1000) {
            char dp[MAX_PATH];
            const char *dt = getenv("TEMP");
            if (!dt) dt = "C:\\Temp";
            snprintf(dp, sizeof(dp), "%s\\khtpm_present_frame.raw", dt);
            FILE *df = fopen(dp, "wb");
            if (df) {
                fwrite(img->data, 1, (size_t)img->height * (size_t)img->bytes_per_line, df);
                fclose(df);
                dump_done = 1;
                x11_trace("PRESENT-DUMP %ux%u bpl=%d ih=%d iw=%d", (unsigned)img->width, (unsigned)img->height, img->bytes_per_line, img->height, img->width);
            }
        }
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = img->width;
        bmi.bmiHeader.biHeight = -img->height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        HDC hdc = GetDC(d->hwnd);
        RECT rc;
        GetClientRect(d->hwnd, &rc);
        d->content_w = (int)w;
        d->content_h = (int)h;
        SetStretchBltMode(hdc, HALFTONE);
        int sdib = StretchDIBits(hdc, 0, 0, rc.right, rc.bottom,
                      0, 0, img->width, img->height,
                      img->data, &bmi, DIB_RGB_COLORS, SRCCOPY);
        {
            static int readback_n = 0;
            if (readback_n < 6) {
                readback_n++;
                int pxs[][2] = { {10,10},{100,15},{165,16},{555,16},{826,10},{18,18} };
                x11_trace("PRESENT-POST hwnd=%p rect=%dx%d img=%dx%d", d->hwnd, rc.right, rc.bottom, img->width, img->height);
                int i;
                for (i = 0; i < 6; i++) {
                    int qx = pxs[i][0], qy = pxs[i][1];
                    if (qx > rc.right - 1) qx = rc.right - 1;
                    if (qy > rc.bottom - 1) qy = rc.bottom - 1;
                    COLORREF after = GetPixel(hdc, qx, qy);
                    unsigned long sr=0, sg=0, sb=0, sa=0;
                    if (img->data && img->width > 0) {
                        size_t si = ((size_t)qy * (size_t)img->width + (size_t)qx) * 4;
                        if (si + 3 < (size_t)img->width * (size_t)img->height * 4) {
                            unsigned char *d8 = (unsigned char *)img->data;
                            sb = d8[si]; sg = d8[si+1]; sr = d8[si+2]; sa = d8[si+3];
                        }
                    }
                    x11_trace("PRESENT-POST(%d,%d) after=0x%08lX img=0x%02lX%02lX%02lX a=%02lX", qx, qy, (unsigned long)after, sr, sg, sb, sa);
                }
            }
        }
        ReleaseDC(d->hwnd, hdc);
    }
}

void XDestroyImage(XImage *img) {
    if (!img) return;
    free(img->data);
    free(img);
}

int XAllocNamedColor(Display *dpy, Colormap cmap, const char *name, XColor *sc, XColor *ec) {
    (void)dpy; (void)cmap;
    if (!name || !sc) return 0;
    unsigned r = 0, g = 0, b = 0;
    const char *n = name;
    if (n[0] == '#') n++;
    if (strlen(n) >= 6 && sscanf(n, "%02x%02x%02x", &r, &g, &b) == 3) {
        /* ok */
    } else if (_stricmp(name, "black") == 0) { r = g = b = 0; }
    else if (_stricmp(name, "white") == 0) { r = g = b = 255; }
    else return 0;
    sc->pixel = ((unsigned long)r << 16) | ((unsigned long)g << 8) | b;
    sc->red = (unsigned short)(r * 257);
    sc->green = (unsigned short)(g * 257);
    sc->blue = (unsigned short)(b * 257);
    if (ec) *ec = *sc;
    return 1;
}

int XQueryColor(Display *dpy, Colormap cmap, XColor *c) {
    (void)dpy; (void)cmap;
    if (!c) return 0;
    unsigned r = (c->pixel >> 16) & 255, g = (c->pixel >> 8) & 255, b = c->pixel & 255;
    c->red = (unsigned short)(r * 257);
    c->green = (unsigned short)(g * 257);
    c->blue = (unsigned short)(b * 257);
    return 1;
}

Atom XInternAtom(Display *dpy, const char *name, int only_if_exists) {
    (void)only_if_exists;
    if (!dpy) return 0;
    if (name && strcmp(name, "_NET_WM_WINDOW_OPACITY") == 0) {
        if (!dpy->opacity_atom) dpy->opacity_atom = dpy->next_atom++;
        return dpy->opacity_atom;
    }
    return dpy->next_atom++;
}

/* --- property store (NEW 2026-09-26) -------------------------------------
 * Bytes per item for each X11 property format. XGetWindowProperty returns a
 * count of 32-bit words regardless of format, so these matter for the
 * long_offset/long_length arithmetic. */
static int prop_item_bytes(int format) {
    switch (format) {
        case 8:  return 1;
        case 16: return 2;
        default: return 4;   /* 32, and anything unknown, as Xlib treats it */
    }
}

static KProp *prop_find(Xd *xd, Atom a) {
    if (!xd) return NULL;
    for (KProp *p = xd->props; p; p = p->next)
        if (p->atom == a) return p;
    return NULL;
}

/* mode: PropModeReplace 0, PropModePrepend 1, PropModeAppend 2 - the real
 * X11 values, and the only thing core_render varies is Replace. */
static int prop_store(Xd *xd, Atom prop, Atom type, int format, int mode,
                      const unsigned char *data, int nelements) {
    if (!xd || !data || format != 8 && format != 16 && format != 32) return 0;
    KProp *p = prop_find(xd, prop);
    if (!p) {
        p = (KProp *)calloc(1, sizeof(KProp));
        if (!p) return 0;
        p->atom = prop;
        p->next = xd->props;
        xd->props = p;
    }
    size_t n = (size_t)nelements * (size_t)prop_item_bytes(format);
    unsigned char *buf = (unsigned char *)malloc(n ? n : 1);
    if (!buf) return 0;
    memcpy(buf, data, n);
    free(p->data);
    if (mode == 0) {            /* Replace */
        p->data = buf;
        p->nitems = (unsigned long)nelements;
    } else if (mode == 1) {     /* Prepend */
        unsigned char *j = (unsigned char *)malloc((p->nitems + (unsigned long)nelements) * (size_t)prop_item_bytes(format));
        if (j) {
            memcpy(j, data, n);
            memcpy(j + n, p->data, (size_t)p->nitems * (size_t)prop_item_bytes(format));
            free(p->data); free(buf);
            p->data = j; p->nitems += (unsigned long)nelements;
        } else free(buf);
    } else {                    /* Append */
        unsigned long esz = (unsigned long)prop_item_bytes(format);
        unsigned char *j = (unsigned char *)realloc(p->data, (size_t)(p->nitems + (unsigned long)nelements) * (size_t)esz);
        if (j) {
            memcpy(j + (size_t)p->nitems * esz, data, n);
            p->data = j; p->nitems += (unsigned long)nelements;
        }
        free(buf);
    }
    (void)type;   /* single-typed properties only; the renderer never mixes */
    return 1;
}

static void props_free(Xd *xd) {
    if (!xd) return;
    KProp *p = xd->props;
    while (p) { KProp *n = p->next; free(p->data); free(p); p = n; }
    xd->props = NULL;
}

int XChangeProperty(Display *dpy, Window w, Atom prop, Atom type, int format,
                    int mode, const unsigned char *data, int nelements) {
    if (!xd_valid(w) || !w->hwnd || !dpy) return 0;
    prop_store(w, prop, type, format, mode, data, nelements);
    if (prop == dpy->opacity_atom && data) {
        unsigned long val = *(const unsigned long *)data;
        BYTE a = (BYTE)(val / (0xFFFFFFFFul / 255ul));
        w->opacity = a;
        SetLayeredWindowAttributes(w->hwnd, 0, a ? a : 1, LWA_ALPHA);
    }
    return 1;
}

/* NEW 2026-09-26 - was declared nowhere and called by nothing, but the
 * delete-after-read path in XGetWindowProperty needs a counterpart, and ICCCM
 * property replacement is cleaner with it. */
int XDeleteProperty(Display *dpy, Window w, Atom prop) {
    (void)dpy;
    if (!xd_valid(w)) return 0;
    KProp **pp = &w->props;
    while (*pp) {
        if ((*pp)->atom == prop) {
            KProp *dead = *pp;
            *pp = dead->next;
            free(dead->data); free(dead);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

XClassHint *XAllocClassHint(void) { return (XClassHint *)calloc(1, sizeof(XClassHint)); }
void XSetClassHint(Display *dpy, Window w, XClassHint *ch) { (void)dpy; (void)w; (void)ch; }
void XFree(void *p) { free(p); }

KeySym XLookupKeysym(XKeyEvent *ev, int idx) {
    (void)idx;
    if (!ev) return 0;
    unsigned kc = ev->keycode;
    if (kc & 0x10000) return 0;
    switch (kc) {
        case VK_LEFT: return XK_Left;
        case VK_RIGHT: return XK_Right;
        case VK_UP: return XK_Up;
        case VK_DOWN: return XK_Down;
        case VK_RETURN: return XK_Return;
        case VK_ESCAPE: return XK_Escape;
        case VK_BACK: return XK_BackSpace;
        case VK_TAB: return XK_Tab;
        default: return 0;
    }
}

int XLookupString(XKeyEvent *ev, char *buf, int n, KeySym *ks, void *compose) {
    (void)compose;
    if (!ev || !buf || n <= 0) return 0;
    buf[0] = 0;
    unsigned kc = ev->keycode;
    if (kc & 0x10000) {
        buf[0] = (char)(kc & 0xff);
        if (n > 1) buf[1] = 0;
        if (ks) *ks = (KeySym)(kc & 0xff);
        return 1;
    }
    KeySym mapped = XLookupKeysym(ev, 0);
    if (ks) *ks = mapped;
    if (mapped == XK_Return || mapped == XK_KP_Enter) { buf[0] = '\r'; if (n > 1) buf[1] = 0; return 1; }
    if (mapped == XK_Escape) { buf[0] = 27; if (n > 1) buf[1] = 0; return 1; }
    if (mapped == XK_BackSpace) { buf[0] = 8; if (n > 1) buf[1] = 0; return 1; }
    if (mapped == XK_Tab) { buf[0] = '\t'; if (n > 1) buf[1] = 0; return 1; }
    return mapped ? 1 : 0;
}

XftFont *XftFontOpenName(Display *dpy, int screen, const char *name) {
    (void)screen; (void)name;
    XftFont *f = (XftFont *)calloc(1, sizeof(XftFont));
    if (!f) return NULL;
    f->hf = dpy && dpy->font ? dpy->font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    f->px = 13;
    f->ascent = 11;
    f->descent = 3;
    return f;
}

XftDraw *XftDrawCreate(Display *dpy, Drawable d, Visual *v, Colormap cmap) {
    (void)dpy; (void)v; (void)cmap;
    XftDraw *dr = (XftDraw *)calloc(1, sizeof(XftDraw));
    if (!dr) return NULL;
    dr->d = d;
    return dr;
}

void XftDrawDestroy(XftDraw *dr) { free(dr); }

int XftColorAllocValue(Display *dpy, Visual *v, Colormap cmap, const XRenderColor *c, XftColor *out) {
    (void)dpy; (void)v; (void)cmap;
    if (!c || !out) return 0;
    unsigned r = c->red >> 8, g = c->green >> 8, b = c->blue >> 8;
    out->pixel = (r << 16) | (g << 8) | b;
    return 1;
}

void XftColorFree(Display *dpy, Visual *v, Colormap cmap, XftColor *c) {
    (void)dpy; (void)v; (void)cmap; (void)c;
}

void XftDrawStringUtf8(XftDraw *dr, const XftColor *col, XftFont *font,
                       int x, int y, const FcChar8 *s, int len) {
    static int first_txt = 0;
    if (first_txt < 120) {
        char tmp[32];
        int n = len < 31 ? len : 31;
        memcpy(tmp, s, n); tmp[n] = 0;
        first_txt++;
        x11_trace("XftDrawStringUtf8(len=%d at %d,%d str=<%s>)", len, x, y, tmp);
    }
    if (!dr || !dr->d || !s || len <= 0) return;
    HDC hdc = dc_of(dr->d);
    if (!hdc) return;
    wchar_t wbuf[1024];
    int n = MultiByteToWideChar(CP_UTF8, 0, (const char *)s, len, wbuf, 1023);
    if (n < 0) n = 0;
    wbuf[n] = 0;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, pix_to_cr(col ? col->pixel : 0xFFFFFFul));
    HFONT old = (HFONT)SelectObject(hdc, font && font->hf ? font->hf : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    /* X11 y is baseline; GDI TextOut y is top */
    TextOutW(hdc, x, y - tm.tmAscent, wbuf, n);
    SelectObject(hdc, old);
    dc_done(dr->d, hdc);
}

void XMapWindow(Display *dpy, Window w) { XMapRaised(dpy, w); }

    void XMoveWindow(Display *dpy, Window w, int x, int y) {
        /* REAL, NEW 2026-09-26 - xd_valid, not a bare `w ?`: XMoveWindow
         * forwards straight into XMoveResizeWindow, so an unregistered
         * Window would be dereferenced there. */
        unsigned ww = xd_valid(w) ? (unsigned)w->w : 1, hh = xd_valid(w) ? (unsigned)w->h : 1;
        XMoveResizeWindow(dpy, w, x, y, ww, hh);
    }

void XClearWindow(Display *dpy, Window w) {
    if (!xd_valid(w) || !w->hwnd) return;
    RECT rc; GetClientRect(w->hwnd, &rc);
    HDC hdc = GetDC(w->hwnd);
    HBRUSH br = CreateSolidBrush(pix_to_cr(w->bg));
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    ReleaseDC(w->hwnd, hdc);
    (void)dpy;
}

void XStoreName(Display *dpy, Window w, const char *name) {
    (void)dpy;
    if (!xd_valid(w) || !w->hwnd || !name) return;
    wchar_t wn[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 256);
    SetWindowTextW(w->hwnd, wn);
}

void XDrawPoint(Display *dpy, Drawable d, GC gc, int x, int y) {
    XFillRectangle(dpy, d, gc, x, y, 1, 1);
}

void XFillArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned w, unsigned h, int a1, int a2) {
    (void)a1; (void)a2;
    HDC hdc = dc_of(d);
    if (!hdc) return;
    HBRUSH br = CreateSolidBrush(pix_to_cr(gc ? gc->foreground : 0));
    HPEN pen = CreatePen(PS_SOLID, 1, pix_to_cr(gc ? gc->foreground : 0));
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, br);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    Ellipse(hdc, x, y, x + (int)w, y + (int)h);
    SelectObject(hdc, oldb); SelectObject(hdc, oldp);
    DeleteObject(br); DeleteObject(pen);
    dc_done(d, hdc);
    (void)dpy;
}

Colormap XCreateColormap(Display *dpy, Window w, Visual *v, int alloc) {
    (void)dpy; (void)w; (void)v; (void)alloc;
    return 1;
}

int XGetGeometry(Display *dpy, Drawable d, Window *root, int *x, int *y,
                 unsigned *w, unsigned *h, unsigned *bw, unsigned *depth) {
    (void)dpy;
    if (root) *root = NULL;
    if (bw) *bw = 0;
    if (depth) *depth = 32;
    if (!d) return 0;
    if (x) *x = d->x;
    if (y) *y = d->y;
    if (w) *w = (unsigned)d->w;
    if (h) *h = (unsigned)d->h;
    return 1;
}

int XCheckWindowEvent(Display *dpy, Window w, long mask, XEvent *ev) {
    (void)mask;
    if (!dpy || !ev) return 0;
    pump(dpy);
    int i = dpy->qt;
    while (i != dpy->qh) {
        XEvent *q = &dpy->q[i];
        Window ow = q->xany.window ? q->xany.window : q->xbutton.window;
        if (ow == w) {
            *ev = *q;
            /* compact queue */
            int j = i;
            while (j != dpy->qh) {
                int n = (j + 1) % EQMAX;
                if (n == dpy->qh) { dpy->qh = j; break; }
                dpy->q[j] = dpy->q[n];
                j = n;
            }
            return 1;
        }
        i = (i + 1) % EQMAX;
    }
    return 0;
}

int XGrabPointer(Display *dpy, Window w, int owner, unsigned mask, int pmode, int kmode,
                 Window confine, int cursor, unsigned long time) {
    (void)dpy; (void)owner; (void)mask; (void)pmode; (void)kmode; (void)confine; (void)cursor; (void)time;
    if (xd_valid(w) && w->hwnd) SetCapture(w->hwnd);
    return GrabSuccess;
}
int XGrabKeyboard(Display *dpy, Window w, int owner, int pmode, int kmode, unsigned long time) {
    (void)dpy; (void)owner; (void)pmode; (void)kmode; (void)time;
    if (xd_valid(w) && w->hwnd) SetFocus(w->hwnd);
    return GrabSuccess;
}
int XUngrabPointer(Display *dpy, unsigned long time) { (void)dpy; (void)time; ReleaseCapture(); return 0; }
int XUngrabKeyboard(Display *dpy, unsigned long time) { (void)dpy; (void)time; return 0; }

void x11_apply_alpha_shape(Window dest, const unsigned char *rgba, int res, int win_px) {
    if (!dest || !dest->hwnd || !rgba || res <= 0 || win_px <= 0) return;
    HRGN acc = CreateRectRgn(0, 0, 0, 0);
    int any = 0;
    for (int y = 0; y < win_px; y++) {
        int sy = (y * res) / win_px;
        if (sy >= res) sy = res - 1;
        int x = 0;
        while (x < win_px) {
            while (x < win_px) {
                int sx = (x * res) / win_px;
                if (sx >= res) sx = res - 1;
                if (rgba[(sy * res + sx) * 4 + 3] > 16) break;
                x++;
            }
            if (x >= win_px) break;
            int x0 = x;
            while (x < win_px) {
                int sx = (x * res) / win_px;
                if (sx >= res) sx = res - 1;
                if (rgba[(sy * res + sx) * 4 + 3] <= 16) break;
                x++;
            }
            HRGN r = CreateRectRgn(x0, y, x, y + 1);
            CombineRgn(acc, acc, r, RGN_OR);
            DeleteObject(r);
            any = 1;
        }
    }
    if (!any) { DeleteObject(acc); return; }
    SetWindowRgn(dest->hwnd, acc, TRUE);
}

void XShapeCombineMask(Display *dpy, Window dest, int dest_kind, int xOff, int yOff, Pixmap mask, int op) {
    (void)dpy; (void)dest_kind; (void)op;
    if (!dest || !dest->hwnd || !mask || !mask->bits) return;
    HRGN acc = CreateRectRgn(0, 0, 0, 0);
    int any = 0;
    int mw = mask->w, mh = mask->h;
    unsigned char *bits = (unsigned char *)mask->bits;
    for (int y = 0; y < mh; y++) {
        int x = 0;
        while (x < mw) {
            while (x < mw) {
                unsigned char *p = bits + (y * mw + x) * 4;
                if (p[0] | p[1] | p[2] | p[3]) break;
                x++;
            }
            if (x >= mw) break;
            int x0 = x;
            while (x < mw) {
                unsigned char *p = bits + (y * mw + x) * 4;
                if (!(p[0] | p[1] | p[2] | p[3])) break;
                x++;
            }
            HRGN r = CreateRectRgn(xOff + x0, yOff + y, xOff + x, yOff + y + 1);
            CombineRgn(acc, acc, r, RGN_OR);
            DeleteObject(r);
            any = 1;
        }
    }
    if (!any) {
        DeleteObject(acc);
        return;
    }
    SetWindowRgn(dest->hwnd, acc, TRUE);
}

XFontStruct *XLoadQueryFont(Display *dpy, const char *name) {
    (void)dpy; (void)name;
    XFontStruct *fs = (XFontStruct *)calloc(1, sizeof(XFontStruct));
    if (!fs) return NULL;
    fs->fid = 1; fs->ascent = 12; fs->descent = 4;
    return fs;
}
int XSetFont(Display *dpy, GC gc, Font font) { (void)dpy; (void)gc; (void)font; return 1; }

XFontSet XCreateFontSet(Display *dpy, const char *name, char ***missing, int *nmissing, char **def) {
    (void)dpy; (void)name; (void)def;
    if (missing) *missing = NULL;
    if (nmissing) *nmissing = 0;
    return (XFontSet)1;
}
int Xutf8DrawString(Display *dpy, Drawable d, XFontSet fs, GC gc, int x, int y, const char *s, int len) {
    (void)fs;
    return XDrawString(dpy, d, gc, x, y, s, len);
}
int Xutf8TextExtents(XFontSet fs, const char *s, int len, XRectangle *ink, XRectangle *logical) {
    (void)fs;
    int w = (len > 0 ? len : (s ? (int)strlen(s) : 0)) * 8;
    if (ink) { ink->x = 0; ink->y = -12; ink->width = (unsigned short)w; ink->height = 16; }
    if (logical) { logical->x = 0; logical->y = -12; logical->width = (unsigned short)w; logical->height = 16; }
    return w;
}
void XFreeStringList(char **list) { (void)list; }
char *XSetLocaleModifiers(const char *mod) { (void)mod; return NULL; }

int x11_process_running(const char *name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    int hit = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (strstr(pe.szExeFile, name)) { hit = 1; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return hit;
}

int x11_spawn_cwd(const char *exe, const char *arg1) {
    wchar_t wexe[4096], wcmd[4352], warg[4096];
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, 4096);
    MultiByteToWideChar(CP_UTF8, 0, arg1 ? arg1 : ".", -1, warg, 4096);
    _snwprintf(wcmd, 4351, L"\"%s\" \"%s\"", wexe, warg);
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
    DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS;
    BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, L".", &si, &pi);
    if (!ok) {
        flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | DETACHED_PROCESS;
        ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, L".", &si, &pi);
    }
    if (!ok) return 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}

/* REAL, NEW 2026-09-27 - two-argument sibling of x11_spawn_cwd() above.
 * Needed by khtpm_entity.c's ensure_taskbar_running() self-heal, which
 * has to relaunch the strip renderer with its REAL two-argument
 * invocation shape:
 *     khtpm_core_render.exe <house_root> <template.xhtpm>
 * x11_spawn_cwd() can only pass one argument, and passing the template
 * in place of house_root (or dropping it) produced a renderer that
 * either exited immediately or drew the wrong window - which is how
 * "no taskbar at all" survived the first attempt at this fix.
 *
 * Same spawn posture as x11_spawn_cwd() (detached, no console, breaks
 * away from a job object when the OS allows it) because this is the
 * same "child must outlive its parent" requirement. arg2 may be NULL
 * for callers that genuinely only need one argument. */
int x11_spawn_cwd2(const char *exe, const char *arg1, const char *arg2) {
    wchar_t wexe[4096], wcmd[4352], warg1[4096], warg2[4096];
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, 4096);
    MultiByteToWideChar(CP_UTF8, 0, arg1 ? arg1 : ".", -1, warg1, 4096);
    if (arg2 && arg2[0]) {
        MultiByteToWideChar(CP_UTF8, 0, arg2, -1, warg2, 4096);
        _snwprintf(wcmd, 4351, L"\"%s\" \"%s\" \"%s\"", wexe, warg1, warg2);
    } else {
        _snwprintf(wcmd, 4351, L"\"%s\" \"%s\"", wexe, warg1);
    }
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
    DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS;
    BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, L".", &si, &pi);
    if (!ok) {
        flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | DETACHED_PROCESS;
        ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, L".", &si, &pi);
    }
    if (!ok) return 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}

XSizeHints *XAllocSizeHints(void) { return (XSizeHints *)calloc(1, sizeof(XSizeHints)); }
XWMHints *XAllocWMHints(void) { return (XWMHints *)calloc(1, sizeof(XWMHints)); }
void XSetWMHints(Display *dpy, Window w, XWMHints *h) { (void)dpy; (void)w; (void)h; }
void XSetWMNormalHints(Display *dpy, Window w, XSizeHints *h) { (void)dpy; (void)w; (void)h; }
int XSetWMProtocols(Display *dpy, Window w, Atom *protocols, int n) { (void)dpy; (void)w; (void)protocols; (void)n; return 1; }
    int XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *wa) {
        (void)dpy;
        if (!wa || !xd_valid(w)) return 0;   /* REAL, NEW 2026-09-26 - guarded */
        wa->x = w->x; wa->y = w->y; wa->width = w->w; wa->height = w->h;
        return 1;
    }
int XGetInputFocus(Display *dpy, Window *w, int *revert) {
    (void)dpy;
    if (w) *w = NULL;
    if (revert) *revert = RevertToParent;
    return 1;
}
unsigned long XGetPixel(XImage *img, int x, int y) {
    if (!img || !img->data || x < 0 || y < 0 || x >= img->width || y >= img->height) return 0;
    unsigned char *p = (unsigned char *)img->data + y * img->bytes_per_line + x * 4;
    return ((unsigned long)p[2] << 16) | ((unsigned long)p[1] << 8) | p[0];
}
char *XKeysymToString(KeySym ks) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "0x%lx", (unsigned long)ks);
    return buf;
}
int XAllocColor(Display *dpy, Colormap cmap, XColor *c) {
    (void)dpy; (void)cmap;
    if (!c) return 0;
    c->pixel = ((c->red >> 8) << 16) | ((c->green >> 8) << 8) | (c->blue >> 8);
    return 1;
}
int XParseColor(Display *dpy, Colormap cmap, const char *spec, XColor *c) {
    XColor sc, ec;
    if (!XAllocNamedColor(dpy, cmap, spec ? spec : "black", &sc, &ec)) return 0;
    if (c) *c = sc;
    return 1;
}
void XftFontClose(Display *dpy, XftFont *font) {
    (void)dpy;
    if (!font) return;
    if (font->hf) DeleteObject(font->hf);
    free(font);
}
void XftTextExtentsUtf8(Display *dpy, XftFont *font, const FcChar8 *s, int len, XGlyphInfo *out) {
    SIZE sz; sz.cx = 8; sz.cy = 13;
    if (out) { memset(out, 0, sizeof(*out)); }
    if (!s || len <= 0) { if (out) out->width = 0; return; }
    HDC hdc = GetDC(NULL);
    HFONT old = NULL;
    if (font && font->hf) old = (HFONT)SelectObject(hdc, font->hf);
    wchar_t wbuf[1024];
    int n = MultiByteToWideChar(CP_UTF8, 0, (const char *)s, len, wbuf, 1023);
    if (n < 0) n = 0;
    wbuf[n] = 0;
    GetTextExtentPoint32W(hdc, wbuf, n, &sz);
    if (old) SelectObject(hdc, old);
    ReleaseDC(NULL, hdc);
    if (out) {
        out->width = (short)sz.cx;
        out->height = (short)sz.cy;
        out->xOff = (short)sz.cx;
        out->yOff = 0;
        out->y = (short)(sz.cy * 4 / 5);
    }
    (void)dpy;
}


/* ======================================================================
 * REAL, NEW 2026-09-26 - the window / selection / event half of Xlib that
 * khtpm_core_render.c needs and this shim did not have.
 *
 * Grouped by what the renderer actually calls them for:
 *
 *  1. Image write:   XPutPixel
 *  2. Window geometry: XConfigureWindow, XResizeWindow,
 *                      XTranslateCoordinates
 *  3. Properties:    XGetWindowProperty (pairs with XChangeProperty above)
 *  4. Selection:     XSetSelectionOwner, XGetSelectionOwner,
 *                    XConvertSelection
 *  5. Events:        XSendEvent, XCheckTypedWindowEvent
 *
 * Honest limitations, called out rather than hidden:
 *  - There is no cross-process window manager and no other X client, so
 *    XSendEvent delivers into this process's own event queue. That is enough
 *    for the renderer's self-addressed ClientMessages (the XDND _XDND_FINISHED
 *    reply at core_render.c:9497, WM_DELETE_WINDOW, _NET_WM_STATE) and for
 *    an in-process drag source/target pair, but it cannot reach a foreign
 *    client the way a real X server would.
 *  - XTranslateCoordinates is exact only for windows this process created,
 *    because their positions come from the real HWNDs. It reports 0 for a
 *    foreign window rather than inventing a position.
 * ====================================================================== */

/* --- 1. image write ---------------------------------------------------
 * Byte-for-byte inverse of XGetPixel, which reads BGRX (a 32-bit DIB), so a
 * pixel written here reads back identical. draw_core.c:662-668 converts an
 * RGBA canvas buffer to 0xRRGGBB and stamps it in through this. */
int XPutPixel(XImage *img, int x, int y, unsigned long pixel) {
    if (!img || !img->data || x < 0 || y < 0 || x >= img->width || y >= img->height) return 0;
    unsigned char *p = (unsigned char *)img->data + (size_t)y * img->bytes_per_line + (size_t)x * 4;
    p[0] = (unsigned char)(pixel & 255);
    p[1] = (unsigned char)((pixel >> 8) & 255);
    p[2] = (unsigned char)((pixel >> 16) & 255);
    p[3] = 255;
    return 1;
}

/* --- 2. window geometry ----------------------------------------------- */

/* Real X11 CWStackMode values are 0/1 for Below/Above; the shim defines
 * CWStackMode itself as 16 (see the header), so both the stack bits and the
 * geometry bits are read from the same XWindowChanges the caller filled. */
static void win_apply(Display *dpy, Xd *xd, XWindowChanges *wc) {
    if (!xd || !wc) return;
    int x = (wc->flags & (CWX | CWY)) ? wc->x : xd->x;
    int y = (wc->flags & (CWX | CWY)) ? wc->y : xd->y;
    int w = (wc->flags & CWWidth)  ? wc->width  : xd->w;
    int h = (wc->flags & CWHeight) ? wc->height : xd->h;
    if (xd->kind == KIND_WIN && xd->hwnd) {
        UINT swp = SWP_NOACTIVATE;
        /* A stack request is meaningful, so do not suppress SWP_NOZORDER when
         * one was made - that is the whole point of
         * render_managed_sink_below(). Otherwise leave the z-order alone. */
        HWND after = NULL;
        if (wc->flags & CWStackMode) {
            if (wc->stack_mode == Above)      after = HWND_TOP;
            else if (wc->stack_mode == Below) after = HWND_BOTTOM;
        } else {
            swp |= SWP_NOZORDER;
        }
        SetWindowPos(xd->hwnd, after, x, y, w, h, swp);
    }
    xd->x = x; xd->y = y;
    if ((wc->flags & (CWWidth | CWHeight))) { xd->w = w; xd->h = h; }
    (void)dpy;
}

void XConfigureWindow(Display *dpy, Window w, unsigned int mask, XWindowChanges *wc) {
    if (!xd_valid(w) || !wc) return;
    wc->flags = (long)mask;
    win_apply(dpy, w, wc);
}

void XResizeWindow(Display *dpy, Window w, unsigned int width, unsigned int height) {
    if (!xd_valid(w)) return;
    if (w->kind == KIND_WIN && w->hwnd)
        SetWindowPos(w->hwnd, NULL, 0, 0, (int)width, (int)height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    w->w = (int)width; w->h = (int)height;
    (void)dpy;
}

Bool XTranslateCoordinates(Display *dpy, Window src_w, Window dest_w,
                           int src_x, int src_y, int *dest_x_return,
                           int *dest_y_return, Window *child_return) {
    (void)dpy;
    POINT p;
    HWND sh = NULL, dh = NULL;
    /* A null/root source means the virtual screen, which is what X calls the
     * root window and what DefaultRootWindow() evaluates to in this shim. */
    if (src_w && xd_valid(src_w) && src_w->hwnd) sh = src_w->hwnd;
    if (dest_w && xd_valid(dest_w) && dest_w->hwnd) dh = dest_w->hwnd;
    if (src_w && !sh) return False;          /* foreign/unknown window */
    if (src_w) {
        RECT r; GetWindowRect(sh, &r);
        p.x = r.left + src_x;
        p.y = r.top + src_y;
    } else {
        p.x = src_x; p.y = src_y;
    }
    if (dh) {
        RECT dr; GetWindowRect(dh, &dr);
        if (dest_x_return) *dest_x_return = p.x - dr.left;
        if (dest_y_return) *dest_y_return = p.y - dr.top;
    } else {
        if (dest_x_return) *dest_x_return = p.x;
        if (dest_y_return) *dest_y_return = p.y;
    }
    if (child_return) *child_return = NULL;  /* no child tracking in the shim */
    return True;
}

/* --- 3. property read ------------------------------------------------- */
int XGetWindowProperty(Display *dpy, Window w, Atom property,
                       long long_offset, long long_length, Bool delete,
                       Atom req_type, Atom *actual_type_return,
                       int *actual_format_return,
                       unsigned long *nitems_return,
                       unsigned long *bytes_after_return,
                       unsigned char **prop_return) {
    (void)dpy;
    if (!xd_valid(w)) return 1;   /* BadWindow */
    KProp *p = prop_find(w, property);
    if (actual_type_return)   *actual_type_return = p ? property : None;
    if (actual_format_return) *actual_format_return = p ? p->format : 0;
    if (nitems_return)        *nitems_return = 0;
    if (bytes_after_return)   *bytes_after_return = 0;
    if (prop_return)          *prop_return = NULL;
    /* Success with a NULL payload is how X11 reports "no such property", and
     * it is what the caller's `data && n > 0` guard expects. */
    if (!p) return 0;
    if (req_type != AnyPropertyType && p->format != 0) {
        /* The shim stores no type atom per property, so a typed request is
         * accepted for the formats the renderer actually uses (32-bit atoms
         * and cardinals) rather than rejected outright. */
    }
    int esz = prop_item_bytes(p->format);
    long total_items = (long)p->nitems;
    long start = long_offset;
    if (start < 0) start = 0;
    long avail = total_items - start;
    if (avail < 0) avail = 0;
    long want = long_length;                 /* caller asks in 32-bit words */
    if (want < 0 || want > avail) want = avail;
    unsigned long nbytes = (unsigned long)want * (unsigned long)esz;
    unsigned char *out = NULL;
    if (nbytes) {
        out = (unsigned char *)malloc(nbytes);
        if (!out) return 2;                  /* AllocError */
        memcpy(out, p->data + (size_t)start * (size_t)esz, nbytes);
    }
    if (nitems_return)      *nitems_return = (unsigned long)want;
    if (bytes_after_return) *bytes_after_return =
        (unsigned long)((avail - want) * esz);
    if (prop_return)        *prop_return = out; else free(out);
    if (delete) XDeleteProperty(dpy, w, property);
    return 0;                                /* Success */
}

/* --- 4. selection ----------------------------------------------------- */
int XSetSelectionOwner(Display *dpy, Atom selection, Window owner, Time when) {
    (void)when;
    if (!dpy) return 0;
    if (selection == None) return 0;
    dpy->sel_atom = selection;
    dpy->sel_owner = (owner == None) ? NULL : owner;
    return owner != None ? 1 : 0;   /* XNone -> success-with-no-owner == 0 */
}

Window XGetSelectionOwner(Display *dpy, Atom selection) {
    if (!dpy || selection != dpy->sel_atom) return None;
    return dpy->sel_owner;
}

/* Real X delivers this by asking the owner to answer: the owner receives an
 * XSelectionRequestEvent and responds with SelectionNotify. Doing the same
 * here means the renderer's drag-source and drop-target code paths both run
 * unchanged, because they are already written as source/target/notify. */
int XConvertSelection(Display *dpy, Atom selection, Atom target, Atom property,
                      Window requestor, Time when) {
    if (!dpy || !dpy->sel_owner || selection != dpy->sel_atom) return 0;
    XEvent rq;
    memset(&rq, 0, sizeof(rq));
    rq.xselectionrequest.type = SelectionRequest;
    rq.xselectionrequest.display = dpy;
    rq.xselectionrequest.owner = dpy->sel_owner;
    rq.xselectionrequest.requestor = requestor;
    rq.xselectionrequest.selection = selection;
    rq.xselectionrequest.target = target;
    rq.xselectionrequest.property = property;
    rq.xselectionrequest.time = when;
    qpush(dpy, &rq);
    return 1;
}

/* --- 5. events -------------------------------------------------------- */
Status XSendEvent(Display *dpy, Window w, Bool propagate, long event_mask, XEvent *ev) {
    (void)propagate; (void)event_mask;
    if (!dpy || !ev) return 0;
    if (w && !xd_valid(w)) return 0;
    qpush(dpy, ev);
    return 1;
}

/* Non-blocking: drains one event of the requested type, leaving anything
 * else queued for XNextEvent. This is the "peek without consuming the rest of
 * the queue" the render loop uses to spot a ClientMessage. */
int XCheckTypedWindowEvent(Display *dpy, Window w, long mask, XEvent *ev) {
    (void)w;
    if (!dpy || !ev) return 0;
    pump(dpy);
    int count = (dpy->qh >= dpy->qt) ? (dpy->qh - dpy->qt)
                                     : (EQMAX - (dpy->qt - dpy->qh));
    for (int i = 0; i < count; i++) {
        int idx = (dpy->qt + i) % EQMAX;
        if (mask && (dpy->q[idx].type & (long)0x7f) != (mask & 0x7f)) continue;
        *ev = dpy->q[idx];
        /* close the hole so the queue stays a ring */
        for (int j = idx; j != dpy->qh; j = (j + 1) % EQMAX)
            dpy->q[j] = dpy->q[(j + 1) % EQMAX];
        dpy->qh = (dpy->qh + EQMAX - 1) % EQMAX;
        return 1;
    }
    return 0;
}

/* =========================================================================
 * REAL, NEW 2026-09-26 - the entity's extra Xlib surface.
 *
 * khtpm_entity.c (the pal/tile process) is a far heavier Xlib user than the
 * strip parser this shim was first written for. It draws its own
 * per-pixel-alpha sprite, opens popups, and drags itself between desktop
 * windows, so it needs window reparenting, visual selection, 1-bit mask
 * transfer, stroked arcs, and pointer position. Each of the five below is a
 * real Win32 implementation, not a stub - a stub here would compile and then
 * render a black rectangle, which is exactly the "it built but nothing
 * appeared" failure mode this port has already hit once.
 * ====================================================================== */

/* --- pointer position --------------------------------------------------- */
Bool XQueryPointer(Display *dpy, Window w, Window *root_return,
                   Window *child_return, int *root_x_return, int *root_y_return,
                   int *win_x_return, int *win_y_return,
                   unsigned int *mask_return) {
    POINT pt;
    if (!GetCursorPos(&pt)) return False;

    if (root_return) *root_return = NULL;   /* the shim's root is None */

    /* child_return: the topmost shim window under the cursor, resolved
     * through the HWND -> Xd back-pointer that XCreateWindow stashes in
     * GWLP_USERDATA. WindowFromPoint can return a window that is not one of
     * ours (or the desktop itself), in which case child is None - which is
     * also what real X11 reports when the pointer is over the bare root. */
    Window child = NULL;
    HWND under = WindowFromPoint(pt);
    if (under) {
        Xd *probe = hwnd_xd(under);
        /* Only accept it if it is a live registered shim window: a foreign
         * HWND that happens to have a non-null GWLP_USERDATA (a Win32
         * control, a tooltip) must not be handed back as a Window. */
        if (probe && xd_valid(probe)) child = probe;
    }
    if (child_return) *child_return = child;

    if (root_x_return) *root_x_return = pt.x;
    if (root_y_return) *root_y_return = pt.y;

    /* win_x/win_y are relative to the WINDOW ASKED ABOUT, not to the child.
     * The entity asks about the root (which is None here), so this reduces to
     * the screen coordinates - but honour `w` when it is a real window, so
     * the answer is right for any caller. */
    POINT local = pt;
    HWND h = (w && xd_valid(w)) ? w->hwnd : NULL;
    if (h) ScreenToClient(h, &local);
    if (win_x_return) *win_x_return = local.x;
    if (win_y_return) *win_y_return = local.y;

    if (mask_return) {
        /* Buttons and modifiers, in the real X11 bit order, so the entity's
         * own mask tests (e.g. "is the left button still down") keep
         * working. GetAsyncKeyState's high bit is the current state. */
        unsigned int m = 0;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) m |= 1u << 8;   /* Button1Mask  */
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) m |= 1u << 9;   /* Button2Mask  */
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) m |= 1u << 10;  /* Button3Mask  */
        if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) m |= ShiftMask;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) m |= ControlMask;
        *mask_return = m;
    }
    (void)dpy;
    return True;
}

/* --- reparenting -------------------------------------------------------- */
void XReparentWindow(Display *dpy, Window w, Window parent, int x, int y) {
    (void)dpy;
    /* Both ends go through xd_valid. The entity's drag path deliberately
     * feeds this a `host` Window that came from casting a raw integer out of
     * a file (khtpm_entity.c:4437, `host = (Window)zwin`), so `parent` is
     * routinely NOT one of ours. Unguarded, that is a wild pointer
     * dereference - the same hazard xd_valid() was added for. */
    if (!xd_valid(w) || !w->hwnd) return;
    if (parent && !xd_valid(parent)) return;

    HWND ph = (parent && xd_valid(parent)) ? parent->hwnd : NULL;

    /* Real X11 reparenting makes the window a CHILD of the new parent, which
     * on Win32 means the WS_CHILD style and a non-NULL parent handle. Going
     * back to the root is the inverse. A child window cannot be TOPMOST, so
     * the ex-style comes off with it. */
    LONG ex = GetWindowLongW(w->hwnd, GWL_EXSTYLE);
    if (ph) {
        SetParent(w->hwnd, ph);
        LONG st = GetWindowLongW(w->hwnd, GWL_STYLE);
        SetWindowLongW(w->hwnd, GWL_STYLE, (st & ~WS_POPUP) | WS_CHILD);
        SetWindowLongW(w->hwnd, GWL_EXSTYLE, ex & ~WS_EX_TOPMOST);
    } else {
        SetParent(w->hwnd, NULL);
        LONG st = GetWindowLongW(w->hwnd, GWL_STYLE);
        SetWindowLongW(w->hwnd, GWL_STYLE, (st & ~WS_CHILD) | WS_POPUP);
        SetWindowLongW(w->hwnd, GWL_EXSTYLE, ex | WS_EX_TOPMOST);
    }
    w->parent = parent;

    /* x/y are relative to the NEW parent, exactly as on X11. Position
     * directly here rather than going through XMoveResizeWindow(), because
     * that one adds the work-area origin on the assumption every window is a
     * top-level popup - true for the taskbar, false for a reparented child. */
    RECT base;
    if (ph) {
        RECT pr;
        GetClientRect(ph, &pr);
        ClientToScreen(ph, (POINT *)&pr.left);
        base = pr;
    } else {
        work_area(&base);
    }
    w->x = x;
    w->y = y;
    SetWindowPos(w->hwnd, ph ? HWND_TOP : HWND_TOPMOST,
                 base.left + x, base.top + y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

/* --- visual selection --------------------------------------------------- */
Status XMatchVisualInfo(Display *dpy, int screen, int depth, int class,
                        XVisualInfo *vinfo_return) {
    if (!dpy || !vinfo_return) return 0;
    /* The shim has exactly ONE visual - a 32-bit one, and 32-bit is what a
     * TrueColor request wants. So: a 32-bit TrueColor request is satisfied
     * with the display's own visual, and anything else honestly reports
     * "no such visual" so the caller's own fallback runs. The entity writes
     * that fallback deliberately (entity.c:4003-4004 falls back to
     * DefaultVisual/DefaultDepth, which return the same visual and depth 32),
     * so both branches land on identical rendering. */
    if (depth != 32 || class != TrueColor) return 0;
    memset(vinfo_return, 0, sizeof(*vinfo_return));
    vinfo_return->visual = &dpy->vis;
    vinfo_return->visualid = 0;
    vinfo_return->screen = screen;
    vinfo_return->depth = 32;
    vinfo_return->class = TrueColor;
    vinfo_return->red_mask = 0xFF0000ul;
    vinfo_return->green_mask = 0x00FF00ul;
    vinfo_return->blue_mask = 0x0000FFul;
    vinfo_return->colormap = (int)DefaultColormap(dpy, screen);
    vinfo_return->bits_per_rgb = 8;
    return 1;
}

/* --- 1-bit mask transfer ------------------------------------------------ */
Pixmap XCreateBitmapFromData(Display *dpy, Drawable d, const char *data,
                             unsigned width, unsigned height) {
    if (!dpy || !data || !width || !height) return NULL;
    Pixmap p = XCreatePixmap(dpy, d, width, height, 1);
    if (!p || !p->bits) { if (p) XFreePixmap(dpy, p); return NULL; }
    /* Real X11 stores a 1-bit bitmap MSB-first with a stride of
     * (width+7)/8 bytes per scanline. The entity's own caller
     * (kh_build_shape_mask_generic, entity.c:1873-1887) builds exactly that
     * layout by hand, so the same stride/bit order is read back here.
     *
     * The shim's Pixmap is a 32-bit DIB, and the one consumer
     * (XShapeCombineMask) treats ANY non-zero channel as "inside the
     * silhouette" and all-zero as "outside". So a set bit becomes solid
     * white and a clear bit solid black, which is a faithful widening of the
     * 1-bit mask into the format this shim already speaks. */
    int stride = (int)((width + 7) / 8);
    unsigned char *px = (unsigned char *)p->bits;
    const unsigned char *bits = (const unsigned char *)data;
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            unsigned long v =
                (bits[(size_t)y * (size_t)stride + (x >> 3)] >> (x & 7)) & 1u;
            unsigned char *q = px + ((size_t)y * (size_t)width + (size_t)x) * 4;
            q[0] = q[1] = q[2] = v ? 0xFF : 0x00;
            q[3] = 0xFF;
        }
    }
    return p;
}

/* --- stroked arc -------------------------------------------------------- */
void XDrawArc(Display *dpy, Drawable d, GC gc, int x, int y,
              unsigned width, unsigned height, int angle1, int angle2) {
    (void)dpy;
    HDC hdc = dc_of(d);
    if (!hdc) return;
    unsigned long fg = gc ? gc->foreground : 0;
    /* NULL_BRUSH is the whole point of XDrawArc: it STROKES the outline and
     * leaves the interior untouched. XFillArc above deliberately fills, and
     * using a solid brush here would erase the middle of cursword's
     * silhouette instead of drawing a reticle on it. */
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, 1, pix_to_cr(fg));
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    /* X11 measures arc angles in 64ths of a degree, 0 at 3 o'clock, growing
     * COUNTER-clockwise, inscribed in the given rectangle as an ellipse.
     *
     * Neither GDI angle primitive maps onto that: AngleArc() takes a RADIUS
     * about a centre point, so it can only ever draw a CIRCLE, and Arc()
     * takes start/end POINT coordinates rather than angles. So the general
     * case goes to Arc() with the two endpoints computed onto the ellipse.
     *
     * Direction needs no fixing up: AD_COUNTERCLOCKWISE is GDI's default arc
     * direction, and the y-negation below puts the endpoints in screen space
     * (y grows downward), which makes GDI's counter-clockwise screen sweep
     * the same visual sweep as X11's counter-clockwise mathematical one.
     *
     * A full turn is special-cased to Ellipse() first, because the endpoints
     * of a full turn coincide at 3 o'clock and GDI's degenerate-endpoint
     * handling is not worth relying on. That case is also the ONLY one the
     * entity uses today - both of its XDrawArc calls (khtpm_entity.c:2043 and
     * :6282) are 0 .. 360*64 - and it is not cosmetic: those two calls are
     * what stroke the 1-bit silhouette/click mask the entity is composited
     * from, so a missed outline there leaves the pal with no visible shape
     * and no hit area. 360 * 64 == 23040 X11 units per turn. */
    long sweep = (long)angle2 - (long)angle1;
    if (sweep % 23040L == 0) {
        Ellipse(hdc, x, y, x + (int)width, y + (int)height);
    } else {
        const double kPi = 3.14159265358979323846;
        double th1 = (double)angle1 / 64.0 * (kPi / 180.0);
        double th2 = (double)angle2 / 64.0 * (kPi / 180.0);
        double cx = (double)x + (double)width / 2.0;
        double cy = (double)y + (double)height / 2.0;
        double rx = (double)width / 2.0;
        double ry = (double)height / 2.0;
        int x3 = (int)(cx + rx * cos(th1) + 0.5);
        int y3 = (int)(cy - ry * sin(th1) + 0.5);
        int x4 = (int)(cx + rx * cos(th2) + 0.5);
        int y4 = (int)(cy - ry * sin(th2) + 0.5);
        Arc(hdc, x, y, x + (int)width, y + (int)height, x3, y3, x4, y4);
    }
    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);
    dc_done(d, hdc);
}
