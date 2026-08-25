/* sw.h — Star Wars Battlefront house clone (shared types) */
#ifndef SW_H
#define SW_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIN_W 1280
#define WIN_H 720
#define MAX_ENTS 96
#define MAX_BULLETS 256
#define MAX_FX 128
#define MAX_BUILD 256
#define MAX_POSTS 8
#define MAX_SHIPS 8
#define TICK_MS 16

enum GameMode {
    MODE_MENU = 0,
    MODE_SUPREMACY,
    MODE_DEATHMATCH,
    MODE_FREEPLAY
};

enum Team {
    TEAM_REBEL = 0,
    TEAM_EMPIRE = 1,
    TEAM_NONE = 2
};

enum ShipType {
    SHIP_INTERCEPTOR = 0, /* A-Wing class — fast */
    SHIP_FIGHTER,         /* X-Wing class — balanced */
    SHIP_BOMBER,          /* Y-Wing class — heavy */
    SHIP_FREIGHTER,       /* Freeplay hauler */
    SHIP_SPEEDER,         /* ground speeder */
    SHIP_COUNT
};

enum Weapon {
    WPN_BLASTER = 0,
    WPN_REPEATER,
    WPN_ROCKET,
    WPN_SABER,
    WPN_COUNT
};

enum Planet {
    PLANET_ENDOR = 0,   /* forest */
    PLANET_TATOOINE,    /* desert */
    PLANET_HOTH,        /* ice */
    PLANET_MUSTAFAR,    /* lava/volcanic */
    PLANET_SPACE,       /* orbital free flight */
    PLANET_COUNT
};

enum BuildType {
    BLD_NONE = 0,
    BLD_TURRET,
    BLD_SHIELD_GEN,
    BLD_OUTPOST,
    BLD_FARM,
    BLD_MINE,
    BLD_COUNT
};

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float m[16];
} Mat4;

typedef struct {
    char name[24];
    float max_hp, max_shield, max_speed, accel, turn, mass;
    float fire_rate, dmg, scale;
    float r, g, b;       /* hull tint */
    float er, eg, eb;    /* engine glow */
    int is_ground;       /* speeder */
} ShipDef;

typedef struct {
    int alive;
    int is_bot;
    int team;
    int ship;            /* ShipType */
    int weapon;
    int in_ship;         /* 1 flying, 0 on foot */
    float x, y, z;
    float vx, vy, vz;
    float yaw, pitch;    /* radians */
    float hp, shield;
    float energy;        /* freeplay / dash / saber */
    float oxygen;        /* freeplay */
    float heat;          /* weapon heat */
    float fire_cd;
    float score;
    float kills, deaths;
    float buff_timer;    /* between-round / pickups */
    float buff_mult;
    int post_cap;        /* which post capturing */
    float cap_progress;
    char name[20];
    /* AI */
    int ai_target;
    float ai_think;
    float ai_strafe;
} Entity;

typedef struct {
    int alive;
    int owner;
    int team;
    int kind; /* 0 blaster 1 rocket 2 saber slash */
    float x, y, z;
    float vx, vy, vz;
    float life;
    float dmg;
    float r, g, b;
} Bullet;

typedef struct {
    int alive;
    int kind; /* 0 explosion 1 spark 2 trail 3 saber arc */
    float x, y, z;
    float vx, vy, vz;
    float life, max_life;
    float size;
    float r, g, b;
} Fx;

typedef struct {
    int active;
    int team; /* TEAM_NONE = neutral */
    float x, y, z;
    float radius;
    float cap; /* -1 empire .. +1 rebel, 0 neutral */
    char name[24];
} CommandPost;

typedef struct {
    int used;
    int type;
    int team;
    float x, y, z;
    float hp;
} Building;

typedef struct {
    enum GameMode mode;
    enum Planet planet;
    int paused;
    int running;
    float time;
    float match_time;
    float ticket_rebel, ticket_empire; /* supremacy */
    float dm_limit;
    int local; /* player entity index */
    int n_ents;
    Entity ents[MAX_ENTS];
    Bullet bullets[MAX_BULLETS];
    Fx fx[MAX_FX];
    CommandPost posts[MAX_POSTS];
    int n_posts;
    Building builds[MAX_BUILD];
    int n_builds;
    /* freeplay resources */
    float res_ore, res_wood, res_scrap;
    int seed;
    /* camera shake */
    float shake;
    /* menu */
    int menu_sel;
    int menu_sub; /* 0 modes 1 ships 2 difficulty */
    int difficulty; /* 0 easy 1 normal 2 hard */
    int selected_ship;
    char status[160];
    int need_redraw;
    float fps;
} Game;

/* gen */
void gen_init(unsigned seed);
float gen_noise2(float x, float y);
float gen_fbm2(float x, float y, int oct);
float gen_height(enum Planet p, float x, float z);
void gen_make_noise_tex(unsigned *tex_id, int size);
void gen_make_star_tex(unsigned *tex_id, int size);
void gen_make_hull_tex(unsigned *tex_id, int size, float r, float g, float b);

/* gfx */
int  gfx_init(void);
void gfx_shutdown(void);
void gfx_resize(int w, int h);
void gfx_begin_frame(const Game *g, float aspect);
void gfx_draw_world(const Game *g);
void gfx_draw_entity(const Game *g, const Entity *e, int is_local);
void gfx_draw_bullets(const Game *g);
void gfx_draw_fx(const Game *g);
void gfx_draw_builds(const Game *g);
void gfx_end_frame(void);
void gfx_draw_sky(const Game *g, float yaw, float pitch);
unsigned gfx_ship_list(int ship);
void gfx_spawn_explosion(Game *g, float x, float y, float z, float size);

/* sim */
void sim_init_menu(Game *g);
void sim_start_mode(Game *g, enum GameMode mode);
void sim_update(Game *g, float dt);
void sim_player_input(Game *g, float dt,
                      int key_w, int key_a, int key_s, int key_d,
                      int key_space, int key_shift, int key_ctrl,
                      int mouse_l, int mouse_r,
                      float mdx, float mdy);
void sim_try_enter_exit(Game *g);
void sim_cycle_weapon(Game *g, int dir);
void sim_cycle_ship(Game *g, int dir);
void sim_place_build(Game *g, int btype);
void sim_fire(Game *g, int ei);
const ShipDef *sim_ship_def(int type);
const char *sim_mode_name(enum GameMode m);
const char *sim_planet_name(enum Planet p);
const char *sim_ship_name(int type);

/* ui */
void ui_draw_menu(const Game *g, int win_w, int win_h);
void ui_draw_hud(const Game *g, int win_w, int win_h);
void ui_draw_text(float x, float y, const char *s);
void ui_draw_text_big(float x, float y, const char *s);

/* math helpers */
static inline float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}
static inline float len3(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}
static inline void norm3(float *x, float *y, float *z) {
    float L = len3(*x, *y, *z);
    if (L > 1e-6f) { *x /= L; *y /= L; *z /= L; }
}
static inline float frand(void) {
    return (float)rand() / (float)RAND_MAX;
}

#endif
