/* ui.c — menu + combat HUD */
#include "sw.h"
#include <GL/gl.h>
#include <GL/glut.h>

void ui_draw_text(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}

void ui_draw_text_big(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s);
}

static void bar(float x, float y, float w, float h, float t,
                float r, float g, float b, float br, float bg, float bb) {
    glColor3f(br, bg, bb);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w * clampf(t, 0, 1), y);
    glVertex2f(x + w * clampf(t, 0, 1), y + h); glVertex2f(x, y + h);
    glEnd();
    glColor3f(0.8f, 0.85f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

static void panel(float x, float y, float w, float h, float a) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.04f, 0.06f, 0.12f, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glColor4f(0.35f, 0.55f, 0.95f, 0.7f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_BLEND);
}

void ui_draw_menu(const Game *g, int win_w, int win_h) {
    int i;
    const char *modes[] = {
        "1  SUPREMACY     — Capture command posts, drain enemy tickets",
        "2  DEATHMATCH    — Orbital dogfight, first to 25 kills",
        "3  FREEPLAY      — Planets, survival, building, ships & sabers"
    };
    const char *diff[] = { "Easy", "Normal", "Hard" };
    char buf[128];
    panel(win_w * 0.12f, win_h * 0.12f, win_w * 0.76f, win_h * 0.76f, 0.88f);

    glColor3f(1.f, 0.85f, 0.25f);
    ui_draw_text_big(win_w * 0.22f, win_h * 0.82f, "STAR WARS  —  BATTLEFRONT");
    glColor3f(0.55f, 0.75f, 1.f);
    ui_draw_text(win_w * 0.22f, win_h * 0.77f, "House freeglut clone  ·  Supremacy · Deathmatch · Freeplay");

    for (i = 0; i < 3; i++) {
        int sel = (g->menu_sel == i);
        if (sel) {
            glColor3f(0.15f, 0.35f, 0.65f);
            glBegin(GL_QUADS);
            glVertex2f(win_w * 0.18f, win_h * 0.62f - i * 40.f);
            glVertex2f(win_w * 0.82f, win_h * 0.62f - i * 40.f);
            glVertex2f(win_w * 0.82f, win_h * 0.62f - i * 40.f + 32.f);
            glVertex2f(win_w * 0.18f, win_h * 0.62f - i * 40.f + 32.f);
            glEnd();
            glColor3f(1.f, 0.95f, 0.4f);
        } else glColor3f(0.8f, 0.85f, 0.95f);
        ui_draw_text(win_w * 0.22f, win_h * 0.63f - i * 40.f + 8.f, modes[i]);
    }

    snprintf(buf, sizeof(buf), "Ship  [ / ] : %s", sim_ship_name(g->selected_ship));
    glColor3f(0.7f, 0.9f, 1.f);
    ui_draw_text(win_w * 0.22f, win_h * 0.38f, buf);
    snprintf(buf, sizeof(buf), "Difficulty  - / + : %s", diff[g->difficulty % 3]);
    ui_draw_text(win_w * 0.22f, win_h * 0.34f, buf);
    snprintf(buf, sizeof(buf), "Freeplay planet  P cycles in-game  ·  now seed %d", g->seed);
    ui_draw_text(win_w * 0.22f, win_h * 0.30f, buf);

    glColor3f(0.5f, 0.9f, 0.55f);
    ui_draw_text(win_w * 0.22f, win_h * 0.22f, "ENTER launch   ·   arrows select   ·   Q quit");
    glColor3f(0.55f, 0.6f, 0.7f);
    ui_draw_text(win_w * 0.22f, win_h * 0.17f,
                 "In-game: mouse look · WASD · LMB fire · E enter/exit · Q menu · Tab weapon");
}

