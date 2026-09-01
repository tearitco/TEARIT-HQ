/* cli_io_window.c - minimal "cli-io + window" console container
 * (NETWORK-CELL-HQ-WINDOWS-DESIGN.md 5.3, network cell Browser stub).
 *
 * Opens a WM-managed X11 window, spawns a child bash over pipes, types
 * lines into the child's stdin and streams child stdout+stderr into a
 * scrollback buffer that is redrawn on each event/read. Standalone
 * binary - deliberately NOT part of khtpm_core_render (HARD
 * BOUNDARY edit rule). Uses only core X fonts (no Xft/fontconfig dep).
 *
 * Usage: cli_io_window [+ title]
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAXLINES 1100
#define MAXCOLS  512
#define INPBUFSZ 512

static char* lines[MAXLINES];
static int nlines = 0;
static char input[INPBUFSZ];
static int ninput = 0;
static const char* fontname = "-misc-fixed-medium-r-normal--14-120-75-75-c-70-iso10646-1";
static int fd_child = -1;
static unsigned long fg, bg;

static void save_line(char* s, int len) {
    size_t plen = (size_t)len;
    if (plen > MAXCOLS) plen = MAXCOLS;
    char* copy = malloc(plen + 1);
    if (!copy) return;
    memcpy(copy, s, plen);
    copy[plen] = '\0';
    if (nlines >= MAXLINES) {
        free(lines[0]);
        memmove(lines, lines + 1, sizeof(char*) * (MAXLINES - 1));
        nlines = MAXLINES - 1;
    }
    lines[nlines++] = copy;
}

static void feed_output(void) {
    char buf[4096];
    for (;;) {
        ssize_t r = read(fd_child, buf, sizeof(buf));
        if (r <= 0) break;
        char* start = buf;
        for (ssize_t i = 0; i < r; i++) {
            if (buf[i] == '\n') {
                save_line(start, (int)(&buf[i] - start));
                start = &buf[i + 1];
            }
        }
        if (start < buf + r) save_line(start, (int)(buf + r - start));
    }
}

static void append_input_str(const char* s, int len) {
    for (int i = 0; i < len; i++) {
        if (ninput < INPBUFSZ - 1) input[ninput++] = s[i];
    }
}

static void send_line(void) {
    if (ninput == 0) { input[0] = '\n'; ninput = 1; }
    int w = write(fd_child, input, ninput);
    (void)w;
    char linebuf[INPBUFSZ + 8];
    memcpy(linebuf, "> ", 2);
    memcpy(linebuf + 2, input, (size_t)ninput);
    save_line(linebuf, ninput + 2);
    ninput = 0;
}

static void init_gc_font(Display* dpy, GC gc, XFontStruct** font) {
    *font = XLoadQueryFont(dpy, fontname);
    if (!*font) *font = XLoadQueryFont(dpy, "fixed");
    if (!*font) *font = XLoadQueryFont(dpy, "9x15");
    if (*font) XSetFont(dpy, gc, (*font)->fid);
}

int main(int argc, char** argv) {
    const char* title = (argc > 1) ? argv[1] : "cli-io";
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cli_io_window: no X display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);

    /* pipes to/from the child shell */
    int to_child[2], from_child[2];
    if (pipe(to_child) || pipe(from_child)) { fprintf(stderr, "pipe failed\n"); return 1; }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        dup2(from_child[1], 2);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execl("/bin/bash", "bash", "-s", NULL);
        _exit(127);
    }
    close(to_child[0]); close(from_child[1]);
    fd_child = from_child[0];
    int flags = fcntl(fd_child, F_GETFL, 0);
    fcntl(fd_child, F_SETFL, flags | O_NONBLOCK);

    Window win = XCreateSimpleWindow(dpy, root, 0, 0, 900, 560, 0,
                                     BlackPixel(dpy, scr), BlackPixel(dpy, scr));
    XStoreName(dpy, win, title);
    XClassHint* ch = XAllocClassHint();
    ch->res_name = "cli_io_window";
    ch->res_class = "CliIo";
    XSetClassHint(dpy, win, ch);
    XFree(ch);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);
    /* REAL FIX 2026-08-31, found live: XSetInputFocus() called here,
     * right after XMapWindow(), fails with a real BadMatch (the window
     * isn't actually viewable yet at this exact point) - and since no
     * X error handler is installed, Xlib's own default handler treats
     * that as FATAL and calls exit() immediately. Net effect: the
     * window opened and died in the same frame, invisible. Real fix:
     * just drop the explicit focus call - a normal WM-managed window
     * (this isn't override_redirect) already gets focus per the WM's
     * own real click/new-window policy without fighting it here. */

    GC gc = XCreateGC(dpy, win, 0, NULL);
    fg = WhitePixel(dpy, scr);
    bg = BlackPixel(dpy, scr);
    XSetForeground(dpy, gc, fg);
    XSetBackground(dpy, gc, bg);
    XFontStruct* font = NULL;
    init_gc_font(dpy, gc, &font);
    int fw = 7, fh = 17;
    if (font) {
        fw = font->max_bounds.rbearing - font->min_bounds.lbearing;
        if (fw < 3) fw = font->max_bounds.width;
        fh = font->ascent + font->descent;
    }
    XSync(dpy, False);

    int running = 1;
    save_line("cli-io window (network cell Browser stub). Type a command and press Enter.", 67);
    save_line("", 0);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_child, &rfds);
        FD_SET(ConnectionNumber(dpy), &rfds);
        struct timeval tv = { 0, 100000 };
        int s = select(ConnectionNumber(dpy) + 1, &rfds, NULL, NULL, &tv);
        if (s < 0 && errno != EINTR) break;

        if (FD_ISSET(fd_child, &rfds)) feed_output();

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                /* redraw below */
            } else if (ev.type == KeyPress) {
                char buf[8];
                int len = XLookupString(&ev.xkey, buf, sizeof(buf), NULL, NULL);
                if (len == 1) {
                    char c = buf[0];
                    if (c == '\r') send_line();
                    else if (c == '\b') { if (ninput > 0) ninput--; }
                    else if (c >= 32 && c < 127) append_input_str(&c, 1);
                } else {
                    KeySym ks = XLookupKeysym(&ev.xkey, 0);
                    if (ks == XK_Escape) running = 0;
                }
            } else if (ev.type == ClientMessage) {
                if ((Atom)ev.xclient.data.l[0] == wm_delete) running = 0;
            } else if (ev.type == DestroyNotify) {
                running = 0;
            }
            if (!running) break;
        }

        /* render */
        XClearWindow(dpy, win);
        XWindowAttributes wa;
        XGetWindowAttributes(dpy, win, &wa);
        int rows = (wa.height - 4) / fh;
        int cols = (wa.width - 4) / fw;
        if (rows < 1) rows = 1;
        int y = 2 + font->ascent;
        int first = nlines - rows + 1;
        if (first < 0) first = 0;
        for (int i = first; i < nlines; i++) {
            char tmp[MAXCOLS + 1];
            snprintf(tmp, sizeof(tmp), "%.*s", cols, lines[i]);
            XDrawString(dpy, win, gc, 2, y, tmp, (int)strlen(tmp));
            y += fh;
        }
        /* input line with cursor, fixed to the bottom of the window */
        char inl[INPBUFSZ + 8];
        snprintf(inl, sizeof(inl), "> %s", input);
        int ibase = wa.height - 4 - font->descent;
        XDrawString(dpy, win, gc, 2, ibase, inl, (int)strlen(inl));
        XDrawRectangle(dpy, win, gc, 2 + fw * (2 + ninput), ibase - font->ascent, fw, fh + 1);

        XFlush(dpy);
    }

    close(fd_child);
    close(to_child[1]);
    if (pid > 0) { kill(pid, SIGTERM); waitpid(pid, NULL, 0); }
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}