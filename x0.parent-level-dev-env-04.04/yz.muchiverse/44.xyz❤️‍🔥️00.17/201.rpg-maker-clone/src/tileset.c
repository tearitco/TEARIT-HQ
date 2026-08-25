/* Generate factory/terrain/deco tiles procedurally (MZ industrial look) */
#include "tileset.h"
#include <GL/glut.h>
#include <math.h>

static int g_tick = 0;

void tileset_set_tick(int tick) { g_tick = tick; }

static void rgb(float r, float g, float b) { glColor3f(r, g, b); }

static void quad(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    glBegin(GL_QUADS);
    glVertex2i(x, y); glVertex2i(x + w, y);
    glVertex2i(x + w, y + h); glVertex2i(x, y + h);
    glEnd();
}

static void lineh(int x, int y, int w, float r, float g, float b) {
    rgb(r, g, b);
    quad(x, y, w, 1);
}

static void linev(int x, int y, int h, float r, float g, float b) {
    rgb(r, g, b);
    quad(x, y, 1, h);
}

/* MZ-style checker under transparent / empty palette cells */
void tileset_draw_checker(int sx, int sy, int s) {
    int i, j, cs = s / 4;
    if (cs < 4) cs = 4;
    for (j = 0; j < s; j += cs) {
        for (i = 0; i < s; i += cs) {
            int light = ((i / cs) + (j / cs)) & 1;
            if (light) rgb(0.72f, 0.72f, 0.74f);
            else rgb(0.55f, 0.55f, 0.58f);
            quad(sx + i, sy + j, cs, cs);
        }
    }
}

static void rivets(int sx, int sy, int s, float shade) {
    int m = s / 8;
    if (m < 2) m = 2;
    rgb(0.55f * shade, 0.55f * shade, 0.60f * shade);
    quad(sx + m, sy + m, m, m);
    quad(sx + s - 2 * m, sy + m, m, m);
    quad(sx + m, sy + s - 2 * m, m, m);
    quad(sx + s - 2 * m, sy + s - 2 * m, m, m);
}

