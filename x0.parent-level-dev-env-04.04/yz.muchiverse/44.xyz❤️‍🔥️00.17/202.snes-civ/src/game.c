/* game.c — units, cities, turns, combat, simple AI */
#include "civ.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

/* ---- names / tables ---- */

static const char *UNIT_NAMES[U_KIND_COUNT] = { "Settler", "Warrior", "Scout" };
static const char *PROD_NAMES[PROD_COUNT]   = { "Warrior", "Settler", "Scout" };
static const char *TERRAIN_NAMES[T_COUNT] = {
    "Ocean", "Plains", "Forest", "Hills", "Mountain", "Special"
};

static const int UNIT_MOVES[U_KIND_COUNT]  = { 1, 1, 2 };
static const int UNIT_ATK[U_KIND_COUNT]    = { 0, 2, 1 };
static const int UNIT_DEF[U_KIND_COUNT]    = { 1, 2, 1 };
static const int UNIT_HP[U_KIND_COUNT]     = { 10, 10, 10 };
static const int PROD_COST[PROD_COUNT]     = { 10, 20, 8 }; /* shields */
static const int PROD_TO_UNIT[PROD_COUNT]  = { U_WARRIOR, U_SETTLER, U_SCOUT };

static const char *CITY_NAME_POOL[] = {
    "Rome", "Antium", "Cumae", "Neapolis", "Pompeii", "Ravenna",
    "Thebes", "Memphis", "Heliopolis", "Abydos",
    "Tenochtitlan", "Tlaxcala", "Texcoco",
    "Babylon", "Ur", "Nineveh", "Assur",
    "Beijing", "Shanghai", "Canton",
    "Athens", "Sparta", "Corinth"
};
#define CITY_NAME_POOL_N (int)(sizeof(CITY_NAME_POOL) / sizeof(CITY_NAME_POOL[0]))

const char *unit_kind_name(int kind) {
    if (kind < 0 || kind >= U_KIND_COUNT) return "?";
    return UNIT_NAMES[kind];
}
const char *prod_kind_name(int kind) {
    if (kind < 0 || kind >= PROD_COUNT) return "?";
    return PROD_NAMES[kind];
}
const char *terrain_name(int t) {
    if (t < 0 || t >= T_COUNT) return "?";
    return TERRAIN_NAMES[t];
}

void game_set_msg(Game *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->msg, MAX_MSG, fmt, ap);
    va_end(ap);
    g->dirty = 1;
}

/* ---- helpers ---- */

int game_unit_at(const Game *g, int x, int y) {
    int i;
    x = map_wrap_x(x);
    y = map_clamp_y(y);
    for (i = 0; i < MAX_UNITS; i++) {
        if (g->units[i].used && g->units[i].x == x && g->units[i].y == y)
            return i;
    }
    return -1;
}

int game_city_at(const Game *g, int x, int y) {
    int i;
    x = map_wrap_x(x);
    y = map_clamp_y(y);
    for (i = 0; i < MAX_CITIES; i++) {
        if (g->cities[i].used && g->cities[i].x == x && g->cities[i].y == y)
            return i;
    }
    return -1;
}

static int find_free_unit(Game *g) {
    int i;
    for (i = 0; i < MAX_UNITS; i++)
        if (!g->units[i].used) return i;
    return -1;
}

static int find_free_city(Game *g) {
    int i;
    for (i = 0; i < MAX_CITIES; i++)
        if (!g->cities[i].used) return i;
    return -1;
}

static int spawn_unit(Game *g, int civ, int kind, int x, int y) {
    int i = find_free_unit(g);
    Unit *u;
    if (i < 0) return -1;
    x = map_wrap_x(x);
    y = map_clamp_y(y);
    u = &g->units[i];
    memset(u, 0, sizeof(*u));
    u->used = 1;
    u->civ = civ;
    u->kind = kind;
    u->x = x;
    u->y = y;
    u->moves_left = UNIT_MOVES[kind];
    u->max_hp = UNIT_HP[kind];
    u->hp = u->max_hp;
    map_reveal(g, civ, x, y, kind == U_SCOUT ? 3 : 2);
    return i;
}

