/* draw.c — freeglut drawing helpers (RMMV chrome DNA from ee_gl_mock) */
#include "rpg.h"
#include <GL/glut.h>

const char PAL_CHARS[N_PAL] = { '.', '#', '~', ',', '+' };
const char *PAL_NAMES[N_PAL] = { "floor", "wall", "water", "grass", "door" };

void d_set_rgb(float r, float g, float b) { glColor3f(r, g, b); }

void d_fill_rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void d_stroke_rect(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void d_text(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}

void d_text_big(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s);
}

void d_text_small(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s);
}

void d_panel(float x, float y, float w, float h, const char *title) {
    d_set_rgb(0.05f, 0.06f, 0.10f);
    d_fill_rect(x + 3, y - 3, w, h);
    d_set_rgb(0.18f, 0.22f, 0.32f);
    d_fill_rect(x, y, w, h);
    d_set_rgb(0.28f, 0.36f, 0.55f);
    d_fill_rect(x, y + h - 28, w, 28);
    d_set_rgb(0.55f, 0.70f, 0.95f);
    d_stroke_rect(x, y, w, h);
    d_set_rgb(0.95f, 0.97f, 1.0f);
    d_text(x + 12, y + h - 19, title ? title : "");
}

void d_tile_color(char t, float *r, float *g, float *b) {
    switch (t) {
    case '#': *r = 0.35f; *g = 0.35f; *b = 0.40f; break;
    case '~': *r = 0.15f; *g = 0.35f; *b = 0.70f; break;
    case ',': *r = 0.25f; *g = 0.55f; *b = 0.25f; break;
    case '+': *r = 0.55f; *g = 0.40f; *b = 0.25f; break;
    case '.':
    default:  *r = 0.22f; *g = 0.28f; *b = 0.22f; break;
    }
}

const char *d_tile_name(char t) {
    int i;
    for (i = 0; i < N_PAL; i++)
        if (PAL_CHARS[i] == t) return PAL_NAMES[i];
    return "floor";
}
