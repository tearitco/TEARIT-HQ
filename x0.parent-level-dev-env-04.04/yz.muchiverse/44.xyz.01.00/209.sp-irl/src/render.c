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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

    case TILE_GYM: {
        /* dark brick gym */
        c_rgb(0.30f, 0.18f, 0.12f);
        fill_rect(px, py, s, s);
        c_rgb(0.45f, 0.28f, 0.18f);
        fill_rect(px + 1, py + 1, s - 2, s - 2);
        c_rgb(0.95f, 0.85f, 0.20f);
        fill_rect(px + 4, py + 4, s - 8, s - 8);
        c_rgb(0.15f, 0.10f, 0.08f);
        fill_rect(px + 6, py + 6, s - 12, s - 12);
        c_rgb(1.00f, 1.00f, 1.00f);
        fill_rect(px + 5, py + s/2, s - 10, 3);
        } break;

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

    /* draw trainers */
    {
        int ti;
        for (ti = 0; ti < g->map.trainers_n; ti++) {
            Trainer *tr = &g->map.trainers[ti];
            int tvx = tr->x - cam_x, tvy = tr->y - cam_y;
            if (tvx < 0 || tvy < 0 || tvx >= VIEW_TW || tvy >= VIEW_TH) continue;
            float px = (float)(tvx * TILE_PX), py = (float)(tvy * TILE_PX);
            if (tr->defeated) {
                c_rgb(0.40f, 0.40f, 0.45f);
            } else {
                c_rgb(0.20f, 0.30f, 0.90f);
            }
            fill_rect(px + 4, py + 3, 8, 4);
            if (tr->defeated) c_rgb(0.50f, 0.45f, 0.40f);
            else c_rgb(1.00f, 0.85f, 0.65f);
            fill_rect(px + 5, py + 7, 6, 4);
            if (tr->defeated) c_rgb(0.35f, 0.35f, 0.45f);
            else c_rgb(0.15f, 0.25f, 0.75f);
            fill_rect(px + 4, py + 11, 8, 5);
            if (tr->defeated) c_rgb(0.35f, 0.35f, 0.35f);
            else c_rgb(0.20f, 0.20f, 0.25f);
            fill_rect(px + 4, py + 15, 3, 2);
            fill_rect(px + 9, py + 15, 3, 2);
        }
    }

    vx = g->player.x - cam_x;
    vy = g->player.y - cam_y;
    if (vx >= 0 && vy >= 0 && vx < VIEW_TW && vy < VIEW_TH)
        draw_player_sprite(vx, vy, g->player.facing);

    /* connection markers — small green arrows at map exits */
    {
        int ci, cvx, cvy;
        for (ci = 0; ci < g->map.conns_n; ci++) {
            cvx = g->map.conns[ci].sx - cam_x;
            cvy = g->map.conns[ci].sy - cam_y;
            if (cvx < 0 || cvy < 0 || cvx >= VIEW_TW || cvy >= VIEW_TH) continue;
            c_rgb(0.20f, 0.80f, 0.20f);
            float cpx = (float)(cvx * TILE_PX), cpy = (float)(cvy * TILE_PX);
            fill_rect(cpx + 4, cpy + 1, 8, 2);
            fill_rect(cpx + 6, cpy + 3, 4, 2);
            fill_rect(cpx + 7, cpy + 5, 2, 2);
        }
    }

    /* GBC HUD — navy + yellow accent */
    c_rgb(0.12f, 0.18f, 0.40f);
    fill_rect(0, GB_H - 14, GB_W, 14);
    c_rgb(0.95f, 0.80f, 0.20f);
    fill_rect(0, GB_H - 14, GB_W, 2);
    {
        char hud[80];
        const PartyMon *p = g->player.party_n > 0 ? &g->player.party[0] : NULL;
        text_rgb(4, GB_H - 14, g->map.name, 0.70f, 0.70f, 0.90f);
        if (p) {
            const Species *sp = mon_species(g, p->species);
            snprintf(hud, sizeof(hud), "%s Lv%d HP%d/%d  M=party B=bag F=fly",
                     sp ? sp->name : "?", p->level, p->hp, p->max_hp);
        } else {
            snprintf(hud, sizeof(hud), "No mon  M=party B=bag F=fly");
        }
        text_rgb(4, GB_H - 4, hud, 1.f, 1.f, 0.95f);
    }
}

