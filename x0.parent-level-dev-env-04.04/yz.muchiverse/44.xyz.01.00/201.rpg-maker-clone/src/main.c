/* main.c — RPG Maker MZ-style single-page map editor + Event + Play
 * Layout target: tile palette | map tree | map canvas | toolbar
 * v4: animated tiles, zoom, grid, event diamonds, transfer, play camera
 */
#include "rpg.h"
#include "tileset.h"
#include <GL/glut.h>
#include <math.h>
#include <time.h>

#define BAR_H 26
#define TOOL_H 40
#define STATUS_H 22
#define LEFT_W 248
#define TREE_H 168

static Project g_proj;
static char g_project_path[MAX_PATH];
static enum Mode g_mode = MODE_MAP;
static int g_tick = 0;
static char g_status[256] = "Map Editor — paint | tree switch map | F2 Event | F3 Play";

/* map editor state */
static int g_ts_page = 0;
static int g_ts_sel = 0;
static int g_cam_x = 0, g_cam_y = 0;
static int g_cursor_x = 5, g_cursor_y = 5;
static int g_painting = 0;
static int g_layer = 0;         /* 0=ground 1=objects 2=events-only view */
static int g_tree_sel = 0;
static int g_tool = 0;          /* 0 pencil 1 rect 2 fill 3 erase */
static int g_rect_x0 = -1, g_rect_y0 = -1;
static int g_db_tab = 0;
static int g_zoom = 1;          /* 1 or 2 */
static int g_show_grid = 1;
static int g_show_events = 1;
static int g_pan_drag = 0;
static int g_pan_lx = 0, g_pan_ly = 0;
static int g_ev_sel = -1;       /* selected event index on map */

/* play */
static int g_px, g_py;
static int g_play_cam_x = 0, g_play_cam_y = 0;
static enum PlaySub g_play_sub = PLAY_WALK;
static char g_msg[MAX_TEXT];
static int g_msg_event = -1;
static int g_msg_line = 0;
static int g_cmd_resume = 0;    /* resume command index after message */

/* event editor */
static int g_ev_idx = -1;
static int g_view_scratch = 0;
static int g_focus = 0;
static int g_n_nav = 0;
typedef struct { int kind; int arg; char label[96]; } Nav;
static Nav g_nav[MAX_NAV];

static void set_status(const char *s) {
    snprintf(g_status, sizeof(g_status), "%s", s);
}

