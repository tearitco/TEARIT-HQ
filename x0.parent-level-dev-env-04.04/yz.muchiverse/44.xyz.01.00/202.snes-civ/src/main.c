/* main.c — freeglut entry, ~60fps timer, input (no idle spin) */
#include "civ.h"

#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static Game g_game;
static int g_running = 1;
static float g_fps = 60.0f;
static double g_last_time = 0.0;
static int g_frame_count = 0;
static double g_fps_timer = 0.0;
static int g_need_redraw = 1;

#define TARGET_MS 16

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static void screen_to_tile(int sx, int sy, int *tx, int *ty) {
    float ox, oy, ts;
    int cols, rows;
    int vx, vy;
    render_map_layout(&g_game, &ox, &oy, &ts, &cols, &rows);
    if (ts < 1.0f) { *tx = *ty = -1; return; }
    vx = (int)((sx - ox) / ts);
    vy = (int)((sy - oy) / ts);
    if (vx < 0 || vy < 0 || vx >= cols || vy >= rows) {
        *tx = *ty = -1;
        return;
    }
    *tx = map_wrap_x(g_game.cam_x + vx);
    *ty = g_game.cam_y + vy;
    if (*ty < 0 || *ty >= MAP_H) { *tx = *ty = -1; }
}

static void display(void) {
    render_frame(&g_game, g_fps);
}

static void reshape(int w, int h) {
    if (w < 320) w = 320;
    if (h < 240) h = 240;
    g_game.win_w = w;
    g_game.win_h = h;
    g_need_redraw = 1;
}

static void timer_cb(int value) {
    double t = now_sec();
    float dt;
    (void)value;

    if (g_last_time <= 0.0) g_last_time = t;
    dt = (float)(t - g_last_time);
    g_last_time = t;
    if (dt > 0.1f) dt = 0.1f;

    g_frame_count++;
    g_fps_timer += dt;
    if (g_fps_timer >= 0.5) {
        g_fps = (float)g_frame_count / (float)g_fps_timer;
        g_frame_count = 0;
        g_fps_timer = 0.0;
        g_need_redraw = 1;
    }

    if (g_game.dirty) {
        g_need_redraw = 1;
        g_game.dirty = 0;
    }

    if (g_need_redraw) {
        g_need_redraw = 0;
        glutPostRedisplay();
    }
    if (g_running)
        glutTimerFunc(TARGET_MS, timer_cb, 0);
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    switch (key) {
    case 27: /* Esc */
    case 'q':
    case 'Q':
        g_running = 0;
        exit(0);
        break;
    case ' ':
    case '\r':
    case '\n':
    case 'e':
    case 'E':
        game_end_turn(&g_game);
        g_need_redraw = 1;
        break;
    case 'n':
    case 'N':
    case '\t':
        game_select_next_unit(&g_game);
        g_need_redraw = 1;
        break;
    case 'b':
    case 'B':
        game_found_city(&g_game);
        g_need_redraw = 1;
        break;
    case '[':
        game_cycle_prod(&g_game, -1);
        g_need_redraw = 1;
        break;
    case ']':
        game_cycle_prod(&g_game, +1);
        g_need_redraw = 1;
        break;
    case 'w':
    case 'W':
        g_game.cam_y -= 1;
        if (g_game.cam_y < 0) g_game.cam_y = 0;
        g_need_redraw = 1;
        break;
    case 's':
    case 'S':
        g_game.cam_y += 1;
        if (g_game.cam_y > MAP_H - 4) g_game.cam_y = MAP_H - 4;
        g_need_redraw = 1;
        break;
    case 'a':
    case 'A':
        g_game.cam_x = map_wrap_x(g_game.cam_x - 1);
        g_need_redraw = 1;
        break;
    case 'd':
    case 'D':
        g_game.cam_x = map_wrap_x(g_game.cam_x + 1);
        g_need_redraw = 1;
        break;
    default:
        break;
    }
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    switch (key) {
    case GLUT_KEY_LEFT:  game_try_move(&g_game, -1, 0); break;
    case GLUT_KEY_RIGHT: game_try_move(&g_game, 1, 0);  break;
    case GLUT_KEY_UP:    game_try_move(&g_game, 0, -1); break;
    case GLUT_KEY_DOWN:  game_try_move(&g_game, 0, 1);  break;
    default: break;
    }
    g_need_redraw = 1;
}

static void mouse(int button, int state, int x, int y) {
    int tx, ty;
    if (state != GLUT_DOWN) return;
    screen_to_tile(x, y, &tx, &ty);
    if (tx < 0) return;
    if (button == GLUT_LEFT_BUTTON)
        game_click(&g_game, tx, ty, 0);
    else if (button == GLUT_RIGHT_BUTTON)
        game_click(&g_game, tx, ty, 2);
    g_need_redraw = 1;
}

static void motion(int x, int y) {
    int tx, ty;
    screen_to_tile(x, y, &tx, &ty);
    if (tx != g_game.hover_x || ty != g_game.hover_y) {
        g_game.hover_x = tx;
        g_game.hover_y = ty;
        g_need_redraw = 1;
    }
}

static void passive(int x, int y) {
    motion(x, y);
}

int main(int argc, char **argv) {
    int seed = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("snes-civ — freeglut Civilization-like MVP\n");
            printf("  --seed N\n");
            printf("Keys: arrows move, N next, B build, Space end turn, Q quit\n");
            return 0;
        }
    }

    game_init(&g_game, seed);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(g_game.win_w, g_game.win_h);
    glutCreateWindow("202.snes-civ — SNES Civilization");
    render_init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passive);
    /* NO glutIdleFunc — timer only */
    glutTimerFunc(TARGET_MS, timer_cb, 0);

    printf("snes-civ ready. Space=End Turn  B=Found City  N=Next Unit  Q=Quit\n");
    glutMainLoop();
    return 0;
}