static void render_title(Game *g) {
    c_rgb(0.10f, 0.10f, 0.20f);
    fill_rect(0, 0, GB_W, GB_H);

    c_rgb(0.95f, 0.85f, 0.10f);
    fill_rect(12, 8, GB_W - 24, 36);
    c_rgb(0.15f, 0.15f, 0.30f);
    fill_rect(14, 10, GB_W - 28, 32);
    text_rgb(28, 22, "SP-IRL", 1.f, 0.85f, 0.15f);
    text_rgb(32, 34, "tactical monsters", 0.60f, 0.60f, 0.80f);

    frame_rect(30, 52, 100, 72, 1);
    text_at(44, 64,  g->title_sel == 0 ? "> NEW GAME" : "  NEW GAME");
    text_at(44, 76,  g->title_sel == 1 ? "> CONTINUE" : "  CONTINUE");
    text_at(44, 88,  g->title_sel == 2 ? "> PVP BATTLE" : "  PVP BATTLE");
    text_at(44, 100, g->title_sel == 3 ? "> QUIT" : "  QUIT");

    text_rgb(12, 130, "Arrows+Z  q=quit", 0.50f, 0.50f, 0.70f);
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

static void render_party(Game *g) {
    int i, y;

    c_rgb(0.08f, 0.08f, 0.18f);
    fill_rect(0, 0, GB_W, GB_H);

    c_rgb(0.95f, 0.85f, 0.10f);
    fill_rect(0, 0, GB_W, 12);
    text_rgb(4, 2, "PARTY  (M=close)", 0.10f, 0.10f, 0.20f);

    if (g->player.party_n == 0) {
        text_rgb(30, 60, "No mons yet!", 0.60f, 0.60f, 0.80f);
        return;
    }

    for (i = 0; i < g->player.party_n; i++) {
        PartyMon *pm = &g->player.party[i];
        const Species *sp = mon_species(g, pm->species);
        y = 16 + i * 20;

        if (i == g->party_sel && g->party_sub == 0) {
            c_rgb(0.20f, 0.20f, 0.40f);
            fill_rect(2, y, GB_W - 4, 18);
        }

        /* type color swatch */
        float tr = 0.85f, tg = 0.75f, tb = 0.45f;
        if (sp) {
            if (sp->type == TYPE_GRASS) { tr=0.25f; tg=0.82f; tb=0.28f; }
            else if (sp->type == TYPE_FIRE) { tr=0.95f; tg=0.40f; tb=0.12f; }
            else if (sp->type == TYPE_WATER) { tr=0.25f; tg=0.55f; tb=0.95f; }
        }
        c_rgb(tr, tg, tb);
        fill_rect(4, y + 1, 4, 16);

        /* name + level */
        if (sp) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s  Lv%d", sp->name, pm->level);
            text_rgb(12, y + 2, buf, 1.0f, 1.0f, 0.95f);
        }

        /* HP bar */
        float hp_ratio = pm->max_hp > 0 ? (float)pm->hp / (float)pm->max_hp : 0;
        if (hp_ratio < 0) hp_ratio = 0;
        c_rgb(0.10f, 0.10f, 0.15f);
        fill_rect(90, y + 2, 60, 6);
        if (hp_ratio > 0.5f) c_rgb(0.15f, 0.82f, 0.25f);
        else if (hp_ratio > 0.2f) c_rgb(0.95f, 0.80f, 0.10f);
        else c_rgb(0.95f, 0.18f, 0.12f);
        fill_rect(90, y + 2, (int)(hp_ratio * 60), 6);

        /* HP text */
        if (pm->hp > 0) {
            char hp_buf[20];
            snprintf(hp_buf, sizeof(hp_buf), "%d/%d", pm->hp, pm->max_hp);
            text_rgb(90, y + 10, hp_buf, 0.70f, 0.70f, 0.80f);
        } else {
            text_rgb(90, y + 10, "FAINTED", 0.95f, 0.18f, 0.12f);
        }

        /* selected indicator */
        if (i == g->party_sel && g->party_sub == 0) {
            text_rgb(GB_W - 14, y + 2, ">", 0.95f, 0.85f, 0.10f);
        }
    }

    /* move detail sub-view */
    if (g->party_sub == 1 && g->party_sel >= 0 && g->party_sel < g->player.party_n) {
        PartyMon *pm = &g->player.party[g->party_sel];
        const MoveDef *m0 = mon_move(g, pm->move_id[0]);
        const MoveDef *m1 = mon_move(g, pm->move_id[1]);

        c_rgb(0.05f, 0.05f, 0.15f);
        fill_rect(0, GB_H - 40, GB_W, 40);
        c_rgb(0.95f, 0.80f, 0.20f);
        fill_rect(0, GB_H - 42, GB_W, 2);

        int my = GB_H - 36;
        if (m0) {
            char buf[48];
            snprintf(buf, sizeof(buf), "1: %s  power=%d  PP=%d/%d",
                     m0->name, m0->power, pm->pp[0], m0->max_pp);
            text_rgb(4, my, buf, 0.90f, 0.90f, 1.00f);
        }
        if (m1) {
            char buf[48];
            snprintf(buf, sizeof(buf), "2: %s  power=%d  PP=%d/%d",
                     m1->name, m1->power, pm->pp[1], m1->max_pp);
            text_rgb(4, my + 14, buf, 0.90f, 0.90f, 1.00f);
        }
        text_rgb(4, GB_H - 6, "Z=back  X=close", 0.50f, 0.50f, 0.70f);
    } else {
        text_rgb(4, GB_H - 6, "Z=view moves  X=close  arrows=move", 0.50f, 0.50f, 0.70f);
    }
}