/* A: floors  B: walls/machines  C: props  D: doors/lights  R: regions */
void tileset_draw(unsigned char id, int sx, int sy, int scale) {
    int page = TILE_PAGE(id);
    int idx = TILE_IDX(id);
    int s = TILE_PX * (scale > 0 ? scale : 1);
    int sub = idx % 8;
    int row = idx / 8;
    float shade = 1.0f - (row % 4) * 0.06f;
    int phase = (g_tick / 12) & 3; /* ~5Hz animation */

    if (page == 0) { /* A — floors / outdoor */
        if (sub < 2) {
            /* grass with noise speckles */
            rgb(0.22f * shade, 0.48f * shade, 0.20f * shade);
            quad(sx, sy, s, s);
            {
                int i, j;
                for (j = 2; j < s; j += 4)
                    for (i = 2; i < s; i += 5) {
                        unsigned h = (unsigned)(i * 37 + j * 91 + idx * 13);
                        if ((h & 3) == 0) {
                            rgb(0.30f, 0.60f, 0.28f);
                            quad(sx + i, sy + j, 2, 2);
                        }
                    }
            }
        } else if (sub < 4) {
            /* factory metal plate with grid + rivets (screenshot core) */
            rgb(0.32f * shade, 0.33f * shade, 0.36f * shade);
            quad(sx, sy, s, s);
            /* plate seams */
            lineh(sx, sy + s / 2, s, 0.22f, 0.22f, 0.24f);
            linev(sx + s / 2, sy, s, 0.22f, 0.22f, 0.24f);
            lineh(sx, sy + 1, s, 0.42f, 0.43f, 0.46f);
            linev(sx + 1, sy, s, 0.42f, 0.43f, 0.46f);
            rivets(sx, sy, s, shade);
            /* subtle grate holes */
            if (sub == 3) {
                int i, j;
                rgb(0.18f, 0.18f, 0.20f);
                for (j = 6; j < s - 4; j += 6)
                    for (i = 6; i < s - 4; i += 6)
                        quad(sx + i, sy + j, 2, 2);
            }
        } else if (sub < 6) {
            /* water — animated shimmer */
            float wave = 0.08f * (float)((phase + sub) % 4);
            rgb(0.12f, 0.28f + wave, 0.55f + wave * 0.5f);
            quad(sx, sy, s, s);
            rgb(0.25f, 0.50f, 0.80f);
            {
                int yoff = (phase * 3 + idx) % (s / 2 + 1);
                quad(sx + 2, sy + 4 + yoff, s - 4, 3);
                quad(sx + 4, sy + 14 + (yoff / 2), s - 8, 2);
            }
            rgb(0.40f, 0.65f, 0.90f);
            quad(sx + s / 3, sy + 6 + phase, 4, 2);
        } else {
            /* sand / path */
            rgb(0.62f * shade, 0.54f * shade, 0.32f * shade);
            quad(sx, sy, s, s);
            rgb(0.50f, 0.42f, 0.24f);
            quad(sx + 4, sy + 10, 6, 2);
            quad(sx + 14, sy + 18, 8, 2);
        }
    } else if (page == 1) { /* B — walls / machines / pipes */
        if (sub < 3) {
            /* concrete wall with top lip */
            rgb(0.38f * shade, 0.40f * shade, 0.46f * shade);
            quad(sx, sy, s, s);
            rgb(0.50f, 0.52f, 0.58f);
            quad(sx, sy + s - 5, s, 5);
            rgb(0.22f, 0.22f, 0.26f);
            quad(sx, sy, s, 3);
            /* vertical panel lines */
            linev(sx + s / 3, sy + 4, s - 8, 0.30f, 0.32f, 0.36f);
            linev(sx + 2 * s / 3, sy + 4, s - 8, 0.30f, 0.32f, 0.36f);
        } else if (sub < 5) {
            /* server rack / blue machine (screenshot servers) */
            rgb(0.18f, 0.22f, 0.32f);
            quad(sx, sy, s, s);
            rgb(0.28f, 0.38f, 0.55f);
            quad(sx + 2, sy + 2, s - 4, s - 4);
            /* rack slots */
            {
                int k;
                for (k = 0; k < 4; k++) {
                    int yy = sy + 4 + k * (s / 5);
                    rgb(0.15f, 0.18f, 0.28f);
                    quad(sx + 4, yy, s - 8, s / 6);
                    /* blink LEDs */
                    if (((g_tick / 20) + k + idx) & 1)
                        rgb(0.95f, 0.25f, 0.25f);
                    else
                        rgb(0.30f, 0.80f, 0.35f);
                    quad(sx + s - 10, yy + 2, 4, 3);
                    rgb(0.4f, 0.7f, 0.95f);
                    quad(sx + 6, yy + 2, s / 3, 2);
                }
            }
        } else if (sub < 7) {
            /* industrial pipes (screenshot corridors) */
            rgb(0.28f, 0.30f, 0.33f);
            quad(sx, sy, s, s);
            /* horizontal pipe body */
            rgb(0.62f, 0.64f, 0.68f);
            quad(sx, sy + s / 2 - 5, s, 10);
            rgb(0.75f, 0.76f, 0.80f);
            quad(sx, sy + s / 2 - 3, s, 3);
            /* vertical tee */
            rgb(0.58f, 0.60f, 0.64f);
            quad(sx + s / 2 - 5, sy, 10, s);
            rgb(0.72f, 0.74f, 0.78f);
            quad(sx + s / 2 - 2, sy, 3, s);
            /* joint rings */
            rgb(0.45f, 0.47f, 0.50f);
            quad(sx + s / 2 - 7, sy + s / 2 - 7, 14, 14);
            rgb(0.68f, 0.70f, 0.74f);
            quad(sx + s / 2 - 4, sy + s / 2 - 4, 8, 8);
        } else {
            /* hazard barrier pink/yellow (screenshot rails) */
            rgb(0.15f, 0.15f, 0.16f);
            quad(sx, sy, s, s);
            {
                int k;
                for (k = -s; k < s * 2; k += 7) {
                    rgb(0.95f, 0.35f, 0.55f); /* pink-magenta like screenshot */
                    glBegin(GL_QUADS);
                    glVertex2i(sx + k, sy);
                    glVertex2i(sx + k + 4, sy);
                    glVertex2i(sx + k + 4 + s / 4, sy + s);
                    glVertex2i(sx + k + s / 4, sy + s);
                    glEnd();
                    rgb(0.95f, 0.85f, 0.15f);
                    glBegin(GL_QUADS);
                    glVertex2i(sx + k + 4, sy);
                    glVertex2i(sx + k + 7, sy);
                    glVertex2i(sx + k + 7 + s / 4, sy + s);
                    glVertex2i(sx + k + 4 + s / 4, sy + s);
                    glEnd();
                }
            }
            /* top rail */
            rgb(0.70f, 0.30f, 0.45f);
            quad(sx, sy + s - 4, s, 4);
        }
    } else if (page == 2) { /* C — furniture / props (transparent checker when empty) */
        /* base: semi-transparent style = draw on checker in palette only;
           on map we darken slightly under prop */
        rgb(0.12f, 0.13f, 0.14f);
        quad(sx, sy, s, s);
        if (sub % 4 == 0) {
            /* wooden crate */
            rgb(0.55f, 0.36f, 0.16f);
            quad(sx + 3, sy + 3, s - 6, s - 6);
            rgb(0.38f, 0.24f, 0.10f);
            quad(sx + 3, sy + s / 2 - 1, s - 6, 3);
            linev(sx + s / 2, sy + 3, s - 6, 0.38f, 0.24f, 0.10f);
            rivets(sx + 2, sy + 2, s - 4, 0.8f);
        } else if (sub % 4 == 1) {
            /* console desk */
            rgb(0.22f, 0.26f, 0.34f);
            quad(sx + 2, sy + 6, s - 4, s - 10);
            rgb(0.15f, 0.55f, 0.75f);
            quad(sx + 5, sy + 12, s - 10, 8);
            if ((g_tick / 15 + idx) & 1)
                rgb(0.4f, 0.95f, 1.0f);
            else
                rgb(0.1f, 0.35f, 0.50f);
            quad(sx + 7, sy + 14, s - 14, 4);
            rgb(0.40f, 0.42f, 0.48f);
            quad(sx + 4, sy + 4, s - 8, 4); /* top shelf */
        } else if (sub % 4 == 2) {
            /* plant pot */
            rgb(0.35f, 0.22f, 0.12f);
            quad(sx + s / 2 - 5, sy + 2, 10, 8);
            rgb(0.18f, 0.55f, 0.22f);
            quad(sx + s / 2 - 8, sy + 10, 16, s - 16);
            rgb(0.25f, 0.70f, 0.30f);
            quad(sx + s / 2 - 4, sy + s - 10, 8, 6);
        } else {
            /* column / pillar */
            rgb(0.55f, 0.52f, 0.48f);
            quad(sx + 6, sy + 2, s - 12, s - 4);
            rgb(0.70f, 0.68f, 0.62f);
            quad(sx + 4, sy + s - 8, s - 8, 6);
            quad(sx + 4, sy + 2, s - 8, 5);
        }
    } else if (page == 3) { /* D — doors / lights / special */
        if (sub < 2) {
            /* metal door */
            rgb(0.40f, 0.28f, 0.14f);
            quad(sx + 3, sy, s - 6, s);
            rgb(0.55f, 0.38f, 0.18f);
            quad(sx + 5, sy + 2, s - 10, s - 4);
            rgb(0.75f, 0.65f, 0.25f);
            quad(sx + s - 12, sy + s / 2 - 2, 5, 5);
            linev(sx + s / 2, sy + 4, s - 8, 0.30f, 0.20f, 0.10f);
        } else if (sub < 4) {
            /* blinking light / lamp */
            rgb(0.18f, 0.18f, 0.22f);
            quad(sx, sy, s, s);
            {
                float pulse = ((g_tick / 8 + idx) & 1) ? 1.0f : 0.45f;
                rgb(1.0f * pulse, 0.92f * pulse, 0.40f * pulse);
                quad(sx + 8, sy + 8, s - 16, s - 16);
                rgb(1.0f, 1.0f, 0.85f);
                quad(sx + s / 2 - 2, sy + s / 2 - 2, 4, 4);
            }
        } else if (sub < 6) {
            /* stairs */
            rgb(0.36f, 0.36f, 0.40f);
            quad(sx, sy, s, s);
            {
                int k;
                for (k = 0; k < 4; k++) {
                    rgb(0.50f + k * 0.04f, 0.50f + k * 0.04f, 0.55f);
                    quad(sx, sy + k * (s / 4), s - k * 2, s / 5);
                }
            }
        } else {
            /* purple event marker tile (editor) */
            rgb(0.16f, 0.16f, 0.22f);
            quad(sx, sy, s, s);
            rgb(0.75f, 0.30f, 0.95f);
            /* diamond */
            glBegin(GL_QUADS);
            glVertex2i(sx + s / 2, sy + 4);
            glVertex2i(sx + s - 4, sy + s / 2);
            glVertex2i(sx + s / 2, sy + s - 4);
            glVertex2i(sx + 4, sy + s / 2);
            glEnd();
        }
    } else { /* R — region tints (semi-transparent style solid tint) */
        float t = (idx % 8) / 8.0f;
        float hues[8][3] = {
            {0.55f, 0.25f, 0.25f}, {0.25f, 0.55f, 0.30f},
            {0.25f, 0.35f, 0.65f}, {0.60f, 0.55f, 0.20f},
            {0.55f, 0.25f, 0.55f}, {0.25f, 0.55f, 0.55f},
            {0.60f, 0.40f, 0.20f}, {0.40f, 0.40f, 0.55f}
        };
        int hi = idx % 8;
        rgb(hues[hi][0] * 0.55f, hues[hi][1] * 0.55f, hues[hi][2] * 0.55f);
        quad(sx, sy, s, s);
        rgb(hues[hi][0], hues[hi][1], hues[hi][2]);
        glBegin(GL_LINE_LOOP);
        glVertex2i(sx + 1, sy + 1); glVertex2i(sx + s - 1, sy + 1);
        glVertex2i(sx + s - 1, sy + s - 1); glVertex2i(sx + 1, sy + s - 1);
        glEnd();
        /* region number glyph area */
        rgb(0.9f, 0.9f, 0.95f);
        quad(sx + s / 2 - 3, sy + s / 2 - 3, 6, 6);
        (void)t;
    }
}

