/* main.c — freeglut entry: ~60fps timer, mouse look, modes */
#include "sw.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <sys/time.h>

static Game g_game;
static int g_win_w = WIN_W, g_win_h = WIN_H;
static int g_keys[256];
static int g_special[512];
static int g_mouse_l = 0, g_mouse_r = 0;
static int g_warping = 0;
static int g_capture = 0;
static float g_mdx = 0, g_mdy = 0;
static double g_last = 0;
static int g_frames = 0;
static double g_fps_t = 0;

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static void display(void) {
    float aspect = (g_win_h > 0) ? (float)g_win_w / (float)g_win_h : 1.f;
    if (g_game.mode == MODE_MENU) {
        glClearColor(0.02f, 0.03f, 0.08f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, g_win_w, 0, g_win_h);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        /* animated backdrop stars */
        {
            int i;
            glBegin(GL_POINTS);
            for (i = 0; i < 200; i++) {
                float n = gen_noise2(i * 0.7f, g_game.time * 0.02f + i);
                glColor3f(n, n, n * 1.1f);
                glVertex2f(fmodf(i * 97.f + g_game.time * 8.f, (float)g_win_w),
                           fmodf(i * 53.f + 20.f, (float)g_win_h));
            }
            glEnd();
        }
        ui_draw_menu(&g_game, g_win_w, g_win_h);
    } else {
        int i;
        gfx_begin_frame(&g_game, aspect);
        gfx_draw_world(&g_game);
        gfx_draw_builds(&g_game);
        for (i = 0; i < g_game.n_ents; i++)
            gfx_draw_entity(&g_game, &g_game.ents[i], i == g_game.local);
        gfx_draw_bullets(&g_game);
        gfx_draw_fx(&g_game);
        gfx_end_frame();
        ui_draw_hud(&g_game, g_win_w, g_win_h);
        glEnable(GL_DEPTH_TEST);
    }
    glutSwapBuffers();
    g_game.need_redraw = 0;
}

static void timer(int v) {
    double t = now_sec();
    float dt;
    (void)v;
    if (g_last < 1.0) g_last = t;
    dt = (float)(t - g_last);
    if (dt > 0.05f) dt = 0.05f;
    g_last = t;

    if (g_game.mode == MODE_MENU) {
        g_game.time += dt;
        g_game.need_redraw = 1;
    } else {
        {
            int mods = glutGetModifiers();
            int sh = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;
            int ct = (mods & GLUT_ACTIVE_CTRL) ? 1 : 0;
            sim_player_input(&g_game, dt,
                             g_keys['w'] | g_keys['W'],
                             g_keys['a'] | g_keys['A'],
                             g_keys['s'] | g_keys['S'],
                             g_keys['d'] | g_keys['D'],
                             g_keys[' '],
                             sh, ct,
                             g_mouse_l, g_mouse_r,
                             g_mdx, g_mdy);
        }
        g_mdx = g_mdy = 0;
        sim_update(&g_game, dt);
    }

    g_frames++;
    g_fps_t += dt;
    if (g_fps_t >= 0.5) {
        g_game.fps = g_frames / (float)g_fps_t;
        g_frames = 0;
        g_fps_t = 0;
        g_game.need_redraw = 1;
    }

    if (g_game.need_redraw || g_game.mode != MODE_MENU)
        glutPostRedisplay();
    glutTimerFunc(TICK_MS, timer, 0);
}

