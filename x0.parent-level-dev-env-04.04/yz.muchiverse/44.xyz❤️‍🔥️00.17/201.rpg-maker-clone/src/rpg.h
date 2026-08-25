/* rpg.h — shared types for RPG Maker clone (self-contained freeglut) */
#ifndef RPG_H
#define RPG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define WIN_W 1280
#define WIN_H 720

#define MAP_W 36
#define MAP_H 28
#define MAX_EVENTS 64
#define MAX_CMDS 32
#define MAX_SWITCHES 64
#define MAX_NAV 64
#define MAX_PATH 512
#define MAX_LINE 256
#define MAX_NAME 64
#define MAX_TEXT 160
#define MAX_MAPS 32

enum Mode {
    MODE_TITLE = 0,
    MODE_MAP,
    MODE_EVENT,
    MODE_PLAY,
    MODE_DATABASE
};

enum Trigger {
    TR_ACTION = 0,
    TR_TOUCH = 1
};

enum CmdType {
    CMD_SHOW_TEXT = 0,
    CMD_SET_SWITCH,
    CMD_IF_SWITCH,
    CMD_END,
    CMD_TRANSFER,
    CMD_RET,
    CMD_COMMENT,
    CMD_EMPTY
};

enum PlaySub {
    PLAY_WALK = 0,
    PLAY_MSG = 1
};

typedef struct {
    enum CmdType type;
    char a[MAX_TEXT];   /* text / switch name / map id */
    char b[32];         /* value / x */
    char c[32];         /* y */
} Command;

typedef struct {
    int used;
    int x, y;
    char name[MAX_NAME];
    enum Trigger trigger;
    char sprite;            /* single glyph */
    Command cmds[MAX_CMDS];
    int n_cmds;
    char dir[MAX_PATH];     /* relative ev_* folder name */
} Event;

typedef struct {
    char name[MAX_NAME];
    int value;
} Switch;

typedef struct {
    char cells[MAP_H][MAP_W + 2];   /* ground layer (collision + base tile) */
    char objects[MAP_H][MAP_W + 2]; /* upper/object layer (props, machines) */
    int w, h;
} Map;

typedef struct {
    char id[MAX_NAME];
    char label[MAX_NAME];
} MapListEntry;

typedef struct {
    char root[MAX_PATH];        /* projects/demo */
    char name[MAX_NAME];
    char start_map[MAX_NAME];
    char map_id[MAX_NAME];
    int start_x, start_y;
    Map map;
    Event events[MAX_EVENTS];
    int n_events;
    Switch switches[MAX_SWITCHES];
    int n_switches;
    MapListEntry maps[MAX_MAPS];
    int n_maps;
    int dirty;
} Project;

/* ---- project I/O ---- */
int  project_load(Project *p, const char *root);
int  project_save(Project *p);
int  project_save_map(Project *p);
int  project_save_switches(Project *p);
int  project_save_event(Project *p, int ei);
int  project_load_map(Project *p, const char *map_id);
int  project_scan_maps(Project *p);
int  project_switch_map(Project *p, const char *map_id);
Event *project_event_at(Project *p, int x, int y);
int  project_add_event(Project *p, int x, int y);
int  project_ensure_dirs(Project *p);
int  switch_get(Project *p, const char *name);
void switch_set(Project *p, const char *name, int val);
void cmd_to_label(const Command *c, char *out, int n);
void cmd_from_pal_line(Command *c, const char *line);
void project_new_demo_defaults(Project *p, const char *root);

/* ---- draw helpers (draw.c) ---- */
void d_set_rgb(float r, float g, float b);
void d_fill_rect(float x, float y, float w, float h);
void d_stroke_rect(float x, float y, float w, float h);
void d_text(float x, float y, const char *s);
void d_text_big(float x, float y, const char *s);
void d_text_small(float x, float y, const char *s);
void d_panel(float x, float y, float w, float h, const char *title);
void d_tile_color(char t, float *r, float *g, float *b);
const char *d_tile_name(char t);

/* terrain palette */
#define N_PAL 5
extern const char PAL_CHARS[N_PAL];
extern const char *PAL_NAMES[N_PAL];

#endif
