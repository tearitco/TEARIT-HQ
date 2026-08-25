/* render.c — Game Boy Color palette, overworld tiles, battle UI
 * Full RGB vivid GBC look (NOT monochrome greenscale).
 * Layout coords: origin top-left, y down; converted to GL y-up in fill_rect/text.
 */
#include "gb.h"
#include <GL/glut.h>

static void c_rgb(float r, float g, float b) {
    glColor3f(r, g, b);
}

/* Layout (top-left y-down) → GL y-up */
static void fill_rect(float x, float y, float w, float h) {
    float y0 = (float)GB_H - y - h;
    float y1 = (float)GB_H - y;
    glBegin(GL_QUADS);
    glVertex2f(x, y0);
    glVertex2f(x + w, y0);
    glVertex2f(x + w, y1);
    glVertex2f(x, y1);
    glEnd();
}

/* GBC dialog: navy border, white paper */
static void frame_rect(float x, float y, float w, float h, float t) {
    c_rgb(0.10f, 0.12f, 0.28f);
    fill_rect(x, y, w, h);
    c_rgb(0.98f, 0.98f, 0.95f);
    fill_rect(x + t, y + t, w - 2 * t, h - 2 * t);
    c_rgb(0.20f, 0.22f, 0.45f);
    fill_rect(x + t * 2, y + t * 2, w - 4 * t, h - 4 * t);
    c_rgb(0.98f, 0.98f, 0.95f);
    fill_rect(x + t * 3, y + t * 3, w - 6 * t, h - 6 * t);
}

static void text_rgb(float x, float y, const char *s, float r, float g, float b) {
    float gy = (float)GB_H - y;
    c_rgb(r, g, b);
    glRasterPos2f(x, gy);
    while (s && *s) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, (int)(unsigned char)*s);
        s++;
    }
}

static void text_at(float x, float y, const char *s) {
    text_rgb(x, y, s, 0.05f, 0.05f, 0.10f);
}

void render_init_gl(void) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glClearColor(0.45f, 0.72f, 0.95f, 1.f);
}

