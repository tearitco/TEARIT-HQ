/* main.c — freeglut entry, ~60fps timer (no idle spin), GBC modes */
#include "gb.h"
#include <GL/glut.h>

Game g;

static double g_last_time = 0.0;
static int g_frame_count = 0;
static double g_fps_timer = 0.0;

/* wild pool for tall grass (species ids) */
static const int WILD_POOL[] = {4, 5, 6, 7, 8, 9}; /* PIDGEY RATTATA CATERPIE WEEDLE MAGIKARP VULPIX */
static const int WILD_POOL_N = 6;

static void show_msg(const char *s) {
    snprintf(g.msg, sizeof(g.msg), "%s", s);
    g.msg_return = MODE_OVERWORLD;
    g.mode = MODE_MSG;
    g.need_redraw = 1;
}

static void heal_party(void) {
    int i;
    for (i = 0; i < g.player.party_n; i++)
        g.player.party[i].hp = g.player.party[i].max_hp;
}

static void begin_new_game(void) {
    g.player.x = g.map.start_x;
    g.player.y = g.map.start_y;
    g.player.facing = 2;
    g.player.move_cd = 0;
    g.player.party_n = 0;
    g.player.has_starter = 0;
    g.starter_sel = 0;
    g.mode = MODE_STARTER;
    g.need_redraw = 1;
}

static void give_starter(int which) {
    /* 0 LEAFY=1, 1 EMBER=2, 2 BUBBLE=3 */
    int sid = which + 1;
    const Species *sp = mon_species(&g, sid);
    PartyMon *p;
    if (!sp) return;
    p = &g.player.party[0];
    mon_init_from_species(p, sp, 5);
    mon_give_starter_moves(&g, p);
    g.player.party_n = 1;
    g.player.has_starter = 1;
    g.mode = MODE_OVERWORLD;
    snprintf(g.status, sizeof(g.status), "Go %s!", sp->name);
    show_msg(g.status);
}

static void try_encounter(void) {
    TileId t = map_tile(&g.map, g.player.x, g.player.y);
    int sid, lv;
    if (t != TILE_TALL) return;
    if ((rand() % 100) >= ENCOUNTER_PCT) return;
    sid = WILD_POOL[rand() % WILD_POOL_N];
    lv = 2 + (rand() % 4); /* 2–5 */
    battle_start_wild(&g, sid, lv);
}

static void try_step(int dx, int dy) {
    int nx, ny;
    TileId t;
    if (g.player.move_cd > 0) return;
    if (dx > 0) g.player.facing = 1;
    else if (dx < 0) g.player.facing = 3;
    else if (dy > 0) g.player.facing = 2;
    else if (dy < 0) g.player.facing = 0;

    nx = g.player.x + dx;
    ny = g.player.y + dy;
    if (!map_walkable(&g.map, nx, ny)) {
        g.need_redraw = 1;
        return;
    }
    g.player.x = nx;
    g.player.y = ny;
    g.player.move_cd = 8; /* step cooldown frames */
    g.need_redraw = 1;

    t = map_tile(&g.map, nx, ny);
    if (t == TILE_PC) {
        heal_party();
        show_msg("Pokecenter: party healed!");
        return;
    }
    try_encounter();
}

static void do_save(void) {
    if (save_write(&g) == 0)
        show_msg("Saved to saves/slot0/");
    else
        show_msg("Save FAILED");
}

/* ---- GLUT callbacks ---- */

static void display(void) {
    render_frame(&g);
}

static void reshape(int w, int h) {
    (void)w; (void)h;
    /* fixed logical resolution; stretch to window */
    glViewport(0, 0, WIN_W, WIN_H);
    g.need_redraw = 1;
}

static void timer_cb(int value) {
    double t = gb_now_sec();
    float dt;
    (void)value;

    if (g_last_time <= 0.0) g_last_time = t;
    dt = (float)(t - g_last_time);
    g_last_time = t;
    if (dt > 0.1f) dt = 0.1f;

    if (g.player.move_cd > 0)
        g.player.move_cd--;

    if (g.mode == MODE_BATTLE)
        battle_tick(&g);

    g_frame_count++;
    g_fps_timer += dt;
    if (g_fps_timer >= 0.5) {
        g.fps = (float)g_frame_count / (float)g_fps_timer;
        g_frame_count = 0;
        g_fps_timer = 0.0;
        /* optional HUD refresh — keep cheap */
    }

    if (g.need_redraw) {
        g.need_redraw = 0;
        glutPostRedisplay();
    }
    if (g.running)
        glutTimerFunc(TARGET_MS, timer_cb, 0);
}

