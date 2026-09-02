/* fort.h — shared types for 201.dwarf-fortress */
#ifndef DF_FORT_H
#define DF_FORT_H

#include <stdint.h>

#define MAP_W 48
#define MAP_H 48
#define MAX_DWARVES 8
#define MAX_ITEMS 256
#define MAX_JOBS 128
#define MAX_PATH 256
#define MAX_MSG 160
#define PANEL_W 220
#define TILE_PX 14

/* Terrain / structure on cell */
enum Terrain {
    TR_OPEN = 0,
    TR_SOIL,
    TR_ROCK,
    TR_FLOOR,
    TR_WATER,
    TR_TREE,
    TR_WALL,
    TR_WORKSHOP,
    TR_COUNT
};

/* Designation overlay */
enum Desig {
    DG_NONE = 0,
    DG_DIG,
    DG_CUT,
    DG_STOCK_WOOD,
    DG_STOCK_STONE,
    DG_BUILD_WALL,
    DG_BUILD_WORKSHOP,
    DG_COUNT
};

/* Loose items on ground */
enum ItemKind {
    IT_NONE = 0,
    IT_WOOD,
    IT_STONE,
    IT_BED,
    IT_CHAIR,
    IT_COUNT
};

/* UI / input modes */
enum UiMode {
    MODE_LOOK = 0,
    MODE_DIG,
    MODE_CUT,
    MODE_STOCK_WOOD,
    MODE_STOCK_STONE,
    MODE_BUILD_WALL,
    MODE_BUILD_WS,
    MODE_QUERY,
    MODE_BUILD_MENU
};

/* Job kinds */
enum JobKind {
    JOB_NONE = 0,
    JOB_DIG,
    JOB_CUT,
    JOB_HAUL,
    JOB_BUILD_WALL,
    JOB_BUILD_WS,
    JOB_CRAFT_BED,
    JOB_CRAFT_CHAIR
};

enum DwarfState {
    DW_IDLE = 0,
    DW_PATH,
    DW_WORK
};

typedef struct {
    uint8_t terrain;
    uint8_t desig;
    uint8_t stock_wood;
    uint8_t stock_stone;
} Tile;

typedef struct {
    int used;
    int kind;
    int x, y;
    int count;
} Item;

typedef struct {
    int used;
    int kind;
    int x, y;
    int target_item;
    int claimed_by;
    int progress;
    int work_need;
} Job;

typedef struct {
    int used;
    int x, y;
    int state;
    int job;
    int path_len;
    int path_i;
    int path_x[MAX_PATH];
    int path_y[MAX_PATH];
    int work_timer;
    int hunger;
    int thirst;
    char name[16];
} Dwarf;

typedef struct {
    Tile  tiles[MAP_H][MAP_W];
    Item  items[MAX_ITEMS];
    Dwarf dwarves[MAX_DWARVES];
    Job   jobs[MAX_JOBS];
    int   n_dwarves;
    int   year;
    int   season;
    int   day;
    int   tick;
    int   wood_stock;
    int   stone_stock;
    int   beds_made;
    int   chairs_made;
    int   seed;
    char  fort_name[32];
    char  msg[MAX_MSG];

    int cam_x, cam_y;
    int cur_x, cur_y;
    int sel_dwarf;
    int mode;
    int drag;
    int drag_x0, drag_y0;

    int paused;
    int dirty;
    int win_w, win_h;
    int craft_order;
} Fort;

#endif /* FORT_H */
