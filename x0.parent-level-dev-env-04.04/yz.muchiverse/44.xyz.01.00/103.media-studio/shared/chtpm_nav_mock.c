/* chtpm_nav_mock.c — top bar + [>]/[ ] continuous nav (ee_gl_mock language) */
#include "chtpm_nav_mock.h"
#include <GL/gl.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char label[40];
    float x, y, w, h;
    int zone;
} Slot;

static Slot g_slots[CHTPM_NAV_MAX];
static int g_n = 0;
static int g_focus = 0;
static int g_digit_mode = 0;
static int g_digit_accum = 0;
static char g_last_act[80] = "";
static int g_win_w = 1280, g_win_h = 800;

void chtpm_nav_set_window(int win_w, int win_h) {
    if (win_w > 0) g_win_w = win_w;
    if (win_h > 0) g_win_h = win_h;
}

int chtpm_nav_bar_h(void) { return CHTPM_NAV_BAR_H; }

void chtpm_nav_begin(void) { g_n = 0; }

void chtpm_nav_add(const char *label, float x, float y, float w, float h, int zone) {
    if (g_n >= CHTPM_NAV_MAX) return;
    Slot *s = &g_slots[g_n++];
    snprintf(s->label, sizeof(s->label), "%s", label ? label : "?");
    s->x = x; s->y = y; s->w = w; s->h = h;
    s->zone = zone;
    if (g_focus >= g_n) g_focus = g_n - 1;
    if (g_focus < 0) g_focus = 0;
}

int chtpm_nav_count(void) { return g_n; }
int chtpm_nav_focus(void) { return g_focus; }
int chtpm_nav_digit_mode(void) { return g_digit_mode; }

const char *chtpm_nav_focus_label(void) {
    if (g_n <= 0 || g_focus < 0 || g_focus >= g_n) return "(none)";
    return g_slots[g_focus].label;
}

static void do_jump(int n1based) {
    if (g_n <= 0) return;
    if (n1based < 1) n1based = 1;
    if (n1based > g_n) n1based = g_n;
    g_focus = n1based - 1;
    g_digit_accum = 0;
    snprintf(g_last_act, sizeof(g_last_act), "jump #%d %s",
             n1based, g_slots[g_focus].label);
}

static void mock_activate(void) {
    if (g_n <= 0) return;
    snprintf(g_last_act, sizeof(g_last_act), "ACTIVATE mock #%d %s",
             g_focus + 1, g_slots[g_focus].label);
}

int chtpm_nav_on_key(unsigned char key, int shift) {
    if (key == '\t') {
        if (g_n <= 0) return 1;
        if (shift) {
            g_focus--;
            if (g_focus < 0) g_focus = g_n - 1;
        } else {
            g_focus++;
            if (g_focus >= g_n) g_focus = 0;
        }
        g_digit_accum = 0;
        snprintf(g_last_act, sizeof(g_last_act), "focus #%d %s",
                 g_focus + 1, g_slots[g_focus].label);
        return 1;
    }
    if (key == '`' || key == '~') {
        g_digit_mode = !g_digit_mode;
        g_digit_accum = 0;
        snprintf(g_last_act, sizeof(g_last_act), g_digit_mode
                 ? "digit-jump ON" : "digit-jump OFF");
        return 1;
    }
    if (key == 13 || key == '\n') {
        mock_activate();
        return g_digit_mode ? 1 : 0;
    }
    if (g_digit_mode && key >= '0' && key <= '9') {
        int d = key - '0';
        g_digit_accum = g_digit_accum * 10 + d;
        if (g_digit_accum > g_n * 10) g_digit_accum = d;
        if (g_digit_accum >= 1 && g_digit_accum <= g_n && g_digit_accum >= 10)
            do_jump(g_digit_accum);
        else
            snprintf(g_last_act, sizeof(g_last_act), "digit… %d_", g_digit_accum);
        return 1;
    }
    return 0;
}

int chtpm_nav_on_special(int key) {
    if (!g_digit_mode || g_n <= 0) return 0;
    if (key == GLUT_KEY_LEFT || key == GLUT_KEY_UP) {
        g_focus--;
        if (g_focus < 0) g_focus = g_n - 1;
        snprintf(g_last_act, sizeof(g_last_act), "focus #%d %s",
                 g_focus + 1, g_slots[g_focus].label);
        return 1;
    }
    if (key == GLUT_KEY_RIGHT || key == GLUT_KEY_DOWN) {
        g_focus++;
        if (g_focus >= g_n) g_focus = 0;
        snprintf(g_last_act, sizeof(g_last_act), "focus #%d %s",
                 g_focus + 1, g_slots[g_focus].label);
        return 1;
    }
    return 0;
}