static void set_ortho_gb(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* y-up so GLUT bitmap fonts are upright */
    glOrtho(0, GB_W, 0, GB_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* ---- vivid GBC tiles ---- */
static void draw_tile(int tx, int ty, TileId t) {
    float px = (float)(tx * TILE_PX);
    float py = (float)(ty * TILE_PX);
    float s = (float)TILE_PX;
    int checker = (tx + ty) & 1;

    switch (t) {
    case TILE_WALL:
        /* brick red / brown walls */
        c_rgb(0.55f, 0.32f, 0.22f);
        fill_rect(px, py, s, s);
        c_rgb(0.72f, 0.42f, 0.30f);
        fill_rect(px + 1, py + 1, s - 2, s - 2);
        c_rgb(0.40f, 0.22f, 0.15f);
        fill_rect(px + 1, py + 7, s - 2, 1);
        fill_rect(px + 8, py + 1, 1, 6);
        fill_rect(px + 4, py + 8, 1, 6);
        break;

    case TILE_WATER:
        /* bright GBC ocean blue + wave highlights */
        c_rgb(0.15f, 0.45f, 0.90f);
        fill_rect(px, py, s, s);
        c_rgb(0.25f, 0.60f, 1.00f);
        fill_rect(px, py, s, s * 0.55f);
        c_rgb(0.55f, 0.85f, 1.00f);
        fill_rect(px + 2, py + 4, 6, 2);
        fill_rect(px + 8, py + 10, 6, 2);
        c_rgb(0.10f, 0.30f, 0.70f);
        fill_rect(px, py + s - 2, s, 2);
        break;

    case TILE_TALL:
        /* vivid tall grass (encounter tiles) */
        c_rgb(0.18f, 0.55f, 0.18f);
        fill_rect(px, py, s, s);
        if (checker) {
            c_rgb(0.14f, 0.48f, 0.14f);
            fill_rect(px, py, s, s);
        }
        c_rgb(0.25f, 0.78f, 0.22f);
        fill_rect(px + 2, py + 3, 3, 11);
        fill_rect(px + 6, py + 1, 3, 13);
        fill_rect(px + 11, py + 4, 3, 10);
        c_rgb(0.55f, 0.95f, 0.35f);
        fill_rect(px + 2, py + 3, 3, 3);
        fill_rect(px + 6, py + 1, 3, 3);
        fill_rect(px + 11, py + 4, 3, 3);
        break;

    case TILE_GRASS:
        /* lawn green */
        c_rgb(0.35f, 0.78f, 0.28f);
        fill_rect(px, py, s, s);
        if (checker) {
            c_rgb(0.30f, 0.70f, 0.24f);
            fill_rect(px + 1, py + 1, s - 2, s - 2);
        }
        c_rgb(0.45f, 0.88f, 0.32f);
        fill_rect(px + 3, py + 10, 2, 3);
        fill_rect(px + 10, py + 6, 2, 3);
        break;

    case TILE_PC:
        /* Pokecenter pink floor + red cross */
        c_rgb(1.00f, 0.75f, 0.80f);
        fill_rect(px, py, s, s);
        c_rgb(0.95f, 0.20f, 0.25f);
        fill_rect(px + 3, py + 3, s - 6, s - 6);
        c_rgb(1.00f, 1.00f, 1.00f);
        fill_rect(px + 6, py + 5, 4, 6);
        fill_rect(px + 4, py + 7, 8, 3);
        break;

    case TILE_HOUSE:
        /* warm wood floor */
        c_rgb(0.72f, 0.52f, 0.28f);
        fill_rect(px, py, s, s);
        c_rgb(0.85f, 0.65f, 0.38f);
        fill_rect(px + 1, py + 1, s - 2, s - 2);
        c_rgb(0.55f, 0.38f, 0.18f);
        fill_rect(px + 1, py + 5, s - 2, 1);
        fill_rect(px + 1, py + 10, s - 2, 1);
        break;

    case TILE_PATH:
    default:
        /* tan dirt path */
        c_rgb(0.82f, 0.70f, 0.45f);
        fill_rect(px, py, s, s);
        if (checker) {
            c_rgb(0.74f, 0.60f, 0.38f);
            fill_rect(px + 1, py + 1, s - 2, s - 2);
        }
        c_rgb(0.90f, 0.80f, 0.55f);
        fill_rect(px + 6, py + 6, 3, 3);
        break;
    }
}

static void draw_player_sprite(int vx, int vy, int facing) {
    float px = (float)(vx * TILE_PX);
    float py = (float)(vy * TILE_PX);
    /* red-hat trainer (GBC Red vibe) */
    c_rgb(0.90f, 0.15f, 0.15f);
    fill_rect(px + 4, py + 2, 8, 4);
    c_rgb(1.00f, 0.85f, 0.65f);
    fill_rect(px + 5, py + 6, 6, 4);
    c_rgb(0.15f, 0.25f, 0.75f);
    fill_rect(px + 4, py + 10, 8, 5);
    c_rgb(0.20f, 0.20f, 0.25f);
    fill_rect(px + 4, py + 14, 3, 2);
    fill_rect(px + 9, py + 14, 3, 2);
    c_rgb(1.f, 1.f, 1.f);
    if (facing == 0) fill_rect(px + 7, py + 1, 2, 2);
    else if (facing == 1) fill_rect(px + 13, py + 8, 2, 2);
    else if (facing == 2) fill_rect(px + 7, py + 14, 2, 2);
    else fill_rect(px + 1, py + 8, 2, 2);
}

static void draw_hp_bar(float x, float y, float w, float h, int hp, int max_hp) {
    float ratio;
    if (max_hp < 1) max_hp = 1;
    ratio = (float)hp / (float)max_hp;
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 1.f) ratio = 1.f;
    c_rgb(0.10f, 0.10f, 0.15f);
    fill_rect(x, y, w, h);
    c_rgb(0.95f, 0.95f, 0.95f);
    fill_rect(x + 1, y + 1, w - 2, h - 2);
    if (ratio > 0.5f)
        c_rgb(0.15f, 0.82f, 0.25f);
    else if (ratio > 0.2f)
        c_rgb(0.95f, 0.80f, 0.10f);
    else
        c_rgb(0.95f, 0.18f, 0.12f);
    fill_rect(x + 1, y + 1, (w - 2) * ratio, h - 2);
}

static void draw_mon_blob(float x, float y, int type, int species_id, int flash) {
    float body_r, body_g, body_b;
    float accent_r, accent_g, accent_b;

    if (flash && (flash % 2)) return;

    switch (type) {
    case TYPE_GRASS:
        body_r = 0.25f; body_g = 0.82f; body_b = 0.28f;
        accent_r = 0.95f; accent_g = 0.55f; accent_b = 0.65f;
        break;
    case TYPE_FIRE:
        body_r = 0.95f; body_g = 0.40f; body_b = 0.12f;
        accent_r = 1.00f; accent_g = 0.85f; accent_b = 0.20f;
        break;
    case TYPE_WATER:
        body_r = 0.25f; body_g = 0.55f; body_b = 0.95f;
        accent_r = 0.55f; accent_g = 0.85f; accent_b = 1.00f;
        break;
    default:
        body_r = 0.85f; body_g = 0.75f; body_b = 0.45f;
        accent_r = 0.55f; accent_g = 0.40f; accent_b = 0.25f;
        break;
    }

    if (species_id == 5) { /* RATTATA purple */
        body_r = 0.55f; body_g = 0.35f; body_b = 0.65f;
        accent_r = 0.90f; accent_g = 0.70f; accent_b = 0.85f;
    } else if (species_id == 8) { /* MAGIKARP orange */
        body_r = 0.95f; body_g = 0.45f; body_b = 0.20f;
        accent_r = 0.95f; accent_g = 0.85f; accent_b = 0.30f;
    } else if (species_id == 9) { /* VULPIX */
        body_r = 0.90f; body_g = 0.50f; body_b = 0.25f;
        accent_r = 1.00f; accent_g = 0.90f; accent_b = 0.70f;
    } else if (species_id == 6 || species_id == 7) { /* bugs */
        body_r = 0.55f; body_g = 0.85f; body_b = 0.25f;
        accent_r = 0.95f; accent_g = 0.90f; accent_b = 0.30f;
    }

    c_rgb(body_r, body_g, body_b);
    fill_rect(x + 2, y + 4, 24, 18);
    c_rgb(body_r * 0.9f, body_g * 0.9f, body_b * 0.9f);
    fill_rect(x + 6, y, 16, 8);
    c_rgb(accent_r, accent_g, accent_b);
    fill_rect(x + 4, y + 14, 8, 4);
    fill_rect(x + 16, y + 14, 8, 4);
    c_rgb(0.05f, 0.05f, 0.08f);
    fill_rect(x + 8, y + 6, 3, 3);
    fill_rect(x + 17, y + 6, 3, 3);
    c_rgb(1.f, 1.f, 1.f);
    fill_rect(x + 9, y + 6, 1, 1);
    fill_rect(x + 18, y + 6, 1, 1);
    c_rgb(0.20f, 0.10f, 0.10f);
    fill_rect(x + 11, y + 12, 6, 2);
}

static void type_color(int type, float *r, float *g, float *b) {
    switch (type) {
    case TYPE_GRASS: *r = 0.30f; *g = 0.75f; *b = 0.30f; break;
    case TYPE_FIRE:  *r = 0.90f; *g = 0.35f; *b = 0.15f; break;
    case TYPE_WATER: *r = 0.25f; *g = 0.50f; *b = 0.95f; break;
    default:         *r = 0.70f; *g = 0.70f; *b = 0.65f; break;
    }
}

static void render_overworld(Game *g) {
    int cam_x, cam_y, tx, ty, vx, vy;
    TileId t;

    cam_x = g->player.x - VIEW_TW / 2;
    cam_y = g->player.y - VIEW_TH / 2;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > g->map.w - VIEW_TW) cam_x = g->map.w - VIEW_TW;
    if (cam_y > g->map.h - VIEW_TH) cam_y = g->map.h - VIEW_TH;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;

    for (vy = 0; vy < VIEW_TH; vy++) {
        for (vx = 0; vx < VIEW_TW; vx++) {
            tx = cam_x + vx;
            ty = cam_y + vy;
            t = map_tile(&g->map, tx, ty);
            draw_tile(vx, vy, t);
        }
    }

    vx = g->player.x - cam_x;
    vy = g->player.y - cam_y;
    if (vx >= 0 && vy >= 0 && vx < VIEW_TW && vy < VIEW_TH)
        draw_player_sprite(vx, vy, g->player.facing);

    /* GBC HUD — navy + yellow accent */
    c_rgb(0.12f, 0.18f, 0.40f);
    fill_rect(0, GB_H - 14, GB_W, 14);
    c_rgb(0.95f, 0.80f, 0.20f);
    fill_rect(0, GB_H - 14, GB_W, 2);
    {
        char hud[80];
        const PartyMon *p = g->player.party_n > 0 ? &g->player.party[0] : NULL;
        if (p) {
            const Species *sp = mon_species(g, p->species);
            snprintf(hud, sizeof(hud), "%s Lv%d HP%d/%d  F5=save",
                     sp ? sp->name : "?", p->level, p->hp, p->max_hp);
        } else {
            snprintf(hud, sizeof(hud), "No mon  arrows move");
        }
        text_rgb(4, GB_H - 4, hud, 1.f, 1.f, 0.95f);
    }
}

