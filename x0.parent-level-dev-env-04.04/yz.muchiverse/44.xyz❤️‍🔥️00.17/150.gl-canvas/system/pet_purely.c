#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

/* Real Xdnd (XSetSelectionOwner/XdndEnter-Position-Drop/SelectionRequest)
 * used to live here - see system/xdnd_source.c/.h. It's not linked into
 * this binary anymore: WM reparenting + cross-connection ClientMessage
 * delivery made it fragile to debug and it was never confirmed working
 * end-to-end. This program now just checks its own release point
 * against the target window's on-screen rect directly and hands off the
 * pet_id via a plain file - both programs are ours, so we don't need a
 * real OS drag-and-drop protocol to talk to each other. */

#define WIN_W 80
#define WIN_H 80
#define DROP_TARGET_TITLE "gl-canvas"

static char project_root[1024];
static Display *dpy;
static Window win;
static int win_x = 100, win_y = 100;
static const char *pet_id = NULL;

static int dragging = 0;
static int drag_start_x, drag_start_y;
static int win_start_x, win_start_y;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static unsigned int hash_str(const char *s) {
    unsigned int h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

static void display_cb(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    unsigned int h = pet_id ? hash_str(pet_id) : 0x666666;
    float r = ((h >> 16) & 0xFF) / 255.0f;
    float g = ((h >> 8) & 0xFF) / 255.0f;
    float b = (h & 0xFF) / 255.0f;
    float mx = 0.3f;
    if (r < mx) r = mx;
    if (g < mx) g = mx;
    if (b < mx) b = mx;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(-0.8f, -0.8f);
    glVertex2f( 0.8f, -0.8f);
    glVertex2f( 0.8f,  0.8f);
    glVertex2f(-0.8f,  0.8f);
    glEnd();

    glXSwapBuffers(dpy, win);
}

/* WM reparenting (mutter wraps client windows in a decoration frame)
 * means a titled window is often not a direct child of root - search
 * recursively, not just one level deep. */
static Window find_window_by_name(Display *d, Window start, const char *target) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strcmp(name, target) == 0) found = children[i];
            XFree(name);
        }
        if (!found) found = find_window_by_name(d, children[i], target);
    }
    if (children) XFree(children);
    return found;
}

/* Absolute (root-relative) on-screen rect for a window found by title.
 * XGetGeometry's x/y are relative to the window's PARENT, not the
 * screen - translate through XTranslateCoordinates to get true screen
 * coordinates regardless of how many WM frames it's nested inside. */
static int find_window_rect_by_name(Display *d, const char *title,
                                     int *out_x, int *out_y, int *out_w, int *out_h) {
    Window root = RootWindow(d, DefaultScreen(d));
    Window w = find_window_by_name(d, root, title);
    if (!w) return 0;

    Window junk;
    int rel_x, rel_y;
    unsigned int ww, wh, wb, wdepth;
    if (!XGetGeometry(d, w, &junk, &rel_x, &rel_y, &ww, &wh, &wb, &wdepth)) return 0;

    int abs_x, abs_y;
    Window child_ret;
    XTranslateCoordinates(d, w, root, 0, 0, &abs_x, &abs_y, &child_ret);

    *out_x = abs_x; *out_y = abs_y; *out_w = (int)ww; *out_h = (int)wh;
    return 1;
}

/* Writes pet_id to a queue file gl_canvas polls for. Write-to-temp then
 * rename so gl_canvas's own polling (idle-driven, could run at any
 * moment) never sees a half-written file. */
static void write_drop_file(const char *pid) {
    char tmp_path[1200], final_path[1200];
    snprintf(final_path, sizeof(final_path), "%s/incoming_drop.txt", project_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/incoming_drop.txt.tmp", project_root);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { fprintf(stderr, "pet_purely: cannot write drop file\n"); return; }
    fprintf(f, "%s\n", pid);
    fclose(f);
    rename(tmp_path, final_path);
}

/* Checks the ButtonRelease point against gl-canvas's current on-screen
 * rect. If it lands inside, hands off pet_id via write_drop_file() and
 * returns 1 (caller should close the window - the "pet" was imported). */
static int check_drop_on_release(int root_x, int root_y) {
    int tx, ty, tw, th;
    if (!find_window_rect_by_name(dpy, DROP_TARGET_TITLE, &tx, &ty, &tw, &th)) return 0;
    if (root_x >= tx && root_x < tx + tw && root_y >= ty && root_y < ty + th) {
        if (pet_id) write_drop_file(pet_id);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    pet_id = (argc >= 2) ? argv[1] : NULL;
    if (argc >= 4) { win_x = atoi(argv[2]); win_y = atoi(argv[3]); }
    resolve_root();

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "pet_purely: cannot open display\n"); return 1; }

    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), att);
    if (!vi) { fprintf(stderr, "pet_purely: no suitable visual\n"); return 1; }

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;
    swa.override_redirect = True;

    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                        win_x, win_y, WIN_W, WIN_H,
                        0, vi->depth, InputOutput, vi->visual,
                        CWColormap | CWEventMask | CWOverrideRedirect, &swa);
    XStoreName(dpy, win, pet_id ? pet_id : "pet_purely");
    XMapWindow(dpy, win);
    XSync(dpy, False);

    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);

    win_start_x = win_x;
    win_start_y = win_y;

    int running = 1;
    while (running) {
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);

            if (xev.type == Expose) display_cb();

            else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
            }

            else if (xev.type == MotionNotify && dragging) {
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_start_x += dx;
                win_start_y += dy;
                XMoveWindow(dpy, win, win_start_x, win_start_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
            }

            else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                if (check_drop_on_release(xev.xbutton.x_root, xev.xbutton.y_root)) {
                    running = 0;
                }
            }
        }

        if (running) {
            display_cb();
            usleep(16000);
        }
    }

    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
