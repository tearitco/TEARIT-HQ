/* ee_gl_mock — RPG Maker MV–style Event Editor (pure freeglut)
 *
 * This is the layout you liked: left Event props, page tabs, conditions,
 * image, trigger fields; right Contents command list; bottom OK/Cancel bar;
 * top method strip. Continuous global nav 1..N (CHTPM digit_accum style).
 *
 * Build target for "pretty GLUT" path (A/B vs chtpm→rgb→gl_mirror).
 *
 * Keys: arrows  digits (multi)  Enter  Tab=Commands|Scratch  Esc/q quit
 * Compile: gcc -O2 -o ee_gl_mock ee_gl_mock.c -lGL -lGLU -lglut
 */
#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WIN_W 960
#define WIN_H 720
#define MAX_NAV 48

typedef struct {
    int zone;
    const char *label;
    int method; /* 1-8 side effects; 5=toggle view */
} NavItem;

enum {
    Z_METH = 0,
    Z_PAGE,
    Z_FIELD,
    Z_CONT,
    Z_FOOT
};

static NavItem items[MAX_NAV];
static int n_items = 0;
static int focus = 0;       /* 0-based; display = focus+1 */
static int digit_accum = 0;
static int view_mode = 0;   /* 0=commands 1=scratch */
static int page_sel = 0;    /* 0-3 */
static int tick = 0;
static char status[192] = "Nav > _   continuous 1..N   multi-digit OK";

static const char *CMDS[] = {
    "Show Text : \"Halt! Who goes there?\"",
    "Show Choices : Fight, Flee, Talk",
    "When [Talk]",
    "  Control Switches : door_open = ON",
    "  Show Text : \"You may pass.\"",
    "End",
    "Conditional Branch : Switch door_open is ON",
    "  Transfer Player : map_02 (3, 8)",
    "End",
    "Comment : dual-source IR / event.pal",
    "Script (.pal) : call_op muta_map_io ...",
    "(empty — insert command)",
};
static const char *BLOCKS[] = {
    "[ when action_button ]",
    "  { show_text \"Halt! Who goes there?\" }",
    "  { show_choices Fight | Flee | Talk }",
    "  [ if choice == Talk ]",
    "    { set_switch door_open ON }",
    "    { show_text \"You may pass.\" }",
    "  [ if switch door_open == ON ]",
    "    { transfer map_02 3 8 }",
    "  [ comment dual_source ]",
    "  { call_op muta_map_io ... }",
    "  + add block ...",
    "(empty block slot)",
};
#define N_CONT 12

static void rebuild_nav(void) {
    int i;
    n_items = 0;
#define ADD(z, lab, m) do { \
        if (n_items < MAX_NAV) { \
            items[n_items].zone = (z); \
            items[n_items].label = (lab); \
            items[n_items].method = (m); \
            n_items++; \
        } \
    } while (0)

    /* 1-8 METHODS */
    ADD(Z_METH, "Save", 1);
    ADD(Z_METH, "Load", 2);
    ADD(Z_METH, "Import", 3);
    ADD(Z_METH, "Export", 4);
    ADD(Z_METH, "Toggle View", 5);
    ADD(Z_METH, "Edit .pal", 6);
    ADD(Z_METH, "New Page", 7);
    ADD(Z_METH, "Help", 8);
    /* 9-12 PAGES */
    ADD(Z_PAGE, "Page 1", 0);
    ADD(Z_PAGE, "Page 2", 0);
    ADD(Z_PAGE, "Page 3", 0);
    ADD(Z_PAGE, "Page 4", 0);
    /* 13-16 FIELDS */
    ADD(Z_FIELD, "Trigger: Action Button", 0);
    ADD(Z_FIELD, "Priority: Same as chars", 0);
    ADD(Z_FIELD, "Options: Direction fix", 0);
    ADD(Z_FIELD, "Walk: Fixed / Through", 0);
    /* 17-28 CONTENTS */
    {
        const char **rows = view_mode ? BLOCKS : CMDS;
        for (i = 0; i < N_CONT; i++)
            ADD(Z_CONT, rows[i], 0);
    }
    /* 29-34 FOOTER */
    ADD(Z_FOOT, "OK", 0);
    ADD(Z_FOOT, "Cancel", 0);
    ADD(Z_FOOT, "Apply", 0);
    ADD(Z_FOOT, "Add Cmd", 0);
    ADD(Z_FOOT, "Palette", 0);
    ADD(Z_FOOT, "Desktop", 0);