static void handle_vk(int vk) {
    switch (g.mode) {
    case MODE_TITLE:
        if (vk == VK_UP) {
            g.title_sel = (g.title_sel + 2) % 3;
            g.need_redraw = 1;
        } else if (vk == VK_DOWN) {
            g.title_sel = (g.title_sel + 1) % 3;
            g.need_redraw = 1;
        } else if (vk == VK_A) {
            if (g.title_sel == 0) {
                begin_new_game();
            } else if (g.title_sel == 1) {
                if (save_load(&g) == 0) {
                    g.mode = MODE_OVERWORLD;
                    show_msg("Continue — save loaded.");
                } else {
                    snprintf(g.msg, sizeof(g.msg), "No save in slot0.");
                    g.msg_return = MODE_TITLE;
                    g.mode = MODE_MSG;
                    g.need_redraw = 1;
                }
            } else {
                g.running = 0;
                exit(0);
            }
        }
        break;

    case MODE_STARTER:
        if (vk == VK_LEFT) {
            g.starter_sel = (g.starter_sel + 2) % 3;
            g.need_redraw = 1;
        } else if (vk == VK_RIGHT) {
            g.starter_sel = (g.starter_sel + 1) % 3;
            g.need_redraw = 1;
        } else if (vk == VK_A) {
            give_starter(g.starter_sel);
        }
        break;

    case MODE_OVERWORLD:
        if (vk == VK_UP) try_step(0, -1);
        else if (vk == VK_DOWN) try_step(0, 1);
        else if (vk == VK_LEFT) try_step(-1, 0);
        else if (vk == VK_RIGHT) try_step(1, 0);
        break;

    case MODE_BATTLE:
        battle_input(&g, vk);
        break;

    case MODE_MSG:
        if (vk == VK_A) {
            g.mode = g.msg_return;
            g.need_redraw = 1;
        }
        break;

    default:
        break;
    }
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 'q' || key == 'Q') {
        g.running = 0;
        exit(0);
    }
    if (key == 27) { /* Esc */
        if (g.mode == MODE_BATTLE)
            battle_input(&g, VK_B);
        else if (g.mode == MODE_OVERWORLD)
            g.mode = MODE_TITLE;
        g.need_redraw = 1;
        return;
    }
    /* save hotkey */
    if (key == 19 || key == 'S') { /* Ctrl+S or S on title-ish — use F5 via special */
        /* fallthrough */
    }

    switch (key) {
    case 'w': case 'W': handle_vk(VK_UP); break;
    case 's': case 'S':
        if (g.mode == MODE_OVERWORLD) {
            /* S alone is down; Shift+S unused */
            handle_vk(VK_DOWN);
        } else {
            handle_vk(VK_DOWN);
        }
        break;
    case 'a': case 'A': handle_vk(VK_LEFT); break;
    case 'd': case 'D': handle_vk(VK_RIGHT); break;
    case 'z': case 'Z': case ' ': case '\r': handle_vk(VK_A); break;
    case 'x': case 'X': handle_vk(VK_B); break;
    case '1': case '2':
        if (g.mode == MODE_BATTLE)
            battle_input(&g, (int)key);
        break;
    default:
        break;
    }
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    switch (key) {
    case GLUT_KEY_UP:    handle_vk(VK_UP); break;
    case GLUT_KEY_DOWN:  handle_vk(VK_DOWN); break;
    case GLUT_KEY_LEFT:  handle_vk(VK_LEFT); break;
    case GLUT_KEY_RIGHT: handle_vk(VK_RIGHT); break;
    case GLUT_KEY_F5:
        if (g.mode == MODE_OVERWORLD || g.mode == MODE_MSG)
            do_save();
        break;
    default:
        break;
    }
}

static int boot_data(void) {
    char path[PATH_LEN];

    snprintf(g.data_dir, sizeof(g.data_dir), "data");
    snprintf(g.map_path, sizeof(g.map_path), "maps/pallet/map.txt");
    snprintf(g.save_dir, sizeof(g.save_dir), "saves/slot0");

    snprintf(path, sizeof(path), "%s/mons.pdl", g.data_dir);
    if (data_load_mons(&g, path) != 0) {
        fprintf(stderr, "boot: mons.pdl failed\n");
        return -1;
    }
    snprintf(path, sizeof(path), "%s/moves.pdl", g.data_dir);
    if (data_load_moves(&g, path) != 0) {
        fprintf(stderr, "boot: moves.pdl failed\n");
        return -1;
    }
    if (map_load(&g.map, g.map_path) != 0) {
        fprintf(stderr, "boot: map failed\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argv;
    memset(&g, 0, sizeof(g));
    g.running = 1;
    g.need_redraw = 1;
    g.mode = MODE_TITLE;
    g.title_sel = 0;
    srand((unsigned)time(NULL));

    if (boot_data() != 0) {
        fprintf(stderr, "Failed to load data/ — run from 203.gb-pokemon/\n");
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("203.gb-pokemon — GBC Pokemon (full color)");
    render_init_gl();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    /* no glutIdleFunc — timer only */
    glutTimerFunc(TARGET_MS, timer_cb, 0);

    fprintf(stderr, "gb-pokemon ready  map %dx%d  species %d  moves %d\n",
            g.map.w, g.map.h, g.species_n, g.moves_n);
    fprintf(stderr, "Title: New/Continue  Overworld: arrows  Tall grass: battle\n");
    fprintf(stderr, "Battle: Fight/Run or 1/2 moves  F5 save  q quit\n");

    glutMainLoop();
    return 0;
}