static void render_bag(Game *g) {
    int i, y;
    c_rgb(0.08f, 0.08f, 0.18f);
    fill_rect(0, 0, GB_W, GB_H);

    c_rgb(0.30f, 0.60f, 1.00f);
    fill_rect(0, 0, GB_W, 12);
    text_rgb(4, 2, "BAG  (B=close)", 0.10f, 0.10f, 0.20f);

    if (g->player.bag_n == 0) {
        text_rgb(30, 60, "Bag is empty!", 0.60f, 0.60f, 0.80f);
        return;
    }

    for (i = 0; i < g->player.bag_n; i++) {
        const ItemDef *def = item_def(g, g->player.bag[i].item_id);
        y = 16 + i * 18;

        if (i == g->bag_sel) {
            c_rgb(0.20f, 0.20f, 0.40f);
            fill_rect(2, y, GB_W - 4, 16);
        }

        if (def) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s x%d", def->name, g->player.bag[i].count);
            text_rgb(8, y + 2, buf, 1.0f, 1.0f, 0.95f);

            /* type label */
            const char *tag = "";
            if (def->type == ITEM_HEAL) tag = "HEAL";
            else if (def->type == ITEM_CATCH) tag = "CATCH";
            text_rgb(100, y + 2, tag, 0.50f, 0.50f, 0.70f);
        }

        if (i == g->bag_sel) {
            text_rgb(GB_W - 14, y + 2, ">", 0.95f, 0.85f, 0.10f);
        }
    }

    text_rgb(4, GB_H - 6, "Z=use  X=close  arrows=move", 0.50f, 0.50f, 0.70f);
}

static void render_bag_use(Game *g) {
    int i, y;

    c_rgb(0.08f, 0.08f, 0.18f);
    fill_rect(0, 0, GB_W, GB_H);

    const ItemDef *def = g->bag_sel >= 0 && g->bag_sel < g->player.bag_n
        ? item_def(g, g->player.bag[g->bag_sel].item_id) : NULL;

    c_rgb(0.30f, 1.00f, 0.60f);
    fill_rect(0, 0, GB_W, 12);
    text_rgb(4, 2, "Use on which mon?  (X=back)", 0.10f, 0.10f, 0.20f);

    if (g->player.party_n == 0) {
        text_rgb(30, 60, "No mons!", 0.60f, 0.60f, 0.80f);
        return;
    }

    for (i = 0; i < g->player.party_n; i++) {
        PartyMon *pm = &g->player.party[i];
        const Species *sp = mon_species(g, pm->species);
        y = 16 + i * 20;

        if (i == g->bag_use_target) {
            c_rgb(0.20f, 0.40f, 0.25f);
            fill_rect(2, y, GB_W - 4, 18);
        }

        if (sp) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s Lv%d  HP %d/%d",
                     sp->name, pm->level, pm->hp, pm->max_hp);
            text_rgb(8, y + 2, buf, 1.0f, 1.0f, 0.95f);

            if (pm->hp <= 0) {
                text_rgb(110, y + 2, "FAINTED", 0.95f, 0.18f, 0.12f);
            } else if (pm->hp >= pm->max_hp && def && def->type == ITEM_HEAL) {
                text_rgb(110, y + 2, "FULL HP", 0.50f, 0.80f, 0.50f);
            }
        }

        if (i == g->bag_use_target) {
            text_rgb(GB_W - 14, y + 2, ">", 0.95f, 0.85f, 0.10f);
        }
    }

    text_rgb(4, GB_H - 6, "Z=use  X=back  arrows=move", 0.50f, 0.50f, 0.70f);
}

