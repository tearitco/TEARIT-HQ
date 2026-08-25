/* map.c — embark generation, tiles, items, designations */
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int map_in_bounds(int x, int y) {
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H;
}

int map_walkable(const Fort *f, int x, int y) {
    uint8_t t;
    if (!map_in_bounds(x, y)) return 0;
    t = f->tiles[y][x].terrain;
    if (t == TR_FLOOR || t == TR_TREE || t == TR_WORKSHOP) return 1;
    return 0;
}

int map_blocks_sight(const Fort *f, int x, int y) {
    uint8_t t;
    if (!map_in_bounds(x, y)) return 1;
    t = f->tiles[y][x].terrain;
    return (t == TR_SOIL || t == TR_ROCK || t == TR_WALL);
}

static unsigned rng_state;
static unsigned rnd(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7fff;
}

void map_generate(Fort *f, int seed) {
    int x, y;
    int cx = MAP_W / 2, cy = MAP_H / 2;
    memset(f->tiles, 0, sizeof(f->tiles));
    memset(f->items, 0, sizeof(f->items));
    memset(f->jobs, 0, sizeof(f->jobs));
    f->seed = seed;
    rng_state = (unsigned)seed ? (unsigned)seed : 1u;

    /* Base: rock walls with a soil band and open surface floor + trees.
     * Embark site: central valley of floor, rock rim, trees, a water pool. */
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int dx = x - cx, dy = y - cy;
            int d2 = dx * dx + dy * dy;
            Tile *t = &f->tiles[y][x];
            t->desig = DG_NONE;
            t->stock_wood = 0;
            t->stock_stone = 0;

            /* border ring = solid rock */
            if (x < 2 || y < 2 || x >= MAP_W - 2 || y >= MAP_H - 2) {
                t->terrain = TR_ROCK;
                continue;
            }

            /* water pool NE of center */
            if (dx > 6 && dx < 14 && dy > -10 && dy < -4) {
                t->terrain = TR_WATER;
                continue;
            }

            /* outer rock mass with soil pockets */
            if (d2 > 18 * 18) {
                if ((rnd() % 100) < 12)
                    t->terrain = TR_SOIL;
                else
                    t->terrain = TR_ROCK;
                continue;
            }

            /* open embark bowl */
            t->terrain = TR_FLOOR;
            /* trees on surface floor */
            if (d2 > 25 && d2 < 16 * 16 && (rnd() % 100) < 28)
                t->terrain = TR_TREE;
            /* scattered soil walls as diggable knolls */
            if (d2 > 40 && d2 < 12 * 12 && (rnd() % 100) < 8)
                t->terrain = TR_SOIL;
        }
    }

    /* carve a rock cliff band on west for mining demo */
    for (y = cy - 6; y <= cy + 6; y++) {
        for (x = 4; x <= 10; x++) {
            if (map_in_bounds(x, y))
                f->tiles[y][x].terrain = TR_ROCK;
        }
    }
    /* small soil patch south */
    for (y = cy + 8; y <= cy + 12; y++) {
        for (x = cx - 4; x <= cx + 4; x++) {
            if (map_in_bounds(x, y) && f->tiles[y][x].terrain != TR_WATER)
                f->tiles[y][x].terrain = TR_SOIL;
        }
    }

    /* ensure center clear floor for dwarves */
    for (y = cy - 2; y <= cy + 2; y++)
        for (x = cx - 2; x <= cx + 2; x++)
            f->tiles[y][x].terrain = TR_FLOOR;
}

const char *terrain_name(int t) {
    static const char *n[] = {
        "open", "soil wall", "rock wall", "floor", "water",
        "tree", "constructed wall", "carpenter workshop"
    };
    if (t < 0 || t >= TR_COUNT) return "?";
    return n[t];
}

const char *desig_name(int d) {
    static const char *n[] = {
        "none", "dig", "cut tree", "stock wood", "stock stone",
        "build wall", "build workshop"
    };
    if (d < 0 || d >= DG_COUNT) return "?";
    return n[d];
}

const char *item_name(int k) {
    static const char *n[] = { "none", "wood", "stone", "bed", "chair" };
    if (k < 0 || k >= IT_COUNT) return "?";
    return n[k];
}