static void reshape(int w, int h) {
    g_win_w = w > 1 ? w : 1;
    g_win_h = h > 1 ? h : 1;
    gfx_resize(g_win_w, g_win_h);
    g_game.need_redraw = 1;
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    g_keys[key] = 1;
    if (key == 'q' || key == 'Q') {
        if (g_game.mode == MODE_MENU) {
            g_game.running = 0;
            exit(0);
        }
        g_game.mode = MODE_MENU;
        g_capture = 0;
        glutSetCursor(GLUT_CURSOR_INHERIT);
        snprintf(g_game.status, sizeof(g_game.status), "Returned to menu");
        return;
    }
    if (g_game.mode == MODE_MENU) {
        if (key == 13 || key == 10) {
            enum GameMode m = MODE_SUPREMACY;
            if (g_game.menu_sel == 1) m = MODE_DEATHMATCH;
            if (g_game.menu_sel == 2) m = MODE_FREEPLAY;
            sim_start_mode(&g_game, m);
            g_capture = 1;
            glutSetCursor(GLUT_CURSOR_NONE);
            glutWarpPointer(g_win_w / 2, g_win_h / 2);
        }
        if (key == '[') sim_cycle_ship(&g_game, -1);
        if (key == ']') sim_cycle_ship(&g_game, 1);
        if (key == '-' || key == '_')
            g_game.difficulty = (g_game.difficulty + 2) % 3;
        if (key == '=' || key == '+')
            g_game.difficulty = (g_game.difficulty + 1) % 3;
        if (key == '1') g_game.menu_sel = 0;
        if (key == '2') g_game.menu_sel = 1;
        if (key == '3') g_game.menu_sel = 2;
        g_game.need_redraw = 1;
        return;
    }
    /* in-game */
    if (key == 27) { /* esc */
        g_game.mode = MODE_MENU;
        g_capture = 0;
        glutSetCursor(GLUT_CURSOR_INHERIT);
        return;
    }
    if (key == 'e' || key == 'E') sim_try_enter_exit(&g_game);
    if (key == '\t') sim_cycle_weapon(&g_game, 1);
    if (key == '[') sim_cycle_ship(&g_game, -1);
    if (key == ']') sim_cycle_ship(&g_game, 1);
    if (key == 'p' || key == 'P') {
        if (g_game.mode == MODE_FREEPLAY) {
            g_game.planet = (enum Planet)((g_game.planet + 1) % PLANET_COUNT);
            {
                Entity *e = &g_game.ents[g_game.local];
                e->x = e->z = 0;
                e->y = (g_game.planet == PLANET_SPACE) ? 50.f
                     : gen_height(g_game.planet, 0, 0) + 10.f;
                e->in_ship = (g_game.planet == PLANET_SPACE);
                snprintf(g_game.status, sizeof(g_game.status), "Warped to %s",
                         sim_planet_name(g_game.planet));
            }
        }
    }
    if (key == 'f' || key == 'F') {
        if (g_game.mode == MODE_FREEPLAY) {
            g_game.res_ore += 5.f + frand() * 5.f;
            g_game.res_wood += 3.f + frand() * 4.f;
            g_game.res_scrap += 2.f;
            snprintf(g_game.status, sizeof(g_game.status),
                     "Mined resources  ore=%.0f wood=%.0f scrap=%.0f",
                     g_game.res_ore, g_game.res_wood, g_game.res_scrap);
        }
    }
    if (key >= '1' && key <= '5') sim_place_build(&g_game, key - '0');
    if (key == 'r' || key == 'R') {
        /* repair / freeplay rest */
        Entity *e = &g_game.ents[g_game.local];
        const ShipDef *sd = sim_ship_def(e->ship);
        e->hp = fminf(sd->max_hp, e->hp + 25.f);
        e->oxygen = fminf(100.f, e->oxygen + 20.f);
        snprintf(g_game.status, sizeof(g_game.status), "Field repair");
    }
}

static void keyboard_up(unsigned char key, int x, int y) {
    (void)x; (void)y;
    g_keys[key] = 0;
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    g_special[key] = 1;
    if (g_game.mode == MODE_MENU) {
        if (key == GLUT_KEY_UP) g_game.menu_sel = (g_game.menu_sel + 2) % 3;
        if (key == GLUT_KEY_DOWN) g_game.menu_sel = (g_game.menu_sel + 1) % 3;
        if (key == GLUT_KEY_LEFT) sim_cycle_ship(&g_game, -1);
        if (key == GLUT_KEY_RIGHT) sim_cycle_ship(&g_game, 1);
        g_game.need_redraw = 1;
    }
}

static void special_up(int key, int x, int y) {
    (void)x; (void)y;
    g_special[key] = 0;
}

static void mouse(int button, int state, int x, int y) {
    (void)x; (void)y;
    if (button == GLUT_LEFT_BUTTON) g_mouse_l = (state == GLUT_DOWN);
    if (button == GLUT_RIGHT_BUTTON) g_mouse_r = (state == GLUT_DOWN);
    if (g_game.mode != MODE_MENU && state == GLUT_DOWN) {
        g_capture = 1;
        glutSetCursor(GLUT_CURSOR_NONE);
        glutWarpPointer(g_win_w / 2, g_win_h / 2);
    }
}

static void motion(int x, int y) {
    int cx, cy;
    if (!g_capture || g_game.mode == MODE_MENU) return;
    if (g_warping) { g_warping = 0; return; }
    cx = g_win_w / 2;
    cy = g_win_h / 2;
    g_mdx += (float)(x - cx);
    g_mdy += (float)(y - cy);
    g_warping = 1;
    glutWarpPointer(cx, cy);
}

static void passive(int x, int y) { motion(x, y); }

int main(int argc, char **argv) {
    (void)argv;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Star Wars Battlefront* — house clone");

    sim_init_menu(&g_game);
    if (gfx_init() != 0) {
        fprintf(stderr, "gfx_init failed\n");
        return 1;
    }

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboard_up);
    glutSpecialFunc(special);
    glutSpecialUpFunc(special_up);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passive);
    glutTimerFunc(TICK_MS, timer, 0);

    fprintf(stderr,
            "sw-battlefront\n"
            "  Menu: 1/2/3 or arrows + Enter\n"
            "  Supremacy | Deathmatch | Freeplay\n"
            "  Ships [ ]  Difficulty -/+  In-game Q menu\n");
    glutMainLoop();
    return 0;
}