#undef ADD
    if (focus >= n_items) focus = n_items - 1;
    if (focus < 0) focus = 0;
}

static void set_rgb(float r, float g, float b) { glColor3f(r, g, b); }

static void fill_rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

static void stroke_rect(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

static void draw_text(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}

static void draw_text_big(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s);
}

static void draw_text_small(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s);
}

static void panel(float x, float y, float w, float h, const char *title) {
    set_rgb(0.05f, 0.06f, 0.10f);
    fill_rect(x + 3, y - 3, w, h);
    set_rgb(0.18f, 0.22f, 0.32f);
    fill_rect(x, y, w, h);
    set_rgb(0.28f, 0.36f, 0.55f);
    fill_rect(x, y + h - 28, w, 28);
    set_rgb(0.55f, 0.70f, 0.95f);
    stroke_rect(x, y, w, h);
    set_rgb(0.95f, 0.97f, 1.0f);
    draw_text(x + 12, y + h - 19, title);
}

static int first_of_zone(int z) {
    int i;
    for (i = 0; i < n_items; i++)
        if (items[i].zone == z) return i;
    return 0;
}

static void toggle_view(void) {
    view_mode = !view_mode;
    rebuild_nav();
    snprintf(status, sizeof(status), "View: %s  (dual-source graph)",
             view_mode ? "SCRATCH blocks" : "COMMANDS list");
}

static void activate(void) {
    if (focus < 0 || focus >= n_items) return;
    if (items[focus].zone == Z_PAGE) {
        page_sel = focus - first_of_zone(Z_PAGE);
        if (page_sel < 0) page_sel = 0;
        if (page_sel > 3) page_sel = 3;
        snprintf(status, sizeof(status), "Page %d selected", page_sel + 1);
        return;
    }
    if (items[focus].method == 5) { toggle_view(); return; }
    if (items[focus].method == 7) {
        page_sel = (page_sel + 1) % 4;
        snprintf(status, sizeof(status), "New/next page -> %d", page_sel + 1);
        return;
    }
    if (items[focus].method == 1)
        snprintf(status, sizeof(status), "Save: STUB -> package flush");
    else if (items[focus].method == 2)
        snprintf(status, sizeof(status), "Load: STUB -> desktop package");
    else if (items[focus].method == 3)
        snprintf(status, sizeof(status), "Import: STUB ee_import_to_world");
    else if (items[focus].method == 4)
        snprintf(status, sizeof(status), "Export: STUB ee_export_entity");
    else if (items[focus].method == 6)
        snprintf(status, sizeof(status), "Edit .pal: STUB raw buffer");
    else if (items[focus].method == 8)
        snprintf(status, sizeof(status),
                 "Help: type 17 for content#1 | Tab toggles Commands/Scratch");
    else if (items[focus].zone == Z_FOOT && strcmp(items[focus].label, "OK") == 0)
        snprintf(status, sizeof(status), "OK: STUB save+close");
    else if (items[focus].zone == Z_FOOT && strcmp(items[focus].label, "Cancel") == 0)
        snprintf(status, sizeof(status), "Cancel: STUB discard");
    else
        snprintf(status, sizeof(status), "Activated #%d  %s",
                 focus + 1, items[focus].label);
}

static void do_jump(int n) {
    if (n >= 1 && n <= n_items) {
        focus = n - 1;
        snprintf(status, sizeof(status), "Nav jump %d  [%s]", n, items[focus].label);
    }
}

static void digit_key(int d) {
    int total = n_items;
    int new_val = digit_accum * 10 + d;
    if (new_val >= 1 && new_val <= total) {
        digit_accum = new_val;
        do_jump(digit_accum);
    } else if (d >= 1 && d <= total) {
        digit_accum = d;
        do_jump(digit_accum);
    } else {
        digit_accum = 0;
    }
}

