/* map.c — heightmap terrain, wrap-X torus, tile costs/colors */
#include "civ.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int map_wrap_x(int x) {
    x %= MAP_W;
    if (x < 0) x += MAP_W;
    return x;
}

int map_clamp_y(int y) {
    if (y < 0) return 0;
    if (y >= MAP_H) return MAP_H - 1;
    return y;
}

static float hash2(int x, int y, int seed) {
    unsigned n = (unsigned)(x * 374761393u + y * 668265263u + seed * 1442695041u);
    n = (n ^ (n >> 13)) * 1274126177u;
    return (float)(n & 0xFFFF) / 65535.0f;
}

static float smooth_noise(float x, float y, int seed) {
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    float fx = x - (float)x0;
    float fy = y - (float)y0;
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fy * fy * (3.0f - 2.0f * fy);
    float a = hash2(x0, y0, seed);
    float b = hash2(x1, y0, seed);
    float c = hash2(x0, y1, seed);
    float d = hash2(x1, y1, seed);
    float ab = a + (b - a) * u;
    float cd = c + (d - c) * u;
    return ab + (cd - ab) * v;
}

static float fbm(float x, float y, int seed) {
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    int o;
    for (o = 0; o < 4; o++) {
        sum += amp * smooth_noise(x * freq, y * freq, seed + o * 17);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
}

void map_generate(Game *g, int seed) {
    int x, y;
    g->seed = seed;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            float nx = (float)x / (float)MAP_W * 4.0f;
            float ny = (float)y / (float)MAP_H * 3.0f;
            float h = fbm(nx, ny, seed);
            /* bias center land a bit */
            float dy = fabsf((float)y - MAP_H * 0.5f) / (MAP_H * 0.5f);
            h -= dy * 0.12f;
            Tile *t = &g->tiles[y][x];
            t->explored = 0;
            if (h < 0.38f) {
                t->terrain = T_OCEAN;
                t->elev = 0;
            } else if (h < 0.52f) {
                t->terrain = T_PLAINS;
                t->elev = 1;
            } else if (h < 0.62f) {
                t->terrain = T_FOREST;
                t->elev = 1;
            } else if (h < 0.74f) {
                t->terrain = T_HILLS;
                t->elev = 2;
            } else {
                t->terrain = T_MOUNTAIN;
                t->elev = 3;
            }
            /* scatter special resources on land */
            if (t->terrain != T_OCEAN && t->terrain != T_MOUNTAIN) {
                if (hash2(x, y, seed + 99) > 0.93f)
                    t->terrain = T_SPECIAL;
            }
        }
    }
}

int map_passable(const Game *g, int x, int y) {
    x = map_wrap_x(x);
    y = map_clamp_y(y);
    if (y < 0 || y >= MAP_H) return 0;
    return g->tiles[y][x].terrain != T_OCEAN;
}

int map_move_cost(const Game *g, int x, int y) {
    uint8_t t;
    x = map_wrap_x(x);
    y = map_clamp_y(y);
    t = g->tiles[y][x].terrain;
    switch (t) {
    case T_PLAINS:
    case T_SPECIAL: return 1;
    case T_FOREST:  return 2;
    case T_HILLS:   return 2;
    case T_MOUNTAIN:return 3;
    default:        return 99;
    }
}

void map_tile_color(const Game *g, int x, int y, float *r, float *gc, float *b) {
    uint8_t t;
    x = map_wrap_x(x);
    y = map_clamp_y(y);
    t = g->tiles[y][x].terrain;
    switch (t) {
    case T_OCEAN:    *r = 0.12f; *gc = 0.28f; *b = 0.55f; break;
    case T_PLAINS:   *r = 0.42f; *gc = 0.62f; *b = 0.28f; break;
    case T_FOREST:   *r = 0.14f; *gc = 0.42f; *b = 0.18f; break;
    case T_HILLS:    *r = 0.48f; *gc = 0.44f; *b = 0.28f; break;
    case T_MOUNTAIN: *r = 0.55f; *gc = 0.55f; *b = 0.58f; break;
    case T_SPECIAL:  *r = 0.72f; *gc = 0.62f; *b = 0.18f; break;
    default:         *r = 0.3f;  *gc = 0.3f;  *b = 0.3f;  break;
    }
}

void map_reveal(Game *g, int civ, int cx, int cy, int radius) {
    int dy, dx;
    uint8_t bit;
    if (civ < 0 || civ >= MAX_CIVS) return;
    bit = (uint8_t)(1u << civ);
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int x, y;
            if (dx * dx + dy * dy > radius * radius + 1) continue;
            x = map_wrap_x(cx + dx);
            y = cy + dy;
            if (y < 0 || y >= MAP_H) continue;
            g->tiles[y][x].explored |= bit;
        }
    }
}
