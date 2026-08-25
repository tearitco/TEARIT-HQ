/* Win32 implementation of the Xlib/Xft subset used by khtpm_strip_parser.c */
#include "khtpm_strip_x11_win.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tlhelp32.h>

#define KIND_WIN 1
#define KIND_PIX 2
#define EQMAX 256

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
};

struct Display {
    int sw, sh;
    Visual vis;
    XEvent q[EQMAX];
    int qh, qt;
    Atom next_atom;
    Atom opacity_atom;
    HFONT font;
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
        ev.type = FocusIn; ev.xbutton.window = xd;
        qpush(d, &ev);
        return 0;
    }
    if (m == WM_KILLFOCUS) {
        XEvent ev; memset(&ev, 0, sizeof(ev));
        ev.type = FocusOut; ev.xbutton.window = xd;
        qpush(d, &ev);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        /* Linux MapWindow delivers Expose; Win WM_PAINT must too or
         * entity tiles never present (need_redraw stays 0). */
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = Expose;
        ev.xany.window = xd;
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
    xd->hwnd = hwnd;
    if (hwnd)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)xd);
    (void)dpy;
    return xd;
}

void XDestroyWindow(Display *dpy, Window w) {
    (void)dpy;
    if (!w) return;
    if (w->hwnd) DestroyWindow(w->hwnd);
    free(w);
}

void XMapRaised(Display *dpy, Window w) {
    if (!w || !w->hwnd) return;
    ShowWindow(w->hwnd, SW_SHOW);
    SetWindowPos(w->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    InvalidateRect(w->hwnd, NULL, FALSE);
    if (dpy) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = Expose;
        ev.xany.window = w;
        qpush(dpy, &ev);
    }
}

void XUnmapWindow(Display *dpy, Window w) {
    (void)dpy;
    if (w && w->hwnd) ShowWindow(w->hwnd, SW_HIDE);
}

void XRaiseWindow(Display *dpy, Window w) {
    (void)dpy;
    if (w && w->hwnd)
        SetWindowPos(w->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void XMoveResizeWindow(Display *dpy, Window w, int x, int y, unsigned width, unsigned height) {
    (void)dpy;
    if (!w || !w->hwnd) return;
    clamp_to_work_area(&x, &y, &width, &height);
    w->x = x; w->y = y; w->w = (int)width; w->h = (int)height;
    RECT wa;
    work_area(&wa);
    SetWindowPos(w->hwnd, HWND_TOPMOST, wa.left + x, wa.top + y,
                 (int)width, (int)height, SWP_NOACTIVATE);
}

void XSetWindowBackground(Display *dpy, Window w, unsigned long pixel) {
    (void)dpy;
    if (w) w->bg = pixel;
}

void XSetInputFocus(Display *dpy, Window w, int revert, unsigned long time) {
    (void)dpy; (void)revert; (void)time;
    if (w && w->hwnd) SetFocus(w->hwnd);
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

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *v) {
    (void)dpy; (void)d; (void)mask;
    GC gc = (GC)calloc(1, sizeof(*gc));
    if (!gc) return NULL;
    gc->foreground = 0xFFFFFFul;
    gc->background = 0;
    if (v) { gc->foreground = v->foreground; gc->background = v->background; }
    return gc;
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
        StretchDIBits(hdc, 0, 0, rc.right, rc.bottom,
                      0, 0, img->width, img->height,
                      img->data, &bmi, DIB_RGB_COLORS, SRCCOPY);
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

int XChangeProperty(Display *dpy, Window w, Atom prop, Atom type, int format,
                    int mode, const unsigned char *data, int nelements) {
    (void)type; (void)format; (void)mode; (void)nelements;
    if (!w || !w->hwnd || !dpy) return 0;
    if (prop == dpy->opacity_atom && data) {
        unsigned long val = *(const unsigned long *)data;
        BYTE a = (BYTE)(val / (0xFFFFFFFFul / 255ul));
        w->opacity = a;
        SetLayeredWindowAttributes(w->hwnd, 0, a ? a : 1, LWA_ALPHA);
    }
    return 1;
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
    unsigned ww = w ? (unsigned)w->w : 1, hh = w ? (unsigned)w->h : 1;
    XMoveResizeWindow(dpy, w, x, y, ww, hh);
}

void XClearWindow(Display *dpy, Window w) {
    if (!w || !w->hwnd) return;
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
    if (!w || !w->hwnd || !name) return;
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
    if (w && w->hwnd) SetCapture(w->hwnd);
    return GrabSuccess;
}
int XGrabKeyboard(Display *dpy, Window w, int owner, int pmode, int kmode, unsigned long time) {
    (void)dpy; (void)owner; (void)pmode; (void)kmode; (void)time;
    if (w && w->hwnd) SetFocus(w->hwnd);
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

XSizeHints *XAllocSizeHints(void) { return (XSizeHints *)calloc(1, sizeof(XSizeHints)); }
XWMHints *XAllocWMHints(void) { return (XWMHints *)calloc(1, sizeof(XWMHints)); }
void XSetWMHints(Display *dpy, Window w, XWMHints *h) { (void)dpy; (void)w; (void)h; }
void XSetWMNormalHints(Display *dpy, Window w, XSizeHints *h) { (void)dpy; (void)w; (void)h; }
int XSetWMProtocols(Display *dpy, Window w, Atom *protocols, int n) { (void)dpy; (void)w; (void)protocols; (void)n; return 1; }
int XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *wa) {
    (void)dpy;
    if (!wa || !w) return 0;
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

