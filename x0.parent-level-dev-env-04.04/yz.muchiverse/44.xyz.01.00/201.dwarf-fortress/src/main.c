/* main.c — freeglut entry, ~60fps timer, designations, pause/save */
#include "fort.h"
#include "map.h"
#include "unit.h"
#include "job.h"
#include "render.h"
#include "save.h"

#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static Fort g_fort;
static int g_running = 1;
static float g_fps = 60.0f;
static double g_last_time = 0.0;
static int g_frame_count = 0;
static double g_fps_timer = 0.0;
static int g_need_redraw = 1;
static char g_save_name[64] = "default";
static char g_saves_root[256] = "saves";
static int g_sim_accum_ms = 0;
static int g_shift = 0;
static int g_ctrl = 0;

#define TARGET_MS 16
#define SIM_MS 120 /* sim step every ~120ms when unpaused */

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static void set_msg(const char *s) {
    snprintf(g_fort.msg, MAX_MSG, "%s", s);
    g_fort.dirty = 1;
    g_need_redraw = 1;
}

static void fort_new(int seed) {
    memset(&g_fort, 0, sizeof(g_fort));
    snprintf(g_fort.fort_name, sizeof(g_fort.fort_name), "Boatmurdered");
    g_fort.year = 125;
    g_fort.season = 0;
    g_fort.day = 1;
    g_fort.paused = 1;
    g_fort.mode = MODE_LOOK;
    g_fort.sel_dwarf = -1;
    g_fort.win_w = 1000;
    g_fort.win_h = 700;
    map_generate(&g_fort, seed);
    unit_spawn_embark(&g_fort);
    item_count_stocks(&g_fort);
    g_fort.cam_x = MAP_W / 2 - 14;
    g_fort.cam_y = MAP_H / 2 - 12;
    g_fort.cur_x = MAP_W / 2;
    g_fort.cur_y = MAP_H / 2;
    if (g_fort.cam_x < 0) g_fort.cam_x = 0;
    if (g_fort.cam_y < 0) g_fort.cam_y = 0;
    set_msg("New fort embarked. Designate dig (d) or cut (t). Space to unpause.");
    g_need_redraw = 1;
}

static void ensure_cursor_visible(void) {
    int map_px_w = g_fort.win_w - PANEL_W;
    int ts = TILE_PX;
    int view_tw = map_px_w / ts;
    int view_th = g_fort.win_h / ts;
    if (view_tw < 8) view_tw = 8;
    if (view_th < 8) view_th = 8;
    if (g_fort.cur_x < g_fort.cam_x)
        g_fort.cam_x = g_fort.cur_x;
    if (g_fort.cur_y < g_fort.cam_y)
        g_fort.cam_y = g_fort.cur_y;
    if (g_fort.cur_x >= g_fort.cam_x + view_tw)
        g_fort.cam_x = g_fort.cur_x - view_tw + 1;
    if (g_fort.cur_y >= g_fort.cam_y + view_th)
        g_fort.cam_y = g_fort.cur_y - view_th + 1;
    if (g_fort.cam_x < 0) g_fort.cam_x = 0;
    if (g_fort.cam_y < 0) g_fort.cam_y = 0;
    if (g_fort.cam_x > MAP_W - 1) g_fort.cam_x = MAP_W - 1;
    if (g_fort.cam_y > MAP_H - 1) g_fort.cam_y = MAP_H - 1;
}

static void move_cursor(int dx, int dy) {
    g_fort.cur_x += dx;
    g_fort.cur_y += dy;
    if (g_fort.cur_x < 0) g_fort.cur_x = 0;
    if (g_fort.cur_y < 0) g_fort.cur_y = 0;
    if (g_fort.cur_x >= MAP_W) g_fort.cur_x = MAP_W - 1;
    if (g_fort.cur_y >= MAP_H) g_fort.cur_y = MAP_H - 1;
    ensure_cursor_visible();
    g_need_redraw = 1;
}