void tileset_draw_palette(int page, int ox, int oy, int selected) {
    int i;
    if (page < 0) page = 0;
    if (page >= TS_PAGES) page = TS_PAGES - 1;
    for (i = 0; i < TS_COLS * TS_ROWS; i++) {
        int col = i % TS_COLS;
        int row = i / TS_COLS;
        int sx = ox + col * (TILE_PX + 2);
        int sy = oy - (row + 1) * (TILE_PX + 2);
        unsigned char id = (unsigned char)TILE_ID(page, i);
        /* checkerboard under every cell (MZ transparent look for C/D) */
        tileset_draw_checker(sx, sy, TILE_PX);
        tileset_draw(id, sx, sy, 1);
        if (i == selected) {
            glColor3f(1.0f, 0.92f, 0.15f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2i(sx - 1, sy - 1); glVertex2i(sx + TILE_PX + 1, sy - 1);
            glVertex2i(sx + TILE_PX + 1, sy + TILE_PX + 1); glVertex2i(sx - 1, sy + TILE_PX + 1);
            glEnd();
            glLineWidth(1.0f);
        }
    }
}

void tileset_init(void) { g_tick = 0; }

int tileset_walkable(unsigned char id) {
    int page = TILE_PAGE(id);
    int idx = TILE_IDX(id);
    int sub = idx % 8;
    if (page == 1 && sub < 3) return 0; /* walls */
    if (page == 0 && sub >= 4 && sub < 6) return 0; /* water */
    if (page == 1 && sub >= 7) return 0; /* hazard rail block */
    return 1;
}

const char *tileset_page_name(int page) {
    static const char *n[] = { "A", "B", "C", "D", "R" };
    if (page < 0 || page >= TS_PAGES) return "?";
    return n[page];
}

const char *tileset_tile_hint(unsigned char id) {
    int page = TILE_PAGE(id);
    int sub = TILE_IDX(id) % 8;
    if (page == 0) {
        if (sub < 2) return "grass";
        if (sub < 4) return "metal floor";
        if (sub < 6) return "water";
        return "path";
    }
    if (page == 1) {
        if (sub < 3) return "wall";
        if (sub < 5) return "machine";
        if (sub < 7) return "pipe";
        return "hazard";
    }
    if (page == 2) {
        if (sub % 4 == 0) return "crate";
        if (sub % 4 == 1) return "console";
        if (sub % 4 == 2) return "plant";
        return "pillar";
    }
    if (page == 3) {
        if (sub < 2) return "door";
        if (sub < 4) return "light";
        if (sub < 6) return "stairs";
        return "event tile";
    }
    return "region";
}
