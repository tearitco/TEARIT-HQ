/* gb.h — SP-IRL shared types / API
 * Tactics Pokémon-style game (freeglut)
 */
#ifndef SPIRL_H
#define SPIRL_H

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
#define PARTY_MAX     6
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
    TILE_GYM,        /* G  gym entrance */
    TILE_COUNT
} TileId;

/* item types */
typedef enum {
    ITEM_HEAL = 1,
    ITEM_CATCH,
    ITEM_OTHER
} ItemType;

#define ITEM_DEF_MAX 16
#define BAG_MAX      16

typedef struct {
    int id;
    char name[NAME_LEN];
    int type;       /* ItemType */
    int heal_amt;
    int price;
} ItemDef;

typedef struct {
    int item_id;
    int count;
} ItemStack;

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
    MODE_MSG,
    MODE_PVP_SETUP,
    MODE_PVP_BATTLE,
    MODE_PARTY,
    MODE_BAG,          /* inventory bag */
    MODE_BAG_USE,      /* bag → pick party member to use item on */
    MODE_FLY,          /* fly / fast travel menu */
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
    int  range;   /* attack range in tiles (tactics) */
} MoveDef;

typedef struct {
    int species;                 /* index into g_species */
    int level;
    int hp, max_hp;
    int atk, def, spd;
    int exp;
    int  move_id[MOVES_PER_MON];
    int pp[MOVES_PER_MON];
    int x, y;            /* grid position in tactics battle */
    int move_range;      /* tiles per turn */
    int atk_range;       /* range of attacks */
} PartyMon;

#define MAX_CONNS    8
#define MAX_TRAINERS 16
#define MAX_BADGES   8

typedef struct {
    int badge_id;
    char name[NAME_LEN];
    int party_n;
    PartyMon party[PARTY_MAX];
    int reward_item_id;
} GymLeader;

typedef struct {
    int sx, sy;                /* source tile on this map */
    char dest_path[PATH_LEN];  /* path to destination map file */
    int dx, dy;                /* destination tile */
    int dfacing;               /* player facing after transition */
} MapConn;

typedef struct {
    int x, y;             /* map position */
    int sight;            /* activation range (manhattan) */
    int party_n;
    PartyMon party[PARTY_MAX];
    char name[NAME_LEN];
    int defeated;
} Trainer;

typedef struct {
    int  id;
    char name[NAME_LEN];
    int  w, h;
    unsigned char cells[MAP_MAX_H][MAP_MAX_W];
    int  start_x, start_y;
    char path[PATH_LEN];
    MapConn conns[MAX_CONNS];
    int conns_n;
    Trainer trainers[MAX_TRAINERS];
    int trainers_n;
    GymLeader gym;
    int has_gym;
} Map;

typedef struct {
    int x, y;            /* tile position */
    int facing;          /* 0=N 1=E 2=S 3=W */
    int move_cd;         /* frames until next step allowed */
    PartyMon party[PARTY_MAX];
    int party_n;
    int has_starter;
    ItemStack bag[BAG_MAX];
    int bag_n;
    int badges[MAX_BADGES];  /* 0=unearned 1=earned */
    int badge_count;
} Player;

/* ---- tactics (PvP grid battle) ---- */
#define TACT_COLS      10
#define TACT_ROWS      8
#define TACT_MAX_UNITS 12
#define TACT_TILE      14
#define TACT_OX        10
#define TACT_OY        16

typedef struct {
    int  active;
    int  player;          /* 0=P1 1=P2 */
    int  x, y;
    int  species, level;
    int  hp, max_hp;
    int  atk, def, spd;
    int  move_range;
    int  move_id[2];
    int  moved, acted;
} TacticsUnit;

typedef struct {
    int active;
    int phase;            /* 0=P1_turn 1=P2_turn 2=winP1 3=winP2 */
    int turn_num;
    int cursor_x, cursor_y;
    int sel_unit;         /* -1 = none */
    int sub_phase;        /* 0=select 1=move 2=attack */
    int move_sel;         /* 0 or 1 — which move to use */
    int ai_state;         /* 0=idle 1=thinking 2=moving 3=attacking 4=enemyTurn */
    int ai_delay;         /* frames between AI steps */
    int ai_unit;          /* current AI unit index */
    int ai_tx, ai_ty;     /* AI move target */
    int ai_target;        /* AI attack target index */
    int ai_stuck;
    int tactics_wild;
    int tactics_trainer;
    int tactics_gym;
    int gym_badge_id;
    int trainer_idx;
    int deploying;        /* 1=deploy phase — player picks mons to send */
    int deploy_sel;       /* cursor in deploy party list */
    int deploy_timer;     /* frames spent in deploy (idle → blackout) */
    char msg[64];
    int wait;
    TacticsUnit units[TACT_MAX_UNITS];
    int unit_n;
} TacticsBattle;

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

    /* tables loaded from data/ */
    Species species[MON_MAX];
    int species_n;
    MoveDef moves[MOVE_MAX];
    int moves_n;
    ItemDef item_defs[ITEM_DEF_MAX];
    int item_defs_n;

    /* tactics state */
    TacticsBattle tactics;

    /* modal message */
    char msg[MSG_LEN];
    GameMode msg_return;

    /* title / starter / menu UI */
    int title_sel;
    int title_opts;
    int starter_sel;
    int party_sel;
    int party_sub;
    int bag_sel;         /* cursor in bag list */
    int bag_use_target;  /* selected party index when using item */
    int fly_sel;         /* cursor in fly menu */

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
int  map_find_conn(const Map *m, int x, int y, MapConn *out);

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

/* save.c */
int  save_write(const Game *g);
int  save_load(Game *g);
int  save_exists(const Game *g);

/* item.c */
int  data_load_items(Game *g, const char *path);
const ItemDef *item_def(const Game *g, int id);
int  item_add(Player *p, int item_id, int count);
int  item_remove(Player *p, int item_id, int count);
int  item_count(const Player *p, int item_id);
int  item_use_heal(Game *g, int item_id, int party_idx);

/* tactics.c */
void tactics_pvp_start(Game *g);
void tactics_start_wild(Game *g);
void tactics_start_trainer(Game *g, Trainer *t);
void tactics_input(Game *g, int vk);
void tactics_tick(Game *g);
void tactics_calc_move_range(Game *g, int uidx, int range_map[TACT_ROWS][TACT_COLS]);

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

#endif /* SPIRL_H */
