/* civ.h — SNES-era Civilization clone shared types & API */
#ifndef CIV_H
#define CIV_H

#include <stdint.h>

#define MAP_W 40
#define MAP_H 30
#define MAX_CIVS 4
#define MAX_UNITS 128
#define MAX_CITIES 48
#define MAX_MSG 160
#define VIEW_MARGIN 8

/* Terrain */
enum Terrain {
    T_OCEAN = 0,
    T_PLAINS,
    T_FOREST,
    T_HILLS,
    T_MOUNTAIN,
    T_SPECIAL, /* bonus plains */
    T_COUNT
};

/* Unit kinds */
enum UnitKind {
    U_SETTLER = 0,
    U_WARRIOR,
    U_SCOUT,
    U_KIND_COUNT
};

/* City production choices */
enum ProdKind {
    PROD_WARRIOR = 0,
    PROD_SETTLER,
    PROD_SCOUT,
    PROD_COUNT
};

typedef struct {
    uint8_t terrain;
    uint8_t explored; /* bit per civ 0..3 */
    int8_t  elev;     /* visual elevation cue 0..3 */
} Tile;

typedef struct {
    int used;
    int civ;
    int kind;
    int x, y;
    int moves_left;
    int hp;       /* 0..10 */
    int max_hp;
} Unit;

typedef struct {
    int used;
    int civ;
    int x, y;
    int pop;          /* 1.. */
    int food_store;
    int food_needed;
    int shields;
    int prod;         /* ProdKind */
    int size_vis;     /* for glyph */
    char name[24];
} City;

typedef struct {
    int used;
    int is_player;
    int gold;
    int science;      /* stub points */
    float color[3];
    char name[24];
    int alive;
} Civ;

typedef struct {
    Tile tiles[MAP_H][MAP_W];
    Civ  civs[MAX_CIVS];
    Unit units[MAX_UNITS];
    City cities[MAX_CITIES];
    int  n_civs;
    int  turn;            /* 0-based */
    int  year;            /* e.g. -4000 = 4000 BC */
    int  active_civ;      /* whose turn; 0 = player */
    int  sel_unit;        /* index or -1 */
    int  sel_city;        /* index or -1 */
    int  cam_x, cam_y;    /* top-left tile of view */
    int  win_w, win_h;
    int  seed;
    char msg[MAX_MSG];
    int  dirty;           /* needs redraw */
    int  game_over;       /* 0 play, 1 win, -1 lose */
    int  hover_x, hover_y;
    int  end_turn_pending;
} Game;

/* map.c */
void map_generate(Game *g, int seed);
int  map_move_cost(const Game *g, int x, int y);
int  map_passable(const Game *g, int x, int y);
void map_tile_color(const Game *g, int x, int y, float *r, float *gcol, float *b);
int  map_wrap_x(int x);
int  map_clamp_y(int y);
void map_reveal(Game *g, int civ, int x, int y, int radius);

/* game.c */
void game_init(Game *g, int seed);
void game_select_unit(Game *g, int idx);
void game_select_next_unit(Game *g);
int  game_try_move(Game *g, int dx, int dy);
int  game_found_city(Game *g);
void game_end_turn(Game *g);
void game_click(Game *g, int tile_x, int tile_y, int button);
void game_cycle_prod(Game *g, int dir);
void game_set_msg(Game *g, const char *fmt, ...);
const char *unit_kind_name(int kind);
const char *prod_kind_name(int kind);
const char *terrain_name(int t);
int  game_unit_at(const Game *g, int x, int y);
int  game_city_at(const Game *g, int x, int y);
void game_center_on(Game *g, int x, int y);

/* render.c */
void render_init(void);
void render_frame(const Game *g, float fps);
void render_map_layout(const Game *g, float *ox, float *oy, float *ts, int *cols, int *rows);

#endif /* CIV_H */