static int mode_to_desig(int mode) {
    switch (mode) {
    case MODE_DIG: return DG_DIG;
    case MODE_CUT: return DG_CUT;
    case MODE_STOCK_WOOD: return DG_STOCK_WOOD;
    case MODE_STOCK_STONE: return DG_STOCK_STONE;
    case MODE_BUILD_WALL: return DG_BUILD_WALL;
    case MODE_BUILD_WS: return DG_BUILD_WORKSHOP;
    default: return DG_NONE;
    }
}

static void apply_designation(void) {
    int dg = mode_to_desig(g_fort.mode);
    int x0, y0, x1, y1;
    if (dg == DG_NONE) {
        /* query / look: select dwarf under cursor */
        int di = unit_at(&g_fort, g_fort.cur_x, g_fort.cur_y);
        g_fort.sel_dwarf = di;
        if (di >= 0)
            set_msg(g_fort.dwarves[di].name);
        else
            set_msg("No unit here.");
        return;
    }
    if (g_fort.drag) {
        x0 = g_fort.drag_x0;
        y0 = g_fort.drag_y0;
        x1 = g_fort.cur_x;
        y1 = g_fort.cur_y;
        g_fort.drag = 0;
    } else {
        x0 = x1 = g_fort.cur_x;
        y0 = y1 = g_fort.cur_y;
    }
    map_designate_rect(&g_fort, x0, y0, x1, y1, dg);
    job_sync_from_designations(&g_fort);
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "Designated %s.", desig_name(dg));
        set_msg(buf);
    }
}

static void do_save(void) {
    if (save_fort(&g_fort, g_saves_root, g_save_name) == 0)
        set_msg("Saved -> saves/default/");
    else
        set_msg("Save FAILED.");
}

static void do_load(void) {
    if (load_fort(&g_fort, g_saves_root, g_save_name) == 0) {
        g_need_redraw = 1;
    } else {
        set_msg("Load FAILED (no save?).");
    }
}

static void sim_step(void) {
    unit_step_all(&g_fort);
    g_fort.tick++;
    /* calendar: 28 days / season, 4 seasons */
    if ((g_fort.tick % 50) == 0) {
        g_fort.day++;
        if (g_fort.day > 28) {
            g_fort.day = 1;
            g_fort.season++;
            if (g_fort.season > 3) {
                g_fort.season = 0;
                g_fort.year++;
            }
        }
    }
    g_fort.dirty = 1;
    g_need_redraw = 1;
}

static void display(void) {
    render_frame(&g_fort, g_fps);
}

static void reshape(int w, int h) {
    if (h < 1) h = 1;
    if (w < 1) w = 1;
    g_fort.win_w = w;
    g_fort.win_h = h;
    g_need_redraw = 1;
}

static void timer_cb(int value) {
    double t = now_sec();
    float dt;
    int dt_ms;
    (void)value;

    if (g_last_time <= 0.0) g_last_time = t;
    dt = (float)(t - g_last_time);
    g_last_time = t;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0f) dt = 0.0f;
    dt_ms = (int)(dt * 1000.0f);

    if (!g_fort.paused) {
        g_sim_accum_ms += dt_ms;
        while (g_sim_accum_ms >= SIM_MS) {
            g_sim_accum_ms -= SIM_MS;
            sim_step();
        }
    }

    g_frame_count++;
    g_fps_timer += dt;
    if (g_fps_timer >= 0.5) {
        g_fps = (float)g_frame_count / (float)g_fps_timer;
        g_frame_count = 0;
        g_fps_timer = 0.0;
        g_need_redraw = 1;
    }

    if (g_need_redraw || g_fort.dirty) {
        g_need_redraw = 0;
        g_fort.dirty = 0;
        glutPostRedisplay();
    }

    if (g_running)
        glutTimerFunc(TARGET_MS, timer_cb, 0);
}