void ui_draw_hud(const Game *g, int win_w, int win_h) {
    const Entity *e = &g->ents[g->local];
    const ShipDef *sd = sim_ship_def(e->ship);
    char buf[160];
    int i;

    /* top bar mode */
    panel(8, win_h - 36, win_w - 16, 28, 0.55f);
    glColor3f(1.f, 0.9f, 0.35f);
    snprintf(buf, sizeof(buf), "%s  ·  %s  ·  %s",
             sim_mode_name(g->mode), sim_planet_name(g->planet),
             e->in_ship ? sd->name : "On Foot");
    ui_draw_text(16, win_h - 26, buf);
    glColor3f(0.6f, 0.85f, 1.f);
    snprintf(buf, sizeof(buf), "%.0f fps", g->fps);
    ui_draw_text(win_w - 70, win_h - 26, buf);

    /* health / shields */
    panel(12, 12, 280, 100, 0.6f);
    glColor3f(0.9f, 0.95f, 1.f);
    ui_draw_text(20, 95, e->name);
    bar(20, 72, 250, 12, e->hp / sd->max_hp, 0.9f, 0.2f, 0.2f, 0.15f, 0.05f, 0.05f);
    ui_draw_text(20, 58, "HULL");
    bar(20, 42, 250, 10, e->shield / fmaxf(1.f, sd->max_shield), 0.25f, 0.55f, 1.f, 0.05f, 0.1f, 0.2f);
    ui_draw_text(20, 28, "SHIELD");
    bar(20, 18, 250, 8, e->heat, 1.f, 0.6f, 0.1f, 0.15f, 0.1f, 0.05f);

    /* score / mode stats */
    panel(win_w - 260, 12, 248, 110, 0.6f);
    glColor3f(0.95f, 0.95f, 0.8f);
    snprintf(buf, sizeof(buf), "Kills %d   Deaths %d", (int)e->kills, (int)e->deaths);
    ui_draw_text(win_w - 248, 100, buf);
    snprintf(buf, sizeof(buf), "Score %.0f", e->score);
    ui_draw_text(win_w - 248, 82, buf);
    if (e->buff_timer > 0) {
        glColor3f(1.f, 0.8f, 0.2f);
        snprintf(buf, sizeof(buf), "BUFF x%.2f  %.1fs", e->buff_mult, e->buff_timer);
        ui_draw_text(win_w - 248, 64, buf);
    }
    if (g->mode == MODE_SUPREMACY) {
        glColor3f(0.4f, 0.7f, 1.f);
        snprintf(buf, sizeof(buf), "Rebel tickets  %.0f", g->ticket_rebel);
        ui_draw_text(win_w - 248, 46, buf);
        glColor3f(1.f, 0.35f, 0.35f);
        snprintf(buf, sizeof(buf), "Empire tickets %.0f", g->ticket_empire);
        ui_draw_text(win_w - 248, 28, buf);
    } else if (g->mode == MODE_DEATHMATCH) {
        glColor3f(1.f, 0.85f, 0.4f);
        snprintf(buf, sizeof(buf), "Frag limit %.0f", g->dm_limit);
        ui_draw_text(win_w - 248, 46, buf);
    } else if (g->mode == MODE_FREEPLAY) {
        glColor3f(0.7f, 0.95f, 0.7f);
        snprintf(buf, sizeof(buf), "Ore %.0f  Wood %.0f  Scrap %.0f",
                 g->res_ore, g->res_wood, g->res_scrap);
        ui_draw_text(win_w - 248, 46, buf);
        bar(win_w - 248, 24, 220, 8, e->oxygen / 100.f, 0.3f, 0.8f, 1.f, 0.05f, 0.1f, 0.15f);
        ui_draw_text(win_w - 248, 12, "O2");
    }

    /* crosshair */
    glColor3f(0.9f, 0.95f, 1.f);
    glBegin(GL_LINES);
    glVertex2f(win_w * 0.5f - 10, win_h * 0.5f);
    glVertex2f(win_w * 0.5f + 10, win_h * 0.5f);
    glVertex2f(win_w * 0.5f, win_h * 0.5f - 10);
    glVertex2f(win_w * 0.5f, win_h * 0.5f + 10);
    glEnd();
    if (e->in_ship) {
        glBegin(GL_LINE_LOOP);
        for (i = 0; i < 24; i++) {
            float a = i / 24.f * 6.2832f;
            glVertex2f(win_w * 0.5f + cosf(a) * 22.f, win_h * 0.5f + sinf(a) * 22.f);
        }
        glEnd();
    }

    /* minimap-ish posts for supremacy */
    if (g->mode == MODE_SUPREMACY) {
        float mx = win_w - 130, my = win_h - 150, sc = 0.7f;
        panel(mx - 70, my - 70, 140, 140, 0.45f);
        glColor3f(0.7f, 0.8f, 1.f);
        ui_draw_text(mx - 50, my + 55, "POSTS");
        for (i = 0; i < g->n_posts; i++) {
            const CommandPost *p = &g->posts[i];
            float px = mx + p->x * sc * 0.5f;
            float pz = my - p->z * sc * 0.5f;
            if (p->team == TEAM_REBEL) glColor3f(0.2f, 0.5f, 1.f);
            else if (p->team == TEAM_EMPIRE) glColor3f(1.f, 0.25f, 0.25f);
            else glColor3f(0.9f, 0.85f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(px - 4, pz - 4); glVertex2f(px + 4, pz - 4);
            glVertex2f(px + 4, pz + 4); glVertex2f(px - 4, pz + 4);
            glEnd();
        }
        glColor3f(0.3f, 1.f, 0.4f);
        glBegin(GL_QUADS);
        glVertex2f(mx + e->x * sc * 0.5f - 3, my - e->z * sc * 0.5f - 3);
        glVertex2f(mx + e->x * sc * 0.5f + 3, my - e->z * sc * 0.5f - 3);
        glVertex2f(mx + e->x * sc * 0.5f + 3, my - e->z * sc * 0.5f + 3);
        glVertex2f(mx + e->x * sc * 0.5f - 3, my - e->z * sc * 0.5f + 3);
        glEnd();
    }

    /* status line */
    panel(win_w * 0.2f, 120, win_w * 0.6f, 24, 0.45f);
    glColor3f(0.55f, 0.95f, 0.6f);
    ui_draw_text(win_w * 0.22f, 128, g->status);

    /* weapon / controls strip */
    panel(12, 120, 200, 48, 0.5f);
    glColor3f(0.85f, 0.9f, 1.f);
    if (e->in_ship)
        snprintf(buf, sizeof(buf), "Cannons  heat %.0f%%", e->heat * 100.f);
    else {
        const char *wn[] = { "Blaster", "Repeater", "Rocket", "Lightsaber" };
        snprintf(buf, sizeof(buf), "%s", wn[e->weapon % WPN_COUNT]);
    }
    ui_draw_text(20, 148, buf);
    glColor3f(0.5f, 0.6f, 0.7f);
    ui_draw_text(20, 130, "Tab wpn  [ ] ship  E seat");
}
