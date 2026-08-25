/* render.c — face-lit voxels + polished HUD */
#include "render.h"

#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* render distance in blocks — keep modest so 60fps timer does not melt a core
 * (immediate-mode quads; no mesh cache yet). */
#define RENDER_RADIUS 28

void render_init(void) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.45f, 0.62f, 0.90f, 1.0f);
    glShadeModel(GL_FLAT);
}

static void draw_quad(float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float x2, float y2, float z2,
                      float x3, float y3, float z3,
                      float r, float g, float b, float shade) {
    glColor3f(r * shade, g * shade, b * shade);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x2, y2, z2);
    glVertex3f(x3, y3, z3);
    glEnd();
}

/* Faces: +Y top, -Y bottom, +X, -X, +Z, -Z with classic MC-like shades */
static void draw_block(int x, int y, int z, uint8_t id,
                       int exp_top, int exp_bot, int exp_px, int exp_nx,
                       int exp_pz, int exp_nz) {
    float r, g, b;
    float X = (float)x, Y = (float)y, Z = (float)z;
    block_color(id, &r, &g, &b);

    /* grass: top greener, sides dirt-tinted already via color */
    if (id == BLK_GRASS) {
        /* top grass, sides slightly dirtier */
        if (exp_top)
            draw_quad(X, Y+1, Z,  X+1, Y+1, Z,  X+1, Y+1, Z+1,  X, Y+1, Z+1,
                      0.30f, 0.75f, 0.28f, 1.00f);
        if (exp_bot)
            draw_quad(X, Y, Z+1,  X+1, Y, Z+1,  X+1, Y, Z,  X, Y, Z,
                      0.45f, 0.30f, 0.16f, 0.55f);
        if (exp_px)
            draw_quad(X+1, Y, Z,  X+1, Y, Z+1,  X+1, Y+1, Z+1,  X+1, Y+1, Z,
                      0.40f, 0.55f, 0.22f, 0.80f);
        if (exp_nx)
            draw_quad(X, Y, Z+1,  X, Y, Z,  X, Y+1, Z,  X, Y+1, Z+1,
                      0.40f, 0.55f, 0.22f, 0.80f);
        if (exp_pz)
            draw_quad(X+1, Y, Z+1,  X, Y, Z+1,  X, Y+1, Z+1,  X+1, Y+1, Z+1,
                      0.40f, 0.55f, 0.22f, 0.72f);
        if (exp_nz)
            draw_quad(X, Y, Z,  X+1, Y, Z,  X+1, Y+1, Z,  X, Y+1, Z,
                      0.40f, 0.55f, 0.22f, 0.72f);
        return;
    }

    if (exp_top)
        draw_quad(X, Y+1, Z,  X+1, Y+1, Z,  X+1, Y+1, Z+1,  X, Y+1, Z+1, r, g, b, 1.00f);
    if (exp_bot)
        draw_quad(X, Y, Z+1,  X+1, Y, Z+1,  X+1, Y, Z,  X, Y, Z, r, g, b, 0.50f);
    if (exp_px)
        draw_quad(X+1, Y, Z,  X+1, Y, Z+1,  X+1, Y+1, Z+1,  X+1, Y+1, Z, r, g, b, 0.80f);
    if (exp_nx)
        draw_quad(X, Y, Z+1,  X, Y, Z,  X, Y+1, Z,  X, Y+1, Z+1, r, g, b, 0.80f);
    if (exp_pz)
        draw_quad(X+1, Y, Z+1,  X, Y, Z+1,  X, Y+1, Z+1,  X+1, Y+1, Z+1, r, g, b, 0.70f);
    if (exp_nz)
        draw_quad(X, Y, Z,  X+1, Y, Z,  X+1, Y+1, Z,  X, Y+1, Z, r, g, b, 0.70f);
}

static int is_opaque(uint8_t id) {
    return id != BLK_AIR;
}