static void keyboard(unsigned char key, int x, int y) {
    int mods = glutGetModifiers();
    (void)x;
    (void)y;
    g_shift = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;
    g_ctrl = (mods & GLUT_ACTIVE_CTRL) ? 1 : 0;

    /* Ctrl+S / Ctrl+L — GLUT often delivers lowercase with ctrl as 0x13/0x0c */
    if (key == 19 || (g_ctrl && (key == 's' || key == 'S'))) {
        do_save();
        return;
    }
    if (key == 12 || (g_ctrl && (key == 'l' || key == 'L'))) {
        do_load();
        return;
    }

    if (key == 27) { /* Esc */
        g_fort.mode = MODE_LOOK;
        g_fort.drag = 0;
        set_msg("Mode: LOOK");
        return;
    }

    if (key == ' ' ) {
        g_fort.paused = !g_fort.paused;
        g_sim_accum_ms = 0;
        set_msg(g_fort.paused ? "Paused." : "Unpaused — dwarves work.");
        return;
    }

    if (key == 'Q' || (g_shift && (key == 'q'))) {
        g_running = 0;
        exit(0);
    }

    if (key == 'd' || key == 'D') {
        g_fort.mode = MODE_DIG;
        g_fort.drag = 0;
        set_msg("DIG mode: Enter mark, drag with mouse or move+Enter.");
        return;
    }
    if (key == 't' || key == 'T') {
        g_fort.mode = MODE_CUT;
        g_fort.drag = 0;
        set_msg("CUT TREE mode: designate trees to fell.");
        return;
    }
    if (key == 'b' || key == 'B') {
        g_fort.mode = MODE_BUILD_MENU;
        set_msg("Build menu: 1 wall 2 workshop 3/4 stock 5 bed 6 chair");
        return;
    }
    if (key == 'q') {
        g_fort.mode = MODE_QUERY;
        set_msg("QUERY: Enter selects unit under cursor.");
        return;
    }

    if (g_fort.mode == MODE_BUILD_MENU) {
        if (key == '1') {
            g_fort.mode = MODE_BUILD_WALL;
            set_msg("BUILD WALL (needs stone). Designate floors.");
            return;
        }
        if (key == '2') {
            g_fort.mode = MODE_BUILD_WS;
            set_msg("BUILD WORKSHOP (needs 3 wood).");
            return;
        }
        if (key == '3') {
            g_fort.mode = MODE_STOCK_WOOD;
            set_msg("WOOD STOCKPILE designate.");
            return;
        }
        if (key == '4') {
            g_fort.mode = MODE_STOCK_STONE;
            set_msg("STONE STOCKPILE designate.");
            return;
        }
        if (key == '5') {
            g_fort.craft_order = 1;
            job_sync_from_designations(&g_fort);
            g_fort.mode = MODE_LOOK;
            set_msg("Ordered bed (2 wood) at workshop.");
            return;
        }
        if (key == '6') {
            g_fort.craft_order = 2;
            job_sync_from_designations(&g_fort);
            g_fort.mode = MODE_LOOK;
            set_msg("Ordered chair (1 wood) at workshop.");
            return;
        }
    }

    if (key == '\r' || key == '\n') {
        apply_designation();
        return;
    }

    /* WASD cursor */
    if (key == 'w' || key == 'W') { move_cursor(0, -1); return; }
    if (key == 's' || key == 'S') { move_cursor(0, 1); return; }
    if (key == 'a' || key == 'A') { move_cursor(-1, 0); return; }
    if (key == 'd') { /* already dig */ }
    /* note: 'd' is dig; use arrows for down... s is south */

    if (key == '[') {
        g_fort.cam_x--;
        if (g_fort.cam_x < 0) g_fort.cam_x = 0;
        g_need_redraw = 1;
        return;
    }
    if (key == ']') {
        g_fort.cam_x++;
        g_need_redraw = 1;
        return;
    }
    if (key == '{' ) {
        g_fort.cam_y--;
        if (g_fort.cam_y < 0) g_fort.cam_y = 0;
        g_need_redraw = 1;
        return;
    }
    if (key == '}') {
        g_fort.cam_y++;
        g_need_redraw = 1;
        return;
    }

    /* cycle select dwarf with tab */
    if (key == '\t') {
        int i, start = g_fort.sel_dwarf;
        for (i = 0; i < MAX_DWARVES; i++) {
            int idx = (start + 1 + i) % MAX_DWARVES;
            if (g_fort.dwarves[idx].used) {
                g_fort.sel_dwarf = idx;
                g_fort.cur_x = g_fort.dwarves[idx].x;
                g_fort.cur_y = g_fort.dwarves[idx].y;
                ensure_cursor_visible();
                set_msg(g_fort.dwarves[idx].name);
                return;
            }
        }
    }

    g_need_redraw = 1;
}

