#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <GL/freeglut.h>
#include <GL/gl.h>

/* Real Xdnd (freeglut fgDisplay.Display hack + XdndAware/Enter/Position/
 * Drop/SelectionRequest handling) used to live here - see
 * system/xdnd_target.c/.h. It's not linked into this binary anymore:
 * WM reparenting (mutter wraps this window in a decoration frame, so
 * the naive direct-child-of-root window search never found it) plus
 * an unrelated CPU-throttle bug in the idle poll made this fragile to
 * debug and contributed to a real system crash during testing. gl_canvas
 * now just polls a plain drop-queue file that pet_purely writes when it
 * detects its own release point landed inside our window's rect - both
 * programs are ours, so we don't need a real OS drag-and-drop protocol
 * to talk to each other. */

#define WIN_W 200
#define WIN_H 200

static char project_root[1024];
static int g_window_w = WIN_W, g_window_h = WIN_H;

static int g_drop_pending = 0;
static time_t g_drop_time = 0;
static char g_drop_pet_id[256] = {0};

/* Reverse flow: click the pet inside the canvas and drag it back out.
 * Same philosophy as the drop side - no X11/Xdnd, just GLUT's own
 * mouse/motion/entry callbacks (GLUT already tells us window-relative
 * cursor position and fires a native leave-event when the pointer
 * crosses our window's edge, regardless of how fast the drag moves). */
static int g_reverse_dragging = 0;
static int g_last_mx = 0, g_last_my = 0;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void trigger_pet_import(const char *pid) {
    if (!pid || !pid[0]) return;
    fprintf(stderr, "gl_canvas: importing pet '%s'\n", pid);
    snprintf(g_drop_pet_id, sizeof(g_drop_pet_id), "%s", pid);
    g_drop_pending = 1;
    g_drop_time = time(NULL);
    glutPostRedisplay();

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./system/import_pet '%s' 2>&1 &", project_root, pid);
    system(cmd);
}

/* pet_purely writes pet_id + '\n' via write-temp-then-rename, so we
 * never see a half-written file here. */
static void check_for_drop(void) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/incoming_drop.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char pid[256] = {0};
    if (fgets(pid, sizeof(pid), f)) {
        size_t len = strlen(pid);
        while (len > 0 && (pid[len - 1] == '\n' || pid[len - 1] == '\r')) pid[--len] = '\0';
    }
    fclose(f);
    remove(path);
    if (pid[0]) trigger_pet_import(pid);
}

static void idle_tick(void) {
    check_for_drop();
    /* freeglut calls the idle func back-to-back with no yield of its
     * own - without this sleep, gl_canvas pegs a full CPU core just
     * polling, even with nothing happening. */
    usleep(16000);
}

/* Spawns pet_purely back on the desktop at the given SCREEN coordinates
 * and clears our own "pet is in the canvas" state. Mirrors
 * trigger_pet_import()'s use of system() to hand off to a sibling
 * binary rather than reimplementing window creation here. */
static void release_pet_back_to_desktop(const char *pid, int screen_x, int screen_y) {
    fprintf(stderr, "gl_canvas: releasing pet '%s' back to desktop at (%d,%d)\n", pid, screen_x, screen_y);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./system/pet_purely '%s' %d %d 2>&1 &",
             project_root, pid, screen_x, screen_y);
    system(cmd);

    g_drop_pet_id[0] = '\0';
    g_drop_pending = 0;
    glutPostRedisplay();
}

static void mouse_cb(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON) return;
    if (state == GLUT_DOWN) {
        if (g_drop_pet_id[0]) {
            g_reverse_dragging = 1;
            g_last_mx = x;
            g_last_my = y;
        }
    } else {
        /* Released without leaving the window - pet stays in the canvas. */
        g_reverse_dragging = 0;
    }
}

static void motion_cb(int x, int y) {
    if (!g_reverse_dragging) return;
    g_last_mx = x;
    g_last_my = y;
    glutPostRedisplay();
}

static void entry_cb(int state) {
    if (state != GLUT_LEFT || !g_reverse_dragging) return;
    /* Pointer just crossed our window's edge while dragging - hand the
     * pet back to a fresh pet_purely at the current screen position. */
    int win_screen_x = glutGet(GLUT_WINDOW_X);
    int win_screen_y = glutGet(GLUT_WINDOW_Y);
    int screen_x = win_screen_x + g_last_mx - 40;  /* centers pet_purely's 80x80 on the cursor */
    int screen_y = win_screen_y + g_last_my - 40;

    char pid_copy[256];
    snprintf(pid_copy, sizeof(pid_copy), "%s", g_drop_pet_id);
    g_reverse_dragging = 0;
    release_pet_back_to_desktop(pid_copy, screen_x, screen_y);
}

static void display(void) {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_drop_pet_id[0]) {
        float cx = 0.0f, cy = 0.0f, half = 0.7f;
        if (g_reverse_dragging) {
            int ww = glutGet(GLUT_WINDOW_WIDTH), wh = glutGet(GLUT_WINDOW_HEIGHT);
            if (ww < 1) ww = 1;
            if (wh < 1) wh = 1;
            cx = ((float)g_last_mx / ww) * 2.0f - 1.0f;
            cy = 1.0f - ((float)g_last_my / wh) * 2.0f;
            half = 0.3f;
        }
        glColor3f(0.3f, 0.6f, 0.3f);
        glBegin(GL_QUADS);
        glVertex2f(cx - half, cy - half);
        glVertex2f(cx + half, cy - half);
        glVertex2f(cx + half, cy + half);
        glVertex2f(cx - half, cy + half);
        glEnd();
    }

    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.95f, -0.95f);
    glVertex2f( 0.95f, -0.95f);
    glVertex2f( 0.95f,  0.95f);
    glVertex2f(-0.95f,  0.95f);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(-0.55f, 0.15f);
    const char *label = "gl-canvas";
    for (const char *c = label; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    if (g_drop_pet_id[0]) {
        glRasterPos2f(-0.55f, -0.1f);
        const char *pfx = "Got: ";
        for (const char *c = pfx; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        for (const char *c = g_drop_pet_id; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    } else {
        glRasterPos2f(-0.7f, -0.1f);
        const char *wait = "Drop a pet here...";
        for (const char *c = wait; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }

    if (g_drop_pending && (time(NULL) - g_drop_time < 2)) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 1.0f, 0.0f, 0.3f);
        glBegin(GL_QUADS);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f( 1.0f, -1.0f);
        glVertex2f( 1.0f,  1.0f);
        glVertex2f(-1.0f,  1.0f);
        glEnd();
        glDisable(GL_BLEND);
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    glutSwapBuffers();
}

static void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv) {
    resolve_root();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(g_window_w, g_window_h);
    glutCreateWindow("gl-canvas");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle_tick);
    glutMouseFunc(mouse_cb);
    glutMotionFunc(motion_cb);
    glutEntryFunc(entry_cb);
    glutMainLoop();
    return 0;
}