void chtpm_nav_status(char *buf, size_t n) {
    if (!buf || n < 8) return;
    if (g_n <= 0) {
        snprintf(buf, n, "CHTPM mock (no slots)");
        return;
    }
    snprintf(buf, n, "CHTPM %s#%d %s%s%s",
             g_digit_mode ? "DIG " : "",
             g_focus + 1,
             g_slots[g_focus].label,
             g_last_act[0] ? " · " : "",
             g_last_act);
}

static void text_small(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}

void chtpm_nav_draw(void) {
    int bar = CHTPM_NAV_BAR_H;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* ---- top methods bar (window space: y=0 is top) ----
     * Hosts draw content with glTranslate(0, bar) so this never covers File etc. */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, g_win_w, g_win_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(0.14f, 0.17f, 0.26f, 1.f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0); glVertex2f((float)g_win_w, 0);
    glVertex2f((float)g_win_w, (float)bar); glVertex2f(0, (float)bar);
    glEnd();
    glColor4f(0.35f, 0.50f, 0.75f, 1.f);
    glBegin(GL_LINES);
    glVertex2f(0, (float)bar - 0.5f); glVertex2f((float)g_win_w, (float)bar - 0.5f);
    glEnd();

    glColor4f(0.55f, 0.70f, 0.95f, 1.f);
    text_small(6, 12, "METHODS");
    text_small(6, 24, "nav");

    if (g_n > 0) {
        float x0 = 52.f;
        float gap = 4.f;
        float usable = (float)g_win_w - x0 - 8.f;
        float chip_w = usable / (float)g_n - gap;
        if (chip_w < 48.f) chip_w = 48.f;
        for (int i = 0; i < g_n; i++) {
            float cx = x0 + i * (chip_w + gap);
            int foc = (i == g_focus);
            if (foc) glColor4f(0.35f, 0.48f, 0.22f, 1.f);
            else glColor4f(0.18f, 0.20f, 0.28f, 1.f);
            glBegin(GL_QUADS);
            glVertex2f(cx, 3); glVertex2f(cx + chip_w, 3);
            glVertex2f(cx + chip_w, (float)bar - 3); glVertex2f(cx, (float)bar - 3);
            glEnd();
            if (foc) glColor4f(1.f, 0.9f, 0.3f, 1.f);
            else glColor4f(0.45f, 0.55f, 0.70f, 1.f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx, 3); glVertex2f(cx + chip_w, 3);
            glVertex2f(cx + chip_w, (float)bar - 3); glVertex2f(cx, (float)bar - 3);
            glEnd();
            char buf[56];
            snprintf(buf, sizeof(buf), "%s%d %s", foc ? "[>]" : "[ ]", i + 1, g_slots[i].label);
            if (foc) glColor4f(1.f, 0.98f, 0.75f, 1.f);
            else glColor4f(0.85f, 0.88f, 0.95f, 1.f);
            text_small(cx + 4, 18, buf);
        }
    }

    /* status snippet right side of bar if room */
    {
        char st[96];
        chtpm_nav_status(st, sizeof(st));
        glColor4f(0.50f, 0.80f, 0.60f, 1.f);
        /* only show short hint if many chips would collide — bottom-right of bar */
        (void)st;
        glColor4f(0.50f, 0.55f, 0.65f, 1.f);
        text_small((float)g_win_w - 210, 18, "Tab  ` dig  (mock)");
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    /* ---- focus outline in *current* modelview (host content space) ---- */
    if (g_n > 0 && g_focus >= 0 && g_focus < g_n) {
        Slot *s = &g_slots[g_focus];
        glColor4f(1.0f, 0.85f, 0.20f, 0.65f);
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        glVertex2f(s->x + 1, s->y + 1);
        glVertex2f(s->x + s->w - 1, s->y + 1);
        glVertex2f(s->x + s->w - 1, s->y + s->h - 1);
        glVertex2f(s->x + 1, s->y + s->h - 1);
        glEnd();
        glLineWidth(1);
        /* small corner badge in content space (not a second header) */
        char buf[48];
        snprintf(buf, sizeof(buf), "[>] %d", g_focus + 1);
        glColor4f(0.20f, 0.28f, 0.10f, 0.85f);
        glBegin(GL_QUADS);
        glVertex2f(s->x + 4, s->y + 4);
        glVertex2f(s->x + 52, s->y + 4);
        glVertex2f(s->x + 52, s->y + 18);
        glVertex2f(s->x + 4, s->y + 18);
        glEnd();
        glColor4f(1.f, 0.95f, 0.4f, 1.f);
        text_small(s->x + 6, s->y + 15, buf);
    }
}