static void render_pvp_setup(Game *g) {
    (void)g;
    tactics_pvp_start(g);
}

static void draw_mini_mon(Game *g, float x, float y, int species_id, int greyed) {
    const Species *sp = mon_species(g, species_id);
    int type = sp ? sp->type : 0;
    float br, bg, bb, f = greyed ? 0.35f : 1.0f;
    switch (type) {
    case TYPE_GRASS: br=0.25f; bg=0.82f; bb=0.28f; break;
    case TYPE_FIRE:  br=0.95f; bg=0.40f; bb=0.12f; break;
    case TYPE_WATER: br=0.25f; bg=0.55f; bb=0.95f; break;
    default:         br=0.85f; bg=0.75f; bb=0.45f; break;
    }
    if (species_id == 5) { br=0.55f; bg=0.35f; bb=0.65f; }
    else if (species_id == 8) { br=0.95f; bg=0.45f; bb=0.20f; }
    else if (species_id == 9) { br=0.90f; bg=0.50f; bb=0.25f; }
    else if (species_id == 6 || species_id == 7) { br=0.55f; bg=0.85f; bb=0.25f; }

    c_rgb(br * f, bg * f, bb * f);
    fill_rect(x+1, y+1, 8, 8);

    c_rgb(1.0f * f, 1.0f * f, 1.0f * f);
    fill_rect(x+3, y+3, 2, 2);
    fill_rect(x+6, y+3, 2, 2);

    c_rgb(1.0f * f, 1.0f * f, 1.0f * f);
    if (!greyed) {
        c_rgb(0.05f * f, 0.05f * f, 0.08f * f);
        fill_rect(x+3, y+8, 4, 1);
    }

    switch (species_id) {
    case 1: /* LEAFY — leaf on top */
        c_rgb(0.30f * f, 0.90f * f, 0.30f * f);
        fill_rect(x+4, y-1, 3, 3); fill_rect(x+3, y, 1, 2);
        break;
    case 2: /* EMBER — flame */
        c_rgb(1.00f * f, 0.85f * f, 0.00f * f);
        fill_rect(x+4, y-1, 2, 3); fill_rect(x+5, y-2, 1, 2);
        break;
    case 3: /* BUBBLE — water splash */
        c_rgb(0.60f * f, 0.85f * f, 1.00f * f);
        fill_rect(x+5, y-1, 3, 2); fill_rect(x+6, y-2, 1, 2);
        break;
    case 4: /* PIDGEY — beak */
        c_rgb(1.00f * f, 0.70f * f, 0.30f * f);
        fill_rect(x+8, y+3, 2, 2);
        break;
    case 5: /* RATTATA — big ears */
        c_rgb(0.70f * f, 0.50f * f, 0.80f * f);
        fill_rect(x+1, y-2, 3, 3); fill_rect(x+7, y-2, 3, 3);
        break;
    case 6: /* CATERPIE — segments */
        c_rgb(0.90f * f, 0.90f * f, 0.30f * f);
        fill_rect(x+1, y+2, 3, 2); fill_rect(x+1, y+6, 3, 2);
        break;
    case 7: /* WEEDLE — horn */
        c_rgb(0.95f * f, 0.60f * f, 0.70f * f);
        fill_rect(x+4, y-2, 2, 3); fill_rect(x+3, y-3, 4, 2);
        break;
    case 8: /* MAGIKARP — horizontal fish */
        fill_rect(x, y+3, 10, 4);
        c_rgb(1.00f * f, 0.85f * f, 0.30f * f);
        fill_rect(x+7, y+1, 3, 2); fill_rect(x+7, y+7, 3, 2);
        break;
    case 9: /* VULPIX — tails */
        c_rgb(1.00f * f, 0.90f * f, 0.70f * f);
        fill_rect(x-1, y+1, 2, 2); fill_rect(x-1, y+4, 2, 2);
        fill_rect(x+9, y+1, 2, 2); fill_rect(x+9, y+4, 2, 2);
        break;
    }
}