static void ortho(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void text(int x, int y, const char *s) {
    d_text((float)x, (float)y, s);
}
static void texts(int x, int y, const char *s) {
    d_text_small((float)x, (float)y, s);
}

static int tile_px(void) { return TILE_PX * (g_zoom < 1 ? 1 : g_zoom); }

/* Map char -> preferred tileset id */
static unsigned char char_to_tile(char c) {
    switch (c) {
    case '#': return (unsigned char)TILE_ID(1, 0);
    case '~': return (unsigned char)TILE_ID(0, 4);
    case '+': return (unsigned char)TILE_ID(3, 0);
    case ',': return (unsigned char)TILE_ID(0, 6);
    case ':': return (unsigned char)TILE_ID(1, 4);
    case '=': return (unsigned char)TILE_ID(1, 5);
    case '!': return (unsigned char)TILE_ID(1, 7);
    case '*': return (unsigned char)TILE_ID(0, 3); /* grate floor */
    case '.':
    default:  return (unsigned char)TILE_ID(0, 2);
    }
}

static char tile_to_char(unsigned char id) {
    int page = TILE_PAGE(id), sub = TILE_IDX(id) % 8;
    if (page == 1 && sub < 3) return '#';
    if (page == 0 && sub >= 4 && sub < 6) return '~';
    if (page == 3 && sub < 2) return '+';
    if (page == 0 && sub >= 6) return ',';
    if (page == 0 && sub == 3) return '*';
    if (page == 1 && sub >= 3 && sub < 5) return ':';
    if (page == 1 && sub >= 5 && sub < 7) return '=';
    if (page == 1 && sub >= 7) return '!';
    return '.';
}

static unsigned char obj_to_tile(char o) {
    switch (o) {
    case 'C': return (unsigned char)TILE_ID(2, 0);
    case 'K': return (unsigned char)TILE_ID(2, 1); /* console */
    case 'P': return (unsigned char)TILE_ID(2, 2); /* plant */
    case 'I': return (unsigned char)TILE_ID(2, 3); /* pillar */
    case 'L': return (unsigned char)TILE_ID(3, 3);
    case '+': return (unsigned char)TILE_ID(3, 0);
    case ':': return (unsigned char)TILE_ID(1, 4);
    case '=': return (unsigned char)TILE_ID(1, 5);
    case '!': return (unsigned char)TILE_ID(1, 7);
    default:  return (unsigned char)TILE_ID(2, 1);
    }
}

static int walkable_at(int x, int y) {
    char c, o;
    if (x < 0 || y < 0 || x >= g_proj.map.w || y >= g_proj.map.h) return 0;
    c = g_proj.map.cells[y][x];
    if (c == '#' || c == '~' || c == '!') return 0;
    o = g_proj.map.objects[y][x];
    if (o == 'I' || o == 'C') return 0; /* solid props */
    return 1;
}

/* ---------- MAP EDITOR layout helpers ---------- */

static int map_origin_x(void) { return LEFT_W + 8; }
static int map_origin_y(void) { return STATUS_H + 8; }
static int map_view_w(void) { return WIN_W - LEFT_W - 16; }
static int map_view_h(void) { return WIN_H - BAR_H - TOOL_H - STATUS_H - 16; }

static void paint_cell(int x, int y, int erase) {
    unsigned char id;
    char ch;
    if (x < 0 || y < 0 || x >= g_proj.map.w || y >= g_proj.map.h) return;
    if (erase || g_tool == 3) {
        if (g_layer == 0) g_proj.map.cells[y][x] = '.';
        else g_proj.map.objects[y][x] = ' ';
        g_proj.dirty = 1;
        return;
    }
    id = (unsigned char)TILE_ID(g_ts_page, g_ts_sel);
    ch = tile_to_char(id);
    if (g_layer == 1 || g_ts_page == 2 || g_ts_page == 3) {
        int sub = g_ts_sel % 8;
        if (g_ts_page == 2) {
            if (sub % 4 == 0) ch = 'C';
            else if (sub % 4 == 1) ch = 'K';
            else if (sub % 4 == 2) ch = 'P';
            else ch = 'I';
        } else if (g_ts_page == 3 && sub < 2) ch = '+';
        else if (g_ts_page == 3 && sub < 4) ch = 'L';
        else if (g_ts_page == 1) ch = tile_to_char(id);
        else ch = 'K';
        g_proj.map.objects[y][x] = ch;
        if (g_layer == 0 && (ch == '+' || ch == '#'))
            g_proj.map.cells[y][x] = ch;
    } else if (g_ts_page == 4) {
        /* region page paints ground tint marker as digit char */
        g_proj.map.objects[y][x] = (char)('0' + (g_ts_sel % 8));
    } else {
        g_proj.map.cells[y][x] = ch;
    }
    g_proj.dirty = 1;
}

static void flood_fill(int x, int y, int erase) {
    int stack[MAP_W * MAP_H][2];
    unsigned char seen[MAP_H][MAP_W];
    int sp = 0;
    char layer_ch;
    if (x < 0 || y < 0 || x >= g_proj.map.w || y >= g_proj.map.h) return;
    memset(seen, 0, sizeof(seen));
    layer_ch = (g_layer == 0) ? g_proj.map.cells[y][x] : g_proj.map.objects[y][x];
    if (!layer_ch) layer_ch = (g_layer == 0) ? '.' : ' ';
    if (!erase) {
        char want;
        if (g_layer == 0)
            want = tile_to_char((unsigned char)TILE_ID(g_ts_page, g_ts_sel));
        else
            want = 'C';
        if (layer_ch == want) return;
    }
    stack[sp][0] = x; stack[sp][1] = y; sp++;
    while (sp > 0) {
        int cx, cy, nx, ny, d;
        int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        sp--;
        cx = stack[sp][0]; cy = stack[sp][1];
        if (cx < 0 || cy < 0 || cx >= g_proj.map.w || cy >= g_proj.map.h) continue;
        if (seen[cy][cx]) continue;
        {
            char c = (g_layer == 0) ? g_proj.map.cells[cy][cx] : g_proj.map.objects[cy][cx];
            if (!c) c = (g_layer == 0) ? '.' : ' ';
            if (c != layer_ch) continue;
        }
        seen[cy][cx] = 1;
        paint_cell(cx, cy, erase);
        for (d = 0; d < 4; d++) {
            nx = cx + dirs[d][0]; ny = cy + dirs[d][1];
            if (nx >= 0 && ny >= 0 && nx < g_proj.map.w && ny < g_proj.map.h &&
                !seen[ny][nx] && sp < MAP_W * MAP_H) {
                stack[sp][0] = nx; stack[sp][1] = ny; sp++;
            }
        }
    }
}

static void paint_rect(int x0, int y0, int x1, int y1, int erase) {
    int x, y, t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            paint_cell(x, y, erase);
}

static void switch_map_by_tree(int idx) {
    char buf[128];
    if (idx < 0 || idx >= g_proj.n_maps) return;
    if (g_proj.dirty) project_save(&g_proj);
    if (project_switch_map(&g_proj, g_proj.maps[idx].id) == 0) {
        g_tree_sel = idx;
        g_cursor_x = 2; g_cursor_y = 2;
        g_cam_x = 0; g_cam_y = 0;
        g_ev_sel = -1; g_ev_idx = -1;
        snprintf(buf, sizeof(buf), "Map: %s (%d events)",
                 g_proj.maps[idx].id, g_proj.n_events);
        set_status(buf);
    } else {
        set_status("Failed to load map");
    }
}

/* icon button: small square with glyph */
static void tool_icon(int x, int y, int w, int h, int on, const char *glyph, const char *tip) {
    if (on) d_set_rgb(0.30f, 0.48f, 0.72f);
    else d_set_rgb(0.22f, 0.24f, 0.28f);
    d_fill_rect((float)x, (float)y, (float)w, (float)h);
    d_set_rgb(0.45f, 0.48f, 0.55f);
    d_stroke_rect((float)x, (float)y, (float)w, (float)h);
    d_set_rgb(0.95f, 0.96f, 0.98f);
    texts(x + (w / 2) - 3, y + (h / 2) - 4, glyph);
    (void)tip;
}

static void draw_menu_bar(void) {
    int y = WIN_H - BAR_H;
    d_set_rgb(0.20f, 0.22f, 0.26f);
    d_fill_rect(0, (float)y, (float)WIN_W, (float)BAR_H);
    d_set_rgb(0.12f, 0.13f, 0.15f);
    d_fill_rect(0, (float)(y + BAR_H - 1), (float)WIN_W, 1);
    d_set_rgb(0.88f, 0.90f, 0.93f);
    texts(10, y + 8, "File   Edit   Mode   Draw   Layer   Scale   Tools   Game   Help");
    d_set_rgb(0.55f, 0.72f, 0.95f);
    texts(WIN_W - 220, y + 8, "planet aether - RPG Maker MZ*");
}

static void draw_toolbar(void) {
    int y = WIN_H - BAR_H - TOOL_H;
    char buf[96];
    int i;
    const char *glyphs[] = { "/", "#", "F", "X" }; /* pencil/rect/fill/erase */
    const char *layers[] = { "G", "O" };
    d_set_rgb(0.16f, 0.18f, 0.22f);
    d_fill_rect(0, (float)y, (float)WIN_W, (float)TOOL_H);
    d_set_rgb(0.30f, 0.33f, 0.40f);
    d_fill_rect(0, (float)(y + TOOL_H - 1), (float)WIN_W, 1);

    /* file-ish icons */
    tool_icon(8, y + 6, 28, 28, 0, "S", "save");
    tool_icon(40, y + 6, 28, 28, 0, "N", "new event");
    tool_icon(72, y + 6, 28, 28, g_show_grid, "#", "grid");
    tool_icon(104, y + 6, 28, 28, g_show_events, "E", "events");

    /* tools */
    for (i = 0; i < 4; i++)
        tool_icon(150 + i * 32, y + 6, 28, 28, g_tool == i, glyphs[i], "");

    /* layers */
    for (i = 0; i < 2; i++)
        tool_icon(300 + i * 32, y + 6, 28, 28, g_layer == i, layers[i], "");

    /* zoom */
    tool_icon(380, y + 6, 28, 28, g_zoom == 1, "1", "zoom1");
    tool_icon(412, y + 6, 28, 28, g_zoom == 2, "2", "zoom2");

    snprintf(buf, sizeof(buf), "%s:%d %s  layer:%s  %dx",
             tileset_page_name(g_ts_page), g_ts_sel,
             tileset_tile_hint((unsigned char)TILE_ID(g_ts_page, g_ts_sel)),
             g_layer == 0 ? "Ground" : "Objects", g_zoom);
    d_set_rgb(0.70f, 0.75f, 0.85f);
    texts(460, y + 14, buf);

    /* Database + Play (right) */
    d_set_rgb(0.32f, 0.34f, 0.50f);
    d_fill_rect((float)(WIN_W - 190), (float)(y + 6), 80, 28);
    d_set_rgb(0.95f, 0.95f, 1.0f);
    texts(WIN_W - 178, y + 14, "Database");
    d_set_rgb(0.18f, 0.52f, 0.32f);
    d_fill_rect((float)(WIN_W - 96), (float)(y + 6), 80, 28);
    d_set_rgb(1, 1, 1);
    texts(WIN_W - 72, y + 14, "Play >");
}

static void draw_tileset_panel(void) {
    int top = WIN_H - BAR_H - TOOL_H - 4;
    int bot = STATUS_H + TREE_H + 8;
    int h = top - bot;
    int i;
    char buf[32];
    d_set_rgb(0.13f, 0.14f, 0.17f);
    d_fill_rect(0, (float)bot, (float)LEFT_W, (float)h);
    d_set_rgb(0.32f, 0.36f, 0.44f);
    d_stroke_rect(0, (float)bot, (float)LEFT_W, (float)h);

    /* tabs A B C D R at bottom of tileset (MZ style) */
    for (i = 0; i < TS_PAGES; i++) {
        int tx = 6 + i * 46;
        int ty = bot + 6;
        int sel = (i == g_ts_page);
        if (sel) d_set_rgb(0.28f, 0.42f, 0.62f);
        else d_set_rgb(0.18f, 0.20f, 0.26f);
        d_fill_rect((float)tx, (float)ty, 42, 20);
        d_set_rgb(0.9f, 0.92f, 0.95f);
        snprintf(buf, sizeof(buf), " %s ", tileset_page_name(i));
        texts(tx + 10, ty + 5, buf);
    }

    /* palette: leave room for tabs at bottom */
    tileset_draw_palette(g_ts_page, 8, top - 8, g_ts_sel);

    d_set_rgb(0.55f, 0.62f, 0.72f);
    texts(8, bot + 30, "Tileset A-R  (1-5 tabs)");
}

static void draw_map_tree(void) {
    int y0 = STATUS_H;
    int i, n;
    char buf[80];
    d_set_rgb(0.11f, 0.12f, 0.15f);
    d_fill_rect(0, (float)y0, (float)LEFT_W, (float)TREE_H);
    d_set_rgb(0.32f, 0.36f, 0.44f);
    d_stroke_rect(0, (float)y0, (float)LEFT_W, (float)TREE_H);

    d_set_rgb(0.55f, 0.75f, 1.0f);
    texts(8, y0 + TREE_H - 16, g_proj.name[0] ? g_proj.name : "Project");
    d_set_rgb(0.55f, 0.58f, 0.65f);
    texts(8, y0 + TREE_H - 32, "- World");
    n = g_proj.n_maps;
    if (n < 1) {
        d_set_rgb(0.6f, 0.6f, 0.6f);
        texts(16, y0 + TREE_H - 50, "  (no maps)");
        return;
    }
    for (i = 0; i < n && i < 9; i++) {
        int ty = y0 + TREE_H - 50 - i * 16;
        int cur = (strcmp(g_proj.maps[i].id, g_proj.map_id) == 0);
        if (i == g_tree_sel || cur) {
            d_set_rgb(0.22f, 0.38f, 0.58f);
            d_fill_rect(4, (float)(ty - 2), (float)(LEFT_W - 8), 16);
        }
        d_set_rgb(cur ? 0.95f : 0.82f, cur ? 0.97f : 0.85f, 1.0f);
        snprintf(buf, sizeof(buf), "  %s%s", g_proj.maps[i].label, cur ? "  *" : "");
        texts(14, ty + 2, buf);
    }
}

static void draw_event_diamond(int sx, int sy, int tw, int selected) {
    int cx = sx + tw / 2;
    int cy = sy + tw / 2;
    int r = tw / 3;
    if (selected) d_set_rgb(1.0f, 0.85f, 0.2f);
    else d_set_rgb(0.78f, 0.28f, 0.95f);
    glBegin(GL_QUADS);
    glVertex2i(cx, cy + r);
    glVertex2i(cx + r, cy);
    glVertex2i(cx, cy - r);
    glVertex2i(cx - r, cy);
    glEnd();
    d_set_rgb(1, 1, 1);
    glBegin(GL_LINE_LOOP);
    glVertex2i(cx, cy + r);
    glVertex2i(cx + r, cy);
    glVertex2i(cx, cy - r);
    glVertex2i(cx - r, cy);
    glEnd();
}

static void draw_map_canvas(void) {
    int ox = map_origin_x();
    int oy = map_origin_y();
    int vw = map_view_w();
    int vh = map_view_h();
    int x, y, sx, sy;
    int tw = tile_px();
    char buf[96];
    int i;

    /* dark canvas bg (MZ outer void) */
    d_set_rgb(0.06f, 0.06f, 0.08f);
    d_fill_rect((float)ox, (float)oy, (float)vw, (float)vh);

    /* ground then objects */
    for (y = 0; y < g_proj.map.h; y++) {
        for (x = 0; x < g_proj.map.w; x++) {
            unsigned char tid;
            char o;
            sx = ox + (x - g_cam_x) * tw;
            sy = oy + (g_proj.map.h - 1 - y - g_cam_y) * tw;
            if (sx + tw < ox || sy + tw < oy || sx > ox + vw || sy > oy + vh)
                continue;
            tid = char_to_tile(g_proj.map.cells[y][x]);
            /* dim ground slightly when editing objects */
            if (g_layer == 1) {
                tileset_draw(tid, sx, sy, g_zoom);
                d_set_rgb(0.0f, 0.0f, 0.0f);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                /* no alpha in fixed pipeline without glColor4 — fake dim with dark overlay */
                d_set_rgb(0.05f, 0.05f, 0.06f);
                /* skip full dim; keep readable */
            } else {
                tileset_draw(tid, sx, sy, g_zoom);
            }
            o = g_proj.map.objects[y][x];
            if (o && o != ' ' && o != '.') {
                if (o >= '0' && o <= '7') {
                    /* region number overlay */
                    unsigned char rid = (unsigned char)TILE_ID(4, o - '0');
                    tileset_draw(rid, sx, sy, g_zoom);
                } else {
                    tileset_draw(obj_to_tile(o), sx, sy, g_zoom);
                }
            }
            if (g_show_grid) {
                d_set_rgb(0.0f, 0.0f, 0.0f);
                glBegin(GL_LINE_LOOP);
                glVertex2i(sx, sy);
                glVertex2i(sx + tw, sy);
                glVertex2i(sx + tw, sy + tw);
                glVertex2i(sx, sy + tw);
                glEnd();
            }
        }
    }

    /* events as purple diamonds (MZ) */
    if (g_show_events) {
        for (i = 0; i < MAX_EVENTS; i++) {
            if (!g_proj.events[i].used) continue;
            sx = ox + (g_proj.events[i].x - g_cam_x) * tw;
            sy = oy + (g_proj.map.h - 1 - g_proj.events[i].y - g_cam_y) * tw;
            if (sx + tw < ox || sy + tw < oy || sx > ox + vw || sy > oy + vh)
                continue;
            draw_event_diamond(sx, sy, tw, i == g_ev_sel || i == g_ev_idx);
            {
                char b[2] = { g_proj.events[i].sprite ? g_proj.events[i].sprite : 'E', 0 };
                d_set_rgb(1, 1, 1);
                texts(sx + tw / 2 - 3, sy + tw / 2 - 4, b);
            }
        }
    }

    /* rect preview */
    if (g_painting && g_tool == 1 && g_rect_x0 >= 0) {
        int x0 = g_rect_x0, y0 = g_rect_y0, x1 = g_cursor_x, y1 = g_cursor_y, t;
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
        sx = ox + (x0 - g_cam_x) * tw;
        sy = oy + (g_proj.map.h - 1 - y1 - g_cam_y) * tw;
        d_set_rgb(0.3f, 0.7f, 1.0f);
        d_stroke_rect((float)sx, (float)sy,
                      (float)((x1 - x0 + 1) * tw), (float)((y1 - y0 + 1) * tw));
    }

    /* cursor */
    sx = ox + (g_cursor_x - g_cam_x) * tw;
    sy = oy + (g_proj.map.h - 1 - g_cursor_y - g_cam_y) * tw;
    d_set_rgb(1.0f, 0.95f, 0.15f);
    glLineWidth(2.0f);
    d_stroke_rect((float)sx, (float)sy, (float)tw, (float)tw);
    glLineWidth(1.0f);

    d_set_rgb(0.38f, 0.42f, 0.50f);
    d_stroke_rect((float)ox, (float)oy, (float)vw, (float)vh);

    snprintf(buf, sizeof(buf), "008:%s (%dx%d)  %d,%d  zoom %dx  %s",
             g_proj.map_id, g_proj.map.w, g_proj.map.h,
             g_cursor_x, g_cursor_y, g_zoom,
             g_proj.dirty ? "*dirty*" : "saved");
    d_set_rgb(0.65f, 0.70f, 0.80f);
    texts(ox + 8, oy + 4, buf);
}

static void draw_status(void) {
    d_set_rgb(0.14f, 0.15f, 0.18f);
    d_fill_rect(0, 0, (float)WIN_W, (float)STATUS_H);
    d_set_rgb(0.45f, 0.85f, 0.50f);
    texts(8, 6, g_status);
    d_set_rgb(0.50f, 0.58f, 0.68f);
    texts(WIN_W - 360, 6, "F2 Event  F3 Play  F4 DB  S save  [ ] zoom  Q quit");
}

static void display_map_editor(void) {
    tileset_set_tick(g_tick);
    glClearColor(0.09f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ortho();
    draw_menu_bar();
    draw_toolbar();
    draw_tileset_panel();
    draw_map_tree();
    draw_map_canvas();
    draw_status();
    glutSwapBuffers();
}

/* ---------- PLAY ---------- */
static void play_start(void) {
    g_px = g_proj.start_x;
    g_py = g_proj.start_y;
    if (g_px < 0 || g_px >= g_proj.map.w) g_px = 2;
    if (g_py < 0 || g_py >= g_proj.map.h) g_py = 2;
    g_play_sub = PLAY_WALK;
    g_msg[0] = 0;
    g_cmd_resume = 0;
    g_mode = MODE_PLAY;
    g_play_cam_x = g_px - 10;
    g_play_cam_y = g_py - 7;
    if (g_play_cam_x < 0) g_play_cam_x = 0;
    if (g_play_cam_y < 0) g_play_cam_y = 0;
    set_status("Play — arrows move, Space action, Esc Map Editor");
}

static void play_center_cam(void) {
    int vw = 24, vh = 16;
    g_play_cam_x = g_px - vw / 2;
    g_play_cam_y = g_py - vh / 2;
    if (g_play_cam_x < 0) g_play_cam_x = 0;
    if (g_play_cam_y < 0) g_play_cam_y = 0;
    if (g_play_cam_x > g_proj.map.w - vw) g_play_cam_x = g_proj.map.w - vw;
    if (g_play_cam_y > g_proj.map.h - vh) g_play_cam_y = g_proj.map.h - vh;
    if (g_play_cam_x < 0) g_play_cam_x = 0;
    if (g_play_cam_y < 0) g_play_cam_y = 0;
}

static int run_event_cmds_from(int ei, int start) {
    Event *e;
    int i, skip = 0;
    if (ei < 0 || ei >= MAX_EVENTS || !g_proj.events[ei].used) return 0;
    e = &g_proj.events[ei];
    for (i = start; i < e->n_cmds; i++) {
        Command *c = &e->cmds[i];
        if (skip) {
            if (c->type == CMD_END) skip = 0;
            continue;
        }
        if (c->type == CMD_SHOW_TEXT) {
            snprintf(g_msg, sizeof(g_msg), "%s", c->a);
            g_msg_event = ei;
            g_msg_line = i;
            g_cmd_resume = i + 1;
            g_play_sub = PLAY_MSG;
            return 1;
        } else if (c->type == CMD_SET_SWITCH) {
            switch_set(&g_proj, c->a, atoi(c->b[0] ? c->b : "1"));
            project_save_switches(&g_proj);
        } else if (c->type == CMD_IF_SWITCH) {
            int v = switch_get(&g_proj, c->a);
            int want = atoi(c->b[0] ? c->b : "1");
            if (v != want) skip = 1;
        } else if (c->type == CMD_TRANSFER) {
            int nx = atoi(c->b), ny = atoi(c->c);
            char buf[128];
            if (g_proj.dirty) project_save(&g_proj);
            if (project_switch_map(&g_proj, c->a) == 0) {
                g_px = nx; g_py = ny;
                if (g_px < 0) g_px = 2;
                if (g_py < 0) g_py = 2;
                play_center_cam();
                snprintf(buf, sizeof(buf), "Transferred to %s (%d,%d)", c->a, g_px, g_py);
                set_status(buf);
            } else {
                set_status("Transfer failed — map missing");
            }
        } else if (c->type == CMD_END || c->type == CMD_RET) {
            break;
        }
    }
    return 0;
}

static int run_event_cmds(int ei) {
    return run_event_cmds_from(ei, 0);
}

static void try_action(void) {
    int i;
    for (i = 0; i < MAX_EVENTS; i++) {
        Event *e = &g_proj.events[i];
        if (!e->used) continue;
        if (e->x == g_px && e->y == g_py && e->trigger == TR_ACTION) {
            if (run_event_cmds(i)) return;
            set_status("Event finished");
            return;
        }
        /* also adjacent facing — allow stand on same tile only for now */
    }
    set_status("Nothing here");
}

static void display_play(void) {
    int ox = 40, oy = 36, tw = 32;
    int x, y, i;
    int maxx, maxy;
    tileset_set_tick(g_tick);
    play_center_cam();
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ortho();

    maxx = g_play_cam_x + (WIN_W - 80) / tw + 1;
    maxy = g_play_cam_y + (WIN_H - 80) / tw + 1;
    if (maxx > g_proj.map.w) maxx = g_proj.map.w;
    if (maxy > g_proj.map.h) maxy = g_proj.map.h;

    for (y = g_play_cam_y; y < maxy; y++) {
        for (x = g_play_cam_x; x < maxx; x++) {
            int sx = ox + (x - g_play_cam_x) * tw;
            int sy = oy + (maxy - 1 - y) * tw;
            char o;
            if (y < 0 || x < 0) continue;
            tileset_draw(char_to_tile(g_proj.map.cells[y][x]), sx, sy, 1);
            o = g_proj.map.objects[y][x];
            if (o && o != ' ' && o != '.' && !(o >= '0' && o <= '7'))
                tileset_draw(obj_to_tile(o), sx, sy, 1);
        }
    }
    /* events as sprites */
    for (i = 0; i < MAX_EVENTS; i++) {
        Event *e = &g_proj.events[i];
        int sx, sy;
        char b[2];
        if (!e->used) continue;
        if (e->x < g_play_cam_x || e->y < g_play_cam_y) continue;
        sx = ox + (e->x - g_play_cam_x) * tw + 2;
        sy = oy + (maxy - 1 - e->y) * tw + 2;
        d_set_rgb(0.65f, 0.25f, 0.90f);
        d_fill_rect((float)sx, (float)sy, 28, 28);
        b[0] = e->sprite ? e->sprite : 'E'; b[1] = 0;
        d_set_rgb(1, 1, 1);
        texts(sx + 10, sy + 10, b);
    }
    /* player */
    {
        int sx = ox + (g_px - g_play_cam_x) * tw + 2;
        int sy = oy + (maxy - 1 - g_py) * tw + 2;
        d_set_rgb(0.1f, 0.1f, 0.15f);
        d_fill_rect((float)(sx + 2), (float)(sy - 2), 28, 6); /* shadow */
        d_set_rgb(0.20f, 0.48f, 0.95f);
        d_fill_rect((float)sx, (float)sy, 28, 28);
        d_set_rgb(1, 1, 1);
        texts(sx + 10, sy + 10, "P");
    }
    if (g_play_sub == PLAY_MSG) {
        d_set_rgb(0.08f, 0.10f, 0.16f);
        d_fill_rect(80, 60, (float)(WIN_W - 160), 110);
        d_set_rgb(0.45f, 0.65f, 0.95f);
        d_stroke_rect(80, 60, (float)(WIN_W - 160), 110);
        d_set_rgb(1, 1, 1);
        text(100, 120, g_msg);
        texts(100, 80, "[Enter/Space] continue");
    }
    d_set_rgb(0.12f, 0.14f, 0.18f);
    d_fill_rect(0, 0, (float)WIN_W, 28);
    d_set_rgb(0.5f, 0.9f, 0.5f);
    texts(8, 8, g_status);
    d_set_rgb(0.6f, 0.7f, 0.8f);
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  (%d,%d)", g_proj.map_id, g_px, g_py);
        texts(WIN_W - 200, 8, buf);
    }
    glutSwapBuffers();
}

/* ---------- EVENT EDITOR ---------- */
static void rebuild_event_nav(void) {
    Event *e;
    int i;
    g_n_nav = 0;
#define ADD(k, a, lab) do { \
        if (g_n_nav < MAX_NAV) { g_nav[g_n_nav].kind = (k); g_nav[g_n_nav].arg = (a); \
            snprintf(g_nav[g_n_nav].label, sizeof(g_nav[g_n_nav].label), "%s", (lab)); g_n_nav++; } \
    } while (0)
    ADD(1, 0, "Save"); ADD(1, 1, "Load"); ADD(1, 2, "New Event"); ADD(1, 3, "Delete");
    ADD(1, 4, "Toggle Scratch"); ADD(1, 5, "Back to Map");
    if (g_ev_idx < 0 || !g_proj.events[g_ev_idx].used) {
        ADD(2, 0, "(no event — place with N on map, then F2)");
        return;
    }
    e = &g_proj.events[g_ev_idx];
    for (i = 0; i < 12; i++) {
        char lab[96];
        if (i < e->n_cmds) cmd_to_label(&e->cmds[i], lab, sizeof(lab));
        else snprintf(lab, sizeof(lab), "(empty)");
        ADD(3, i, lab);
    }
#undef ADD
    if (g_focus >= g_n_nav) g_focus = g_n_nav - 1;
}

