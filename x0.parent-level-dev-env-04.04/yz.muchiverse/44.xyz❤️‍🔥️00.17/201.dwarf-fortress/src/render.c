/* render.c — top-down colored tiles + polished side HUD */
#include "render.h"
#include "map.h"
#include "unit.h"
#include "job.h"
#include <GL/glut.h>
#include <stdio.h>
#include <string.h>

void render_init(void) {
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glDisable(GL_DEPTH_TEST);
}

static void draw_rect(float x, float y, float w, float h, float r, float g,
                      float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void draw_box(float x, float y, float w, float h, float r, float g,
                     float b) {
    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + 0.5f, y + 0.5f);
    glVertex2f(x + w - 0.5f, y + 0.5f);
    glVertex2f(x + w - 0.5f, y + h - 0.5f);
    glVertex2f(x + 0.5f, y + h - 0.5f);
    glEnd();
}

static void text_at(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, (unsigned char)*s++);
}

static void text_at9(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (unsigned char)*s++);
}

static const char *season_name(int s) {
    static const char *n[] = { "Spring", "Summer", "Autumn", "Winter" };
    if (s < 0 || s > 3) return "?";
    return n[s];
}

static const char *mode_name(int m) {
    switch (m) {
    case MODE_LOOK: return "LOOK";
    case MODE_DIG: return "DIG [d]";
    case MODE_CUT: return "CUT TREE [t]";
    case MODE_STOCK_WOOD: return "STOCK WOOD";
    case MODE_STOCK_STONE: return "STOCK STONE";
    case MODE_BUILD_WALL: return "BUILD WALL";
    case MODE_BUILD_WS: return "BUILD WORKSHOP";
    case MODE_QUERY: return "QUERY [q]";
    case MODE_BUILD_MENU: return "BUILD MENU [b]";
    default: return "?";
    }
}

static void glyph_at_tile(float px, float py, float ts, char ch, float r,
                          float g, float b) {
    /* center bitmap roughly in tile */
    float ox = px + ts * 0.28f;
    float oy = py + ts * 0.22f;
    char buf[2] = { ch, 0 };
    glColor3f(r, g, b);
    /* use slightly brighter glyph */
    text_at(ox, oy, buf);
}