static void render_battle(Game *g) {
    Battle *b = &g->battle;
    PartyMon *p = NULL;
    const Species *ws, *ps;
    char buf[64];
    float tr, tg, tb;
    int i;

    for (i = 0; i < g->player.party_n; i++) {
        if (g->player.party[i].hp > 0) { p = &g->player.party[i]; break; }
    }
    if (!p && g->player.party_n > 0) p = &g->player.party[0];

    ws = mon_species(g, b->wild.species);
    ps = p ? mon_species(g, p->species) : NULL;

    /* sky + grass field */
    c_rgb(0.45f, 0.75f, 0.95f);
    fill_rect(0, 0, GB_W, 70);
    c_rgb(0.55f, 0.82f, 0.35f);
    fill_rect(0, 55, GB_W, 55);
    c_rgb(0.40f, 0.65f, 0.25f);
    fill_rect(0, 100, GB_W, 12);

    type_color(ws ? ws->type : 0, &tr, &tg, &tb);
    c_rgb(tr, tg, tb);
    fill_rect(100, 8, 56, 4);

    draw_mon_blob(110, 14, ws ? ws->type : 0, b->wild.species, b->flash);

    frame_rect(4, 4, 92, 38, 1);
    snprintf(buf, sizeof(buf), "%s  Lv%d", ws ? ws->name : "?", b->wild.level);
    text_at(10, 16, buf);
    draw_hp_bar(10, 22, 72, 7, b->wild.hp, b->wild.max_hp);
    snprintf(buf, sizeof(buf), "%d/%d", b->wild.hp, b->wild.max_hp);
    text_at(10, 36, buf);

    if (p) {
        type_color(ps ? ps->type : 0, &tr, &tg, &tb);
        c_rgb(tr, tg, tb);
        fill_rect(10, 74, 40, 4);
        draw_mon_blob(16, 78, ps ? ps->type : 0, p->species, 0);
        frame_rect(64, 70, 92, 38, 1);
        snprintf(buf, sizeof(buf), "%s  Lv%d", ps ? ps->name : "?", p->level);
        text_at(70, 82, buf);
        draw_hp_bar(70, 88, 72, 7, p->hp, p->max_hp);
        snprintf(buf, sizeof(buf), "%d/%d", p->hp, p->max_hp);
        text_at(70, 102, buf);
    }

    frame_rect(2, 112, GB_W - 4, 30, 1);
    text_at(8, 124, b->line);

    if (b->phase == BPHASE_MENU && b->wait_frames == 0) {
        c_rgb(0.15f, 0.15f, 0.30f);
        fill_rect(96, 112, 62, 30);
        c_rgb(0.98f, 0.95f, 0.90f);
        fill_rect(98, 114, 58, 26);

        if (b->menu_sel == 0) {
            c_rgb(0.90f, 0.25f, 0.20f);
            fill_rect(100, 116, 54, 11);
            text_rgb(104, 125, ">FIGHT", 1.f, 1.f, 1.f);
            text_at(104, 136, " RUN");
        } else {
            text_at(104, 125, " FIGHT");
            c_rgb(0.20f, 0.40f, 0.90f);
            fill_rect(100, 128, 54, 11);
            text_rgb(104, 136, ">RUN", 1.f, 1.f, 1.f);
        }

        {
            const MoveDef *m0 = mon_move(g, p ? p->move_id[0] : 1);
            const MoveDef *m1 = mon_move(g, p ? p->move_id[1] : 1);
            snprintf(buf, sizeof(buf), "1:%s 2:%s",
                     m0 ? m0->name : "?", m1 ? m1->name : "?");
            text_at(8, 136, buf);
        }
    }
}