static void display(void) {
    int i;
    char buf[160];
    int meth0 = first_of_zone(Z_METH);
    int page0 = first_of_zone(Z_PAGE);
    int field0 = first_of_zone(Z_FIELD);
    int cont0 = first_of_zone(Z_CONT);
    int foot0 = first_of_zone(Z_FOOT);

    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* ===== TITLE + status (RMMV chrome) ===== */
    set_rgb(0.22f, 0.26f, 0.38f);
    fill_rect(0, WIN_H - 52, WIN_W, 52);
    set_rgb(0.95f, 0.96f, 1.0f);
    draw_text_big(16, WIN_H - 28, "Event Editor");
    set_rgb(0.55f, 0.65f, 0.80f);
    draw_text_small(160, WIN_H - 26, "RPG Maker MV–style  |  pure GLUT build target");
    set_rgb(0.12f, 0.14f, 0.20f);
    fill_rect(0, WIN_H - 52, WIN_W, 18);
    set_rgb(0.45f, 0.90f, 0.55f);
    draw_text_small(16, WIN_H - 48, status);

    /* ===== TOP METHOD BAR (1-8) — like piece_methods ===== */
    set_rgb(0.15f, 0.18f, 0.28f);
    fill_rect(10, WIN_H - 108, WIN_W - 20, 50);
    set_rgb(0.45f, 0.60f, 0.85f);
    stroke_rect(10, WIN_H - 108, WIN_W - 20, 50);
    set_rgb(0.70f, 0.80f, 0.95f);
    draw_text_small(16, WIN_H - 70, "METHODS  (continuous nav #1-8)");

    {
        float mw = (WIN_W - 40) / 8.0f;
        for (i = 0; i < 8; i++) {
            int idx = meth0 + i;
            float mx = 16 + i * mw;
            int foc = (focus == idx);
            if (foc) set_rgb(0.35f, 0.50f, 0.75f);
            else set_rgb(0.18f, 0.22f, 0.32f);
            fill_rect(mx, WIN_H - 104, mw - 6, 28);
            if (foc) set_rgb(1.0f, 0.9f, 0.3f);
            else set_rgb(0.40f, 0.50f, 0.65f);
            stroke_rect(mx, WIN_H - 104, mw - 6, 28);
            snprintf(buf, sizeof(buf), "%s%d %s", foc ? "[>] " : "[ ] ",
                     idx + 1, items[idx].label);
            set_rgb(foc ? 1.0f : 0.88f, foc ? 1.0f : 0.92f, foc ? 0.8f : 0.98f);
            draw_text_small(mx + 4, WIN_H - 96, buf);
        }
    }

    /* ===== LEFT: Event (RMMV left column) ===== */
    panel(12, 78, 310, 520, "Event");

    set_rgb(0.85f, 0.90f, 1.0f);
    draw_text(24, 560, "Name");
    set_rgb(0.10f, 0.12f, 0.18f);
    fill_rect(72, 552, 230, 24);
    set_rgb(0.50f, 0.80f, 1.0f);
    stroke_rect(72, 552, 230, 24);
    set_rgb(1.0f, 1.0f, 0.9f);
    draw_text(80, 558, "door_guard");

    /* Pages as tabs 9-12 */
    set_rgb(0.85f, 0.90f, 1.0f);
    draw_text(24, 520, "Pages");
    for (i = 0; i < 4; i++) {
        int idx = page0 + i;
        float px = 24 + i * 70;
        int foc = (focus == idx);
        int sel = (page_sel == i);
        if (foc) set_rgb(0.35f, 0.50f, 0.75f);
        else if (sel) set_rgb(0.28f, 0.40f, 0.58f);
        else set_rgb(0.14f, 0.16f, 0.24f);
        fill_rect(px, 488, 64, 26);
        set_rgb(foc ? 1.0f : 0.55f, foc ? 0.9f : 0.70f, foc ? 0.3f : 0.95f);
        stroke_rect(px, 488, 64, 26);
        snprintf(buf, sizeof(buf), "%s%d", foc ? ">" : " ", idx + 1);
        set_rgb(1, 1, 1);
        draw_text_small(px + 8, 496, buf);
        draw_text_small(px + 28, 496, items[idx].label + 5); /* "1" from "Page 1" */
    }

    /* Conditions box */
    set_rgb(0.14f, 0.16f, 0.24f);
    fill_rect(24, 350, 286, 120);
    set_rgb(0.45f, 0.55f, 0.75f);
    stroke_rect(24, 350, 286, 120);
    set_rgb(0.90f, 0.92f, 1.0f);
    draw_text(32, 448, "Conditions");
    set_rgb(0.70f, 0.85f, 0.70f);
    draw_text_small(36, 420, "[x] Switch   door_open   is OFF");
    draw_text_small(36, 400, "[ ] Variable ................");
    draw_text_small(36, 380, "[ ] Self Switch  A B C D");
    draw_text_small(36, 360, "[ ] Item ....................");

    /* Image / sprite */
    set_rgb(0.14f, 0.16f, 0.24f);
    fill_rect(24, 200, 130, 135);
    set_rgb(0.45f, 0.55f, 0.75f);
    stroke_rect(24, 200, 130, 135);
    set_rgb(0.90f, 0.92f, 1.0f);
    draw_text(32, 315, "Image");
    set_rgb(0.25f, 0.45f, 0.30f);
    fill_rect(44, 220, 90, 80);
    set_rgb(1.0f, 0.95f, 0.4f);
    draw_text_big(74, 255, "@");
    set_rgb(0.65f, 0.75f, 0.85f);
    draw_text_small(40, 208, "(event sprite)");

    /* Fields 13-16 as stacked rows */
    for (i = 0; i < 4; i++) {
        int idx = field0 + i;
        float fy = 300 - i * 32;
        int foc = (focus == idx);
        if (foc) set_rgb(0.32f, 0.48f, 0.72f);
        else set_rgb(0.12f, 0.14f, 0.20f);
        fill_rect(168, fy, 140, 28);
        if (foc) set_rgb(1.0f, 0.9f, 0.3f);
        else set_rgb(0.40f, 0.48f, 0.60f);
        stroke_rect(168, fy, 140, 28);
        snprintf(buf, sizeof(buf), "%s%d.", foc ? "[>]" : "[ ]", idx + 1);
        set_rgb(foc ? 1.0f : 0.6f, foc ? 0.95f : 0.7f, foc ? 0.4f : 0.85f);
        draw_text_small(172, fy + 8, buf);
        set_rgb(0.90f, 0.95f, 1.0f);
        draw_text_small(210, fy + 8, items[idx].label);
    }

    set_rgb(0.50f, 0.60f, 0.75f);
    draw_text_small(24, 175, "map_start @ (5,4)");
    draw_text_small(24, 158, "package: #.desktop/events/...");
    draw_text_small(24, 141, "event.pal + event.ir.pdl");
    draw_text_small(24, 124, "build target: pure GLUT RMMV UI");

    /* ===== RIGHT: Contents (RMMV script list) ===== */
    {
        char title[96];
        snprintf(title, sizeof(title), "Contents   [ %s | %s ]",
                 view_mode == 0 ? "COMMANDS*" : "Commands",
                 view_mode == 1 ? "SCRATCH*" : "Scratch");
        panel(336, 78, 612, 520, title);
    }

    /* Toggle chips */
    if (view_mode == 0) set_rgb(0.30f, 0.55f, 0.40f); else set_rgb(0.16f, 0.18f, 0.26f);
    fill_rect(350, 548, 130, 28);
    set_rgb(0.50f, 0.90f, 0.60f);
    stroke_rect(350, 548, 130, 28);
    set_rgb(1, 1, 1);
    draw_text_small(372, 556, "Commands");

    if (view_mode == 1) set_rgb(0.55f, 0.40f, 0.25f); else set_rgb(0.16f, 0.18f, 0.26f);
    fill_rect(490, 548, 130, 28);
    set_rgb(1.0f, 0.75f, 0.40f);
    stroke_rect(490, 548, 130, 28);
    set_rgb(1, 1, 1);
    draw_text_small(518, 556, "Scratch");

    set_rgb(0.55f, 0.65f, 0.80f);
    draw_text_small(640, 556, "Tab or #5  |  list nav #17-28");

    /* Command list area */
    set_rgb(0.12f, 0.14f, 0.22f);
    fill_rect(348, 120, 588, 420);
    set_rgb(0.40f, 0.55f, 0.80f);
    stroke_rect(348, 120, 588, 420);

    set_rgb(0.22f, 0.28f, 0.42f);
    fill_rect(348, 520, 588, 22);
    set_rgb(0.85f, 0.90f, 1.0f);
    if (view_mode == 0)
        draw_text_small(358, 526, "#   Command  (RMMV Contents)");
    else
        draw_text_small(358, 526, "#   Scratch block  (same IR as .pal)");

    for (i = 0; i < N_CONT; i++) {
        int idx = cont0 + i;
        float cy = 495 - i * 30;
        int foc = (focus == idx);
        if (foc) {
            set_rgb(0.30f, 0.45f, 0.70f);
            fill_rect(352, cy - 6, 580, 28);
        } else if (i % 2 == 0) {
            set_rgb(0.14f, 0.16f, 0.24f);
            fill_rect(352, cy - 6, 580, 28);
        } else if (view_mode == 1) {
            set_rgb(0.16f, 0.20f, 0.26f);
            fill_rect(352, cy - 6, 580, 28);
        }
        snprintf(buf, sizeof(buf), "%02d", idx + 1);
        set_rgb(0.50f, 0.60f, 0.75f);
        draw_text_small(360, cy, buf);
        snprintf(buf, sizeof(buf), "%s %s", foc ? "[>]" : "[ ]", items[idx].label);
        if (foc) set_rgb(1.0f, 1.0f, 0.75f);
        else set_rgb(0.88f, 0.92f, 0.98f);
        draw_text_small(390, cy, buf);
    }

    /* ===== FOOTER 29-34 ===== */
    set_rgb(0.16f, 0.18f, 0.26f);
    fill_rect(0, 0, WIN_W, 68);
    set_rgb(0.40f, 0.50f, 0.70f);
    stroke_rect(0, 0, WIN_W, 68);

    {
        float fw = (WIN_W - 40) / 6.0f;
        for (i = 0; i < 6; i++) {
            int idx = foot0 + i;
            float fx = 14 + i * fw;
            int foc = (focus == idx);
            if (foc) set_rgb(0.35f, 0.50f, 0.75f);
            else set_rgb(0.28f, 0.38f, 0.58f);
            fill_rect(fx, 18, fw - 10, 36);
            set_rgb(foc ? 1.0f : 0.60f, foc ? 0.9f : 0.80f, foc ? 0.3f : 1.0f);
            stroke_rect(fx, 18, fw - 10, 36);
            snprintf(buf, sizeof(buf), "%s %d. %s", foc ? "[>]" : "[ ]",
                     idx + 1, items[idx].label);
            set_rgb(0.95f, 0.97f, 1.0f);
            draw_text(fx + 12, 30, buf);
        }
    }
    set_rgb(0.50f, 0.60f, 0.75f);
    draw_text_small(14, 6,
        "arrows=nav  digits=multi-digit (e.g. 17)  Enter=OK  Tab=Commands|Scratch  Esc/q=quit");

    if ((tick / 25) % 2 == 0 && digit_accum > 0) {
        snprintf(buf, sizeof(buf), "accum:%d", digit_accum);
        set_rgb(1.0f, 0.9f, 0.3f);
        draw_text_small(WIN_W - 90, WIN_H - 28, buf);
    }

    glutSwapBuffers();
}

