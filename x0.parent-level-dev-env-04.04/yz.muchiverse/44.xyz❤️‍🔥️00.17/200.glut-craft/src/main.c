/* main.c — freeglut entry, ~60fps timer loop, input */
#include "world.h"
#include "player.h"
#include "render.h"
#include "inv.h"

#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

static World g_world;
static Player g_player;
static Inventory g_inv;

static int g_running = 1;
static float g_fps = 60.0f;
static char g_status[128];
static char g_save_name[64] = "default";
static char g_saves_root[256] = "saves";
static int g_shift_down = 0;
static int g_warping = 0;
static double g_last_time = 0.0;
static int g_frame_count = 0;
static double g_fps_timer = 0.0;
static int g_need_redraw = 1;
static float g_prev_yaw, g_prev_pitch, g_prev_x, g_prev_y, g_prev_z;

#define TARGET_MS 16 /* sim ~60 Hz; draw only when dirty or HUD tick */

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static void set_status(const char *s) {
    snprintf(g_status, sizeof(g_status), "%s", s);
    g_need_redraw = 1;
}

static void spawn_player_on_surface(void) {
    int x = WORLD_W / 2;
    int z = WORLD_D / 2;
    int y;
    for (y = WORLD_H - 2; y > 1; y--) {
        if (world_get(&g_world, x, y, z) != BLK_AIR &&
            world_get(&g_world, x, y + 1, z) == BLK_AIR) {
            player_init(&g_player, (float)x + 0.5f, (float)y + 2.7f, (float)z + 0.5f);
            return;
        }
    }
    player_init(&g_player, (float)x + 0.5f, 40.0f, (float)z + 0.5f);
}

static void do_save(void) {
    if (world_save(&g_world, g_saves_root, g_save_name) == 0)
        set_status("Saved world -> saves/default/");
    else
        set_status("Save FAILED");
}

static void do_load(void) {
    if (world_load(&g_world, g_saves_root, g_save_name) == 0) {
        spawn_player_on_surface();
        set_status("Loaded world from saves/default/");
    } else {
        set_status("Load FAILED (missing save?)");
    }
}

static void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_world(&g_world, &g_player);
    render_hud(&g_player, &g_inv, g_fps, g_status);
    glutSwapBuffers();
}

static void reshape(int w, int h) {
    if (h < 1) h = 1;
    g_player.win_w = w;
    g_player.win_h = h;
    glViewport(0, 0, w, h);
}

static void timer_cb(int value) {
    double t = now_sec();
    float dt;
    (void)value;

    if (g_last_time <= 0.0) g_last_time = t;
    dt = (float)(t - g_last_time);
    g_last_time = t;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0f) dt = 0.0f;

    /* shift-as-down in fly: track via special keys stored on player */
    if (g_player.flying && g_shift_down) {
        /* apply downward bias each frame via temporary key */
        g_player.keys[(unsigned char)'c'] = 1;
    }

    player_update(&g_player, &g_world, dt);

    if (g_player.flying && g_shift_down)
        g_player.keys[(unsigned char)'c'] = 0;

    /* Redraw only when camera/world changed — idle must not peg a core
     * redrawing 28^3 immediate-mode cubes every 16ms for nothing. */
    {
        float dx = g_player.x - g_prev_x, dy = g_player.y - g_prev_y, dz = g_player.z - g_prev_z;
        float dyaw = g_player.yaw - g_prev_yaw, dpitch = g_player.pitch - g_prev_pitch;
        int moved = (dx * dx + dy * dy + dz * dz > 1e-8f) ||
                    (dyaw * dyaw + dpitch * dpitch > 1e-8f);
        if (moved) g_need_redraw = 1;
        g_prev_x = g_player.x;
        g_prev_y = g_player.y;
        g_prev_z = g_player.z;
        g_prev_yaw = g_player.yaw;
        g_prev_pitch = g_player.pitch;
    }

    g_frame_count++;
    g_fps_timer += dt;
    if (g_fps_timer >= 0.5) {
        g_fps = (float)g_frame_count / (float)g_fps_timer;
        g_frame_count = 0;
        g_fps_timer = 0.0;
        g_need_redraw = 1; /* refresh HUD fps occasionally */
    }

    if (g_need_redraw) {
        g_need_redraw = 0;
        glutPostRedisplay();
    }
    if (g_running)
        glutTimerFunc(TARGET_MS, timer_cb, 0);
}

static void keyboard(unsigned char key, int x, int y) {
    int mods = glutGetModifiers();
    (void)x; (void)y;
    g_shift_down = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;

    if (key == 27) { /* Esc */
        if (g_player.mouse_captured) {
            g_player.mouse_captured = 0;
            glutSetCursor(GLUT_CURSOR_INHERIT);
            set_status("Mouse released (Esc)");
        } else {
            set_status("Esc: mouse free | Q to quit");
        }
        return;
    }
    if (key == 'q' || key == 'Q') {
        g_running = 0;
        world_free(&g_world);
        exit(0);
    }
    if (key == 'f' || key == 'F') {
        g_player.flying = !g_player.flying;
        g_player.vx = g_player.vy = g_player.vz = 0.0f;
        set_status(g_player.flying ? "Fly ON" : "Fly OFF (gravity)");
        return;
    }
    /* Ctrl+S / Ctrl+L (also raw control codes 19 / 12) */
    if (key == 19 || ((mods & GLUT_ACTIVE_CTRL) && (key == 's' || key == 'S'))) {
        do_save();
        return;
    }
    if (key == 12 || ((mods & GLUT_ACTIVE_CTRL) && (key == 'l' || key == 'L'))) {
        do_load();
        return;
    }

    if (key >= '1' && key <= '9') {
        inv_select(&g_inv, key - '1');
        return;
    }

    g_player.keys[(unsigned char)key] = 1;
}