static void render_tactics(Game *g) {
    TacticsBattle *tb = &g->tactics;
    int gx, gy, i;
    float px, py;

    /* faint header — just text, no background bar */
    if (tb->phase < 2) {
        if (tb->tactics_wild)
            text_rgb(4, 2, "WILD", 0.40f, 1.00f, 0.40f);
        else if (tb->tactics_trainer)
            text_rgb(4, 2, "TRAINER", 0.30f, 0.60f, 1.00f);
        else
            text_rgb(4, 2, "PVP", 1.00f, 0.60f, 0.30f);
        if (tb->phase == 0) text_rgb(52, 2, "YOUR TURN", 0.30f, 0.60f, 1.00f);
        else if (tb->phase == 1) text_rgb(52, 2, tb->tactics_wild ? "WILD TURN" : "AI TURN", 1.00f, 0.30f, 0.30f);
    } else {
        if (tb->phase == 2) text_rgb(40, 2, "YOU WIN!", 0.15f, 0.82f, 0.25f);
        else if (tb->phase == 3) text_rgb(40, 2, "YOU LOSE!", 0.95f, 0.18f, 0.12f);
    }
    { char ts[16]; snprintf(ts, sizeof(ts), "T%d", tb->turn_num); text_rgb(140, 2, ts, 0.50f, 0.50f, 0.70f); }

    /* grid lines only — no background fill */
    for (gy = 0; gy <= TACT_ROWS; gy++) {
        py = TACT_OY + gy * TACT_TILE;
        c_rgb(0.30f, 0.30f, 0.40f);
        fill_rect(TACT_OX, py, TACT_COLS * TACT_TILE, 1);
    }
    for (gx = 0; gx <= TACT_COLS; gx++) {
        px = TACT_OX + gx * TACT_TILE;
        c_rgb(0.30f, 0.30f, 0.40f);
        fill_rect(px, TACT_OY, 1, TACT_ROWS * TACT_TILE);
    }

    if (tb->sub_phase == 1 && tb->sel_unit >= 0) {
        int rmap[TACT_ROWS][TACT_COLS], j, k;
        tactics_calc_move_range(g, tb->sel_unit, rmap);
        for (j = 0; j < TACT_ROWS; j++) for (k = 0; k < TACT_COLS; k++)
            if (rmap[j][k]) {
                px = TACT_OX + k * TACT_TILE; py = TACT_OY + j * TACT_TILE;
                c_rgb(0.10f, 0.60f, 0.15f);
                fill_rect(px+1, py+1, TACT_TILE-2, TACT_TILE-2);
            }
    }

    if (tb->sub_phase == 2 && tb->sel_unit >= 0) {
        TacticsUnit *u = &tb->units[tb->sel_unit];
        const MoveDef *mv = mon_move(g, u->move_id[tb->move_sel]);
        if (!mv) mv = mon_move(g, u->move_id[0]);
        int atk_r = mv ? mv->range : 1;
        for (i = 0; i < tb->unit_n; i++) {
            TacticsUnit *e = &tb->units[i];
            if (!(e->active && e->hp > 0 && e->player != u->player)) continue;
            if (abs(u->x - e->x) + abs(u->y - e->y) > atk_r) continue;
            px = TACT_OX + e->x * TACT_TILE; py = TACT_OY + e->y * TACT_TILE;
            c_rgb(0.95f, 0.15f, 0.15f);
            fill_rect(px, py, TACT_TILE, 2); fill_rect(px, py+TACT_TILE-2, TACT_TILE, 2);
            fill_rect(px, py, 2, TACT_TILE); fill_rect(px+TACT_TILE-2, py, 2, TACT_TILE);
        }
    }

    for (i = 0; i < tb->unit_n; i++) {
        TacticsUnit *u = &tb->units[i];
        if (!u->active || u->hp <= 0) continue;
        gx = u->x; gy = u->y;
        px = TACT_OX + gx * TACT_TILE + 2; py = TACT_OY + gy * TACT_TILE + 2;

        { float hr = u->max_hp>0?(float)u->hp/(float)u->max_hp:0; if(hr<0)hr=0;
          c_rgb(0.10f,0.10f,0.15f); fill_rect(px+1, py-3, 8, 2);
          if(hr>0.5f)c_rgb(0.15f,0.82f,0.25f);else if(hr>0.2f)c_rgb(0.95f,0.80f,0.10f);else c_rgb(0.95f,0.18f,0.12f);
          fill_rect(px+1, py-3, (int)(hr*7+1), 2); }

        int greyed = (u->moved && u->acted) ? 1 : 0;
        if (greyed) {
            c_rgb(0.30f, 0.30f, 0.35f);
            fill_rect(px, py, 10, 10);
        }
        draw_mini_mon(g, px, py, u->species, greyed);

        if (i == tb->sel_unit) {
            float tx=TACT_OX+gx*TACT_TILE, ty=TACT_OY+gy*TACT_TILE;
            c_rgb(1.00f,0.85f,0.10f);
            fill_rect(tx,ty,TACT_TILE,2); fill_rect(tx,ty+TACT_TILE-2,TACT_TILE,2);
            fill_rect(tx,ty,2,TACT_TILE); fill_rect(tx+TACT_TILE-2,ty,2,TACT_TILE);
        }

        if (u->player == 0 && !(u->moved && u->acted) && u->hp > 0) {
            c_rgb(0.25f, 0.55f, 1.00f);
            fill_rect(px+2, py+9, 6, 1);
        } else if (u->player == 1 && !(u->moved && u->acted) && u->hp > 0) {
            c_rgb(1.00f, 0.25f, 0.25f);
            fill_rect(px+2, py+9, 6, 1);
        }
    }

    px = TACT_OX + tb->cursor_x * TACT_TILE; py = TACT_OY + tb->cursor_y * TACT_TILE;
    c_rgb(1.00f,1.00f,1.00f);
    fill_rect(px,py,TACT_TILE,1); fill_rect(px,py+TACT_TILE-1,TACT_TILE,1);
    fill_rect(px,py,1,TACT_TILE); fill_rect(px+TACT_TILE-1,py,1,TACT_TILE);

    /* deploy overlay */
    if (tb->deploying) {
        int bb_y = TACT_OY + TACT_ROWS * TACT_TILE;
        c_rgb(0.05f, 0.05f, 0.10f); fill_rect(0, bb_y-2, GB_W, GB_H-bb_y+2);
        text_rgb(4, bb_y+1, "DEPLOY — pick a mon (Z=send  X=done  arrows=move)", 0.95f, 0.85f, 0.10f);
        int di;
        for (di = 0; di < g->player.party_n; di++) {
            int dy = bb_y + 14 + di * 14;
            if (dy > GB_H - 14) break;
            PartyMon *pm = &g->player.party[di];
            const Species *sp = mon_species(g, pm->species);
            if (di == tb->deploy_sel) {
                c_rgb(0.20f, 0.20f, 0.40f);
                fill_rect(2, dy, GB_W-4, 12);
            }
            if (sp) {
                char dbuf[48];
                snprintf(dbuf, sizeof(dbuf), "%s Lv%d HP%d/%d",
                         sp->name, pm->level, pm->hp, pm->max_hp);
                float tc = pm->hp <= 0 ? 0.50f : 1.0f;
                text_rgb(6, dy+1, dbuf, tc, tc, tc*0.95f);
            }
            if (pm->hp <= 0) text_rgb(110, dy+1, "FAINTED", 0.95f, 0.18f, 0.12f);
            if (di == tb->deploy_sel) text_rgb(GB_W-12, dy+1, ">", 0.95f, 0.85f, 0.10f);
        }
        /* deploy timer */
        { char tbuf[32]; snprintf(tbuf, sizeof(tbuf), "Time: %d/%d",
          tb->deploy_timer/6, 300/6); text_rgb(120, bb_y+1, tbuf, 0.60f, 0.60f, 0.80f); }
        return; /* skip normal HUD */
    }

    { int bb_y = TACT_OY + TACT_ROWS * TACT_TILE;
      c_rgb(0.12f,0.12f,0.25f); fill_rect(0,bb_y,GB_W,GB_H-bb_y);

      if (tb->wait > 0 && tb->msg[0])
          text_rgb(4, bb_y+2, tb->msg, 0.90f, 0.90f, 0.80f);
      else if (tb->sub_phase == 2 && tb->sel_unit >= 0 && tb->phase < 2) {
          /* Attack phase — show move menu */
          TacticsUnit *u = &tb->units[tb->sel_unit];
          const MoveDef *m0 = mon_move(g, u->move_id[0]);
          const MoveDef *m1 = mon_move(g, u->move_id[1]);
          char menu[64];
          if (m0 && m1) {
              int sel = tb->move_sel;
              snprintf(menu, sizeof(menu), "%s%s(r%d p%d)  %s%s(r%d p%d)  X=wait",
                       sel==0?"1>":"1:", m0->name, m0->range, m0->power,
                       sel==1?"2>":"2:", m1->name, m1->range, m1->power);
          } else snprintf(menu, sizeof(menu), "[attack]  X=wait");
          text_rgb(4, bb_y+2, menu, 1.0f, 1.0f, 0.90f);
      } else {
          int found = 0;
          for (i = 0; i < tb->unit_n; i++) {
              TacticsUnit *u = &tb->units[i];
              if (u->active && u->hp > 0 && u->x == tb->cursor_x && u->y == tb->cursor_y) {
                  const Species *sp = mon_species(g, u->species);
                  if (sp) {
                      char info[48];
                      snprintf(info, sizeof(info), "%s HP %d/%d mr:%d",
                               sp->name, u->hp, u->max_hp, u->move_range);
                      c_rgb(0.15f,0.15f,0.30f); fill_rect(0,bb_y,GB_W,GB_H-bb_y);
                      text_rgb(4, bb_y+2, info, 1.0f, 1.0f, 0.90f);
                      text_rgb(100, bb_y+2, u->player==0?"[P1]":"[P2]",
                               u->player==0?0.40f:1.0f, 0.40f, u->player==0?1.0f:0.40f);
                  }
                  found=1; break;
              }
          }
          if (!found) {
              if (tb->phase < 2 && tb->wait == 0) {
                  if (tb->sub_phase == 0) text_rgb(4, bb_y+2, "A=select  arrows=move", 0.50f,0.50f,0.60f);
                  else if (tb->sub_phase == 1) text_rgb(4, bb_y+2, "A=move  X=skip", 0.50f,0.50f,0.60f);
              }
          }
      }
    }
}