void render_world(const World *w, const Player *p) {
    int cx = (int)floorf(p->x);
    int cy = (int)floorf(p->y);
    int cz = (int)floorf(p->z);
    int x0 = cx - RENDER_RADIUS;
    int x1 = cx + RENDER_RADIUS;
    int y0 = cy - RENDER_RADIUS;
    int y1 = cy + RENDER_RADIUS;
    int z0 = cz - RENDER_RADIUS;
    int z1 = cz + RENDER_RADIUS;
    int x, y, z;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 >= WORLD_W) x1 = WORLD_W - 1;
    if (y1 >= WORLD_H) y1 = WORLD_H - 1;
    if (z1 >= WORLD_D) z1 = WORLD_D - 1;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    {
        float aspect = (p->win_h > 0) ? (float)p->win_w / (float)p->win_h : 1.777f;
        gluPerspective(70.0, aspect, 0.05, 300.0);
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    /* camera: look from eye with yaw/pitch */
    glRotatef(-p->pitch, 1, 0, 0);
    glRotatef(-p->yaw, 0, 1, 0);
    glTranslatef(-p->x, -p->y, -p->z);

    /* sky-ish clear already; slight fog */
    {
        GLfloat fogColor[4] = {0.55f, 0.70f, 0.92f, 1.0f};
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fogColor);
        glFogf(GL_FOG_START, (float)(RENDER_RADIUS - 18));
        glFogf(GL_FOG_END, (float)(RENDER_RADIUS + 2));
        glHint(GL_FOG_HINT, GL_NICEST);
    }

    for (z = z0; z <= z1; z++) {
        for (y = y0; y <= y1; y++) {
            for (x = x0; x <= x1; x++) {
                uint8_t id = world_get(w, x, y, z);
                int exp_top, exp_bot, exp_px, exp_nx, exp_pz, exp_nz;
                if (!is_opaque(id)) continue;
                /* skip fully buried */
                exp_top = !is_opaque(world_get(w, x, y + 1, z));
                exp_bot = !is_opaque(world_get(w, x, y - 1, z));
                exp_px  = !is_opaque(world_get(w, x + 1, y, z));
                exp_nx  = !is_opaque(world_get(w, x - 1, y, z));
                exp_pz  = !is_opaque(world_get(w, x, y, z + 1));
                exp_nz  = !is_opaque(world_get(w, x, y, z - 1));
                if (!(exp_top | exp_bot | exp_px | exp_nx | exp_pz | exp_nz))
                    continue;
                draw_block(x, y, z, id, exp_top, exp_bot, exp_px, exp_nx, exp_pz, exp_nz);
            }
        }
    }

    /* target wireframe highlight */
    if (p->has_target) {
        float X = (float)p->tx, Y = (float)p->ty, Z = (float)p->tz;
        glDisable(GL_FOG);
        glLineWidth(2.0f);
        glColor3f(0.05f, 0.05f, 0.05f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(X, Y, Z); glVertex3f(X+1, Y, Z); glVertex3f(X+1, Y, Z+1); glVertex3f(X, Y, Z+1);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex3f(X, Y+1, Z); glVertex3f(X+1, Y+1, Z); glVertex3f(X+1, Y+1, Z+1); glVertex3f(X, Y+1, Z+1);
        glEnd();
        glBegin(GL_LINES);
        glVertex3f(X, Y, Z); glVertex3f(X, Y+1, Z);
        glVertex3f(X+1, Y, Z); glVertex3f(X+1, Y+1, Z);
        glVertex3f(X+1, Y, Z+1); glVertex3f(X+1, Y+1, Z+1);
        glVertex3f(X, Y, Z+1); glVertex3f(X, Y+1, Z+1);
        glEnd();
        glEnable(GL_FOG);
    }

    glDisable(GL_FOG);
}

static void ortho_begin(int w, int h) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

static void ortho_end(void) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

static void draw_text(int x, int y, const char *s) {
    glRasterPos2i(x, y);
    while (*s) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, (int)(unsigned char)*s);
        s++;
    }
}

static void draw_text_small(int x, int y, const char *s) {
    glRasterPos2i(x, y);
    while (*s) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)(unsigned char)*s);
        s++;
    }
}

