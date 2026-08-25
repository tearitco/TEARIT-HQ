/* gb.h — 203.gb-pokemon shared types / API
 * Game Boy Color (GBC) Pokémon overworld + wild battles (freeglut MVP)
 * Full RGB color — not monochrome original GB.
 */
#ifndef GB_POKEMON_H
#define GB_POKEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

/* ---- limits ---- */
#define MAP_MAX_W     64
#define MAP_MAX_H     64
#define MON_MAX       32
#define MOVE_MAX      32
#define PARTY_MAX     2
#define MOVES_PER_MON 2
#define NAME_LEN      16
#define MSG_LEN       96
#define PATH_LEN      512

/* GB screen * scale (chunky pixels) */
#define GB_W          160
#define GB_H          144
#define SCALE         4
#define WIN_W         (GB_W * SCALE)
#define WIN_H         (GB_H * SCALE)
#define TILE_PX       16          /* logical GB pixels per tile */
#define VIEW_TW       (GB_W / TILE_PX)  /* 10 */
#define VIEW_TH       (GB_H / TILE_PX)  /* 9  */

#define TARGET_MS     16          /* ~60 Hz timer, no idle spin */
#define ENCOUNTER_PCT 18          /* tall grass encounter % */

/* tile codes (from map.txt) */
typedef enum {
    TILE_PATH = 0,   /* . */
    TILE_WALL,       /* # */
    TILE_WATER,      /* ~ */
    TILE_GRASS,      /* , */
    TILE_TALL,       /* T */
    TILE_PC,         /* P  pokecenter heal */
    TILE_HOUSE,      /* H  house floor */
    TILE_COUNT
} TileId;

/* elemental types — rock-paper-scissors + normal */
typedef enum {
    TYPE_NORMAL = 0,
    TYPE_GRASS,
    TYPE_FIRE,
    TYPE_WATER,
    TYPE_COUNT
} ElemType;

typedef enum {
    MODE_TITLE = 0,
    MODE_STARTER,
    MODE_OVERWORLD,
    MODE_BATTLE,
    MODE_MSG,          /* modal message overlay */
    MODE_COUNT
} GameMode;

typedef enum {
    BPHASE_INTRO = 0,  /* "A wild X appeared!" */
    BPHASE_MENU,       /* Fight / Run */
    BPHASE_PLAYER,     /* player attack resolve */
    BPHASE_ENEMY,      /* wild attack resolve */
    BPHASE_WIN,
    BPHASE_LOSE,
    BPHASE_RUN_OK,
    BPHASE_RUN_FAIL
} BattlePhase;

typedef struct {
    int  id;
    char name[NAME_LEN];
    int  type;       /* ElemType */
    int  base_hp, base_atk, base_def, base_spd;
} Species;

typedef struct {
    int  id;
    char name[NAME_LEN];
    int  type;
    int  power;
    int  max_pp;
} MoveDef;

typedef struct {
    int species;                 /* index into g_species */
    int level;
    int hp, max_hp;
    int atk, def, spd;
    int exp;
    int move_id[MOVES_PER_MON];  /* MoveDef ids */
    int pp[MOVES_PER_MON];
} PartyMon;

typedef struct {
    int  w, h;
    unsigned char cells[MAP_MAX_H][MAP_MAX_W];
    int  start_x, start_y;
    char path[PATH_LEN];
} Map;

typedef struct {
    int x, y;            /* tile position */
    int facing;          /* 0=N 1=E 2=S 3=W */
    int move_cd;         /* frames until next step allowed */
    PartyMon party[PARTY_MAX];
    int party_n;
    int has_starter;
} Player;

typedef struct {
    int active;
    BattlePhase phase;
    PartyMon wild;
    int menu_sel;        /* 0=Fight 1=Run */
    int move_sel;
    int flash;
    char line[MSG_LEN];
    int wait_frames;     /* hold message before advancing */
} Battle;

typedef struct {
    GameMode mode;
    Map map;
    Player player;
    Battle battle;

    /* species / move tables loaded from data/ */
    Species species[MON_MAX];
    int species_n;
    MoveDef moves[MOVE_MAX];
    int moves_n;

    /* modal message */
    char msg[MSG_LEN];
    GameMode msg_return;

    /* title / starter UI */
    int title_sel;       /* 0 New 1 Continue 2 Quit */
    int starter_sel;     /* 0 LEAFY 1 EMBER 2 BUBBLE */

    /* frame / dirty */
    int need_redraw;
    int running;
    float fps;
    char status[64];

    /* paths (relative) */
    char data_dir[PATH_LEN];
    char map_path[PATH_LEN];
    char save_dir[PATH_LEN];
} Game;

/* ---- globals (one game instance) ---- */
extern Game g;

/* map.c */
int  map_load(Map *m, const char *path);
int  map_walkable(const Map *m, int x, int y);
TileId map_tile(const Map *m, int x, int y);

/* mon.c */
int  data_load_mons(Game *g, const char *path);
int  data_load_moves(Game *g, const char *path);
void mon_init_from_species(PartyMon *pm, const Species *sp, int level);
void mon_give_starter_moves(Game *g, PartyMon *pm);
int  mon_type_mult(int atk_type, int def_type); /* 0=0x 1=half 2=normal 4=super (x2 as *2) */
int  mon_calc_damage(const PartyMon *atk, const PartyMon *def, const MoveDef *mv);
int  mon_calc_damage_typed(const Game *g, const PartyMon *atk, const PartyMon *def,
                           const MoveDef *mv);
const Species *mon_species(const Game *g, int id);
const MoveDef *mon_move(const Game *g, int id);

/* virtual keys for battle/overworld (avoid GLUT in modules) */
#define VK_UP     1001
#define VK_DOWN   1002
#define VK_LEFT   1003
#define VK_RIGHT  1004
#define VK_A      1010  /* confirm Z/Enter/Space */
#define VK_B      1011  /* cancel X/Esc */

/* battle.c */
void battle_start_wild(Game *g, int species_id, int level);
void battle_input(Game *g, int key);
void battle_tick(Game *g);

/* save.c */
int  save_write(const Game *g);
int  save_load(Game *g);
int  save_exists(const Game *g);

/* render.c */
void render_frame(Game *g);
void render_init_gl(void);

/* util */
static inline double gb_now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static inline int gb_clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

#endif /* GB_POKEMON_H */