static void display_event(void) {
    Event *e = (g_ev_idx >= 0 && g_proj.events[g_ev_idx].used) ? &g_proj.events[g_ev_idx] : NULL;
    int i;
    char buf[128];
    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ortho();
    d_set_rgb(0.22f, 0.26f, 0.38f);
    d_fill_rect(0, (float)(WIN_H - 48), (float)WIN_W, 48);
    d_set_rgb(1, 1, 1);
    d_text_big(16, (float)(WIN_H - 28), "Event Editor");
    d_set_rgb(0.45f, 0.9f, 0.55f);
    texts(200, WIN_H - 28, g_status);

    for (i = 0; i < 6 && i < g_n_nav; i++) {
        int foc = (g_focus == i);
        float x = 12 + i * 150;
        if (foc) d_set_rgb(0.35f, 0.5f, 0.75f); else d_set_rgb(0.18f, 0.22f, 0.32f);
        d_fill_rect(x, (float)(WIN_H - 90), 140, 28);
        d_set_rgb(1, 1, 1);
        snprintf(buf, sizeof(buf), "%s%d %s", foc ? "[>]" : "[ ]", i + 1, g_nav[i].label);
        texts((int)x + 4, WIN_H - 82, buf);
    }

    d_panel(12, 80, 300, 500, "Event");
    if (e) {
        snprintf(buf, sizeof(buf), "Name: %s", e->name);
        texts(24, 540, buf);
        snprintf(buf, sizeof(buf), "Pos: %d,%d  trigger:%s", e->x, e->y,
                 e->trigger == TR_ACTION ? "Action" : "Touch");
        texts(24, 520, buf);
        d_set_rgb(0.25f, 0.45f, 0.3f);
        d_fill_rect(40, 400, 80, 80);
        d_set_rgb(1, 1, 0.5f);
        { char sp[2] = { e->sprite ? e->sprite : '@', 0 };
          d_text_big(70, 430, sp); }
    } else {
        texts(24, 540, "No event selected");
        texts(24, 520, "On Map: move cursor, press N to add event");
    }

    d_panel(330, 80, 900, 500, g_view_scratch ? "Contents [SCRATCH]" : "Contents [COMMANDS]");
    for (i = 6; i < g_n_nav; i++) {
        int foc = (g_focus == i);
        float cy = 520 - (i - 6) * 28;
        if (foc) d_set_rgb(0.3f, 0.45f, 0.7f); else d_set_rgb(0.14f, 0.16f, 0.22f);
        d_fill_rect(340, cy - 4, 880, 26);
        d_set_rgb(1, 1, foc ? 0.75f : 1);
        snprintf(buf, sizeof(buf), "%s %2d. %s", foc ? "[>]" : "[ ]", i + 1, g_nav[i].label);
        texts(348, (int)cy, buf);
    }
    d_set_rgb(0.15f, 0.17f, 0.22f);
    d_fill_rect(0, 0, (float)WIN_W, 40);
    d_set_rgb(0.7f, 0.8f, 0.9f);
    texts(12, 14, "Esc/F1 Map Editor  |  Enter activate  |  arrows nav");
    glutSwapBuffers();
}

