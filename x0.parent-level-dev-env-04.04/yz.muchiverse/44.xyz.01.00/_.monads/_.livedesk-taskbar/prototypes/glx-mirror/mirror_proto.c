/* mirror_proto.c — KHTPM refactor plan §6 step 1: GLX/GLUT mirror
 * prototype, translucency via the PROVEN _NET_WM_WINDOW_OPACITY EWMH
 * property (same mechanism already working for the taskbar's own bottom
 * bar, tp_taskbar.c's set_window_opacity()/load_theme_opacity()) rather
 * than fighting a 32-bit ARGB visual from scratch.
 *
 * REVISED 2026-08-10, direct instruction: the first version of this
 * prototype tried to build true per-pixel window transparency via a
 * hand-selected 32-bit ARGB X visual (glXChooseVisual-adjacent,
 * XGetVisualInfo for depth=32) applied AFTER glutCreateWindow - this
 * failed with a real X BadMatch error (confirmed empirically: you
 * cannot retroactively change a window's own visual/colormap after
 * creation), and even glutInitDisplayString couldn't request a real
 * 32-bit window visual (confirmed empirically: "depth>=8" in a GLUT
 * display string requests Z-BUFFER depth, not X11 visual color depth -
 * a real, confusing name collision, verified via a standalone test that
 * printed the actual window visual depth GLUT assigned: 24, not 32).
 *
 * The user then pointed out something much simpler already exists and
 * is PROVEN: the taskbar's bottom bar is already visibly translucent in
 * production, via the standard _NET_WM_WINDOW_OPACITY property on a
 * PLAIN window - no ARGB visual, no GLX-specific concerns, works
 * uniformly on any window (X11-managed content or GL-rendered content,
 * since the property applies to the whole window regardless of how its
 * pixels were drawn). This prototype now uses that exact mechanism.
 *
 * NOTE ON VERIFICATION: XGetImage/xwd-based pixel sampling was tried to
 * verify this and gave a FALSE NEGATIVE - under this Wayland-hosted
 * XWayland setup, Mutter composites at the Wayland/GPU level, outside
 * the X11 client's own rendering pipeline entirely, so XGetImage only
 * ever sees the window's own un-composited content, never the true
 * final blended-with-desktop pixels. glReadPixels (GL's own equivalent)
 * has the identical blind spot for the identical reason. On THIS kind
 * of session, real compositor-level effects (opacity, blur) can only be
 * verified by a human actually looking at the screen - !.HOUSE_STDS.md
 * §C's "read the raw RGBA buffer directly" convention is NOT sufficient
 * proof of on-screen compositing specifically, on Wayland/XWayland,
 * even though it IS still valid for verifying blend math WITHIN a
 * buffer (e.g. two GL-drawn shapes blending against each other).
 */
#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 400
#define WIN_H 200

static double g_opacity = 0.5;

/* Verbatim same mechanism as tp_taskbar.c's set_window_opacity() -
 * deliberately kept identical, not reinvented, so behavior matches the
 * already-proven bottom bar exactly. */
static void set_window_opacity(Display *dpy, Window w, double opacity) {
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    Atom opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long val = (unsigned long)(opacity * (double)0xFFFFFFFFUL);
    XChangeProperty(dpy, w, opacity_atom, XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *)&val, 1);
}

static void apply_taskbar_style_window_setup(void) {
    Display *dpy = glXGetCurrentDisplay();
    Window win = glXGetCurrentDrawable();
    if (!dpy || !win) {
        fprintf(stderr, "apply_taskbar_style_window_setup: no current GLX display/drawable\n");
        return;
    }
    /* override_redirect: same as tp_taskbar.c's own popups/bars - bypass
     * the WM entirely, required for taskbar-style always-visible chrome. */
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attrs);
    /* Real, proven mechanism - no ARGB visual needed at all. */
    set_window_opacity(dpy, win, g_opacity);
    XMapRaised(dpy, win);
    fprintf(stderr, "apply_taskbar_style_window_setup: override_redirect set, "
                     "_NET_WM_WINDOW_OPACITY set to %.2f (matching taskbar's proven bottom-bar mechanism)\n",
            g_opacity);
}

static void display(void) {
    /* Normal opaque clear within the buffer's own content - the WINDOW's
     * overall translucency comes from the EWMH property above, not from
     * this clear color needing to be transparent itself (that was the
     * old, abandoned ARGB-visual approach's job; not needed anymore). */
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Simple content - a couple of solid shapes, matching what a real
     * taskbar popup would draw (buttons/text), to confirm GL content
     * still renders correctly INSIDE a window that also has real
     * whole-window opacity applied via the EWMH property. */
    glColor4f(0.2f, 0.6f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
        glVertex2f(20, 20); glVertex2f(180, 20);
        glVertex2f(180, 180); glVertex2f(20, 180);
    glEnd();

    glColor4f(1.0f, 0.3f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
        glVertex2f(220, 20); glVertex2f(380, 20);
        glVertex2f(380, 180); glVertex2f(220, 180);
    glEnd();

    glutSwapBuffers();
}

static void timer(int value) {
    (void)value;
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);   /* ~60fps - responsiveness check, per §6 step 1 */
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--opacity=", 10) == 0) g_opacity = atof(argv[i] + 10);
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("mirror_proto");

    apply_taskbar_style_window_setup();

    glutDisplayFunc(display);
    glutTimerFunc(16, timer, 0);

    fprintf(stderr, "mirror_proto: running with opacity=%.2f. Ctrl+C to quit.\n"
                     "Human verification needed (Wayland compositing blind spot, see header comment) - "
                     "look at the window and confirm the desktop shows through.\n", g_opacity);

    glutMainLoop();
    return 0;
}