static void render_title(Game *g) {
    c_rgb(0.30f, 0.55f, 0.95f);
    fill_rect(0, 0, GB_W, 50);
    c_rgb(0.40f, 0.75f, 0.35f);
    fill_rect(0, 50, GB_W, GB_H - 50);

    c_rgb(0.95f, 0.85f, 0.15f);
    fill_rect(12, 12, GB_W - 24, 36);
    c_rgb(0.90f, 0.20f, 0.20f);
    fill_rect(14, 14, GB_W - 28, 32);
    text_rgb(22, 28, "GBC POKEMON MVP", 1.f, 1.f, 0.95f);
    text_rgb(30, 40, "FULL COLOR EDITION", 1.f, 0.95f, 0.40f);

    frame_rect(30, 58, 100, 58, 1);
    text_at(44, 74,  g->title_sel == 0 ? "> NEW GAME" : "  NEW GAME");
    text_at(44, 88,  g->title_sel == 1 ? "> CONTINUE" : "  CONTINUE");
    text_at(44, 102, g->title_sel == 2 ? "> QUIT" : "  QUIT");

    text_rgb(16, 132, "Arrows+Z   q=quit", 0.10f, 0.20f, 0.15f);
    text_rgb(16, 142, "Game Boy Color", 0.15f, 0.25f, 0.65f);
}

