/* render.c — shaded tiles, units, cities, polished dark HUD chrome */
#include "civ.h"

#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOP_H  36
#define BOT_H  78
#define PAD    10

static void draw_text(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)(unsigned char)*s++);
}

static void draw_text9(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, (int)(unsigned char)*s++);
}

static void draw_text18(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, (int)(unsigned char)*s++);
}

static void panel_fill(float x0, float y0, float x1, float y1,
                       float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}

static void panel_chrome(float x0, float y0, float x1, float y1) {
    /* dark body */
    panel_fill(x0, y0, x1, y1, 0.08f, 0.09f, 0.12f, 0.92f);
    /* top highlight */
    glColor3f(0.35f, 0.40f, 0.50f);
    glBegin(GL_LINES);
    glVertex2f(x0 + 1, y0 + 1); glVertex2f(x1 - 1, y0 + 1);
    glVertex2f(x0 + 1, y0 + 1); glVertex2f(x0 + 1, y1 - 1);
    glEnd();
    /* bottom shadow */
    glColor3f(0.02f, 0.02f, 0.03f);
    glBegin(GL_LINES);
    glVertex2f(x0 + 1, y1 - 1); glVertex2f(x1 - 1, y1 - 1);
    glVertex2f(x1 - 1, y0 + 1); glVertex2f(x1 - 1, y1 - 1);
    glEnd();
    /* gold accent line */
    glColor3f(0.72f, 0.58f, 0.22f);
    glBegin(GL_LINES);
    glVertex2f(x0 + 4, y0 + 3); glVertex2f(x1 - 4, y0 + 3);
    glEnd();
}

void render_init(void) {
    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);
}

/* compute tile pixel size and origin for map viewport */
static void map_layout(const Game *g, float *ox, float *oy, float *ts, int *cols, int *rows) {
    float avail_w = (float)g->win_w - 2 * PAD;
    float avail_h = (float)g->win_h - TOP_H - BOT_H - 2 * PAD;
    float tsw, tsh, t;
    int c, r;
    if (avail_w < 40) avail_w = 40;
    if (avail_h < 40) avail_h = 40;
    /* fit as many tiles as reasonable */
    c = (int)(avail_w / 18.0f);
    r = (int)(avail_h / 18.0f);
    if (c < 12) c = 12;
    if (r < 8) r = 8;
    if (c > MAP_W) c = MAP_W;
    if (r > MAP_H) r = MAP_H;
    tsw = avail_w / (float)c;
    tsh = avail_h / (float)r;
    t = tsw < tsh ? tsw : tsh;
    if (t < 10.0f) t = 10.0f;
    if (t > 28.0f) t = 28.0f;
    *ts = t;
    *cols = c;
    *rows = r;
    *ox = PAD + (avail_w - t * (float)c) * 0.5f;
    *oy = TOP_H + PAD + (avail_h - t * (float)r) * 0.5f;
}

static void shade_quad(float x0, float y0, float x1, float y1,
                       float r, float g, float b, float elev) {
    float tl = 1.08f + elev * 0.06f;
    float br = 0.72f - elev * 0.04f;
    float tr = 0.95f;
    float bl = 0.82f;
    glBegin(GL_QUADS);
    glColor3f(r * tl, g * tl, b * tl);
    glVertex2f(x0, y0);
    glColor3f(r * tr, g * tr, b * tr);
    glVertex2f(x1, y0);
    glColor3f(r * br, g * br, b * br);
    glVertex2f(x1, y1);
    glColor3f(r * bl, g * bl, b * bl);
    glVertex2f(x0, y1);
    glEnd();
}