static void fill_rect(int x, int y, int w, int h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

static void stroke_rect(int x, int y, int w, int h, float r, float g, float b) {
    glColor3f(r, g, b);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

void render_hud(const Player *p, const Inventory *inv, float fps, const char *status) {
    int w = p->win_w;
    int h = p->win_h;
    char buf[256];
    int i;
    int bar_w = 9 * 44 + 16;
    int bar_h = 52;
    int bar_x = (w - bar_w) / 2;
    int bar_y = 16;
    int cx = w / 2;
    int cy = h / 2;

    ortho_begin(w, h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* top-left info panel */
    fill_rect(8, h - 92, 320, 84, 0.05f, 0.07f, 0.12f, 0.55f);
    stroke_rect(8, h - 92, 320, 84, 0.35f, 0.45f, 0.65f);
    glColor3f(0.95f, 0.96f, 0.98f);
    snprintf(buf, sizeof(buf), "FPS: %.0f", fps);
    draw_text(16, h - 28, buf);
    snprintf(buf, sizeof(buf), "Pos: %.1f  %.1f  %.1f", p->x, p->y, p->z);
    draw_text(16, h - 48, buf);
    snprintf(buf, sizeof(buf), "Fly: %s  |  %s",
             p->flying ? "ON (F)" : "OFF (F)",
             p->mouse_captured ? "mouse captured" : "click to capture");
    draw_text_small(16, h - 68, buf);
    if (status && status[0]) {
        glColor3f(0.85f, 0.95f, 0.55f);
        draw_text_small(16, h - 84, status);
    }

    /* crosshair */
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2i(cx - 10, cy); glVertex2i(cx + 10, cy);
    glVertex2i(cx, cy - 10); glVertex2i(cx, cy + 10);
    glEnd();
    /* subtle dark outline */
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2i(cx - 11, cy - 1); glVertex2i(cx + 11, cy - 1);
    glVertex2i(cx - 1, cy - 11); glVertex2i(cx - 1, cy + 11);
    glEnd();

    /* hotbar chrome */
    fill_rect(bar_x, bar_y, bar_w, bar_h, 0.08f, 0.09f, 0.12f, 0.75f);
    stroke_rect(bar_x, bar_y, bar_w, bar_h, 0.55f, 0.58f, 0.65f);

    for (i = 0; i < HOTBAR_SLOTS; i++) {
        int sx = bar_x + 8 + i * 44;
        int sy = bar_y + 8;
        int sw = 36, sh = 36;
        float r, g, b;
        uint8_t id = inv->slots[i];
        int selected = (i == inv->selected);

        fill_rect(sx, sy, sw, sh, 0.18f, 0.19f, 0.22f, 0.95f);
        if (id != BLK_AIR) {
            block_color(id, &r, &g, &b);
            fill_rect(sx + 4, sy + 4, sw - 8, sh - 8, r, g, b, 1.0f);
            /* top face highlight */
            fill_rect(sx + 4, sy + sh - 12, sw - 8, 4, r * 1.15f > 1 ? 1 : r * 1.15f,
                      g * 1.15f > 1 ? 1 : g * 1.15f, b * 1.15f > 1 ? 1 : b * 1.15f, 0.9f);
        }
        if (selected)
            stroke_rect(sx - 1, sy - 1, sw + 2, sh + 2, 1.0f, 0.92f, 0.25f);
        else
            stroke_rect(sx, sy, sw, sh, 0.40f, 0.42f, 0.48f);

        glColor3f(0.95f, 0.95f, 0.98f);
        {
            char n[4];
            snprintf(n, sizeof(n), "%d", i + 1);
            draw_text_small(sx + 2, sy + sh - 12, n);
        }
    }

    /* selected block name under hotbar */
    {
        uint8_t id = inv_selected_block(inv);
        glColor3f(0.9f, 0.92f, 0.95f);
        snprintf(buf, sizeof(buf), "[%s]", block_name(id));
        draw_text_small(bar_x + bar_w / 2 - 30, bar_y + bar_h + 6, buf);
    }

    /* help strip */
    fill_rect(8, 8, w > 640 ? 520 : w - 16, 22, 0.05f, 0.06f, 0.10f, 0.45f);
    glColor3f(0.75f, 0.78f, 0.85f);
    draw_text_small(14, 14, "WASD move | mouse look | LMB break RMB place | F fly | Space jump/up | Ctrl+S save | Ctrl+L load | Q quit");

    glDisable(GL_BLEND);
    ortho_end();
}