void map_tile_color(const Fort *f, int x, int y, float *r, float *g, float *b) {
    uint8_t t;
    if (!map_in_bounds(x, y)) {
        *r = *g = *b = 0.05f;
        return;
    }
    t = f->tiles[y][x].terrain;
    switch (t) {
    case TR_OPEN:   *r = 0.08f; *g = 0.08f; *b = 0.10f; break;
    case TR_SOIL:   *r = 0.45f; *g = 0.32f; *b = 0.18f; break;
    case TR_ROCK:   *r = 0.42f; *g = 0.42f; *b = 0.48f; break;
    case TR_FLOOR:  *r = 0.22f; *g = 0.28f; *b = 0.18f; break;
    case TR_WATER:  *r = 0.15f; *g = 0.35f; *b = 0.65f; break;
    case TR_TREE:   *r = 0.12f; *g = 0.48f; *b = 0.18f; break;
    case TR_WALL:   *r = 0.55f; *g = 0.50f; *b = 0.40f; break;
    case TR_WORKSHOP:*r = 0.55f; *g = 0.38f; *b = 0.22f; break;
    default:        *r = 0.2f; *g = 0.2f; *b = 0.2f; break;
    }
    /* stockpile tint */
    if (f->tiles[y][x].stock_wood) {
        *r = (*r + 0.55f) * 0.5f;
        *g = (*g + 0.40f) * 0.5f;
    }
    if (f->tiles[y][x].stock_stone) {
        *g = (*g + 0.45f) * 0.5f;
        *b = (*b + 0.55f) * 0.5f;
    }
}

char map_tile_glyph(const Fort *f, int x, int y) {
    uint8_t t;
    if (!map_in_bounds(x, y)) return ' ';
    t = f->tiles[y][x].terrain;
    switch (t) {
    case TR_SOIL: return '#';
    case TR_ROCK: return 177; /* ▒-ish via extended — fallback # */
    case TR_FLOOR: return '.';
    case TR_WATER: return '~';
    case TR_TREE: return 5; /* ♣ */
    case TR_WALL: return 'O';
    case TR_WORKSHOP: return 'W';
    default: return ' ';
    }
}

int item_add(Fort *f, int kind, int x, int y, int count) {
    int i;
    if (count <= 0) return -1;
    /* stack with same kind at tile */
    for (i = 0; i < MAX_ITEMS; i++) {
        if (f->items[i].used && f->items[i].x == x && f->items[i].y == y &&
            f->items[i].kind == kind) {
            f->items[i].count += count;
            return i;
        }
    }
    for (i = 0; i < MAX_ITEMS; i++) {
        if (!f->items[i].used) {
            f->items[i].used = 1;
            f->items[i].kind = kind;
            f->items[i].x = x;
            f->items[i].y = y;
            f->items[i].count = count;
            return i;
        }
    }
    return -1;
}

int item_at(const Fort *f, int x, int y, int kind) {
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (!f->items[i].used) continue;
        if (f->items[i].x != x || f->items[i].y != y) continue;
        if (kind == IT_NONE || f->items[i].kind == kind) return i;
    }
    return -1;
}

int item_take(Fort *f, int idx, int n) {
    if (idx < 0 || idx >= MAX_ITEMS || !f->items[idx].used) return 0;
    if (n >= f->items[idx].count) {
        f->items[idx].used = 0;
        f->items[idx].count = 0;
        return 1;
    }
    f->items[idx].count -= n;
    return 1;
}

void item_count_stocks(Fort *f) {
    int i;
    f->wood_stock = 0;
    f->stone_stock = 0;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (!f->items[i].used) continue;
        if (f->items[i].kind == IT_WOOD) f->wood_stock += f->items[i].count;
        if (f->items[i].kind == IT_STONE) f->stone_stock += f->items[i].count;
    }
}

static int desig_ok(const Fort *f, int x, int y, int desig) {
    uint8_t t;
    if (!map_in_bounds(x, y)) return 0;
    t = f->tiles[y][x].terrain;
    switch (desig) {
    case DG_DIG:
        return (t == TR_SOIL || t == TR_ROCK);
    case DG_CUT:
        return (t == TR_TREE);
    case DG_STOCK_WOOD:
    case DG_STOCK_STONE:
        return (t == TR_FLOOR || t == TR_TREE || t == TR_WORKSHOP);
    case DG_BUILD_WALL:
        return (t == TR_FLOOR);
    case DG_BUILD_WORKSHOP:
        return (t == TR_FLOOR);
    default:
        return 0;
    }
}

void map_designate_rect(Fort *f, int x0, int y0, int x1, int y1, int desig) {
    int x, y, t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            if (!desig_ok(f, x, y, desig)) continue;
            if (desig == DG_STOCK_WOOD) {
                f->tiles[y][x].stock_wood = 1;
                f->tiles[y][x].desig = DG_NONE;
            } else if (desig == DG_STOCK_STONE) {
                f->tiles[y][x].stock_stone = 1;
                f->tiles[y][x].desig = DG_NONE;
            } else {
                f->tiles[y][x].desig = (uint8_t)desig;
            }
        }
    }
    f->dirty = 1;
}

void map_clear_desig_at(Fort *f, int x, int y) {
    if (!map_in_bounds(x, y)) return;
    f->tiles[y][x].desig = DG_NONE;
}