static void draw_tile_detail(float x0, float y0, float ts, int terrain, float elev) {
    float m = ts * 0.15f;
    if (terrain == T_FOREST) {
        glColor3f(0.08f, 0.28f, 0.10f);
        glBegin(GL_TRIANGLES);
        glVertex2f(x0 + ts * 0.5f, y0 + m);
        glVertex2f(x0 + m, y0 + ts - m);
        glVertex2f(x0 + ts - m, y0 + ts - m);
        glEnd();
    } else if (terrain == T_HILLS || terrain == T_MOUNTAIN) {
        float peak = (terrain == T_MOUNTAIN) ? 0.12f : 0.28f;
        glColor3f(0.35f + elev * 0.1f, 0.35f, 0.32f);
        glBegin(GL_TRIANGLES);
        glVertex2f(x0 + ts * 0.5f, y0 + ts * peak);
        glVertex2f(x0 + m, y0 + ts - m);
        glVertex2f(x0 + ts - m, y0 + ts - m);
        glEnd();
        if (terrain == T_MOUNTAIN) {
            glColor3f(0.92f, 0.94f, 0.98f);
            glBegin(GL_TRIANGLES);
            glVertex2f(x0 + ts * 0.5f, y0 + ts * peak);
            glVertex2f(x0 + ts * 0.38f, y0 + ts * 0.38f);
            glVertex2f(x0 + ts * 0.62f, y0 + ts * 0.38f);
            glEnd();
        }
    } else if (terrain == T_SPECIAL) {
        glColor3f(1.0f, 0.9f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(x0 + ts * 0.35f, y0 + ts * 0.35f);
        glVertex2f(x0 + ts * 0.65f, y0 + ts * 0.35f);
        glVertex2f(x0 + ts * 0.65f, y0 + ts * 0.65f);
        glVertex2f(x0 + ts * 0.35f, y0 + ts * 0.65f);
        glEnd();
    } else if (terrain == T_OCEAN) {
        /* wave line */
        glColor4f(0.4f, 0.65f, 0.95f, 0.35f);
        glBegin(GL_LINES);
        glVertex2f(x0 + m, y0 + ts * 0.45f);
        glVertex2f(x0 + ts - m, y0 + ts * 0.55f);
        glVertex2f(x0 + m, y0 + ts * 0.65f);
        glVertex2f(x0 + ts - m, y0 + ts * 0.70f);
        glEnd();
    }
}

static void draw_unit_glyph(float cx, float cy, float ts, const Unit *u, const Civ *civ, int selected) {
    float s = ts * 0.32f;
    float r = civ->color[0], g = civ->color[1], b = civ->color[2];
    const char *ch = "?";
    if (u->kind == U_SETTLER) ch = "S";
    else if (u->kind == U_WARRIOR) ch = "W";
    else if (u->kind == U_SCOUT) ch = "C";

    if (selected) {
        glColor3f(1.0f, 1.0f, 0.3f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(cx - s - 2, cy - s - 2);
        glVertex2f(cx + s + 2, cy - s - 2);
        glVertex2f(cx + s + 2, cy + s + 2);
        glVertex2f(cx - s - 2, cy + s + 2);
        glEnd();
    }
    /* body */
    glColor3f(r * 0.9f, g * 0.9f, b * 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(cx - s, cy - s);
    glVertex2f(cx + s, cy - s);
    glVertex2f(cx + s, cy + s);
    glVertex2f(cx - s, cy + s);
    glEnd();
    /* border */
    glColor3f(0.05f, 0.05f, 0.08f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - s, cy - s);
    glVertex2f(cx + s, cy - s);
    glVertex2f(cx + s, cy + s);
    glVertex2f(cx - s, cy + s);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text9(cx - 3.5f, cy + 4.0f, ch);
    /* HP bar */
    if (u->hp < u->max_hp) {
        float w = s * 2.0f;
        float pct = (float)u->hp / (float)u->max_hp;
        glColor3f(0.1f, 0.1f, 0.1f);
        glBegin(GL_QUADS);
        glVertex2f(cx - s, cy + s + 1);
        glVertex2f(cx + s, cy + s + 1);
        glVertex2f(cx + s, cy + s + 3);
        glVertex2f(cx - s, cy + s + 3);
        glEnd();
        glColor3f(0.2f, 0.85f, 0.25f);
        glBegin(GL_QUADS);
        glVertex2f(cx - s, cy + s + 1);
        glVertex2f(cx - s + w * pct, cy + s + 1);
        glVertex2f(cx - s + w * pct, cy + s + 3);
        glVertex2f(cx - s, cy + s + 3);
        glEnd();
    }
}

static void draw_city_marker(float cx, float cy, float ts, const City *city, const Civ *civ) {
    float s = ts * 0.38f;
    char buf[8];
    glColor3f(civ->color[0], civ->color[1], civ->color[2]);
    glBegin(GL_QUADS);
    glVertex2f(cx - s, cy - s);
    glVertex2f(cx + s, cy - s);
    glVertex2f(cx + s, cy + s);
    glVertex2f(cx - s, cy + s);
    glEnd();
    glColor3f(0.95f, 0.9f, 0.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - s, cy - s);
    glVertex2f(cx + s, cy - s);
    glVertex2f(cx + s, cy + s);
    glVertex2f(cx - s, cy + s);
    glEnd();
    /* roof */
    glColor3f(civ->color[0] * 0.7f, civ->color[1] * 0.7f, civ->color[2] * 0.7f);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx, cy - s - ts * 0.15f);
    glVertex2f(cx - s, cy - s);
    glVertex2f(cx + s, cy - s);
    glEnd();
    snprintf(buf, sizeof(buf), "%d", city->pop);
    glColor3f(1, 1, 1);
    draw_text9(cx - 3.0f, cy + 4.0f, buf);
}

static void draw_minimap(const Game *g, float x0, float y0, float w, float h) {
    float tw = w / (float)MAP_W;
    float th = h / (float)MAP_H;
    int x, y, i;
    uint8_t pbit = 1u;
    panel_chrome(x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            float r, gc, b;
            float px = x0 + x * tw;
            float py = y0 + y * th;
            if (!(g->tiles[y][x].explored & pbit)) {
                glColor3f(0.04f, 0.04f, 0.06f);
            } else {
                map_tile_color(g, x, y, &r, &gc, &b);
                glColor3f(r, gc, b);
            }
            glBegin(GL_QUADS);
            glVertex2f(px, py);
            glVertex2f(px + tw, py);
            glVertex2f(px + tw, py + th);
            glVertex2f(px, py + th);
            glEnd();
        }
    }
    /* cities */
    for (i = 0; i < MAX_CITIES; i++) {
        if (!g->cities[i].used) continue;
        if (!(g->tiles[g->cities[i].y][g->cities[i].x].explored & pbit)) continue;
        {
            const Civ *cv = &g->civs[g->cities[i].civ];
            float px = x0 + g->cities[i].x * tw + tw * 0.2f;
            float py = y0 + g->cities[i].y * th + th * 0.2f;
            glColor3f(cv->color[0], cv->color[1], cv->color[2]);
            glBegin(GL_QUADS);
            glVertex2f(px, py);
            glVertex2f(px + tw * 0.6f, py);
            glVertex2f(px + tw * 0.6f, py + th * 0.6f);
            glVertex2f(px, py + th * 0.6f);
            glEnd();
        }
    }
    /* camera rect */
    {
        float ox, oy, ts;
        int cols, rows;
        map_layout(g, &ox, &oy, &ts, &cols, &rows);
        (void)ox; (void)oy; (void)ts;
        glColor3f(1, 1, 0.4f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x0 + g->cam_x * tw, y0 + g->cam_y * th);
        glVertex2f(x0 + (g->cam_x + cols) * tw, y0 + g->cam_y * th);
        glVertex2f(x0 + (g->cam_x + cols) * tw, y0 + (g->cam_y + rows) * th);
        glVertex2f(x0 + g->cam_x * tw, y0 + (g->cam_y + rows) * th);
        glEnd();
    }
}

void render_frame(const Game *g, float fps) {
    float ox, oy, ts;
    int cols, rows, vx, vy, i;
    char buf[128];
    char yearbuf[32];
    uint8_t pbit = 1u;
    int mm_w = 120, mm_h = 90;

    glViewport(0, 0, g->win_w, g->win_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g->win_w, g->win_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);

    map_layout(g, &ox, &oy, &ts, &cols, &rows);

    /* map backdrop */
    panel_fill(ox - 3, oy - 3, ox + cols * ts + 3, oy + rows * ts + 3,
               0.04f, 0.05f, 0.07f, 1.0f);

    for (vy = 0; vy < rows; vy++) {
        for (vx = 0; vx < cols; vx++) {
            int tx = map_wrap_x(g->cam_x + vx);
            int ty = g->cam_y + vy;
            float x0, y0, r, gc, b;
            if (ty < 0 || ty >= MAP_H) continue;
            x0 = ox + vx * ts;
            y0 = oy + vy * ts;
            if (!(g->tiles[ty][tx].explored & pbit)) {
                glColor3f(0.06f, 0.06f, 0.09f);
                glBegin(GL_QUADS);
                glVertex2f(x0, y0);
                glVertex2f(x0 + ts, y0);
                glVertex2f(x0 + ts, y0 + ts);
                glVertex2f(x0, y0 + ts);
                glEnd();
                /* subtle fog noise */
                glColor3f(0.09f, 0.09f, 0.12f);
                glBegin(GL_LINES);
                glVertex2f(x0 + 2, y0 + ts * 0.5f);
                glVertex2f(x0 + ts - 2, y0 + ts * 0.5f);
                glEnd();
                continue;
            }
            map_tile_color(g, tx, ty, &r, &gc, &b);
            shade_quad(x0, y0, x0 + ts, y0 + ts, r, gc, b, (float)g->tiles[ty][tx].elev);
            draw_tile_detail(x0, y0, ts, g->tiles[ty][tx].terrain, (float)g->tiles[ty][tx].elev);
            /* grid */
            glColor4f(0, 0, 0, 0.18f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x0, y0);
            glVertex2f(x0 + ts, y0);
            glVertex2f(x0 + ts, y0 + ts);
            glVertex2f(x0, y0 + ts);
            glEnd();
        }
    }

    /* cities then units */
    for (i = 0; i < MAX_CITIES; i++) {
        int vx2, vy2;
        float cx, cy;
        if (!g->cities[i].used) continue;
        if (!(g->tiles[g->cities[i].y][g->cities[i].x].explored & pbit)) continue;
        vx2 = g->cities[i].x - g->cam_x;
        /* wrap relative cam */
        if (vx2 < -MAP_W / 2) vx2 += MAP_W;
        if (vx2 > MAP_W / 2) vx2 -= MAP_W;
        /* recompute with wrap: find visible slot */
        {
            int found = 0;
            for (vx = 0; vx < cols; vx++) {
                if (map_wrap_x(g->cam_x + vx) == g->cities[i].x) {
                    vx2 = vx; found = 1; break;
                }
            }
            if (!found) continue;
        }
        vy2 = g->cities[i].y - g->cam_y;
        if (vy2 < 0 || vy2 >= rows) continue;
        cx = ox + (vx2 + 0.5f) * ts;
        cy = oy + (vy2 + 0.5f) * ts;
        draw_city_marker(cx, cy, ts, &g->cities[i], &g->civs[g->cities[i].civ]);
    }

    for (i = 0; i < MAX_UNITS; i++) {
        int vx2 = -1, vy2;
        float cx, cy;
        if (!g->units[i].used) continue;
        if (!(g->tiles[g->units[i].y][g->units[i].x].explored & pbit)) continue;
        for (vx = 0; vx < cols; vx++) {
            if (map_wrap_x(g->cam_x + vx) == g->units[i].x) {
                vx2 = vx; break;
            }
        }
        if (vx2 < 0) continue;
        vy2 = g->units[i].y - g->cam_y;
        if (vy2 < 0 || vy2 >= rows) continue;
        cx = ox + (vx2 + 0.5f) * ts;
        cy = oy + (vy2 + 0.55f) * ts;
        draw_unit_glyph(cx, cy, ts, &g->units[i], &g->civs[g->units[i].civ],
                        i == g->sel_unit);
    }

    /* hover highlight */
    if (g->hover_x >= 0 && g->hover_y >= 0) {
        for (vx = 0; vx < cols; vx++) {
            if (map_wrap_x(g->cam_x + vx) == g->hover_x) {
                int hy = g->hover_y - g->cam_y;
                if (hy >= 0 && hy < rows) {
                    float x0 = ox + vx * ts;
                    float y0 = oy + hy * ts;
                    glColor4f(1, 1, 1, 0.35f);
                    glBegin(GL_LINE_LOOP);
                    glVertex2f(x0 + 1, y0 + 1);
                    glVertex2f(x0 + ts - 1, y0 + 1);
                    glVertex2f(x0 + ts - 1, y0 + ts - 1);
                    glVertex2f(x0 + 1, y0 + ts - 1);
                    glEnd();
                }
                break;
            }
        }
    }

    /* ---- HUD top ---- */
    panel_chrome(0, 0, (float)g->win_w, (float)TOP_H);
    if (g->year < 0)
        snprintf(yearbuf, sizeof(yearbuf), "%d BC", -g->year);
    else
        snprintf(yearbuf, sizeof(yearbuf), "%d AD", g->year);

    glColor3f(0.95f, 0.88f, 0.55f);
    snprintf(buf, sizeof(buf), "SNES-CIV  |  %s", g->civs[0].name);
    draw_text(12, 22, buf);

    glColor3f(0.85f, 0.90f, 1.0f);
    snprintf(buf, sizeof(buf), "%s   Turn %d", yearbuf, g->turn + 1);
    draw_text((float)g->win_w * 0.38f, 22, buf);

    glColor3f(1.0f, 0.85f, 0.30f);
    snprintf(buf, sizeof(buf), "Gold %d", g->civs[0].gold);
    draw_text((float)g->win_w * 0.58f, 22, buf);

    glColor3f(0.55f, 0.85f, 1.0f);
    snprintf(buf, sizeof(buf), "Sci %d", g->civs[0].science);
    draw_text((float)g->win_w * 0.70f, 22, buf);

    glColor3f(0.55f, 0.55f, 0.60f);
    snprintf(buf, sizeof(buf), "%.0f fps", fps);
    draw_text((float)g->win_w - 70.0f, 22, buf);

    /* ---- HUD bottom ---- */
    panel_chrome(0, (float)(g->win_h - BOT_H), (float)g->win_w, (float)g->win_h);

    /* status message */
    glColor3f(0.90f, 0.92f, 0.85f);
    draw_text(12, (float)(g->win_h - BOT_H + 20), g->msg);

    /* selection detail */
    if (g->sel_unit >= 0 && g->units[g->sel_unit].used) {
        const Unit *u = &g->units[g->sel_unit];
        glColor3f(0.70f, 0.85f, 1.0f);
        snprintf(buf, sizeof(buf),
                 "Unit: %s  HP %d/%d  MP %d  @(%d,%d)  [%s]",
                 unit_kind_name(u->kind), u->hp, u->max_hp, u->moves_left,
                 u->x, u->y, terrain_name(g->tiles[u->y][u->x].terrain));
        draw_text(12, (float)(g->win_h - BOT_H + 42), buf);
    } else if (g->sel_city >= 0 && g->cities[g->sel_city].used) {
        const City *c = &g->cities[g->sel_city];
        int food = 0, sh = 0, gold = 0;
        /* recompute yields inline for HUD */
        {
            int dx, dy;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    int x = map_wrap_x(c->x + dx), y = c->y + dy;
                    uint8_t t;
                    if (y < 0 || y >= MAP_H) continue;
                    t = g->tiles[y][x].terrain;
                    if (t == T_OCEAN) continue;
                    if (t == T_PLAINS) { food += 2; sh += 1; }
                    else if (t == T_SPECIAL) { food += 3; sh += 2; }
                    else if (t == T_FOREST) { food += 1; sh += 2; }
                    else if (t == T_HILLS) { food += 1; sh += 2; }
                    else if (t == T_MOUNTAIN) { sh += 1; }
                }
            food = food / 2 + c->pop;
            sh = sh / 3 + 1;
            gold = c->pop;
        }
        glColor3f(0.95f, 0.85f, 0.55f);
        snprintf(buf, sizeof(buf),
                 "City: %s  Pop %d  Food %d/%d  Shields %d  Prod: %s  (+%df +%ds +%dg)",
                 c->name, c->pop, c->food_store, c->food_needed, c->shields,
                 prod_kind_name(c->prod), food, sh, gold);
        draw_text(12, (float)(g->win_h - BOT_H + 42), buf);
    } else {
        glColor3f(0.55f, 0.58f, 0.62f);
        draw_text(12, (float)(g->win_h - BOT_H + 42),
                  "Select a unit (click / N)  |  Arrows move  |  B found city  |  Space end turn");
    }

    glColor3f(0.45f, 0.48f, 0.55f);
    draw_text9(12, (float)(g->win_h - 14),
               "Keys: Arrows/click move  N next unit  B build city  [ ] prod  WASD camera  Space end turn  Q quit");

    /* minimap bottom-right */
    draw_minimap(g, (float)(g->win_w - mm_w - 14),
                 (float)(g->win_h - BOT_H - mm_h - 8),
                 (float)mm_w, (float)mm_h);

    /* civ legend top-right strip */
    {
        float lx = (float)g->win_w - 200.0f;
        float ly = (float)TOP_H + 8;
        panel_chrome(lx, ly, (float)g->win_w - 8, ly + 18.0f * g->n_civs + 10);
        for (i = 0; i < g->n_civs; i++) {
            float y = ly + 8 + i * 18;
            glColor3f(g->civs[i].color[0], g->civs[i].color[1], g->civs[i].color[2]);
            glBegin(GL_QUADS);
            glVertex2f(lx + 8, y);
            glVertex2f(lx + 20, y);
            glVertex2f(lx + 20, y + 12);
            glVertex2f(lx + 8, y + 12);
            glEnd();
            if (!g->civs[i].alive)
                glColor3f(0.4f, 0.4f, 0.4f);
            else
                glColor3f(0.9f, 0.9f, 0.92f);
            snprintf(buf, sizeof(buf), "%s%s", g->civs[i].name,
                     i == 0 ? " (you)" : (g->civs[i].alive ? "" : " X"));
            draw_text9(lx + 26, y + 10, buf);
        }
    }

    if (g->game_over) {
        panel_fill(0, 0, (float)g->win_w, (float)g->win_h, 0, 0, 0, 0.45f);
        glColor3f(1.0f, 0.9f, 0.3f);
        if (g->game_over > 0)
            draw_text18((float)g->win_w * 0.35f, (float)g->win_h * 0.45f, "VICTORY");
        else
            draw_text18((float)g->win_w * 0.38f, (float)g->win_h * 0.45f, "DEFEAT");
        glColor3f(0.85f, 0.85f, 0.85f);
        draw_text((float)g->win_w * 0.32f, (float)g->win_h * 0.52f, "Press Q to quit");
    }

    glutSwapBuffers();
}

/* export layout for main click mapping */
void render_map_layout(const Game *g, float *ox, float *oy, float *ts, int *cols, int *rows) {
    map_layout(g, ox, oy, ts, cols, rows);
}