static void render_fly(Game *g) {
    int ci;
    c_rgb(0.05f, 0.05f, 0.15f);
    fill_rect(0, 0, GB_W, GB_H);
    text_rgb(4, 4, "  FLY — fast travel", 0.95f, 0.85f, 0.10f);
    c_rgb(0.95f, 0.80f, 0.20f);
    fill_rect(0, 14, GB_W, 2);
    char name_buf[NAME_LEN];
    for (ci = 0; ci < g->map.conns_n; ci++) {
        MapConn *c = &g->map.conns[ci];
        int y = 20 + ci * 14;
        if (ci == g->fly_sel) {
            c_rgb(0.15f, 0.15f, 0.35f);
            fill_rect(2, y, GB_W - 4, 12);
        }
        name_buf[0] = 0;
        Map tmp;
        if (map_load(&tmp, c->dest_path) == 0)
            snprintf(name_buf, sizeof(name_buf), "%s", tmp.name);
        else
            snprintf(name_buf, sizeof(name_buf), "?");
        text_rgb(6, y + 1, name_buf, 0.90f, 0.90f, 0.95f);
    }
    text_rgb(4, GB_H - 14, "Z=travel  X=cancel  arrows=move", 0.50f, 0.50f, 0.60f);
}

void render_frame(Game *g) {
    glViewport(0, 0, WIN_W, WIN_H);
    glClear(GL_COLOR_BUFFER_BIT);
    set_ortho_gb();

    switch (g->mode) {
    case MODE_TITLE:       render_title(g); break;
    case MODE_STARTER:     render_starter(g); break;
    case MODE_OVERWORLD:   render_overworld(g); break;
    case MODE_BATTLE:
    case MODE_PVP_BATTLE:
        render_overworld(g);
        render_tactics(g);
        break;
    case MODE_PARTY:       render_party(g); break;
    case MODE_BAG:         render_bag(g); break;
    case MODE_BAG_USE:     render_bag_use(g); break;
    case MODE_FLY:         render_fly(g); break;
    case MODE_MSG:         render_msg(g); break;
    case MODE_PVP_SETUP:   render_pvp_setup(g); break;
    default:               render_title(g); break;
    }

    glutSwapBuffers();
}