static void special(int key, int x, int y) {
    int step = 1;
    int mods = glutGetModifiers();
    (void)x;
    (void)y;
    g_shift = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;
    if (g_shift) step = 5;
    switch (key) {
    case GLUT_KEY_LEFT:  move_cursor(-step, 0); break;
    case GLUT_KEY_RIGHT: move_cursor(step, 0); break;
    case GLUT_KEY_UP:    move_cursor(0, -step); break;
    case GLUT_KEY_DOWN:  move_cursor(0, step); break;
    default: break;
    }
}

static void mouse(int button, int state, int x, int y) {
    int map_px_w = g_fort.win_w - PANEL_W;
    int ts = TILE_PX;
    int mx, my;
    if (x >= map_px_w) return; /* panel click ignore */
    mx = g_fort.cam_x + x / ts;
    my = g_fort.cam_y + y / ts;
    if (!map_in_bounds(mx, my)) return;

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        g_fort.cur_x = mx;
        g_fort.cur_y = my;
        if (mode_to_desig(g_fort.mode) != DG_NONE) {
            g_fort.drag = 1;
            g_fort.drag_x0 = mx;
            g_fort.drag_y0 = my;
        } else {
            int di = unit_at(&g_fort, mx, my);
            g_fort.sel_dwarf = di;
        }
        g_need_redraw = 1;
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        g_fort.cur_x = mx;
        g_fort.cur_y = my;
        if (g_fort.drag && mode_to_desig(g_fort.mode) != DG_NONE) {
            apply_designation();
        }
        g_need_redraw = 1;
    }
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        g_fort.mode = MODE_LOOK;
        g_fort.drag = 0;
        set_msg("Cancelled to LOOK.");
    }
}

static void motion(int x, int y) {
    int map_px_w = g_fort.win_w - PANEL_W;
    int ts = TILE_PX;
    int mx, my;
    if (x >= map_px_w || x < 0) return;
    mx = g_fort.cam_x + x / ts;
    my = g_fort.cam_y + y / ts;
    if (!map_in_bounds(mx, my)) return;
    if (g_fort.cur_x != mx || g_fort.cur_y != my) {
        g_fort.cur_x = mx;
        g_fort.cur_y = my;
        g_need_redraw = 1;
    }
}

static void passive(int x, int y) {
    motion(x, y);
}

int main(int argc, char **argv) {
    int seed = (int)time(NULL);
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--save") == 0 && i + 1 < argc)
            snprintf(g_save_name, sizeof(g_save_name), "%s", argv[++i]);
        else if (strcmp(argv[i], "--saves-root") == 0 && i + 1 < argc)
            snprintf(g_saves_root, sizeof(g_saves_root), "%s", argv[++i]);
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1000, 700);
    glutCreateWindow("201.dwarf-fortress — fort sim");

    fort_new(seed);
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

    glutMainLoop();
    return 0;
}