static void timer(int v) {
    (void)v;
    tick++;
    /* blink caret only needs ~4Hz; still schedule 16ms for snappy nav after keys */
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 'q' || key == 'Q') exit(0);
    if (key == 27) {
        if (digit_accum) digit_accum = 0;
        else exit(0);
    } else if (key == '\t') {
        digit_accum = 0;
        toggle_view();
    } else if (key == 10 || key == 13) {
        if (digit_accum > 0) {
            do_jump(digit_accum);
            digit_accum = 0;
        }
        activate();
    } else if (key >= '0' && key <= '9') {
        digit_key(key - '0');
    }
    glutPostRedisplay();
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    digit_accum = 0;
    if (key == GLUT_KEY_UP) {
        focus--;
        if (focus < 0) focus = n_items - 1;
    } else if (key == GLUT_KEY_DOWN) {
        focus++;
        if (focus >= n_items) focus = 0;
    } else if (key == GLUT_KEY_LEFT) {
        /* jump to previous zone start */
        int z = items[focus].zone;
        while (focus > 0 && items[focus].zone == z) focus--;
        if (items[focus].zone == z && focus > 0) focus--;
        while (focus > 0 && items[focus - 1].zone == items[focus].zone) focus--;
    } else if (key == GLUT_KEY_RIGHT) {
        int z = items[focus].zone;
        while (focus < n_items - 1 && items[focus].zone == z) focus++;
        if (focus < n_items - 1 && items[focus].zone == z) focus++;
    } else if (key == GLUT_KEY_PAGE_UP) {
        focus -= 8;
        if (focus < 0) focus = 0;
    } else if (key == GLUT_KEY_PAGE_DOWN) {
        focus += 8;
        if (focus >= n_items) focus = n_items - 1;
    }
    snprintf(status, sizeof(status), "Nav > _  #%d  %s", focus + 1, items[focus].label);
    glutPostRedisplay();
}

static void reshape(int w, int h) { glViewport(0, 0, w, h); }

int main(int argc, char **argv) {
    rebuild_nav();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Event Editor — RMMV layout (pure GLUT build target)");
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutReshapeFunc(reshape);
    glutTimerFunc(16, timer, 0);
    fprintf(stderr,
        "Event Editor RMMV freeglut — build target for pretty path\n"
        "  continuous nav 1..%d  multi-digit  Tab=Commands|Scratch\n"
        "  product A/B still:  sh ../button.sh r\n",
        n_items);
    glutMainLoop();
    return 0;
}