void render_frame(const Fort *f, float fps) {
    int win_w = f->win_w > 0 ? f->win_w : 960;
    int win_h = f->win_h > 0 ? f->win_h : 640;
    int map_px_w = win_w - PANEL_W;
    int ts = TILE_PX;
    int view_tw, view_th;
    int x, y, i;
    char buf[128];

    if (map_px_w < 100) map_px_w = 100;

    /* dynamic tile size to fit */
    view_tw = map_px_w / ts;
    view_th = win_h / ts;
    if (view_tw < 8) view_tw = 8;
    if (view_th < 8) view_th = 8;

    glViewport(0, 0, win_w, win_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, win_w, 0, win_h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* map background */
    draw_rect(0, 0, (float)map_px_w, (float)win_h, 0.06f, 0.07f, 0.09f, 1.0f);

    for (y = 0; y < view_th; y++) {
        for (x = 0; x < view_tw; x++) {
            int mx = f->cam_x + x;
            int my = f->cam_y + y;
            float px = (float)(x * ts);
            float py = (float)(win_h - (y + 1) * ts);
            float r, g, b;
            char glyph;
            if (!map_in_bounds(mx, my)) {
                draw_rect(px, py, (float)ts - 1, (float)ts - 1, 0.04f, 0.04f,
                          0.05f, 1.0f);
                continue;
            }
            map_tile_color(f, mx, my, &r, &g, &b);
            draw_rect(px, py, (float)ts - 1, (float)ts - 1, r, g, b, 1.0f);

            /* designation overlay */
            {
                uint8_t d = f->tiles[my][mx].desig;
                if (d == DG_DIG)
                    draw_rect(px, py, (float)ts - 1, (float)ts - 1, 1.0f, 0.85f,
                              0.1f, 0.28f);
                else if (d == DG_CUT)
                    draw_rect(px, py, (float)ts - 1, (float)ts - 1, 0.2f, 1.0f,
                              0.3f, 0.28f);
                else if (d == DG_BUILD_WALL || d == DG_BUILD_WORKSHOP)
                    draw_rect(px, py, (float)ts - 1, (float)ts - 1, 0.9f, 0.5f,
                              0.2f, 0.30f);
            }

            glyph = map_tile_glyph(f, mx, my);
            if (f->tiles[my][mx].terrain == TR_ROCK)
                glyph = '#';
            if (f->tiles[my][mx].terrain == TR_TREE)
                glyph = 'T';
            if (glyph != '.' && glyph != ' ') {
                float gr = r * 0.5f + 0.5f, gg = g * 0.5f + 0.5f,
                      gb = b * 0.5f + 0.5f;
                if (f->tiles[my][mx].terrain == TR_TREE) {
                    gr = 0.3f;
                    gg = 0.95f;
                    gb = 0.35f;
                }
                if (f->tiles[my][mx].terrain == TR_WATER) {
                    gr = 0.6f;
                    gg = 0.85f;
                    gb = 1.0f;
                }
                glyph_at_tile(px, py, (float)ts, glyph, gr, gg, gb);
            }
        }
    }

    /* items */
    for (i = 0; i < MAX_ITEMS; i++) {
        int mx, my, vx, vy;
        float px, py;
        char gch = '*';
        if (!f->items[i].used) continue;
        mx = f->items[i].x;
        my = f->items[i].y;
        vx = mx - f->cam_x;
        vy = my - f->cam_y;
        if (vx < 0 || vy < 0 || vx >= view_tw || vy >= view_th) continue;
        px = (float)(vx * ts);
        py = (float)(win_h - (vy + 1) * ts);
        switch (f->items[i].kind) {
        case IT_WOOD: gch = '='; break;
        case IT_STONE: gch = '*'; break;
        case IT_BED: gch = 'B'; break;
        case IT_CHAIR: gch = 'h'; break;
        default: break;
        }
        glyph_at_tile(px, py, (float)ts, gch, 0.95f, 0.9f, 0.55f);
    }

    /* dwarves */
    for (i = 0; i < MAX_DWARVES; i++) {
        int mx, my, vx, vy;
        float px, py;
        if (!f->dwarves[i].used) continue;
        mx = f->dwarves[i].x;
        my = f->dwarves[i].y;
        vx = mx - f->cam_x;
        vy = my - f->cam_y;
        if (vx < 0 || vy < 0 || vx >= view_tw || vy >= view_th) continue;
        px = (float)(vx * ts);
        py = (float)(win_h - (vy + 1) * ts);
        draw_rect(px + 2, py + 2, (float)ts - 5, (float)ts - 5, 0.95f, 0.75f,
                  0.25f, 0.85f);
        glyph_at_tile(px, py, (float)ts, '@', 1.0f, 0.95f, 0.4f);
        if (i == f->sel_dwarf)
            draw_box(px, py, (float)ts - 1, (float)ts - 1, 1.0f, 1.0f, 0.3f);
    }

    /* drag preview */
    if (f->drag) {
        int x0 = f->drag_x0, y0 = f->drag_y0, x1 = f->cur_x, y1 = f->cur_y;
        int t;
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
        for (y = y0; y <= y1; y++) {
            for (x = x0; x <= x1; x++) {
                int vx = x - f->cam_x, vy = y - f->cam_y;
                float px, py;
                if (vx < 0 || vy < 0 || vx >= view_tw || vy >= view_th) continue;
                px = (float)(vx * ts);
                py = (float)(win_h - (vy + 1) * ts);
                draw_rect(px, py, (float)ts - 1, (float)ts - 1, 1.0f, 1.0f, 1.0f,
                          0.18f);
            }
        }
    }

    /* cursor */
    {
        int vx = f->cur_x - f->cam_x, vy = f->cur_y - f->cam_y;
        if (vx >= 0 && vy >= 0 && vx < view_tw && vy < view_th) {
            float px = (float)(vx * ts);
            float py = (float)(win_h - (vy + 1) * ts);
            draw_box(px, py, (float)ts - 1, (float)ts - 1, 1.0f, 0.95f, 0.4f);
            draw_box(px + 1, py + 1, (float)ts - 3, (float)ts - 3, 0.2f, 0.2f,
                     0.1f);
        }
    }

    /* side panel chrome */
    {
        float px = (float)map_px_w;
        float pw = (float)PANEL_W;
        draw_rect(px, 0, pw, (float)win_h, 0.12f, 0.13f, 0.16f, 1.0f);
        /* left edge accent */
        draw_rect(px, 0, 3, (float)win_h, 0.35f, 0.55f, 0.75f, 1.0f);
        draw_rect(px + 3, (float)win_h - 36, pw - 3, 36, 0.16f, 0.18f, 0.22f,
                  1.0f);

        glColor3f(0.85f, 0.88f, 0.92f);
        text_at9(px + 12, (float)win_h - 24, f->fort_name);

        glColor3f(0.65f, 0.70f, 0.78f);
        snprintf(buf, sizeof(buf), "%s yr %d  day %d", season_name(f->season),
                 f->year, f->day);
        text_at(px + 12, (float)win_h - 48, buf);

        snprintf(buf, sizeof(buf), "Pop %d   tick %d", f->n_dwarves, f->tick);
        text_at(px + 12, (float)win_h - 64, buf);

        glColor3f(f->paused ? 1.0f : 0.4f, f->paused ? 0.45f : 0.9f, 0.4f);
        text_at9(px + 12, (float)win_h - 88, f->paused ? "[PAUSED]" : "[RUNNING]");

        glColor3f(0.95f, 0.85f, 0.4f);
        snprintf(buf, sizeof(buf), "MODE: %s", mode_name(f->mode));
        text_at(px + 12, (float)win_h - 112, buf);

        glColor3f(0.7f, 0.75f, 0.8f);
        snprintf(buf, sizeof(buf), "Cursor %d,%d", f->cur_x, f->cur_y);
        text_at(px + 12, (float)win_h - 130, buf);

        if (map_in_bounds(f->cur_x, f->cur_y)) {
            const Tile *t = &f->tiles[f->cur_y][f->cur_x];
            snprintf(buf, sizeof(buf), "Tile: %s", terrain_name(t->terrain));
            text_at(px + 12, (float)win_h - 148, buf);
            if (t->desig) {
                snprintf(buf, sizeof(buf), "Desig: %s", desig_name(t->desig));
                text_at(px + 12, (float)win_h - 164, buf);
            }
            if (t->stock_wood)
                text_at(px + 12, (float)win_h - 180, "Wood stockpile");
            if (t->stock_stone)
                text_at(px + 12, (float)win_h - 180, "Stone stockpile");
            {
                int ii = item_at(f, f->cur_x, f->cur_y, IT_NONE);
                if (ii >= 0) {
                    snprintf(buf, sizeof(buf), "Item: %s x%d",
                             item_name(f->items[ii].kind), f->items[ii].count);
                    text_at(px + 12, (float)win_h - 196, buf);
                }
            }
            {
                int di = unit_at(f, f->cur_x, f->cur_y);
                if (di >= 0) {
                    snprintf(buf, sizeof(buf), "Unit: %s", f->dwarves[di].name);
                    glColor3f(1.0f, 0.9f, 0.5f);
                    text_at(px + 12, (float)win_h - 212, buf);
                }
            }
        }

        /* stocks */
        glColor3f(0.55f, 0.60f, 0.68f);
        text_at(px + 12, (float)win_h - 240, "--- Stocks ---");
        glColor3f(0.8f, 0.75f, 0.55f);
        snprintf(buf, sizeof(buf), "Wood  %d", f->wood_stock);
        text_at(px + 12, (float)win_h - 256, buf);
        snprintf(buf, sizeof(buf), "Stone %d", f->stone_stock);
        text_at(px + 12, (float)win_h - 272, buf);
        snprintf(buf, sizeof(buf), "Beds  %d  Chairs %d", f->beds_made,
                 f->chairs_made);
        text_at(px + 12, (float)win_h - 288, buf);

        /* dwarves list */
        glColor3f(0.55f, 0.60f, 0.68f);
        text_at(px + 12, (float)win_h - 316, "--- Dwarves ---");
        for (i = 0; i < MAX_DWARVES; i++) {
            float yy;
            if (!f->dwarves[i].used) continue;
            yy = (float)win_h - 334 - i * 28;
            if (yy < 120) break;
            if (i == f->sel_dwarf)
                glColor3f(1.0f, 0.95f, 0.5f);
            else
                glColor3f(0.75f, 0.78f, 0.82f);
            snprintf(buf, sizeof(buf), "%s  %s", f->dwarves[i].name,
                     dwarf_state_name(f->dwarves[i].state));
            text_at(px + 12, yy, buf);
            glColor3f(0.55f, 0.58f, 0.62f);
            if (f->dwarves[i].job >= 0 && f->jobs[f->dwarves[i].job].used) {
                snprintf(buf, sizeof(buf), "  > %s",
                         job_kind_name(f->jobs[f->dwarves[i].job].kind));
                text_at(px + 12, yy - 12, buf);
            } else {
                text_at(px + 12, yy - 12, "  > no job");
            }
        }

        /* build menu hint */
        if (f->mode == MODE_BUILD_MENU) {
            float by = 200;
            draw_rect(px + 8, by - 10, pw - 16, 110, 0.18f, 0.2f, 0.24f, 0.95f);
            draw_box(px + 8, by - 10, pw - 16, 110, 0.45f, 0.55f, 0.7f);
            glColor3f(0.9f, 0.9f, 0.95f);
            text_at(px + 16, by + 80, "Build menu");
            text_at(px + 16, by + 60, "1  Wall (1 stone)");
            text_at(px + 16, by + 44, "2  Carpenter WS (3 wood)");
            text_at(px + 16, by + 28, "3  Wood stockpile");
            text_at(px + 16, by + 12, "4  Stone stockpile");
            text_at(px + 16, by - 4, "5/6 Craft bed/chair");
        }

        /* controls footer */
        glColor3f(0.45f, 0.48f, 0.52f);
        text_at(px + 10, 88, "Space pause  Esc cancel");
        text_at(px + 10, 74, "d dig  t cut  b build");
        text_at(px + 10, 60, "q query  Enter designate");
        text_at(px + 10, 46, "Arrows/WASD cursor");
        text_at(px + 10, 32, "[ ] camera  +/- zoom n/a");
        text_at(px + 10, 18, "Ctrl+S save  Ctrl+L load");
        snprintf(buf, sizeof(buf), "%.0f fps  Shift+Q quit", fps);
        text_at(px + 10, 4, buf);
    }

    /* bottom message bar over map */
    draw_rect(0, 0, (float)map_px_w, 22, 0.05f, 0.06f, 0.08f, 0.92f);
    glColor3f(0.85f, 0.88f, 0.7f);
    text_at(8, 6, f->msg[0] ? f->msg : "Embark site ready. d=dig t=cut b=build Space=pause");

    glDisable(GL_BLEND);
    glutSwapBuffers();
}
