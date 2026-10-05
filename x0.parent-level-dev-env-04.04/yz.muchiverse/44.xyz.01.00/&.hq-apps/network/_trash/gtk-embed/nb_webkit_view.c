/* nb_webkit_view.c - the "hybrid escape hatch" for network-browser:
 * a real WebKitGTK window for the pages the hand-built engine can't do
 * (heavy SPAs, WebGL/WASM, DRM video). NB-JS-ENGINE-ROADMAP.md §8 path C.
 *
 * Deliberately minimal - one GtkWindow + one WebKitWebView, no tabs, no
 * bookmarks. The hand-built nb browser stays the default; this is the
 * "open in real browser, in our own window" button.
 *
 * usage: nb_webkit_view.+x <url>
 *
 * Built ONLY when webkit2gtk-4.0 + gtk+-3.0 dev packages are present
 * (see build_nb_webkit_view.sh). On this box (2026): libwebkit2gtk-4.0
 * 2.50.4 is already installed - the .so is ~86 MB, deps push the live
 * footprint to ~150-250 MB, ~150-400 MB RSS per view (multiprocess).
 * No new install needed here; it's an optional compile.
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>
#include <X11/Xlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*   nb_webkit_view.+x <url>                 - own top-level window
 *   nb_webkit_view.+x --xembed <xid> <url>  - render the page INSIDE the
 *       X window <xid> (a sub-window the khtpm renderer created for the
 *       content pane). Our x11-hq chrome (toolbar/tabs/sidebar/address)
 *       stays drawn by khtpm_core_render; only the page pixels are the
 *       real engine's. Geometry/resize is driven by the parent via
 *       ConfigureNotify on <xid>.
 *
 * "chrome, not gl": WebKitGTK's default paint IS an X11/cairo surface
 * (GTK's x11 backend). No GL/EGL window of our own; the WebProcess may
 * use GL internally for accel but the embedded widget is a normal X
 * child window. Set WEBKIT_DISABLE_COMPOSITING_MODE=1 to force pure
 * software/cairo if a driver misbehaves. */

static Window g_container = 0;

static void fit_to_container(GtkWidget *win) {
    if (!g_container) return;
    Display *dpy = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(win));
    Window r; int x, y; unsigned int w, h, bw, d;
    if (XGetGeometry(dpy, g_container, &r, &x, &y, &w, &h, &bw, &d) && w > 0 && h > 0)
        gtk_window_resize(GTK_WINDOW(win), (int)w, (int)h);
}

static gboolean poll_container(gpointer win) {
    fit_to_container(GTK_WIDGET(win));
    /* position-lock ourselves onto the container's root rect instead of
     * reparenting the GTK toplevel - XReparentWindow of a GTK toplevel
     * is what killed this in 2026-09-03 (GTK owns its toplevel and
     * reasserts/quits on a foreign reparent). Visible result: the real
     * engine sits exactly on the pane's rect, tracked on every poll. */
    if (g_container) {
        Display *dpy = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(win));
        Window r; int x, y; unsigned int w, h, bw, d;
        if (XGetGeometry(dpy, g_container, &r, &x, &y, &w, &h, &bw, &d) && w > 0 && h > 0) {
            int rx, ry; Window child;
            XTranslateCoordinates(dpy, g_container, RootWindow(dpy, DefaultScreen(dpy)),
                                  0, 0, &rx, &ry, &child);
            gtk_window_move(GTK_WINDOW(win), rx, ry);
            gtk_window_resize(GTK_WINDOW(win), (int)w, (int)h);
        }
    }
    return G_SOURCE_CONTINUE;
}

static void on_title(WebKitWebView *v, GParamSpec *ps, gpointer win) {
    (void)ps;
    if (g_container) return;                 /* embedded: parent owns the title */
    const char *t = webkit_web_view_get_title(v);
    char buf[512];
    snprintf(buf, sizeof(buf), "%s - network (real engine)", (t && *t) ? t : "loading");
    gtk_window_set_title(GTK_WINDOW(win), buf);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    const char *url = "about:blank";
    int ai = 1;
    if (argc > 2 && strcmp(argv[1], "--xembed") == 0) {
        g_container = (Window)strtoul(argv[2], NULL, 0);
        ai = 3;
    }
    if (argc > ai && argv[ai][0]) url = argv[ai];

    char norm[4096];
    if (!strstr(url, "://") && strncmp(url, "about:", 6) != 0) {
        snprintf(norm, sizeof(norm), "https://%s", url);
        url = norm;
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 800);
    gtk_window_set_title(GTK_WINDOW(win), "network (real engine)");
    if (g_container) {
        /* no WM decorations - this window becomes a child of <xid> */
        gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
        gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DOCK);
    }
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    WebKitWebView *view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_signal_connect(view, "notify::title", G_CALLBACK(on_title), win);
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(view));
    webkit_web_view_load_uri(view, url);

    gtk_widget_realize(win);
    if (g_container) {
        /* no XReparentWindow - see poll_container's own note above. */
        g_timeout_add(200, poll_container, win);
    }
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