static void render_starter(Game *g) {
    const char *names[3] = {"LEAFY Grass", "EMBER Fire", "BUBBLE Water"};
    float cols[3][3] = {
        {0.30f, 0.80f, 0.30f},
        {0.95f, 0.40f, 0.15f},
        {0.25f, 0.50f, 0.95f}
    };
    int i;

    c_rgb(0.55f, 0.80f, 0.95f);
    fill_rect(0, 0, GB_W, GB_H);
    text_rgb(20, 14, "Choose your starter!", 0.10f, 0.15f, 0.35f);

    for (i = 0; i < 3; i++) {
        float x = 10.f + i * 50.f;
        if (g->starter_sel == i) {
            c_rgb(cols[i][0], cols[i][1], cols[i][2]);
            fill_rect(x - 2, 30, 48, 60);
        }
        frame_rect(x, 32, 44, 56, 1);
        draw_mon_blob(x + 8, 40, i + 1, i + 1, 0);
        if (g->starter_sel == i)
            text_rgb(x + 14, 82, "OK", cols[i][0], cols[i][1] * 0.5f, cols[i][2] * 0.5f);
    }

    {
        float r = cols[g->starter_sel][0];
        float gg = cols[g->starter_sel][1];
        float b = cols[g->starter_sel][2];
        c_rgb(r, gg, b);
        fill_rect(8, 100, GB_W - 16, 18);
        text_rgb(16, 112, names[g->starter_sel], 1.f, 1.f, 1.f);
    }
    text_rgb(16, 132, "Left/Right  Z=OK", 0.15f, 0.20f, 0.30f);
}

static void render_msg(Game *g) {
    render_overworld(g);
    c_rgb(0.05f, 0.05f, 0.15f);
    fill_rect(0, 40, GB_W, 64);
    frame_rect(8, 48, GB_W - 16, 48, 1);
    text_at(16, 68, g->msg);
    text_rgb(16, 84, "Z/Enter...", 0.30f, 0.30f, 0.45f);
}

void render_frame(Game *g) {
    glViewport(0, 0, WIN_W, WIN_H);
    glClear(GL_COLOR_BUFFER_BIT);
    set_ortho_gb();

    switch (g->mode) {
    case MODE_TITLE:     render_title(g); break;
    case MODE_STARTER:   render_starter(g); break;
    case MODE_OVERWORLD: render_overworld(g); break;
    case MODE_BATTLE:    render_battle(g); break;
    case MODE_MSG:       render_msg(g); break;
    default:             render_title(g); break;
    }

    glutSwapBuffers();
}