static void display_database(void) {
    int i;
    char buf[128];
    glClearColor(0.12f, 0.13f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ortho();
    d_set_rgb(0.22f, 0.26f, 0.38f);
    d_fill_rect(0, (float)(WIN_H - 40), (float)WIN_W, 40);
    d_set_rgb(1, 1, 1);
    d_text_big(16, (float)(WIN_H - 28), "Database");
    d_set_rgb(0.5f, 0.9f, 0.5f);
    texts(200, WIN_H - 28, "Actors | Items | Switches — F1 Map Editor");

    for (i = 0; i < 3; i++) {
        const char *tabs[] = { "Actors", "Items", "Switches" };
        if (g_db_tab == i) d_set_rgb(0.35f, 0.5f, 0.7f);
        else d_set_rgb(0.2f, 0.22f, 0.28f);
        d_fill_rect((float)(20 + i * 120), (float)(WIN_H - 80), 110, 28);
        d_set_rgb(1, 1, 1);
        texts(36 + i * 120, WIN_H - 72, tabs[i]);
    }

    d_panel(20, 40, 600, 560, g_db_tab == 0 ? "Actors" : g_db_tab == 1 ? "Items" : "Switches");
    if (g_db_tab == 0) {
        texts(40, 550, "1. Hero (party leader)  HP 450  ATK 32");
        texts(40, 530, "2. Guard (NPC template) HP 280  ATK 22");
        texts(40, 510, "3. Engineer              HP 320  ATK 18");
        texts(40, 480, "MVP: templates. Full stat edit later.");
    } else if (g_db_tab == 1) {
        texts(40, 550, "1. Potion     (+30 HP stub)");
        texts(40, 530, "2. Keycard    (factory access)");
        texts(40, 510, "3. Pipe wrench (tool)");
        texts(40, 490, "4. — empty —");
    } else {
        for (i = 0; i < g_proj.n_switches && i < 14; i++) {
            snprintf(buf, sizeof(buf), "%2d. %-18s = %d", i + 1,
                     g_proj.switches[i].name, g_proj.switches[i].value);
            texts(40, 550 - i * 18, buf);
        }
        if (g_proj.n_switches < 1) texts(40, 550, "(no switches yet — play events to set)");
    }
    d_panel(640, 40, 600, 560, "Notes");
    texts(660, 550, "Database hub mirrors MZ categories.");
    texts(660, 530, "Switches live in projects/demo/switches.pdl");
    texts(660, 510, "Keys: 1-3 tabs | F1 map | Esc map");
    glutSwapBuffers();
}

static void display(void) {
    if (g_mode == MODE_MAP) display_map_editor();
    else if (g_mode == MODE_PLAY) display_play();
    else if (g_mode == MODE_EVENT) {
        rebuild_event_nav();
        display_event();
    } else if (g_mode == MODE_DATABASE) display_database();
    else display_map_editor();
}

static void timer(int v) {
    (void)v;
    g_tick++;
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

static int hit_event(int mx, int my) {
    int i;
    for (i = 0; i < MAX_EVENTS; i++) {
        if (!g_proj.events[i].used) continue;
        if (g_proj.events[i].x == mx && g_proj.events[i].y == my)
            return i;
    }
    return -1;
}

static void mouse(int button, int state, int x, int y) {
    int gy = WIN_H - y;
    int ox, oy, tw;
    if (g_mode != MODE_MAP) return;

    /* middle button pan */
    if (button == GLUT_MIDDLE_BUTTON) {
        g_pan_drag = (state == GLUT_DOWN);
        g_pan_lx = x; g_pan_ly = gy;
        return;
    }
    if (button != GLUT_LEFT_BUTTON) return;
    tw = tile_px();

    /* tile palette */
    if (x < LEFT_W && gy > STATUS_H + TREE_H) {
        int bot = STATUS_H + TREE_H + 8;
        int top = WIN_H - BAR_H - TOOL_H - 4;
        int i;
        /* tabs at bottom of panel */
        if (gy >= bot + 6 && gy <= bot + 26) {
            i = (x - 6) / 46;
            if (i >= 0 && i < TS_PAGES) { g_ts_page = i; g_ts_sel = 0; }
            return;
        }
        for (i = 0; i < 64; i++) {
            int col = i % 8, row = i / 8;
            int sx = 8 + col * (TILE_PX + 2);
            int sy = top - 8 - (row + 1) * (TILE_PX + 2);
            if (x >= sx && x < sx + TILE_PX && gy >= sy && gy < sy + TILE_PX) {
                g_ts_sel = i;
                {
                    char buf[80];
                    snprintf(buf, sizeof(buf), "Tile %s:%d (%s)",
                             tileset_page_name(g_ts_page), g_ts_sel,
                             tileset_tile_hint((unsigned char)TILE_ID(g_ts_page, g_ts_sel)));
                    set_status(buf);
                }
                return;
            }
        }
    }

    /* map tree */
    if (x < LEFT_W && gy < STATUS_H + TREE_H) {
        int rel = (STATUS_H + TREE_H - 50 - gy) / 16;
        if (rel >= 0 && rel < g_proj.n_maps) {
            g_tree_sel = rel;
            if (state == GLUT_DOWN) switch_map_by_tree(rel);
        }
        return;
    }

    /* toolbar */
    if (gy > WIN_H - BAR_H - TOOL_H && gy < WIN_H - BAR_H) {
        int ty = WIN_H - BAR_H - TOOL_H + 6;
        int i;
        if (state != GLUT_DOWN) return;
        if (x > WIN_W - 96) { play_start(); return; }
        if (x > WIN_W - 190 && x < WIN_W - 100) {
            g_mode = MODE_DATABASE;
            set_status("Database — Actors / Items / Switches (F1 back)");
            return;
        }
        /* S save */
        if (x >= 8 && x < 36) {
            project_save(&g_proj);
            set_status("Saved");
            return;
        }
        /* N event */
        if (x >= 40 && x < 68) {
            int ei = project_add_event(&g_proj, g_cursor_x, g_cursor_y);
            if (ei >= 0) { g_ev_idx = ei; g_ev_sel = ei; set_status("Event created"); }
            return;
        }
        if (x >= 72 && x < 100) { g_show_grid = !g_show_grid; return; }
        if (x >= 104 && x < 132) { g_show_events = !g_show_events; return; }
        for (i = 0; i < 4; i++) {
            int tx = 150 + i * 32;
            if (x >= tx && x < tx + 28) { g_tool = i; return; }
        }
        for (i = 0; i < 2; i++) {
            int tx = 300 + i * 32;
            if (x >= tx && x < tx + 28) { g_layer = i; return; }
        }
        if (x >= 380 && x < 408) { g_zoom = 1; return; }
        if (x >= 412 && x < 440) { g_zoom = 2; return; }
        (void)ty;
        return;
    }

    /* map canvas */
    ox = map_origin_x();
    oy = map_origin_y();
    if (x >= ox && gy >= oy) {
        int mx = g_cam_x + (x - ox) / tw;
        int my = g_cam_y + (g_proj.map.h - 1 - (gy - oy) / tw);
        if (mx >= 0 && my >= 0 && mx < g_proj.map.w && my < g_proj.map.h) {
            int ei;
            g_cursor_x = mx;
            g_cursor_y = my;
            ei = hit_event(mx, my);
            if (state == GLUT_DOWN && ei >= 0 && !(glutGetModifiers() & GLUT_ACTIVE_SHIFT)) {
                /* select event; second click while selected opens editor */
                if (g_ev_sel == ei) {
                    g_ev_idx = ei;
                    g_mode = MODE_EVENT;
                    rebuild_event_nav();
                    set_status("Event Editor (double-click)");
                    return;
                }
                g_ev_sel = ei;
                g_ev_idx = ei;
                set_status("Event selected — click again to edit, or F2");
                return;
            }
            if (state == GLUT_DOWN) {
                g_ev_sel = -1;
                if (g_tool == 2) {
                    flood_fill(mx, my, 0);
                    set_status("Fill");
                } else if (g_tool == 1) {
                    g_rect_x0 = mx; g_rect_y0 = my;
                    g_painting = 1;
                } else {
                    g_painting = 1;
                    paint_cell(mx, my, g_tool == 3);
                }
            } else if (state == GLUT_UP && g_tool == 1 && g_rect_x0 >= 0) {
                paint_rect(g_rect_x0, g_rect_y0, mx, my, g_tool == 3);
                g_rect_x0 = g_rect_y0 = -1;
                g_painting = 0;
            }
        }
    }
    if (state == GLUT_UP && g_tool != 1) g_painting = 0;
}

static void motion(int x, int y) {
    int gy = WIN_H - y;
    int ox, oy, tw = tile_px();
    if (g_pan_drag && g_mode == MODE_MAP) {
        int dx = x - g_pan_lx;
        int dy = gy - g_pan_ly;
        if (dx <= -tw) { g_cam_x++; g_pan_lx = x; }
        if (dx >= tw) { if (g_cam_x > 0) g_cam_x--; g_pan_lx = x; }
        if (dy <= -tw) { if (g_cam_y > 0) g_cam_y--; g_pan_ly = gy; }
        if (dy >= tw) { g_cam_y++; g_pan_ly = gy; }
        return;
    }
    if (!g_painting || g_mode != MODE_MAP) return;
    if (g_tool == 1) {
        /* update rect preview cursor */
        ox = map_origin_x();
        oy = map_origin_y();
        if (x >= ox && gy >= oy) {
            int mx = g_cam_x + (x - ox) / tw;
            int my = g_cam_y + (g_proj.map.h - 1 - (gy - oy) / tw);
            if (mx >= 0 && my >= 0 && mx < g_proj.map.w && my < g_proj.map.h) {
                g_cursor_x = mx;
                g_cursor_y = my;
            }
        }
        return;
    }
    if (g_tool != 0 && g_tool != 3) return;
    ox = map_origin_x();
    oy = map_origin_y();
    if (x >= ox && gy >= oy) {
        int mx = g_cam_x + (x - ox) / tw;
        int my = g_cam_y + (g_proj.map.h - 1 - (gy - oy) / tw);
        if (mx >= 0 && my >= 0 && mx < g_proj.map.w && my < g_proj.map.h) {
            g_cursor_x = mx;
            g_cursor_y = my;
            paint_cell(mx, my, g_tool == 3);
        }
    }
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 'q' || key == 'Q') {
        if (g_proj.dirty) project_save(&g_proj);
        exit(0);
    }

    if (g_mode == MODE_PLAY) {
        if (g_play_sub == PLAY_MSG) {
            if (key == 13 || key == 10 || key == ' ') {
                g_play_sub = PLAY_WALK;
                g_msg[0] = 0;
                if (g_msg_event >= 0)
                    run_event_cmds_from(g_msg_event, g_cmd_resume);
            }
            return;
        }
        if (key == 27) { g_mode = MODE_MAP; set_status("Back to Map Editor"); return; }
        if (key == ' ' || key == 13) try_action();
        return;
    }

    if (g_mode == MODE_EVENT) {
        if (key == 27) {
            g_mode = MODE_MAP;
            set_status("Map Editor");
            return;
        }
        if (key == 13 || key == 10) {
            if (g_focus < g_n_nav && g_nav[g_focus].kind == 1) {
                if (g_nav[g_focus].arg == 5) { g_mode = MODE_MAP; return; }
                if (g_nav[g_focus].arg == 4) g_view_scratch = !g_view_scratch;
                if (g_nav[g_focus].arg == 0) {
                    if (g_ev_idx >= 0) project_save_event(&g_proj, g_ev_idx);
                    project_save(&g_proj);
                    set_status("Project saved");
                }
                if (g_nav[g_focus].arg == 2) {
                    int ei = project_add_event(&g_proj, g_cursor_x, g_cursor_y);
                    if (ei >= 0) { g_ev_idx = ei; set_status("Event added"); }
                }
                if (g_nav[g_focus].arg == 3 && g_ev_idx >= 0) {
                    g_proj.events[g_ev_idx].used = 0;
                    g_ev_idx = -1;
                    g_ev_sel = -1;
                    g_proj.dirty = 1;
                    set_status("Event deleted (save to flush files)");
                }
            }
            rebuild_event_nav();
            return;
        }
        if (key >= '1' && key <= '9') {
            int n = key - '0';
            if (n >= 1 && n <= g_n_nav) g_focus = n - 1;
            return;
        }
        return;
    }

    if (g_mode == MODE_DATABASE) {
        if (key == 27) { g_mode = MODE_MAP; return; }
        if (key >= '1' && key <= '3') g_db_tab = key - '1';
        return;
    }

    /* MAP mode */
    if (key == 27) {
        set_status("Map Editor — Q quit | F2 Event F3 Play F4 Database");
        return;
    }
    if (key == 's' || key == 'S') {
        project_save(&g_proj);
        set_status("Saved project (map + map_obj + events + switches)");
        return;
    }
    if (key == 'n' || key == 'N') {
        int ei = project_add_event(&g_proj, g_cursor_x, g_cursor_y);
        if (ei >= 0) {
            g_ev_idx = ei;
            g_ev_sel = ei;
            set_status("Event created at cursor — F2 or double-click to edit");
        }
        return;
    }
    if (key == 'p' || key == 'P') { g_tool = 0; set_status("Tool: Pencil"); return; }
    if (key == 'r' || key == 'R') { g_tool = 1; set_status("Tool: Rect"); return; }
    if (key == 'f' || key == 'F') { g_tool = 2; set_status("Tool: Fill"); return; }
    if (key == 'e' || key == 'E') { g_tool = 3; set_status("Tool: Erase"); return; }
    if (key == 'g' || key == 'G') { g_layer = 0; set_status("Layer: Ground"); return; }
    if (key == 'o' || key == 'O') { g_layer = 1; set_status("Layer: Objects"); return; }
    if (key == 'w' || key == 'W') { if (g_cam_y > 0) g_cam_y--; return; }
    if (key == 'a' || key == 'A') { if (g_cam_x > 0) g_cam_x--; return; }
    if (key == 'z' || key == 'Z') { g_cam_y++; return; }
    if (key == 'd' || key == 'D') { g_cam_x++; return; }
    if (key == '[') { g_zoom = 1; set_status("Zoom 1x"); return; }
    if (key == ']') { g_zoom = 2; set_status("Zoom 2x"); return; }
    if (key == 'h' || key == 'H') { g_show_grid = !g_show_grid; return; }
    if (key >= '1' && key <= '5') {
        g_ts_page = key - '1';
        return;
    }
    if (key == ' ') {
        paint_cell(g_cursor_x, g_cursor_y, g_tool == 3);
        return;
    }
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    if (key == GLUT_KEY_F1) {
        g_mode = MODE_MAP;
        set_status("Map Editor (MZ-style one page)");
        return;
    }
    if (key == GLUT_KEY_F2) {
        g_mode = MODE_EVENT;
        if (g_ev_idx < 0) {
            Event *e = project_event_at(&g_proj, g_cursor_x, g_cursor_y);
            if (e) g_ev_idx = (int)(e - g_proj.events);
        }
        rebuild_event_nav();
        set_status("Event Editor");
        return;
    }
    if (key == GLUT_KEY_F3) { play_start(); return; }
    if (key == GLUT_KEY_F4) {
        g_mode = MODE_DATABASE;
        set_status("Database");
        return;
    }

    if (g_mode == MODE_PLAY && g_play_sub == PLAY_WALK) {
        int nx = g_px, ny = g_py;
        if (key == GLUT_KEY_UP) ny--;
        else if (key == GLUT_KEY_DOWN) ny++;
        else if (key == GLUT_KEY_LEFT) nx--;
        else if (key == GLUT_KEY_RIGHT) nx++;
        if (walkable_at(nx, ny)) {
            g_px = nx; g_py = ny;
            /* touch triggers */
            {
                int i;
                for (i = 0; i < MAX_EVENTS; i++) {
                    Event *e = &g_proj.events[i];
                    if (e->used && e->trigger == TR_TOUCH && e->x == g_px && e->y == g_py)
                        run_event_cmds(i);
                }
            }
        }
        return;
    }

    if (g_mode == MODE_EVENT) {
        if (key == GLUT_KEY_UP) { g_focus--; if (g_focus < 0) g_focus = g_n_nav - 1; }
        if (key == GLUT_KEY_DOWN) { g_focus++; if (g_focus >= g_n_nav) g_focus = 0; }
        return;
    }

    if (g_mode == MODE_MAP) {
        if (key == GLUT_KEY_UP) { if (g_cursor_y > 0) g_cursor_y--; }
        else if (key == GLUT_KEY_DOWN) { if (g_cursor_y < g_proj.map.h - 1) g_cursor_y++; }
        else if (key == GLUT_KEY_LEFT) { if (g_cursor_x > 0) g_cursor_x--; }
        else if (key == GLUT_KEY_RIGHT) { if (g_cursor_x < g_proj.map.w - 1) g_cursor_x++; }
        /* keep cursor in view */
        {
            int tw = tile_px();
            int vis_w = map_view_w() / tw;
            int vis_h = map_view_h() / tw;
            if (g_cursor_x < g_cam_x) g_cam_x = g_cursor_x;
            if (g_cursor_y < g_cam_y) g_cam_y = g_cursor_y;
            if (g_cursor_x >= g_cam_x + vis_w) g_cam_x = g_cursor_x - vis_w + 1;
            if (g_cursor_y >= g_cam_y + vis_h) g_cam_y = g_cursor_y - vis_h + 1;
            if (g_cam_x < 0) g_cam_x = 0;
            if (g_cam_y < 0) g_cam_y = 0;
        }
    }
}

static void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

int main(int argc, char **argv) {
    const char *path = "projects/demo";
    if (argc > 1) path = argv[1];
    snprintf(g_project_path, sizeof(g_project_path), "%s", path);

    tileset_init();
    if (project_load(&g_proj, g_project_path) != 0) {
        project_new_demo_defaults(&g_proj, g_project_path);
        project_save(&g_proj);
    }
    project_scan_maps(&g_proj);

    g_cursor_x = g_proj.start_x > 0 ? g_proj.start_x : 5;
    g_cursor_y = g_proj.start_y > 0 ? g_proj.start_y : 5;
    g_mode = MODE_MAP;
    set_status("Map Editor v4 — tiles animate | zoom [ ] | MMB pan | click event twice");

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("planet aether - RPG Maker MZ* (house clone)");
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutReshapeFunc(reshape);
    glutTimerFunc(16, timer, 0);

    fprintf(stderr,
            "rpg-maker-clone MZ v4\n"
            "  ONE PAGE: tileset A-R | map tree | canvas | toolbar\n"
            "  Tools P/R/F/E  Layers G/O  Zoom [/]  Grid H  MMB pan\n"
            "  F2 Event | F3 Play | F4 Database | S save | N event\n");
    glutMainLoop();
    return 0;
}