static void kill_unit(Game *g, int idx) {
    if (idx < 0 || idx >= MAX_UNITS) return;
    if (g->sel_unit == idx) g->sel_unit = -1;
    g->units[idx].used = 0;
}

static int land_tile_ok_for_start(const Game *g, int x, int y) {
    int t;
    if (!map_passable(g, x, y)) return 0;
    t = g->tiles[y][map_wrap_x(x)].terrain;
    if (t == T_MOUNTAIN) return 0;
    if (game_city_at(g, x, y) >= 0) return 0;
    return 1;
}

static int find_land_spot(const Game *g, int prefer_x, int prefer_y, int *ox, int *oy) {
    int r, dy, dx;
    for (r = 0; r < MAP_W; r++) {
        for (dy = -r; dy <= r; dy++) {
            for (dx = -r; dx <= r; dx++) {
                int x = map_wrap_x(prefer_x + dx);
                int y = prefer_y + dy;
                if (y < 0 || y >= MAP_H) continue;
                if (abs(dx) != r && abs(dy) != r && r > 0) continue;
                if (land_tile_ok_for_start(g, x, y)) {
                    *ox = x; *oy = y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void game_center_on(Game *g, int x, int y) {
    /* keep selected roughly in view; cam is top-left of tile grid */
    int vis_w = 20, vis_h = 14;
    g->cam_x = map_wrap_x(x - vis_w / 2);
    g->cam_y = y - vis_h / 2;
    if (g->cam_y < 0) g->cam_y = 0;
    if (g->cam_y > MAP_H - vis_h) g->cam_y = MAP_H - vis_h;
    if (g->cam_y < 0) g->cam_y = 0;
    g->dirty = 1;
}

void game_select_unit(Game *g, int idx) {
    if (idx < 0 || idx >= MAX_UNITS || !g->units[idx].used) {
        g->sel_unit = -1;
        g->dirty = 1;
        return;
    }
    if (g->units[idx].civ != 0) {
        /* can still select enemy for inspect but no move */
    }
    g->sel_unit = idx;
    g->sel_city = game_city_at(g, g->units[idx].x, g->units[idx].y);
    game_center_on(g, g->units[idx].x, g->units[idx].y);
    g->dirty = 1;
}

void game_select_next_unit(Game *g) {
    int start = g->sel_unit < 0 ? 0 : g->sel_unit + 1;
    int i, n;
    for (n = 0; n < MAX_UNITS; n++) {
        i = (start + n) % MAX_UNITS;
        if (g->units[i].used && g->units[i].civ == 0 && g->units[i].moves_left > 0) {
            game_select_unit(g, i);
            game_set_msg(g, "Selected %s (%d MP)",
                         unit_kind_name(g->units[i].kind),
                         g->units[i].moves_left);
            return;
        }
    }
    /* any player unit */
    for (i = 0; i < MAX_UNITS; i++) {
        if (g->units[i].used && g->units[i].civ == 0) {
            game_select_unit(g, i);
            game_set_msg(g, "Selected %s (no moves left)",
                         unit_kind_name(g->units[i].kind));
            return;
        }
    }
    game_set_msg(g, "No player units");
}

/* ---- combat ---- */

static int combat(Game *g, int atk_i, int def_i) {
    Unit *a = &g->units[atk_i];
    Unit *d = &g->units[def_i];
    int atk = UNIT_ATK[a->kind];
    int def = UNIT_DEF[d->kind];
    int roll_a, roll_d, dmg;
    City *c;
    /* city defense bonus */
    c = (game_city_at(g, d->x, d->y) >= 0)
        ? &g->cities[game_city_at(g, d->x, d->y)] : NULL;
    if (c && c->civ == d->civ) def += 1 + c->pop / 3;
    if (atk < 1) atk = 1;
    roll_a = atk + (rand() % 3);
    roll_d = def + (rand() % 3);
    if (roll_a >= roll_d) {
        dmg = 3 + (roll_a - roll_d);
        d->hp -= dmg;
        if (d->hp <= 0) {
            game_set_msg(g, "%s defeats %s!",
                         unit_kind_name(a->kind), unit_kind_name(d->kind));
            kill_unit(g, def_i);
            return 1; /* attacker wins tile */
        }
        game_set_msg(g, "%s hits %s (-%d HP, %d left)",
                     unit_kind_name(a->kind), unit_kind_name(d->kind),
                     dmg, d->hp);
    } else {
        dmg = 2 + (roll_d - roll_a) / 2;
        a->hp -= dmg;
        if (a->hp <= 0) {
            game_set_msg(g, "%s dies attacking %s",
                         unit_kind_name(a->kind), unit_kind_name(d->kind));
            kill_unit(g, atk_i);
            return -1;
        }
        game_set_msg(g, "%s rebuffed (-%d HP)", unit_kind_name(a->kind), dmg);
    }
    return 0;
}

/* ---- movement ---- */

int game_try_move(Game *g, int dx, int dy) {
    Unit *u;
    int nx, ny, cost, other, city_i;
    if (g->game_over) return 0;
    if (g->active_civ != 0) return 0;
    if (g->sel_unit < 0) return 0;
    u = &g->units[g->sel_unit];
    if (!u->used || u->civ != 0) return 0;
    if (u->moves_left <= 0) {
        game_set_msg(g, "No movement points — End Turn or N for next");
        return 0;
    }
    if (dx == 0 && dy == 0) return 0;
    /* one step cardinal or diagonal ok */
    if (abs(dx) > 1 || abs(dy) > 1) return 0;

    nx = map_wrap_x(u->x + dx);
    ny = u->y + dy;
    if (ny < 0 || ny >= MAP_H) {
        game_set_msg(g, "Edge of the world");
        return 0;
    }
    if (!map_passable(g, nx, ny)) {
        game_set_msg(g, "Cannot enter ocean");
        return 0;
    }
    cost = map_move_cost(g, nx, ny);
    /* allow move if at least 1 MP remains (last step can overspend like Civ) */
    if (u->moves_left <= 0) return 0;

    other = game_unit_at(g, nx, ny);
    if (other >= 0 && other != g->sel_unit) {
        if (g->units[other].civ == u->civ) {
            game_set_msg(g, "Tile occupied by friendly unit");
            return 0;
        }
        /* attack */
        {
            int r = combat(g, g->sel_unit, other);
            u->moves_left = 0;
            if (r == 1 && u->used) {
                u->x = nx;
                u->y = ny;
                map_reveal(g, u->civ, nx, ny, u->kind == U_SCOUT ? 3 : 2);
                /* capture empty enemy city? */
                city_i = game_city_at(g, nx, ny);
                if (city_i >= 0 && g->cities[city_i].civ != u->civ) {
                    /* if no defenders left */
                    if (game_unit_at(g, nx, ny) < 0 ||
                        g->units[game_unit_at(g, nx, ny)].civ == u->civ) {
                        g->cities[city_i].civ = u->civ;
                        game_set_msg(g, "Captured %s!", g->cities[city_i].name);
                    }
                }
            }
            g->dirty = 1;
            return 1;
        }
    }

    city_i = game_city_at(g, nx, ny);
    if (city_i >= 0 && g->cities[city_i].civ != u->civ) {
        /* enter enemy city only if empty of defenders */
        other = game_unit_at(g, nx, ny);
        if (other >= 0 && g->units[other].civ != u->civ) {
            int r = combat(g, g->sel_unit, other);
            u->moves_left = 0;
            g->dirty = 1;
            if (r != 1) return 1;
        }
        if (u->used) {
            g->cities[city_i].civ = u->civ;
            u->x = nx;
            u->y = ny;
            u->moves_left -= cost;
            if (u->moves_left < 0) u->moves_left = 0;
            map_reveal(g, u->civ, nx, ny, 2);
            game_set_msg(g, "Captured city %s!", g->cities[city_i].name);
            g->dirty = 1;
            return 1;
        }
    }

    u->x = nx;
    u->y = ny;
    u->moves_left -= cost;
    if (u->moves_left < 0) u->moves_left = 0;
    map_reveal(g, u->civ, nx, ny, u->kind == U_SCOUT ? 3 : 2);
    g->sel_city = game_city_at(g, nx, ny);
    game_set_msg(g, "%s → (%d,%d) MP %d  [%s]",
                 unit_kind_name(u->kind), nx, ny, u->moves_left,
                 terrain_name(g->tiles[ny][nx].terrain));
    g->dirty = 1;
    return 1;
}

/* ---- cities ---- */

static int tile_food(const Game *g, int x, int y) {
    uint8_t t = g->tiles[map_clamp_y(y)][map_wrap_x(x)].terrain;
    switch (t) {
    case T_PLAINS: return 2;
    case T_SPECIAL: return 3;
    case T_FOREST: return 1;
    case T_HILLS: return 1;
    case T_MOUNTAIN: return 0;
    default: return 0;
    }
}

static int tile_shields(const Game *g, int x, int y) {
    uint8_t t = g->tiles[map_clamp_y(y)][map_wrap_x(x)].terrain;
    switch (t) {
    case T_PLAINS: return 1;
    case T_SPECIAL: return 2;
    case T_FOREST: return 2;
    case T_HILLS: return 2;
    case T_MOUNTAIN: return 1;
    default: return 0;
    }
}

static void city_yields(const Game *g, const City *c, int *food, int *shields, int *gold) {
    int dx, dy, f = 0, s = 0;
    /* center + ring1 */
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int x = map_wrap_x(c->x + dx);
            int y = c->y + dy;
            if (y < 0 || y >= MAP_H) continue;
            if (g->tiles[y][x].terrain == T_OCEAN) continue;
            f += tile_food(g, x, y);
            s += tile_shields(g, x, y);
        }
    }
    /* pop scales a bit */
    f = f / 2 + c->pop;
    s = s / 3 + 1;
    if (food) *food = f;
    if (shields) *shields = s;
    if (gold) *gold = c->pop;
}

static void refresh_city_name(Game *g, City *c, int idx) {
    int n = idx % CITY_NAME_POOL_N;
    snprintf(c->name, sizeof(c->name), "%s", CITY_NAME_POOL[n]);
    (void)g;
}

int game_found_city(Game *g) {
    Unit *u;
    int ci, x, y;
    City *c;
    if (g->game_over || g->active_civ != 0) return 0;
    if (g->sel_unit < 0) {
        game_set_msg(g, "Select a Settler first (N)");
        return 0;
    }
    u = &g->units[g->sel_unit];
    if (!u->used || u->civ != 0 || u->kind != U_SETTLER) {
        game_set_msg(g, "Only Settlers can found cities (B)");
        return 0;
    }
    x = u->x; y = u->y;
    if (game_city_at(g, x, y) >= 0) {
        game_set_msg(g, "City already here");
        return 0;
    }
    if (!map_passable(g, x, y)) return 0;
    ci = find_free_city(g);
    if (ci < 0) {
        game_set_msg(g, "City limit reached");
        return 0;
    }
    c = &g->cities[ci];
    memset(c, 0, sizeof(*c));
    c->used = 1;
    c->civ = 0;
    c->x = x;
    c->y = y;
    c->pop = 1;
    c->food_store = 0;
    c->food_needed = 10;
    c->shields = 0;
    c->prod = PROD_WARRIOR;
    c->size_vis = 1;
    refresh_city_name(g, c, ci);
    /* consume settler */
    kill_unit(g, g->sel_unit);
    g->sel_unit = -1;
    g->sel_city = ci;
    map_reveal(g, 0, x, y, 3);
    game_set_msg(g, "Founded %s!", c->name);
    g->dirty = 1;
    return 1;
}

void game_cycle_prod(Game *g, int dir) {
    City *c;
    if (g->sel_city < 0 || !g->cities[g->sel_city].used) return;
    c = &g->cities[g->sel_city];
    if (c->civ != 0) return;
    c->prod = (c->prod + dir + PROD_COUNT) % PROD_COUNT;
    game_set_msg(g, "%s produces %s (%d shields)",
                 c->name, prod_kind_name(c->prod), PROD_COST[c->prod]);
    g->dirty = 1;
}

/* ---- turn processing ---- */

static void process_city(Game *g, int ci) {
    City *c = &g->cities[ci];
    int food, shields, gold, need;
    if (!c->used) return;
    city_yields(g, c, &food, &shields, &gold);
    g->civs[c->civ].gold += gold;
    g->civs[c->civ].science += 1; /* stub */

    c->food_store += food - c->pop; /* pop eats 1 each */
    if (c->food_store < 0) {
        c->food_store = 0;
        if (c->pop > 1) c->pop--;
    }
    c->food_needed = 10 + c->pop * 5;
    if (c->food_store >= c->food_needed) {
        c->food_store -= c->food_needed;
        c->pop++;
        if (c->pop > 20) c->pop = 20;
        c->size_vis = c->pop;
    }

    c->shields += shields;
    need = PROD_COST[c->prod];
    if (c->shields >= need) {
        int ui, sx = c->x, sy = c->y;
        int kind = PROD_TO_UNIT[c->prod];
        /* find free adjacent or same tile */
        if (game_unit_at(g, sx, sy) >= 0) {
            int d;
            int found = 0;
            for (d = 0; d < 8 && !found; d++) {
                static const int odx[8] = {1,-1,0,0,1,1,-1,-1};
                static const int ody[8] = {0,0,1,-1,1,-1,1,-1};
                int tx = map_wrap_x(c->x + odx[d]);
                int ty = c->y + ody[d];
                if (ty < 0 || ty >= MAP_H) continue;
                if (!map_passable(g, tx, ty)) continue;
                if (game_unit_at(g, tx, ty) >= 0) continue;
                sx = tx; sy = ty;
                found = 1;
            }
            if (!found) {
                /* still spawn on city; stacking not allowed so skip */
                if (c->civ == 0)
                    game_set_msg(g, "%s: no space for unit", c->name);
                return;
            }
        }
        c->shields -= need;
        ui = spawn_unit(g, c->civ, kind, sx, sy);
        if (ui >= 0 && c->civ == 0)
            game_set_msg(g, "%s built a %s!", c->name, unit_kind_name(kind));
    }
}

static void refresh_moves(Game *g, int civ) {
    int i;
    for (i = 0; i < MAX_UNITS; i++) {
        if (g->units[i].used && g->units[i].civ == civ)
            g->units[i].moves_left = UNIT_MOVES[g->units[i].kind];
    }
}

static int civ_has_units_or_cities(const Game *g, int civ) {
    int i;
    for (i = 0; i < MAX_UNITS; i++)
        if (g->units[i].used && g->units[i].civ == civ) return 1;
    for (i = 0; i < MAX_CITIES; i++)
        if (g->cities[i].used && g->cities[i].civ == civ) return 1;
    return 0;
}

static void check_victory(Game *g) {
    int c, players = 0, enemies = 0;
    if (!g->civs[0].alive) {
        g->game_over = -1;
        game_set_msg(g, "DEFEAT — your civilization has fallen.");
        return;
    }
    for (c = 0; c < g->n_civs; c++) {
        if (!g->civs[c].alive) continue;
        if (c == 0) players++;
        else enemies++;
    }
    if (enemies == 0 && players > 0) {
        g->game_over = 1;
        game_set_msg(g, "VICTORY — all rival civs eliminated!");
    }
}

/* ---- AI (greedy / random) ---- */

static int ai_found_city(Game *g, int ui) {
    Unit *u = &g->units[ui];
    int ci;
    City *c;
    if (u->kind != U_SETTLER) return 0;
    if (game_city_at(g, u->x, u->y) >= 0) return 0;
    /* don't settle adjacent to own city */
    {
        int dx, dy;
        for (dy = -2; dy <= 2; dy++)
            for (dx = -2; dx <= 2; dx++) {
                int cidx = game_city_at(g, u->x + dx, u->y + dy);
                if (cidx >= 0 && g->cities[cidx].civ == u->civ) return 0;
            }
    }
    ci = find_free_city(g);
    if (ci < 0) return 0;
    c = &g->cities[ci];
    memset(c, 0, sizeof(*c));
    c->used = 1;
    c->civ = u->civ;
    c->x = u->x;
    c->y = u->y;
    c->pop = 1;
    c->food_needed = 10;
    c->prod = PROD_WARRIOR;
    c->size_vis = 1;
    refresh_city_name(g, c, ci + u->civ * 7);
    kill_unit(g, ui);
    map_reveal(g, c->civ, c->x, c->y, 3);
    return 1;
}

static void ai_move_unit(Game *g, int ui) {
    Unit *u = &g->units[ui];
    int tries, t;
    if (!u->used) return;
    /* settlers: prefer founding if good plains */
    if (u->kind == U_SETTLER && u->moves_left > 0) {
        int ttype = g->tiles[u->y][u->x].terrain;
        if ((ttype == T_PLAINS || ttype == T_SPECIAL || ttype == T_FOREST) &&
            (rand() % 3) == 0) {
            if (ai_found_city(g, ui)) return;
        }
    }
    for (tries = 0; tries < 12 && u->used && u->moves_left > 0; tries++) {
        int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        int d = rand() % 8;
        int nx = map_wrap_x(u->x + dirs[d][0]);
        int ny = u->y + dirs[d][1];
        int other, cost;
        if (ny < 0 || ny >= MAP_H) continue;
        if (!map_passable(g, nx, ny)) continue;
        cost = map_move_cost(g, nx, ny);
        other = game_unit_at(g, nx, ny);
        if (other >= 0) {
            if (g->units[other].civ == u->civ) continue;
            /* attack player / rivals if warrior-ish */
            if (UNIT_ATK[u->kind] > 0) {
                int r = combat(g, ui, other);
                u->moves_left = 0;
                if (r == 1 && u->used) {
                    u->x = nx;
                    u->y = ny;
                    map_reveal(g, u->civ, nx, ny, 2);
                    t = game_city_at(g, nx, ny);
                    if (t >= 0 && g->cities[t].civ != u->civ)
                        g->cities[t].civ = u->civ;
                }
            }
            break;
        }
        t = game_city_at(g, nx, ny);
        if (t >= 0 && g->cities[t].civ != u->civ) {
            if (UNIT_ATK[u->kind] > 0) {
                g->cities[t].civ = u->civ;
                u->x = nx; u->y = ny;
                u->moves_left = 0;
                map_reveal(g, u->civ, nx, ny, 2);
            }
            break;
        }
        u->x = nx;
        u->y = ny;
        u->moves_left -= cost;
        if (u->moves_left < 0) u->moves_left = 0;
        map_reveal(g, u->civ, nx, ny, u->kind == U_SCOUT ? 3 : 2);
    }
    /* settler last chance to found */
    if (u->used && u->kind == U_SETTLER)
        ai_found_city(g, ui);
}

static void ai_turn(Game *g, int civ) {
    int i;
    if (!g->civs[civ].alive) return;
    refresh_moves(g, civ);
    for (i = 0; i < MAX_CITIES; i++) {
        if (g->cities[i].used && g->cities[i].civ == civ) {
            /* AI production bias */
            if (g->cities[i].pop >= 2 && (rand() % 4) == 0)
                g->cities[i].prod = PROD_SETTLER;
            else if ((rand() % 3) == 0)
                g->cities[i].prod = PROD_SCOUT;
            else
                g->cities[i].prod = PROD_WARRIOR;
            process_city(g, i);
        }
    }
    for (i = 0; i < MAX_UNITS; i++) {
        if (g->units[i].used && g->units[i].civ == civ)
            ai_move_unit(g, i);
    }
}

void game_end_turn(Game *g) {
    int c, i;
    if (g->game_over) return;
    if (g->active_civ != 0) return;

    /* player city production */
    for (i = 0; i < MAX_CITIES; i++)
        if (g->cities[i].used && g->cities[i].civ == 0)
            process_city(g, i);

    /* AI civs */
    for (c = 1; c < g->n_civs; c++) {
        if (!civ_has_units_or_cities(g, c)) {
            if (g->civs[c].alive) {
                g->civs[c].alive = 0;
                game_set_msg(g, "%s has been destroyed!", g->civs[c].name);
            }
            continue;
        }
        ai_turn(g, c);
        if (!civ_has_units_or_cities(g, c))
            g->civs[c].alive = 0;
    }

    if (!civ_has_units_or_cities(g, 0)) {
        g->civs[0].alive = 0;
        check_victory(g);
        g->dirty = 1;
        return;
    }

    /* next year */
    g->turn++;
    if (g->year < 0) {
        g->year += 20;
        if (g->year >= 0) g->year = 1; /* skip year 0 */
    } else {
        g->year += (g->year < 1000) ? 20 : 10;
    }

    refresh_moves(g, 0);
    g->active_civ = 0;
    check_victory(g);

    /* auto-select next unit with moves */
    g->sel_unit = -1;
    game_select_next_unit(g);
    if (g->game_over == 0)
        game_set_msg(g, "Turn %d — %s  (Space=End Turn)",
                     g->turn + 1,
                     g->year < 0 ? "BC" : "AD");
    /* fix year message properly */
    {
        char ybuf[32];
        if (g->year < 0)
            snprintf(ybuf, sizeof(ybuf), "%d BC", -g->year);
        else
            snprintf(ybuf, sizeof(ybuf), "%d AD", g->year);
        if (g->game_over == 0)
            game_set_msg(g, "%s — your turn. Gold %d",
                         ybuf, g->civs[0].gold);
    }
    g->dirty = 1;
}

/* ---- click ---- */

void game_click(Game *g, int tile_x, int tile_y, int button) {
    int ui, ci;
    tile_x = map_wrap_x(tile_x);
    tile_y = map_clamp_y(tile_y);
    if (tile_y < 0 || tile_y >= MAP_H) return;

    ui = game_unit_at(g, tile_x, tile_y);
    ci = game_city_at(g, tile_x, tile_y);

    if (button == 0) { /* LMB select / move */
        if (ui >= 0 && g->units[ui].civ == 0) {
            game_select_unit(g, ui);
            game_set_msg(g, "Selected %s", unit_kind_name(g->units[ui].kind));
            return;
        }
        if (ci >= 0 && g->cities[ci].civ == 0) {
            g->sel_city = ci;
            g->sel_unit = -1;
            game_center_on(g, tile_x, tile_y);
            game_set_msg(g, "City %s pop %d — [ / ] production",
                         g->cities[ci].name, g->cities[ci].pop);
            return;
        }
        /* move selected toward tile (stepwise one cell) */
        if (g->sel_unit >= 0 && g->units[g->sel_unit].used &&
            g->units[g->sel_unit].civ == 0) {
            Unit *u = &g->units[g->sel_unit];
            int dx = tile_x - u->x;
            int dy = tile_y - u->y;
            /* shortest wrap on X */
            if (dx > MAP_W / 2) dx -= MAP_W;
            if (dx < -MAP_W / 2) dx += MAP_W;
            if (dx != 0) dx = (dx > 0) ? 1 : -1;
            if (dy != 0) dy = (dy > 0) ? 1 : -1;
            game_try_move(g, dx, dy);
            return;
        }
        if (ui >= 0) {
            game_set_msg(g, "Enemy %s (civ %s)",
                         unit_kind_name(g->units[ui].kind),
                         g->civs[g->units[ui].civ].name);
        }
    }
    g->dirty = 1;
}

/* ---- init ---- */

static void setup_civs(Game *g) {
    static const char *names[MAX_CIVS] = { "Rome", "Egypt", "Aztecs", "Babylon" };
    static const float colors[MAX_CIVS][3] = {
        {0.25f, 0.45f, 0.95f},
        {0.90f, 0.75f, 0.20f},
        {0.85f, 0.20f, 0.20f},
        {0.30f, 0.80f, 0.40f}
    };
    int i;
    g->n_civs = MAX_CIVS;
    for (i = 0; i < MAX_CIVS; i++) {
        Civ *c = &g->civs[i];
        memset(c, 0, sizeof(*c));
        c->used = 1;
        c->is_player = (i == 0);
        c->alive = 1;
        c->gold = 50;
        c->science = 0;
        c->color[0] = colors[i][0];
        c->color[1] = colors[i][1];
        c->color[2] = colors[i][2];
        snprintf(c->name, sizeof(c->name), "%s", names[i]);
    }
}

void game_init(Game *g, int seed) {
    int c, x, y, tries;
    memset(g, 0, sizeof(*g));
    g->sel_unit = -1;
    g->sel_city = -1;
    g->year = -4000;
    g->turn = 0;
    g->active_civ = 0;
    g->win_w = 960;
    g->win_h = 640;
    g->hover_x = g->hover_y = -1;

    if (seed == 0) seed = (int)time(NULL) ^ 0x51C15EED;
    srand((unsigned)seed);
    map_generate(g, seed);
    setup_civs(g);

    /* place each civ with settler + warrior */
    for (c = 0; c < g->n_civs; c++) {
        int ok = 0;
        int prefer_x = (MAP_W * (c * 2 + 1)) / (g->n_civs * 2);
        int prefer_y = MAP_H / 2 + (c % 2 ? 4 : -4);
        for (tries = 0; tries < 200 && !ok; tries++) {
            if (tries < 80)
                ok = find_land_spot(g, prefer_x + (rand() % 9) - 4,
                                    prefer_y + (rand() % 7) - 3, &x, &y);
            else
                ok = find_land_spot(g, rand() % MAP_W, rand() % MAP_H, &x, &y);
            /* keep civs spaced */
            if (ok) {
                int o;
                for (o = 0; o < MAX_UNITS; o++) {
                    if (!g->units[o].used) continue;
                    {
                        int dx = abs(g->units[o].x - x);
                        int dy = abs(g->units[o].y - y);
                        if (dx > MAP_W / 2) dx = MAP_W - dx;
                        if (dx < 6 && dy < 5) { ok = 0; break; }
                    }
                }
            }
        }
        if (!ok) {
            /* emergency: any land */
            find_land_spot(g, prefer_x, prefer_y, &x, &y);
        }
        spawn_unit(g, c, U_SETTLER, x, y);
        {
            int wx = x, wy = y;
            int d, found = 0;
            static const int odx[8] = {1,-1,0,0,1,1,-1,-1};
            static const int ody[8] = {0,0,1,-1,1,-1,1,-1};
            for (d = 0; d < 8; d++) {
                int tx = map_wrap_x(x + odx[d]);
                int ty = y + ody[d];
                if (ty >= 0 && ty < MAP_H && map_passable(g, tx, ty) &&
                    game_unit_at(g, tx, ty) < 0) {
                    wx = tx; wy = ty; found = 1; break;
                }
            }
            if (found)
                spawn_unit(g, c, U_WARRIOR, wx, wy);
        }
        if (c == 0) {
            int d;
            static const int odx[8] = {0,1,-1,0,0,1,-1,1};
            static const int ody[8] = {-1,0,0,1,-1,-1,1,1};
            for (d = 0; d < 8; d++) {
                int tx = map_wrap_x(x + odx[d]);
                int ty = y + ody[d];
                if (ty >= 0 && ty < MAP_H && map_passable(g, tx, ty) &&
                    game_unit_at(g, tx, ty) < 0) {
                    spawn_unit(g, 0, U_SCOUT, tx, ty);
                    break;
                }
            }
            game_center_on(g, x, y);
        }
    }

    /* reveal start areas already done by spawn; player sees own */
    game_select_next_unit(g);
    game_set_msg(g, "Welcome, Emperor of %s! Found cities (B), End Turn (Space)",
                 g->civs[0].name);
    g->dirty = 1;
}
