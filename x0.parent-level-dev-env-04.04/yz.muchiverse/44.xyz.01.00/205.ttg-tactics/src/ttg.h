/* ttg.h — Community Tabletop Tactics core (house dual-render package) */
#ifndef TTG_H
#define TTG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#define BOARD_W 12
#define BOARD_H 12
#define FRAME_W 48
#define FRAME_H 22
#define MAX_UNITS 32
#define MAX_SEATS 4
#define MAX_PATH 512
#define MAX_LINE 256
#define MAX_ID 48
#define MAX_MSG 160

enum Phase { PH_TITLE = 0, PH_MATCH, PH_END };
enum Role { ROLE_KING=0, ROLE_SOLDIER, ROLE_WIZARD, ROLE_FARMER, ROLE_COUNT };

typedef struct {
    int used;
    char id[MAX_ID];
    int seat;
    enum Role role;
    int x, y;
    int hp, max_hp, atk, def;
    int moved, acted;
    int alive;
} Unit;

typedef struct {
    char root[MAX_PATH];
    enum Phase phase;
    int board_w, board_h;
    int seat_count;
    int active_seat;
    int turn_index;
    int seat_type[MAX_SEATS]; /* 0 human 1 ai */
    int clock_ms[MAX_SEATS];
    int clock_frozen;
    int pot_balance;
    int pot_settled;
    int ante;
    int match_clock_ms;
    char winner[16];
    char end_reason[32];
    char msg[MAX_MSG];
    Unit units[MAX_UNITS];
    int n_units;
    int cursor_x, cursor_y;
    char selected[MAX_ID];
    int ui_mode; /* 0 play 1 help 2 title menu */
    int menu_sel; /* title: 0 play 1 quit */
    long history_off;
    unsigned epoch;
} Game;

/* paths */
void ttg_set_root(Game *g, const char *root);
void ttg_path(const Game *g, char *out, size_t n, const char *rel);

/* fs */
int ttg_mkdir_p(const char *path);
int ttg_write_file(const char *path, const char *content);
int ttg_append_file(const char *path, const char *line);
int ttg_read_kv(const char *path, const char *key, char *val, size_t n);
int ttg_write_kv_file(const char *path, const char *const *keys, const char *const *vals, int n);

/* core */
void ttg_init_empty(Game *g);
int  ttg_init_match(Game *g, int clock_ms, int ante);
int  ttg_save_all(const Game *g);
int  ttg_load_all(Game *g);
void ttg_ledger(Game *g, const char *actor, const char *atype, const char *adata);
Unit *ttg_unit_at(Game *g, int x, int y);
Unit *ttg_unit_by_id(Game *g, const char *id);
Unit *ttg_selected(Game *g);
int  ttg_can_move(const Game *g, const Unit *u, int x, int y);
int  ttg_move(Game *g, Unit *u, int x, int y);
int  ttg_can_attack(const Game *g, const Unit *a, const Unit *d);
int  ttg_attack(Game *g, Unit *a, Unit *d);
void ttg_end_turn(Game *g);
void ttg_check_end(Game *g);
const char *ttg_role_name(enum Role r);
char ttg_role_glyph(enum Role r, int seat);

/* frame */
int ttg_compose_frame(const Game *g);
int ttg_compose_rgb(const Game *g);
void ttg_pulse(const Game *g);

/* input */
int ttg_read_keys(Game *g, int max_keys); /* process new history lines; returns count */
void ttg_handle_key(Game *g, int key);
void ttg_ai_turn_keys(Game *g); /* append keycodes for AI plan */

/* helpers */
static inline int ttg_clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }

#endif