static void keyboard_up(unsigned char key, int x, int y) {
    (void)x; (void)y;
    g_player.keys[(unsigned char)key] = 0;
}

static void special(int key, int x, int y) {
    int mods = glutGetModifiers();
    (void)x; (void)y;
    g_shift_down = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;
    if (key >= 0 && key < 256)
        g_player.special[key] = 1;
}

static void special_up(int key, int x, int y) {
    int mods = glutGetModifiers();
    (void)x; (void)y;
    g_shift_down = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;
    if (key >= 0 && key < 256)
        g_player.special[key] = 0;
}

static void mouse(int button, int state, int x, int y) {
    (void)x; (void)y;
    if (state != GLUT_DOWN) return;

    if (!g_player.mouse_captured) {
        g_player.mouse_captured = 1;
        glutSetCursor(GLUT_CURSOR_NONE);
        g_warping = 1;
        glutWarpPointer(g_player.win_w / 2, g_player.win_h / 2);
        set_status("Mouse captured — Esc to release");
        return;
    }

    g_need_redraw = 1;
    if (button == GLUT_LEFT_BUTTON && g_player.has_target) {
        world_set(&g_world, g_player.tx, g_player.ty, g_player.tz, BLK_AIR);
        set_status("Broke block");
    } else if (button == GLUT_RIGHT_BUTTON && g_player.has_target) {
        uint8_t place = inv_selected_block(&g_inv);
        if (place != BLK_AIR &&
            world_in_bounds(g_player.px, g_player.py, g_player.pz) &&
            world_get(&g_world, g_player.px, g_player.py, g_player.pz) == BLK_AIR) {
            /* don't place inside player AABB roughly */
            float px = (float)g_player.px + 0.5f;
            float py = (float)g_player.py + 0.5f;
            float pz = (float)g_player.pz + 0.5f;
            float dx = px - g_player.x;
            float dy = py - g_player.y;
            float dz = pz - g_player.z;
            if (fabsf(dx) > 0.7f || fabsf(dz) > 0.7f || fabsf(dy) > 1.2f) {
                world_set(&g_world, g_player.px, g_player.py, g_player.pz, place);
                set_status("Placed block");
            } else {
                set_status("Can't place inside self");
            }
        }
    }
}

static void motion(int x, int y) {
    int cx, cy;
    float dx, dy;
    if (!g_player.mouse_captured) return;
    if (g_warping) {
        g_warping = 0;
        return;
    }
    cx = g_player.win_w / 2;
    cy = g_player.win_h / 2;
    dx = (float)(x - cx);
    dy = (float)(y - cy);
    if (dx == 0.0f && dy == 0.0f) return;
    player_look(&g_player, dx, dy);
    g_warping = 1;
    glutWarpPointer(cx, cy);
}

static void passive_motion(int x, int y) {
    motion(x, y);
}

/* track ctrl via glutGetModifiers on events — also poll in idle path via keyboard */
static void entry(int state) {
    (void)state;
}

int main(int argc, char **argv) {
    int seed = 42;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) {
            snprintf(g_save_name, sizeof(g_save_name), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--saves-root") == 0 && i + 1 < argc) {
            snprintf(g_saves_root, sizeof(g_saves_root), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("glut-craft — freeglut voxel MVP\n");
            printf("  --seed N\n  --save NAME\n  --saves-root DIR\n");
            return 0;
        }
    }

    world_init(&g_world, seed);
    snprintf(g_world.name, sizeof(g_world.name), "%s", g_save_name);

    /* try load existing, else generate */
    if (world_load(&g_world, g_saves_root, g_save_name) != 0) {
        printf("Generating world seed=%d size=%dx%dx%d ...\n", seed, WORLD_W, WORLD_H, WORLD_D);
        world_generate(&g_world);
        set_status("New world generated");
    } else {
        printf("Loaded save '%s'\n", g_save_name);
        set_status("Loaded existing save");
    }

    inv_init(&g_inv);
    spawn_player_on_surface();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("glut-craft — 200.glut-craft");

    render_init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboard_up);
    glutSpecialFunc(special);
    glutSpecialUpFunc(special_up);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passive_motion);
    glutEntryFunc(entry);
    glutIgnoreKeyRepeat(1);

    g_last_time = now_sec();
    glutTimerFunc(TARGET_MS, timer_cb, 0);

    printf("glut-craft ready. Click window to capture mouse. Q quit.\n");
    glutMainLoop();
    return 0;
}
