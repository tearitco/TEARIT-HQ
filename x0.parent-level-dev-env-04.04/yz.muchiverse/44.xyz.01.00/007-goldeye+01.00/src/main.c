/* 007-goldeye-clysim — voxel GoldenEye split DM
 * Controls: ARROWS fwd/back/strafe, A/D turn, J jump (no mouse).
 * Seeded: outdoors + multistory buildings (stairs/windows/roofs), tank + heli.
 */
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIN_W 1280
#define WIN_H 720
#define MAP_W 100
#define MAP_H 28
#define MAP_D 100
#define MAX_P 4
#define MAX_GUNS 40
#define MAX_BUL 128
#define MAX_VEH 8
#define EYE 1.55f
#define WATER_LEVEL 3

enum { ST_MENU = 0, ST_PLAY = 1, ST_END = 2, ST_PAUSE = 3 };
enum {
    BLK_AIR = 0, BLK_WALL, BLK_FLOOR, BLK_GRASS, BLK_CRATE,
    BLK_STAIR, BLK_WINDOW, BLK_ROOF, BLK_BEDROCK,
    BLK_WATER, BLK_SAND, BLK_SNOW, BLK_STONE
};
enum { VEH_TANK = 0, VEH_HELI = 1 };

typedef struct {
    int alive;
    float x, feet_y, z; /* feet_y = floor of feet */
    float yaw, pitch;
    float vy;           /* jump / gravity */
    int on_ground;
    float hp;
    int kills, deaths;
    int weapon, ammo;
    float fire_cd;
    int is_ai;
    float ai_t;
    int ai_target;
    int veh; /* -1 none, else vehicle index */
    float r, g, b;
    char name[16];
} Player;

typedef struct {
    int used;
    float x, y, z;
    int kind;
} GunPick;

typedef struct {
    int alive;
    int owner;
    float x, y, z;
    float vx, vy, vz;
    float life, dmg;
} Bullet;

typedef struct {
    int used;
    int kind; /* VEH_TANK / VEH_HELI */
    float x, y, z, yaw;
    int driver; /* -1 free */
    float r, g, b;
} Vehicle;

static int g_st = ST_MENU;
static int g_nplayers = 4;
static unsigned g_seed = 1337;
static char g_seed_str[32] = "1337";
static int g_menu_sel = 2;
static unsigned char g_map[MAP_H][MAP_D][MAP_W];
static Player g_pl[MAX_P];
static GunPick g_guns[MAX_GUNS];
static Bullet g_bul[MAX_BUL];
static Vehicle g_veh[MAX_VEH];
static int g_tick = 0;
static int g_keys[512];
static int g_special[512];
static char g_status[160] = "Menu: arrows | In-game: arrows move/strafe, A/D turn, J jump";
static int g_winner = -1;
static int g_frag_limit = 10;
static int g_win_w = WIN_W, g_win_h = WIN_H;
static int g_spawn_x[16], g_spawn_z[16], g_n_spawns;
static int g_cam_mode = 0; /* 0=first, 1=third */
static int g_last_ms = 0;

/* ---- rng ---- */
static unsigned g_rng;
static void rng_seed(unsigned s) { g_rng = s ? s : 1u; }
static unsigned rng_u(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng; }
static float rng_f(void) { return (rng_u() & 0xffff) / 65535.f; }
static int rng_i(int n) { return n <= 0 ? 0 : (int)(rng_u() % (unsigned)n); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float len3(float x, float y, float z) { return sqrtf(x * x + y * y + z * z); }
static float eye_y(const Player *p) { return p->feet_y + EYE; }

static int in_map(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < MAP_W && y < MAP_H && z < MAP_D;
}
static unsigned char map_get(int x, int y, int z) {
    if (!in_map(x, y, z)) return BLK_WALL;
    return g_map[y][z][x];
}
static void map_set(int x, int y, int z, unsigned char b) {
    if (in_map(x, y, z)) g_map[y][z][x] = b;
}
static int is_solid(unsigned char b) {
    return b == BLK_WALL || b == BLK_CRATE || b == BLK_ROOF || b == BLK_BEDROCK || b == BLK_WINDOW || b == BLK_WATER || b == BLK_STONE;
}
static int solid_at(int x, int y, int z) { return is_solid(map_get(x, y, z)); }

static int is_walk_floor(unsigned char b) {
    return b == BLK_FLOOR || b == BLK_GRASS || b == BLK_STAIR || b == BLK_ROOF || b == BLK_SAND || b == BLK_SNOW;
}
/* y of topmost non-AIR block at (x,z), or -1 if none */
static int surface_y(int x, int z) {
    int y;
    if (!in_map(x, 1, z)) return -1;
    for (y = MAP_H - 1; y >= 0; y--)
        if (g_map[y][z][x] != BLK_AIR) return y;
    return -1;
}

/* Support height under feet: highest solid/floor surface y where standing */
static float ground_height(float x, float z) {
    int ix = (int)floorf(x), iz = (int)floorf(z);
    int y;
    for (y = MAP_H - 2; y >= 0; y--) {
        unsigned char b = map_get(ix, y, iz);
        if (b != BLK_AIR) return (float)y + 1.f;
    }
    return 1.f;
}

/* ---- terrain noise ---- */
static unsigned pos_hash(int x, int z) {
    unsigned h = (unsigned)(x * 374761393u + z * 668265263u);
    h = (h ^ g_seed) * 1274126177u;
    return h ^ (h >> 16);
}
static float smooth_noise(float x, float z, float scale) {
    float sx = x * scale, sz = z * scale;
    int ix = (int)floorf(sx), iz = (int)floorf(sz);
    float fx = sx - ix, fz = sz - iz;
    fx = fx * fx * (3.f - 2.f * fx);
    fz = fz * fz * (3.f - 2.f * fz);
    unsigned h00 = pos_hash(ix, iz), h10 = pos_hash(ix + 1, iz);
    unsigned h01 = pos_hash(ix, iz + 1), h11 = pos_hash(ix + 1, iz + 1);
    float v00 = (h00 & 0xffff) / 65535.f, v10 = (h10 & 0xffff) / 65535.f;
    float v01 = (h01 & 0xffff) / 65535.f, v11 = (h11 & 0xffff) / 65535.f;
    float v0 = v00 + (v10 - v00) * fx;
    float v1 = v01 + (v11 - v01) * fx;
    return v0 + (v1 - v0) * fz;
}
static float terrain_height_at(int x, int z) {
    float h = 0;
    h += smooth_noise((float)x, (float)z, 0.018f) * 8.f;
    h += smooth_noise((float)x, (float)z, 0.05f) * 3.f;
    h += smooth_noise((float)x, (float)z, 0.12f) * 1.2f;
    return h;
}
static int biome_at(int x, int z) {
    float b = smooth_noise((float)x, (float)z, 0.006f);
    if (b < 0.25f) return 0; /* plains */
    if (b < 0.50f) return 1; /* desert */
    if (b < 0.75f) return 2; /* snowy */
    return 3; /* forest */
}

/* ---- level: outdoor island terrain with biomes ---- */
static void gen_terrain(void) {
    int x, y, z;
    for (y = 0; y < MAP_H; y++)
        for (z = 0; z < MAP_D; z++)
            for (x = 0; x < MAP_W; x++)
                g_map[y][z][x] = BLK_AIR;
    /* bedrock */
    for (z = 0; z < MAP_D; z++)
        for (x = 0; x < MAP_W; x++)
            g_map[0][z][x] = BLK_BEDROCK;
    /* terrain column per (x,z) */
    for (z = 0; z < MAP_D; z++) {
        for (x = 0; x < MAP_W; x++) {
            int edge = x;
            if (MAP_W - 1 - x < edge) edge = MAP_W - 1 - x;
            if (z < edge) edge = z;
            if (MAP_D - 1 - z < edge) edge = MAP_D - 1 - z;
            if (edge < 4) {
                for (y = 1; y <= WATER_LEVEL; y++)
                    g_map[y][z][x] = BLK_WATER;
                continue;
            }
            int biome = biome_at(x, z);
            float h = terrain_height_at(x, z);
            int height = 2 + (int)(h + 0.5f);
            if (height < 2) height = 2;
            if (height >= MAP_H - 2) height = MAP_H - 3;
            unsigned char surf;
            switch (biome) {
                case 0: surf = BLK_GRASS; break;
                case 1: surf = BLK_SAND; break;
                case 2: surf = BLK_SNOW; break;
                default: surf = BLK_GRASS; break;
            }
            /* if height is below water level, fill with water */
            if (height < WATER_LEVEL) {
                for (y = 1; y <= WATER_LEVEL; y++)
                    g_map[y][z][x] = BLK_WATER;
            } else {
                for (y = 1; y <= height; y++) {
                    if (y == height) g_map[y][z][x] = surf;
                    else if (y > height - 3) g_map[y][z][x] = (biome == 2) ? BLK_SNOW : surf;
                    else g_map[y][z][x] = BLK_STONE;
                }
            }
        }
    }
    /* trees in forest biome */
    for (z = 4; z < MAP_D - 4; z++) {
        for (x = 4; x < MAP_W - 4; x++) {
            if (biome_at(x, z) != 3) continue;
            if (pos_hash(x, z) % 25 != 0) continue;
            int y;
            for (y = MAP_H - 2; y > 0; y--)
                if (g_map[y][z][x] != BLK_AIR) break;
            if (g_map[y][z][x] == BLK_GRASS) {
                int trunk_h = 3 + (pos_hash(x + 7, z + 11) % 3);
                int i;
                for (i = 1; i <= trunk_h; i++)
                    if (y + i < MAP_H) g_map[y + i][z][x] = BLK_WALL;
                int ly = y + trunk_h;
                for (int dx = -2; dx <= 2; dx++)
                    for (int dz = -2; dz <= 2; dz++) {
                        int lx = x + dx, lz = z + dz;
                        if (in_map(lx, ly, lz) && g_map[ly][lz][lx] == BLK_AIR)
                            g_map[ly][lz][lx] = BLK_ROOF;
                    }
            }
        }
    }
}

/* Multistory building placed on terrain surface if area is flat */
static void place_building(int x0, int z0, int bw, int bd, int stories) {
    int s, x, z, y;
    if (stories < 1) stories = 1;
    if (stories > 3) stories = 3;
    if (x0 < 3) x0 = 3;
    if (z0 < 3) z0 = 3;
    if (x0 + bw >= MAP_W - 3) bw = MAP_W - 4 - x0;
    if (z0 + bd >= MAP_D - 3) bd = MAP_D - 4 - z0;
    if (bw < 6 || bd < 6) return;
    /* check terrain is flat enough */
    int base = surface_y(x0, z0);
    if (base < 2) return;
    for (x = x0; x <= x0 + bw; x++)
        for (z = z0; z <= z0 + bd; z++) {
            int sy = surface_y(x, z);
            if (sy < 2 || sy > base + 1) return;
        }
    if (base >= MAP_H - 5) return;
    /* clear site */
    for (z = z0; z <= z0 + bd; z++)
        for (x = x0; x <= x0 + bw; x++)
            for (y = base + 1; y <= base + 3 * stories + 2 && y < MAP_H; y++)
                map_set(x, y, z, BLK_AIR);

    for (s = 0; s < stories; s++) {
        int fy = base + s * 3; /* floor block y */
        int ceil_y = fy + 3;
        if (ceil_y >= MAP_H - 1) break;

        /* slab floor / clear interior */
        for (z = z0; z <= z0 + bd; z++) {
            for (x = x0; x <= x0 + bw; x++) {
                int edge = (x == x0 || x == x0 + bw || z == z0 || z == z0 + bd);
                /* clear volume */
                for (y = fy + 1; y < ceil_y; y++)
                    map_set(x, y, z, BLK_AIR);
                map_set(x, fy, z, BLK_FLOOR);
                if (s == stories - 1)
                    map_set(x, ceil_y, z, BLK_ROOF);
                else
                    map_set(x, ceil_y, z, BLK_FLOOR); /* floor of next = ceiling */

                if (edge) {
                    for (y = fy + 1; y < ceil_y; y++) {
                        int mid = (y == fy + 2);
                        int win = mid && !((x + z + s) % 3 == 0);
                        /* door on ground floor south wall */
                        if (s == 0 && z == z0 + bd && x == x0 + bw / 2 && y <= fy + 2) {
                            map_set(x, y, z, BLK_AIR);
                        } else if (win && (x == x0 || x == x0 + bw || z == z0 || z == z0 + bd)) {
                            map_set(x, y, z, BLK_WINDOW);
                        } else {
                            map_set(x, y, z, BLK_WALL);
                        }
                    }
                }
            }
        }

        /* stairs in NE corner: each step raises one y over one x */
        {
            int sx = x0 + bw - 2;
            int sz = z0 + 2;
            int step;
            for (step = 0; step < 3; step++) {
                int sy = fy + 1 + step;
                int px = sx - step;
                if (px <= x0) break;
                map_set(px, fy, sz, BLK_FLOOR);
                map_set(px, sy, sz, BLK_STAIR);
                map_set(px, sy + 1, sz, BLK_AIR);
                map_set(px, sy + 2, sz, BLK_AIR);
                /* carve wall for stairwell */
                map_set(px, fy + 1, sz, BLK_AIR);
                map_set(px, fy + 2, sz, BLK_AIR);
            }
            /* open hatch to next floor */
            map_set(sx - 2, ceil_y, sz, BLK_AIR);
            map_set(sx - 1, ceil_y, sz, BLK_AIR);
            map_set(sx - 2, ceil_y - 1, sz, BLK_AIR);
        }

        /* a few crates */
        map_set(x0 + 2, fy + 1, z0 + 2, BLK_CRATE);
        map_set(x0 + bw - 3, fy + 1, z0 + bd - 3, BLK_CRATE);
    }

    /* door path clear outdoor */
    map_set(x0 + bw / 2, base + 1, z0 + bd + 1, BLK_AIR);
    map_set(x0 + bw / 2, base + 2, z0 + bd + 1, BLK_AIR);
    map_set(x0 + bw / 2, base, z0 + bd + 1, g_map[base][z0 + bd][x0 + bw / 2]);
}

static void add_spawn(int x, int z) {
    if (g_n_spawns >= 16) return;
    int sy = surface_y(x, z);
    if (sy < 2) return;
    unsigned char top = g_map[sy][z][x];
    if (!is_walk_floor(top)) return;
    if (solid_at(x, sy + 1, z) || solid_at(x, sy + 2, z)) return;
    g_spawn_x[g_n_spawns] = x;
    g_spawn_z[g_n_spawns] = z;
    g_n_spawns++;
}

static int outdoor_open(int x, int z) {
    int sy = surface_y(x, z);
    if (sy < 2) return 0;
    unsigned char top = g_map[sy][z][x];
    if (top != BLK_GRASS && top != BLK_SAND && top != BLK_SNOW) return 0;
    if (solid_at(x, sy + 1, z)) return 0;
    if (solid_at(x, sy + 2, z)) return 0;
    if (solid_at(x, sy + 3, z)) return 0;
    return 1;
}

static void place_vehicle(int kind) {
    int i, tries;
    for (i = 0; i < MAX_VEH; i++) {
        if (g_veh[i].used) continue;
        for (tries = 0; tries < 120; tries++) {
            int x = 4 + rng_i(MAP_W - 8);
            int z = 4 + rng_i(MAP_D - 8);
            if (!outdoor_open(x, z) || !outdoor_open(x + 1, z) || !outdoor_open(x, z + 1))
                continue;
            g_veh[i].used = 1;
            g_veh[i].kind = kind;
            g_veh[i].x = x + 0.5f;
            g_veh[i].y = ground_height(x + 0.5f, z + 0.5f) + (kind == VEH_HELI ? 0.3f : 0.1f);
            g_veh[i].z = z + 0.5f;
            g_veh[i].yaw = rng_f() * 6.28f;
            g_veh[i].driver = -1;
            if (kind == VEH_TANK) {
                g_veh[i].r = 0.25f; g_veh[i].g = 0.45f; g_veh[i].b = 0.22f;
            } else {
                g_veh[i].r = 0.35f; g_veh[i].g = 0.40f; g_veh[i].b = 0.55f;
            }
            return;
        }
        return;
    }
}

static void gen_level(unsigned seed) {
    int b, i;
    rng_seed(seed);
    g_n_spawns = 0;
    memset(g_veh, 0, sizeof(g_veh));
    for (i = 0; i < MAX_VEH; i++) g_veh[i].driver = -1;

    gen_terrain();

    /* buildings scattered on flatter terrain */
    for (b = 0; b < 4; b++) {
        int bw = 8 + rng_i(5);
        int bd = 8 + rng_i(5);
        int stories = 2 + rng_i(2);
        int x0 = 4 + rng_i(MAP_W - bw - 10);
        int z0 = 4 + rng_i(MAP_D - bd - 10);
        place_building(x0, z0, bw, bd, stories);
    }

    /* outdoor spawns */
    for (i = 0; i < 60 && g_n_spawns < 12; i++) {
        int x = 6 + rng_i(MAP_W - 12);
        int z = 6 + rng_i(MAP_D - 12);
        if (outdoor_open(x, z)) add_spawn(x, z);
    }
    if (g_n_spawns < 4) {
        /* spawn in central area on terrain surface */
        int sx, sz;
        for (i = 0; i < 20; i++) {
            sx = 35 + rng_i(30); sz = 35 + rng_i(30);
            if (outdoor_open(sx, sz)) { add_spawn(sx, sz); }
        }
    }

    /* guns on terrain surface and in buildings */
    memset(g_guns, 0, sizeof(g_guns));
    for (i = 0; i < MAX_GUNS; i++) {
        int tries, x, z, y;
        for (tries = 0; tries < 80; tries++) {
            x = 4 + rng_i(MAP_W - 8);
            z = 4 + rng_i(MAP_D - 8);
            int sy = surface_y(x, z);
            if (sy < 2) continue;
            unsigned char top = g_map[sy][z][x];
            if (is_walk_floor(top) || top == BLK_FLOOR) {
                y = sy;
                if (!solid_at(x, y + 1, z)) {
                    g_guns[i].used = 1;
                    g_guns[i].x = x + 0.5f;
                    g_guns[i].y = (float)y + 1.2f;
                    g_guns[i].z = z + 0.5f;
                    g_guns[i].kind = 1 + rng_i(3);
                    break;
                }
            }
        }
    }

    place_vehicle(VEH_TANK);
    place_vehicle(VEH_TANK);
    place_vehicle(VEH_HELI);
    place_vehicle(VEH_HELI);
}

static void spawn_players(void) {
    static const float cols[MAX_P][3] = {
        {0.25f, 0.75f, 1.0f}, {1.0f, 0.35f, 0.25f},
        {0.35f, 0.95f, 0.40f}, {1.0f, 0.90f, 0.25f}
    };
    static const char *names[MAX_P] = { "YOU", "AI-Bond", "AI-Oddjob", "AI-Xenia" };
    int i;
    for (i = 0; i < MAX_P; i++) {
        Player *p = &g_pl[i];
        int sx, sz, t;
        memset(p, 0, sizeof(*p));
        p->veh = -1;
        if (i >= g_nplayers) continue;
        if (i < g_n_spawns) {
            int si = (i + 2) % g_n_spawns; /* spread apart */
            sx = g_spawn_x[si];
            sz = g_spawn_z[si];
        } else {
            sx = 35 + i * 10; sz = 35 + i * 10;
            if (sx >= MAP_W - 4) sx = MAP_W - 5;
            if (sz >= MAP_D - 4) sz = MAP_D - 5;
        }
        /* guarantee not trapped */
        for (t = 0; t < 100; t++) {
            if (outdoor_open(sx, sz)) break;
            if (g_n_spawns > 0) {
                int k = rng_i(g_n_spawns);
                sx = g_spawn_x[k]; sz = g_spawn_z[k];
            } else {
                sx = 10 + rng_i(MAP_W - 20); sz = 10 + rng_i(MAP_D - 20);
            }
        }
        int sy = surface_y(sx, sz);
        if (sy < 2) sy = 3;
        p->alive = 1;
        p->x = sx + 0.5f;
        p->z = sz + 0.5f;
        p->feet_y = (float)sy + 1.f;
        p->yaw = (float)i * 1.2f + 3.14f; /* face inward */
        p->pitch = 0.f;
        p->hp = (i == 0) ? 150.f : 100.f;
        p->is_ai = (i != 0);
        p->r = cols[i][0]; p->g = cols[i][1]; p->b = cols[i][2];
        snprintf(p->name, sizeof(p->name), "%s", names[i]);
    }
}

static void start_match(void) {
    unsigned s = (unsigned)strtoul(g_seed_str, NULL, 10);
    if (!g_seed_str[0] || s == 0) s = (unsigned)time(NULL) & 0xffffffu;
    g_seed = s;
    snprintf(g_seed_str, sizeof(g_seed_str), "%u", g_seed);
    gen_level(g_seed);
    spawn_players();
    memset(g_bul, 0, sizeof(g_bul));
    g_st = ST_PLAY;
    g_last_ms = glutGet(GLUT_ELAPSED_TIME);
    g_winner = -1;
    glutSetCursor(GLUT_CURSOR_INHERIT);
    snprintf(g_status, sizeof(g_status),
             "SEED %u | Arrows move/strafe | A/D turn | J jump | E vehicle | F fire",
             g_seed);
}

/* ---- collision ---- */
static int collide_player(float x, float feet, float z, float rad) {
    float head = feet + EYE + 0.15f;
    int x0 = (int)floorf(x - rad), x1 = (int)floorf(x + rad);
    int z0 = (int)floorf(z - rad), z1 = (int)floorf(z + rad);
    int y0 = (int)floorf(feet + 0.1f), y1 = (int)floorf(head);
    int ix, iy, iz;
    for (iy = y0; iy <= y1; iy++)
        for (iz = z0; iz <= z1; iz++)
            for (ix = x0; ix <= x1; ix++) {
                unsigned char b = map_get(ix, iy, iz);
                if (b == BLK_STAIR) continue; /* walk through stair voxels */
                if (is_solid(b)) return 1;
            }
    return 0;
}

static void snap_feet(Player *p) {
    float gh = ground_height(p->x, p->z);
    int ix = (int)floorf(p->x), iz = (int)floorf(p->z);
    int y;
    if (p->veh >= 0) return;
    /* stair assist */
    for (y = (int)floorf(p->feet_y); y <= (int)floorf(p->feet_y) + 2 && y < MAP_H; y++) {
        if (map_get(ix, y, iz) == BLK_STAIR)
            gh = (float)y + 1.f;
    }
    /* step up while walking */
    if (p->vy <= 0.f && gh > p->feet_y && gh - p->feet_y < 1.25f) {
        p->feet_y = gh;
        p->vy = 0.f;
        p->on_ground = 1;
    } else if (p->vy <= 0.f && p->feet_y <= gh + 0.05f) {
        p->feet_y = gh;
        p->vy = 0.f;
        p->on_ground = 1;
    } else {
        p->on_ground = 0;
    }
}

/* gravity + jump */
static void integrate_vertical(Player *p, float dt) {
    float gh;
    int ix, iz, hy;
    if (p->veh >= 0) return;
    p->vy -= 28.f * dt;
    if (p->vy < -22.f) p->vy = -22.f;
    p->feet_y += p->vy * dt;
    /* hit ceiling */
    ix = (int)floorf(p->x);
    iz = (int)floorf(p->z);
    hy = (int)floorf(p->feet_y + EYE + 0.2f);
    if (hy < MAP_H && solid_at(ix, hy, iz) && map_get(ix, hy, iz) != BLK_STAIR) {
        p->feet_y = (float)hy - EYE - 0.25f;
        p->vy = 0.f;
    }
    gh = ground_height(p->x, p->z);
    for (hy = (int)floorf(p->feet_y); hy <= (int)floorf(p->feet_y) + 2 && hy < MAP_H; hy++) {
        if (map_get(ix, hy, iz) == BLK_STAIR) gh = (float)hy + 1.f;
    }
    if (p->feet_y <= gh) {
        p->feet_y = gh;
        p->vy = 0.f;
        p->on_ground = 1;
    } else {
        p->on_ground = 0;
    }
}

static void try_move_player(Player *p, float dx, float dz) {
    float nx = p->x + dx, nz = p->z + dz;
    if (!collide_player(nx, p->feet_y, p->z, 0.28f)) p->x = nx;
    if (!collide_player(p->x, p->feet_y, nz, 0.28f)) p->z = nz;
    snap_feet(p);
    /* unstick if trapped */
    if (collide_player(p->x, p->feet_y, p->z, 0.25f)) {
        int t;
        for (t = 0; t < 8; t++) {
            float a = t * 0.785f;
            float tx = p->x + cosf(a) * 0.6f;
            float tz = p->z + sinf(a) * 0.6f;
            if (!collide_player(tx, p->feet_y, tz, 0.25f)) {
                p->x = tx; p->z = tz; break;
            }
        }
        snap_feet(p);
    }
}

/* ---- vehicles ---- */
static int near_vehicle(const Player *p) {
    int i;
    for (i = 0; i < MAX_VEH; i++) {
        float d;
        if (!g_veh[i].used || g_veh[i].driver >= 0) continue;
        d = len3(g_veh[i].x - p->x, g_veh[i].y - p->feet_y, g_veh[i].z - p->z);
        if (d < 2.8f) return i;
    }
    return -1;
}

static void enter_exit_vehicle(int pi) {
    Player *p = &g_pl[pi];
    if (!p->alive) return;
    if (p->veh >= 0) {
        Vehicle *v = &g_veh[p->veh];
        v->driver = -1;
        p->x = v->x + cosf(v->yaw) * 2.f;
        p->z = v->z + sinf(v->yaw) * 2.f;
        p->feet_y = ground_height(p->x, p->z);
        p->veh = -1;
        snprintf(g_status, sizeof(g_status), "%s left vehicle", p->name);
        return;
    }
    {
        int vi = near_vehicle(p);
        if (vi < 0) {
            snprintf(g_status, sizeof(g_status), "No vehicle nearby (tank/heli outdoors)");
            return;
        }
        g_veh[vi].driver = pi;
        p->veh = vi;
        p->x = g_veh[vi].x;
        p->z = g_veh[vi].z;
        p->feet_y = g_veh[vi].y;
        p->yaw = g_veh[vi].yaw;
        snprintf(g_status, sizeof(g_status), "%s entered %s",
                 p->name, g_veh[vi].kind == VEH_TANK ? "TANK" : "HELICOPTER");
    }
}

/* Vehicle: fwd/back along facing, strafe left/right, turn via a/d, heli climb via look keys */
static void drive_vehicle(int pi, float dt, int fwd, int back, int strafe_l, int strafe_r,
                          int turn_l, int turn_r, int climb, int descend) {
    Player *p = &g_pl[pi];
    Vehicle *v;
    float sp, sx, sz;
    if (p->veh < 0 || p->veh >= MAX_VEH) return;
    v = &g_veh[p->veh];
    if (!v->used) return;
    if (turn_l) v->yaw -= 1.8f * dt;
    if (turn_r) v->yaw += 1.8f * dt;
    p->yaw = v->yaw;
    sp = (v->kind == VEH_TANK) ? 7.f : 12.f;
    /* forward = +yaw dir (same as player: sin/-cos) */
    if (fwd) {
        v->x += sinf(v->yaw) * sp * dt;
        v->z += -cosf(v->yaw) * sp * dt;
    }
    if (back) {
        v->x -= sinf(v->yaw) * sp * dt;
        v->z += cosf(v->yaw) * sp * dt;
    }
    /* strafe: perpendicular */
    sx = cosf(v->yaw); sz = sinf(v->yaw);
    if (strafe_l) { v->x -= sx * sp * dt; v->z -= sz * sp * dt; }
    if (strafe_r) { v->x += sx * sp * dt; v->z += sz * sp * dt; }
    v->x = clampf(v->x, 1.5f, MAP_W - 1.5f);
    v->z = clampf(v->z, 1.5f, MAP_D - 1.5f);
    if (v->kind == VEH_TANK) {
        v->y = ground_height(v->x, v->z) + 0.2f;
        if (solid_at((int)v->x, (int)v->y + 1, (int)v->z)) {
            v->x -= sinf(v->yaw) * sp * dt;
            v->z += cosf(v->yaw) * sp * dt;
        }
    } else {
        if (climb) v->y += 6.f * dt;
        if (descend) v->y -= 6.f * dt;
        {
            float gnd = ground_height(v->x, v->z) + 0.4f;
            if (v->y < gnd) v->y = gnd;
            if (v->y > MAP_H - 2) v->y = MAP_H - 2;
        }
    }
    p->x = v->x;
    p->z = v->z;
    p->feet_y = v->y;
}

/* ---- combat ---- */
static void fire(int pi) {
    Player *p = &g_pl[pi];
    int i, shots = 1;
    float fx, fy, fz, sp, dmg, ey;
    if (!p->alive || p->fire_cd > 0.f) return;
    ey = eye_y(p);
    /* vehicle weapons */
    if (p->veh >= 0) {
        Vehicle *v = &g_veh[p->veh];
        p->fire_cd = (v->kind == VEH_TANK) ? 0.55f : 0.12f;
        dmg = (v->kind == VEH_TANK) ? 45.f : 14.f;
        sp = (v->kind == VEH_TANK) ? 40.f : 55.f;
        fx = sinf(p->yaw) * cosf(p->pitch);
        fy = -sinf(p->pitch);
        fz = -cosf(p->yaw) * cosf(p->pitch);
        for (i = 0; i < MAX_BUL; i++) {
            if (g_bul[i].alive) continue;
            g_bul[i].alive = 1;
            g_bul[i].owner = pi;
            g_bul[i].x = p->x + fx * 1.2f;
            g_bul[i].y = ey + fy * 1.2f;
            g_bul[i].z = p->z + fz * 1.2f;
            g_bul[i].vx = fx * sp; g_bul[i].vy = fy * sp; g_bul[i].vz = fz * sp;
            g_bul[i].life = 1.5f; g_bul[i].dmg = dmg;
            break;
        }
        return;
    }
    if (p->weapon == 0) {
        p->fire_cd = 0.3f;
        fx = sinf(p->yaw); fz = -cosf(p->yaw);
        for (i = 0; i < g_nplayers; i++) {
            Player *t = &g_pl[i];
            float dx, dy, dz, d, dot;
            if (i == pi || !t->alive) continue;
            dx = t->x - p->x; dy = eye_y(t) - ey; dz = t->z - p->z;
            d = len3(dx, dy, dz);
            if (d > 2.5f || d < 1e-4f) continue;
            dx /= d; dz /= d;
            dot = dx * fx + dz * fz;
            if (dot > 0.4f) {
                t->hp -= 14.f;
                if (t->hp <= 0.f) {
                    t->alive = 0; t->deaths++; p->kills++; t->hp = -1.f;
                    if (t->veh >= 0) { g_veh[t->veh].driver = -1; t->veh = -1; }
                    if (p->kills >= g_frag_limit) {
                        g_winner = pi; g_st = ST_END;
                    }
                }
            }
        }
        return;
    }
    if (p->ammo <= 0) { p->weapon = 0; return; }
    if (p->weapon == 1) { p->fire_cd = 0.18f; dmg = 20.f; sp = 55.f; p->ammo--; }
    else if (p->weapon == 2) { p->fire_cd = 0.07f; dmg = 11.f; sp = 60.f; p->ammo--; }
    else { p->fire_cd = 0.5f; dmg = 14.f; sp = 48.f; shots = 5; p->ammo--; if (p->ammo < 0) p->ammo = 0; }
    fx = sinf(p->yaw) * cosf(p->pitch);
    fy = -sinf(p->pitch);
    fz = -cosf(p->yaw) * cosf(p->pitch);
    while (shots-- > 0) {
        float jx = fx + (rng_f() - 0.5f) * (p->weapon == 3 ? 0.15f : 0.02f);
        float jy = fy + (rng_f() - 0.5f) * (p->weapon == 3 ? 0.15f : 0.02f);
        float jz = fz + (rng_f() - 0.5f) * (p->weapon == 3 ? 0.15f : 0.02f);
        float L = len3(jx, jy, jz);
        if (L > 1e-4f) { jx /= L; jy /= L; jz /= L; }
        for (i = 0; i < MAX_BUL; i++) {
            if (g_bul[i].alive) continue;
            g_bul[i].alive = 1;
            g_bul[i].owner = pi;
            g_bul[i].x = p->x + jx * 0.5f;
            g_bul[i].y = ey + jy * 0.5f;
            g_bul[i].z = p->z + jz * 0.5f;
            g_bul[i].vx = jx * sp; g_bul[i].vy = jy * sp; g_bul[i].vz = jz * sp;
            g_bul[i].life = 1.4f; g_bul[i].dmg = dmg;
            break;
        }
    }
}

static void pickup_guns(Player *p) {
    int i;
    if (p->veh >= 0) return;
    for (i = 0; i < MAX_GUNS; i++) {
        float d;
        if (!g_guns[i].used) continue;
        d = len3(g_guns[i].x - p->x, g_guns[i].y - eye_y(p), g_guns[i].z - p->z);
        if (d < 1.5f) {
            p->weapon = g_guns[i].kind;
            p->ammo = (g_guns[i].kind == 2) ? 48 : (g_guns[i].kind == 3) ? 16 : 28;
            g_guns[i].used = 0;
            snprintf(g_status, sizeof(g_status), "%s got gun %d", p->name, p->weapon);
        }
    }
}

static void respawn(int pi) {
    Player *p = &g_pl[pi];
    int sx, sz, t;
    if (p->veh >= 0) { g_veh[p->veh].driver = -1; p->veh = -1; }
    if (g_n_spawns > 0) {
        int k = rng_i(g_n_spawns);
        sx = g_spawn_x[k]; sz = g_spawn_z[k];
    } else { sx = 40; sz = 40; }
    for (t = 0; t < 60; t++) {
        if (outdoor_open(sx, sz)) break;
        if (g_n_spawns > 0) {
            int k = rng_i(g_n_spawns);
            sx = g_spawn_x[k]; sz = g_spawn_z[k];
        }
    }
    p->alive = 1;
    p->x = sx + 0.5f; p->z = sz + 0.5f;
    p->feet_y = ground_height(p->x, p->z);
    p->hp = (pi == 0) ? 150.f : 100.f;
    p->weapon = 0; p->ammo = 0; p->pitch = 0.f;
}

static void ai_step(int pi, float dt) {
    Player *p = &g_pl[pi];
    int i, best = -1;
    float best_d = 1e9f, dx, dz, dist;
    if (!p->alive || !p->is_ai) return;
    if (p->veh >= 0) {
        /* drive toward enemy */
        for (i = 0; i < g_nplayers; i++) {
            float d;
            if (i == pi || !g_pl[i].alive) continue;
            d = len3(g_pl[i].x - p->x, 0, g_pl[i].z - p->z);
            if (d < best_d) { best_d = d; best = i; }
        }
        if (best >= 0) {
            float want = atan2f(g_pl[best].x - p->x, -(g_pl[best].z - p->z));
            float dy = want - g_veh[p->veh].yaw;
            while (dy > M_PI) dy -= 2.f * (float)M_PI;
            while (dy < -M_PI) dy += 2.f * (float)M_PI;
            drive_vehicle(pi, dt, best_d > 6.f, 0, 0, 0,
                          dy < -0.1f, dy > 0.1f,
                          g_veh[p->veh].kind == VEH_HELI && g_pl[best].feet_y > p->feet_y + 1.f,
                          g_veh[p->veh].kind == VEH_HELI && g_pl[best].feet_y < p->feet_y - 1.f);
            if (best_d < 40.f) fire(pi);
        }
        return;
    }
    /* sometimes board vehicle */
    if (p->ai_t < 0.f && rng_i(40) == 0) {
        int vi = near_vehicle(p);
        if (vi >= 0) enter_exit_vehicle(pi);
        p->ai_t = 1.f;
    }
    p->ai_t -= dt;
    for (i = 0; i < g_nplayers; i++) {
        float d;
        if (i == pi || !g_pl[i].alive) continue;
        d = len3(g_pl[i].x - p->x, 0, g_pl[i].z - p->z);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) return;
    dx = g_pl[best].x - p->x; dz = g_pl[best].z - p->z;
    p->yaw = atan2f(dx, -dz);
    dist = len3(dx, 0, dz);
    if (dist > 4.f) try_move_player(p, sinf(p->yaw) * 5.5f * dt, -cosf(p->yaw) * 5.5f * dt);
    try_move_player(p, cosf(p->yaw) * sinf(g_tick * 0.05f + pi) * 2.5f * dt,
                    sinf(p->yaw) * sinf(g_tick * 0.05f + pi) * 2.5f * dt);
    pickup_guns(p);
    if (p->weapon == 0) {
        float gd = 1e9f; int gi = -1;
        for (i = 0; i < MAX_GUNS; i++) {
            float d;
            if (!g_guns[i].used) continue;
            d = len3(g_guns[i].x - p->x, 0, g_guns[i].z - p->z);
            if (d < gd) { gd = d; gi = i; }
        }
        if (gi >= 0) {
            p->yaw = atan2f(g_guns[gi].x - p->x, -(g_guns[gi].z - p->z));
            try_move_player(p, sinf(p->yaw) * 5.5f * dt, -cosf(p->yaw) * 5.5f * dt);
        }
    }
    /* aim pitch slightly for multi-level */
    if (best >= 0) {
        float dy = eye_y(&g_pl[best]) - eye_y(p);
        p->pitch = clampf(-atan2f(dy, fmaxf(dist, 0.5f)), -0.8f, 0.8f);
    }
    if (dist < 35.f) fire(pi);
}

static void update_play(float dt) {
    int i, j;
    Player *h = &g_pl[0];
    int fwd, back, strafe_l, strafe_r, turn_l, turn_r, look_up, look_dn;
    if (g_st != ST_PLAY) return;

    /* ARROWS: up/down = forward/back, left/right = strafe */
    fwd = g_special[GLUT_KEY_UP];
    back = g_special[GLUT_KEY_DOWN];
    strafe_l = g_special[GLUT_KEY_LEFT];
    strafe_r = g_special[GLUT_KEY_RIGHT];
    /* A/D turn (need facing without mouse) */
    turn_l = g_keys[(int)'d'] || g_keys[(int)'D'];
    turn_r = g_keys[(int)'a'] || g_keys[(int)'A'];
    look_up = g_special[GLUT_KEY_PAGE_UP] || g_keys[(int)'r'] || g_keys[(int)'R'];
    look_dn = g_special[GLUT_KEY_PAGE_DOWN] || g_keys[(int)'v'] || g_keys[(int)'V'];

    if (h->alive) {
        if (h->veh >= 0) {
            drive_vehicle(0, dt, fwd, back, strafe_l, strafe_r, turn_l, turn_r, look_up, look_dn);
        } else {
            float sp = (g_keys[(int)'f'] || g_keys[(int)'F']) ? 8.5f : 5.8f;
            if (turn_l) h->yaw -= 2.4f * dt;
            if (turn_r) h->yaw += 2.4f * dt;
            float fx = sinf(h->yaw), fz = -cosf(h->yaw);
            float rx = cosf(h->yaw), rz = sinf(h->yaw);
            if (look_up) h->pitch = clampf(h->pitch - 1.4f * dt, -1.3f, 1.3f);
            if (look_dn) h->pitch = clampf(h->pitch + 1.4f * dt, -1.3f, 1.3f);
            if (fwd) try_move_player(h, fx * sp * dt, fz * sp * dt);
            if (back) try_move_player(h, -fx * sp * dt, -fz * sp * dt);
            if (strafe_l) try_move_player(h, -rx * sp * dt, -rz * sp * dt);
            if (strafe_r) try_move_player(h, rx * sp * dt, rz * sp * dt);
            /* Space jump */
            if ((g_keys[(int)' '] || g_keys[32]) && h->on_ground) {
                h->vy = 9.5f;
                h->on_ground = 0;
            }
            integrate_vertical(h, dt);
            pickup_guns(h);
        }
        if (g_keys[(int)'w'] || g_keys[(int)'W'])
            fire(0);
    }

    for (i = 0; i < g_nplayers; i++) {
        if (g_pl[i].fire_cd > 0.f) g_pl[i].fire_cd -= dt;
        if (g_pl[i].is_ai) ai_step(i, dt);
        if (!g_pl[i].alive) {
            g_pl[i].hp -= dt * 40.f;
            if (g_pl[i].hp < -90.f) respawn(i);
        } else if (g_pl[i].veh < 0) {
            if (i != 0) integrate_vertical(&g_pl[i], dt);
        }
    }

    for (i = 0; i < MAX_BUL; i++) {
        Bullet *b = &g_bul[i];
        unsigned char hit;
        if (!b->alive) continue;
        b->x += b->vx * dt; b->y += b->vy * dt; b->z += b->vz * dt;
        b->life -= dt;
        hit = map_get((int)floorf(b->x), (int)floorf(b->y), (int)floorf(b->z));
        /* windows: bullets pass through */
        if (b->life <= 0.f || (is_solid(hit) && hit != BLK_WINDOW)) {
            b->alive = 0; continue;
        }
        for (j = 0; j < g_nplayers; j++) {
            float dx, dy, dz, ey;
            if (j == b->owner || !g_pl[j].alive) continue;
            ey = eye_y(&g_pl[j]);
            dx = g_pl[j].x - b->x; dy = ey - b->y; dz = g_pl[j].z - b->z;
            if (dx * dx + dy * dy + dz * dz < 0.55f * 0.55f) {
                float mul = (g_pl[j].veh >= 0 && g_veh[g_pl[j].veh].kind == VEH_TANK) ? 0.45f : 1.f;
                g_pl[j].hp -= b->dmg * mul;
                b->alive = 0;
                if (g_pl[j].hp <= 0.f) {
                    g_pl[j].alive = 0; g_pl[j].deaths++;
                    g_pl[b->owner].kills++;
                    g_pl[j].hp = -1.f;
                    if (g_pl[j].veh >= 0) {
                        g_veh[g_pl[j].veh].driver = -1;
                        g_pl[j].veh = -1;
                    }
                    if (g_pl[b->owner].kills >= g_frag_limit) {
                        g_winner = b->owner; g_st = ST_END;
                    }
                }
                break;
            }
        }
    }
}

/* ---- draw ---- */
static void text2d(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s);
}
static void text2ds(float x, float y, const char *s) {
    if (!s) return;
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}
static void begin_2d(int w, int h) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
}
static void fill_rect(float x, float y, float w, float h, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

static void draw_cube(float x0, float y0, float z0, float s, float r, float g, float b) {
    float x1 = x0 + s, y1 = y0 + s, z1 = z0 + s;
    glBegin(GL_QUADS);
    glColor3f(r, g, b);
    glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glColor3f(r * 0.45f, g * 0.45f, b * 0.45f);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0);
    glColor3f(r * 0.85f, g * 0.85f, b * 0.85f);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    glColor3f(r * 0.7f, g * 0.7f, b * 0.7f);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1);
    glColor3f(r * 0.78f, g * 0.78f, b * 0.78f);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glColor3f(r * 0.62f, g * 0.62f, b * 0.62f);
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    glEnd();
}

static void blk_rgb(unsigned char id, int x, int z, float *r, float *g, float *b) {
    int c = (x + z) & 1;
    switch (id) {
    case BLK_GRASS: *r = c ? 0.28f : 0.22f; *g = c ? 0.55f : 0.48f; *b = c ? 0.22f : 0.18f; break;
    case BLK_FLOOR: *r = c ? 0.50f : 0.42f; *g = c ? 0.48f : 0.40f; *b = c ? 0.38f : 0.32f; break;
    case BLK_WALL:  *r = 0.58f; *g = 0.55f; *b = 0.50f; break;
    case BLK_WINDOW:*r = 0.35f; *g = 0.65f; *b = 0.85f; break;
    case BLK_ROOF:  *r = 0.30f; *g = 0.50f; *b = 0.22f; break;
    case BLK_STAIR: *r = 0.70f; *g = 0.60f; *b = 0.35f; break;
    case BLK_CRATE: *r = 0.75f; *g = 0.50f; *b = 0.18f; break;
    case BLK_BEDROCK:*r = 0.15f; *g = 0.15f; *b = 0.16f; break;
    case BLK_WATER: *r = 0.15f; *g = 0.30f; *b = 0.60f; break;
    case BLK_SAND:  *r = c ? 0.85f : 0.76f; *g = c ? 0.80f : 0.70f; *b = c ? 0.60f : 0.55f; break;
    case BLK_SNOW:  *r = c ? 0.95f : 0.88f; *g = c ? 0.95f : 0.88f; *b = c ? 0.98f : 0.90f; break;
    case BLK_STONE: *r = c ? 0.48f : 0.42f; *g = c ? 0.45f : 0.40f; *b = c ? 0.42f : 0.38f; break;
    default: *r = *g = *b = 0.12f; break;
    }
}

static void draw_world(float cx, float cy, float cz) {
    int x, y, z, rad = 16;
    int x0 = (int)cx - rad, x1 = (int)cx + rad;
    int z0 = (int)cz - rad, z1 = (int)cz + rad;
    int y0 = (int)cy - 8, y1 = (int)cy + 8;
    if (x0 < 0) x0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 >= MAP_W) x1 = MAP_W - 1;
    if (z1 >= MAP_D) z1 = MAP_D - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= MAP_H) y1 = MAP_H - 1;
    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++) {
                unsigned char id = g_map[y][z][x];
                float r, g, b;
                if (id == BLK_AIR) continue;
                blk_rgb(id, x, z, &r, &g, &b);
                if (id == BLK_WINDOW) {
                    draw_cube((float)x + 0.15f, (float)y + 0.1f, (float)z + 0.15f, 0.7f, r, g, b);
                } else if (id == BLK_WATER) {
                    glDisable(GL_CULL_FACE);
                    draw_cube((float)x, (float)y, (float)z, 1.f, r * 0.5f, g * 0.6f, b);
                    glEnable(GL_CULL_FACE);
                } else {
                    draw_cube((float)x, (float)y, (float)z, 1.f, r, g, b);
                }
            }
}

static void draw_vehicles(void) {
    int i;
    for (i = 0; i < MAX_VEH; i++) {
        Vehicle *v = &g_veh[i];
        float s;
        if (!v->used) continue;
        glPushMatrix();
        glTranslatef(v->x, v->y, v->z);
        glRotatef(-v->yaw * 180.f / (float)M_PI, 0, 1, 0);
        if (v->kind == VEH_TANK) {
            s = 1.4f;
            draw_cube(-0.9f, 0.f, -1.1f, s, v->r, v->g, v->b);
            draw_cube(-0.4f, 0.9f, -0.5f, 0.9f, v->r * 0.8f, v->g * 0.8f, v->b * 0.8f);
            draw_cube(-0.1f, 1.2f, -1.6f, 0.25f, 0.2f, 0.2f, 0.2f); /* barrel */
        } else {
            draw_cube(-0.8f, 0.2f, -1.2f, 0.5f, v->r, v->g, v->b);
            draw_cube(-1.6f, 0.9f, -0.15f, 0.25f, 0.7f, 0.7f, 0.75f); /* rotor bit */
            draw_cube(0.9f, 0.5f, -0.1f, 0.2f, v->r * 0.9f, v->g * 0.9f, v->b * 0.9f);
        }
        glPopMatrix();
    }
}

static void draw_entities(int skip) {
    int i;
    for (i = 0; i < MAX_GUNS; i++) {
        float bob;
        if (!g_guns[i].used) continue;
        bob = 0.08f * sinf(g_tick * 0.12f + i);
        glPushMatrix();
        glTranslatef(g_guns[i].x, g_guns[i].y + bob, g_guns[i].z);
        /* barrel */
        glPushMatrix();
        glScalef(0.10f, 0.10f, 0.40f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, 0.30f, 0.18f, 0.10f);
        glPopMatrix();
        /* grip */
        glPushMatrix();
        glTranslatef(0.f, -0.10f, 0.20f);
        glScalef(0.14f, 0.20f, 0.14f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, 0.22f, 0.14f, 0.08f);
        glPopMatrix();
        /* muzzle tip */
        glPushMatrix();
        glTranslatef(0.f, 0.f, -0.30f);
        glScalef(0.06f, 0.06f, 0.10f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, 1.f, 0.85f, 0.10f);
        glPopMatrix();
        glPopMatrix();
    }
    draw_vehicles();
    for (i = 0; i < g_nplayers; i++) {
        Player *p = &g_pl[i];
        if (!p->alive || i == skip || p->veh >= 0) continue;
        glPushMatrix();
        glTranslatef(p->x, p->feet_y, p->z);
        /* legs */
        glPushMatrix();
        glTranslatef(-0.10f, 0.10f, 0.f);
        glScalef(0.16f, 0.24f, 0.16f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, p->r * 0.7f, p->g * 0.7f, p->b * 0.7f);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0.10f, 0.10f, 0.f);
        glScalef(0.16f, 0.24f, 0.16f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, p->r * 0.7f, p->g * 0.7f, p->b * 0.7f);
        glPopMatrix();
        /* torso */
        glPushMatrix();
        glTranslatef(0.f, 0.38f, 0.f);
        glScalef(0.40f, 0.40f, 0.24f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, p->r, p->g, p->b);
        glPopMatrix();
        /* head */
        glPushMatrix();
        glTranslatef(0.f, 0.72f, 0.f);
        glScalef(0.24f, 0.20f, 0.24f);
        draw_cube(-0.5f, -0.5f, -0.5f, 1.f, p->r * 0.85f, p->g * 0.85f, p->b * 0.85f);
        glPopMatrix();
        glPopMatrix();
    }
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.f);
    glBegin(GL_LINES);
    for (i = 0; i < MAX_BUL; i++) {
        if (!g_bul[i].alive) continue;
        glColor3f(1.f, 0.95f, 0.25f);
        glVertex3f(g_bul[i].x, g_bul[i].y, g_bul[i].z);
        glVertex3f(g_bul[i].x - g_bul[i].vx * 0.04f, g_bul[i].y - g_bul[i].vy * 0.04f,
                   g_bul[i].z - g_bul[i].vz * 0.04f);
    }
    glEnd();
    glLineWidth(1.f);
    glEnable(GL_DEPTH_TEST);
}

static void draw_minimap(int vw, int vh, int pi) {
    int x, z, i;
    float sx = (float)vw / MAP_W, sz = (float)vh / MAP_D;
    fill_rect(0, 0, (float)vw, (float)vh, 0.04f, 0.06f, 0.05f);
    for (z = 0; z < MAP_D; z++) {
        for (x = 0; x < MAP_W; x++) {
            int sy = surface_y(x, z);
            if (sy < 1) continue;
            unsigned char top = g_map[sy][z][x];
            switch (top) {
                case BLK_GRASS: fill_rect(x * sx, (MAP_D - 1 - z) * sz, sx, sz, 0.22f, 0.50f, 0.22f); break;
                case BLK_SAND:  fill_rect(x * sx, (MAP_D - 1 - z) * sz, sx, sz, 0.76f, 0.70f, 0.50f); break;
                case BLK_SNOW:  fill_rect(x * sx, (MAP_D - 1 - z) * sz, sx, sz, 0.92f, 0.92f, 0.95f); break;
                case BLK_WATER: fill_rect(x * sx, (MAP_D - 1 - z) * sz, sx, sz, 0.15f, 0.35f, 0.65f); break;
                case BLK_BEDROCK: fill_rect(x * sx, (MAP_D - 1 - z) * sz, sx, sz, 0.15f, 0.15f, 0.16f); break;
                case BLK_FLOOR:
                case BLK_WALL:
                case BLK_WINDOW:
                case BLK_ROOF: fill_rect(x * sx, (MAP_D - 1 - z) * sz, sx, sz, 0.50f, 0.48f, 0.42f); break;
                default: break;
            }
        }
    }
    for (i = 0; i < MAX_VEH; i++) {
        if (!g_veh[i].used) continue;
        fill_rect(g_veh[i].x * sx - 3, (MAP_D - g_veh[i].z) * sz - 3, 7, 7,
                  g_veh[i].r, g_veh[i].g, g_veh[i].b);
        glColor3f(1, 1, 1);
        glRasterPos2f(g_veh[i].x * sx - 2, (MAP_D - g_veh[i].z) * sz - 2);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, g_veh[i].kind == VEH_TANK ? 'T' : 'H');
    }
    for (i = 0; i < g_nplayers; i++) {
        float px, pz;
        if (!g_pl[i].alive) continue;
        px = g_pl[i].x * sx; pz = (MAP_D - g_pl[i].z) * sz;
        fill_rect(px - 2.5f, pz - 2.5f, 5, 5, g_pl[i].r, g_pl[i].g, g_pl[i].b);
        if (i == pi) {
            glColor3f(1, 1, 1);
            glBegin(GL_LINE_LOOP);
            glVertex2f(px - 4, pz - 4); glVertex2f(px + 4, pz - 4);
            glVertex2f(px + 4, pz + 4); glVertex2f(px - 4, pz + 4);
            glEnd();
        }
    }
    /* weapon pickups on minimap */
    for (i = 0; i < MAX_GUNS; i++) {
        if (!g_guns[i].used) continue;
        fill_rect(g_guns[i].x * sx - 1, (MAP_D - g_guns[i].z) * sz - 1, 3, 3, 1.f, 0.85f, 0.1f);
    }
}

static void draw_hud(int vw, int vh, int pi) {
    Player *p = &g_pl[pi];
    char buf[140];
    const char *wn[] = { "FISTS", "PISTOL", "AUTO", "SHOTGUN" };
    float hpw;
    begin_2d(vw, vh);
    fill_rect(0, 0, (float)vw, 40, 0.04f, 0.06f, 0.09f);
    fill_rect(8, 12, 120, 14, 0.25f, 0.05f, 0.05f);
    hpw = 120.f * clampf(p->hp / 100.f, 0.f, 1.f);
    fill_rect(8, 12, hpw, 14, 0.9f, 0.2f, 0.15f);
    glColor3f(1, 1, 1);
    if (p->veh >= 0)
        snprintf(buf, sizeof(buf), "%s  HP%.0f  [%s]  K%d/D%d  Arrows move  A/D turn  E exit  F fire",
                 p->name, p->hp < 0 ? 0 : p->hp,
                 g_veh[p->veh].kind == VEH_TANK ? "TANK" : "HELI", p->kills, p->deaths);
    else
        snprintf(buf, sizeof(buf), "%s  HP%.0f  %s:%d  K%d/D%d  Arrows move  A/D turn  J jump  F fire",
                 p->name, p->hp < 0 ? 0 : p->hp, wn[p->weapon & 3], p->ammo, p->kills, p->deaths);
    text2ds(10, 24, buf);
    glColor3f(1, 1, 1);
    glBegin(GL_LINES);
    glVertex2f(vw * 0.5f - 10, vh * 0.5f); glVertex2f(vw * 0.5f + 10, vh * 0.5f);
    glVertex2f(vw * 0.5f, vh * 0.5f - 10); glVertex2f(vw * 0.5f, vh * 0.5f + 10);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

static void draw_pane(int pi, int vx, int vy, int vw, int vh) {
    Player *p = &g_pl[pi];
    float ey;
    if (vw < 8 || vh < 8) return;
    ey = eye_y(p);

    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.45f, 0.68f, 0.95f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(72.0, (double)vw / (double)vh, 0.08, 140.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    if (g_cam_mode == 1) {
        float dist = 5.f, up = 2.5f;
        float cx = p->x - sinf(p->yaw) * dist;
        float cz = p->z + cosf(p->yaw) * dist;
        float cy = p->feet_y + up;
        gluLookAt(cx, cy, cz, p->x, p->feet_y + 1.2f, p->z, 0, 1, 0);
    } else {
        glRotatef(-p->pitch * 180.f / (float)M_PI, 1, 0, 0);
        glRotatef(-p->yaw * 180.f / (float)M_PI, 0, 1, 0);
        glTranslatef(-p->x, -ey, -p->z);
    }

    draw_world(p->x, ey, p->z);
    draw_entities(pi);

    {
        int mw = 112, mh = 112;
        int mx = vx + vw - mw - 10, my = vy + 46;
        glViewport(mx, my, mw, mh);
        glScissor(mx, my, mw, mh);
        begin_2d(mw, mh);
        draw_minimap(mw, mh, pi);
        glEnable(GL_DEPTH_TEST);
    }
    glViewport(vx, vy, vw, vh);
    glScissor(vx, vy, vw, vh);
    draw_hud(vw, vh, pi);

    /* Damage / death overlay */
    if (p->hp < 30.f) {
        begin_2d(vw, vh);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (p->hp <= 0.f) {
            fill_rect(0, 0, (float)vw, (float)vh, 0.5f, 0.f, 0.f);
        } else {
            fill_rect(0, 0, (float)vw, (float)vh, 0.3f, 0.f, 0.f);
        }
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    begin_2d(vw, vh);
    glColor3f(p->r, p->g, p->b);
    glLineWidth(3.f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(2, 2); glVertex2f((float)vw - 2, 2);
    glVertex2f((float)vw - 2, (float)vh - 2); glVertex2f(2, (float)vh - 2);
    glEnd();
    glLineWidth(1.f);
    glColor3f(1, 1, 1);
    text2ds(8, (float)vh - 16, p->name);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
}

static void layout_panes(void) {
    int w = g_win_w > 10 ? g_win_w : WIN_W;
    int h = g_win_h > 10 ? g_win_h : WIN_H;
    int hw = w / 2, hh = h / 2;
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.02f, 0.02f, 0.03f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (g_nplayers <= 2) {
        draw_pane(0, 0, 0, hw, h);
        if (g_nplayers > 1) draw_pane(1, hw, 0, w - hw, h);
    } else if (g_nplayers == 3) {
        draw_pane(0, 0, hh, hw, h - hh);
        draw_pane(1, hw, hh, w - hw, h - hh);
        draw_pane(2, 0, 0, w, hh);
    } else {
        draw_pane(0, 0, hh, hw, h - hh);
        draw_pane(1, hw, hh, w - hw, h - hh);
        draw_pane(2, 0, 0, hw, hh);
        draw_pane(3, hw, 0, w - hw, hh);
    }
}

static void draw_menu(void) {
    int w = g_win_w > 10 ? g_win_w : WIN_W;
    int h = g_win_h > 10 ? g_win_h : WIN_H;
    char buf[180];
    float px = w * 0.10f, py = h * 0.10f, pw = w * 0.80f, ph = h * 0.80f;

    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.06f, 0.09f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_2d(w, h);
    fill_rect(0, 0, (float)w, (float)h, 0.07f, 0.10f, 0.16f);
    fill_rect(px + 5, py - 5, pw, ph, 0.02f, 0.03f, 0.05f);
    fill_rect(px, py, pw, ph, 0.12f, 0.16f, 0.24f);
    glColor3f(1.f, 0.85f, 0.2f);
    text2d(px + 36, py + ph - 48, "GOLDENEYE CLYSIM");
    glColor3f(0.7f, 0.88f, 1.f);
    text2ds(px + 36, py + ph - 74,
            "Open island terrain, 4 biomes, water, mountains, trees  |  Tank & Helicopter");

    if (g_menu_sel == 0) fill_rect(px + 28, py + ph - 160, pw - 56, 40, 0.18f, 0.35f, 0.55f);
    else fill_rect(px + 28, py + ph - 160, pw - 56, 40, 0.10f, 0.14f, 0.20f);
    glColor3f(1, 1, 1);
    snprintf(buf, sizeof(buf), "%s PLAYERS: %d   (Left/Right)", g_menu_sel == 0 ? ">" : " ", g_nplayers);
    text2d(px + 44, py + ph - 145, buf);

    if (g_menu_sel == 1) fill_rect(px + 28, py + ph - 220, pw - 56, 40, 0.18f, 0.35f, 0.55f);
    else fill_rect(px + 28, py + ph - 220, pw - 56, 40, 0.10f, 0.14f, 0.20f);
    snprintf(buf, sizeof(buf), "%s SEED: %s_   (type digits)", g_menu_sel == 1 ? ">" : " ", g_seed_str);
    text2d(px + 44, py + ph - 205, buf);

    if (g_menu_sel == 2) fill_rect(px + 28, py + ph - 300, pw - 56, 52, 0.12f, 0.45f, 0.22f);
    else fill_rect(px + 28, py + ph - 300, pw - 56, 52, 0.10f, 0.22f, 0.14f);
    glColor3f(0.5f, 1.f, 0.55f);
    text2d(px + 44, py + ph - 278, ">>>  ENTER = START MATCH  <<<");

    glColor3f(0.8f, 0.88f, 0.95f);
    text2ds(px + 36, py + 130, "IN MATCH:");
    text2ds(px + 36, py + 108, "  ARROWS move/strafe  A/D turn  J jump  F fire");
    text2ds(px + 36, py + 88,  "  E enter/exit vehicle  Space sprint  P pause");
    text2ds(px + 36, py + 68,  "  1=1st person  2=3rd person  S reset camera");
    text2ds(px + 36, py + 48,  "  Esc menu  Ctrl+C quit  Frag limit 10  K/D shown");
    glColor3f(1.f, 0.8f, 0.35f);
    text2ds(px + 36, py + 24, g_status);
    glEnable(GL_DEPTH_TEST);
}

static void draw_pause(void) {
    int w = g_win_w > 10 ? g_win_w : WIN_W;
    int h = g_win_h > 10 ? g_win_h : WIN_H;
    float px = w * 0.10f, py = h * 0.10f, pw = w * 0.80f, ph = h * 0.80f;

    layout_panes();
    begin_2d(w, h);
    fill_rect(px + 5, py - 5, pw, ph, 0.02f, 0.03f, 0.05f);
    fill_rect(px, py, pw, ph, 0.12f, 0.16f, 0.24f);
    glColor3f(1.f, 0.85f, 0.2f);
    text2d(px + 36, py + ph - 48, "PAUSED");
    glColor3f(0.7f, 0.88f, 1.f);
    text2ds(px + 36, py + ph - 74,
            "Press P to resume  |  Esc to menu");

    glColor3f(0.8f, 0.88f, 0.95f);
    text2ds(px + 36, py + 130, "DEBUG / CHEATS (placeholder):");
    text2ds(px + 36, py + 108, "  [1] Toggle god mode");
    text2ds(px + 36, py + 88,  "  [2] Give all weapons");
    text2ds(px + 36, py + 68,  "  [3] Spawn vehicle nearby");
    text2ds(px + 36, py + 48,  "  [4] Toggle AI");
    glColor3f(1.f, 0.8f, 0.35f);
    text2ds(px + 36, py + 24, g_status);
    glEnable(GL_DEPTH_TEST);
}
    static void draw_end_overlay(void) {
    int w = g_win_w > 10 ? g_win_w : WIN_W;
    int h = g_win_h > 10 ? g_win_h : WIN_H;
    char buf[160];
    layout_panes();
    begin_2d(w, h);
    fill_rect(w * 0.15f, h * 0.40f, w * 0.70f, h * 0.20f, 0.05f, 0.08f, 0.12f);
    glColor3f(1.f, 0.9f, 0.25f);
    if (g_winner >= 0 && g_winner < g_nplayers)
        snprintf(buf, sizeof(buf), "WINNER: %s  (%d frags)  — ENTER menu",
                 g_pl[g_winner].name, g_pl[g_winner].kills);
    else
        snprintf(buf, sizeof(buf), "MATCH OVER — ENTER menu");
    text2d(w * 0.22f, h * 0.48f, buf);
    glEnable(GL_DEPTH_TEST);
}

static void display(void) {
    if (g_st == ST_MENU) draw_menu();
    else if (g_st == ST_PAUSE) draw_pause();
    else if (g_st == ST_END) draw_end_overlay();
    else layout_panes();
    glutSwapBuffers();
}

static void timer(int v) {
    (void)v;
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (float)(now - g_last_ms) / 1000.f;
    if (dt < 0.f) dt = 0.f;
    if (dt > 0.1f) dt = 0.1f;
    g_last_ms = now;
    g_tick++;
    if (g_st == ST_PLAY) update_play(dt);
    glutPostRedisplay();
    int delay = (g_st == ST_MENU || g_st == ST_PAUSE || g_st == ST_END) ? 200 : 16;
    glutTimerFunc(delay, timer, 0);
}

static void reshape(int w, int h) {
    g_win_w = w > 32 ? w : 32;
    g_win_h = h > 32 ? h : 32;
    glViewport(0, 0, g_win_w, g_win_h);
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    g_keys[(int)(unsigned char)key] = 1;
    if (g_st == ST_MENU) {
        if (key == 13 || key == 10) { start_match(); return; }
        if (g_menu_sel == 1) {
            if (key == 8 || key == 127) {
                size_t n = strlen(g_seed_str);
                if (n) g_seed_str[n - 1] = 0;
            } else if (isdigit((unsigned char)key) && strlen(g_seed_str) < 9) {
                size_t n = strlen(g_seed_str);
                g_seed_str[n] = (char)key; g_seed_str[n + 1] = 0;
            }
        }
        if (key == '2') g_nplayers = 2;
        if (key == '3') g_nplayers = 3;
        if (key == '4') g_nplayers = 4;
        return;
    }
    if (g_st == ST_END && (key == 13 || key == 10 || key == 27)) {
        g_st = ST_MENU; return;
    }
    if (key == 27) { g_st = ST_MENU; return; }
    if (key == 'p' || key == 'P') {
        if (g_st == ST_PLAY) g_st = ST_PAUSE;
        else if (g_st == ST_PAUSE) g_st = ST_PLAY;
        return;
    }
    if (key == '1') g_cam_mode = 0;
    if (key == '2') g_cam_mode = 1;
    if (key == 's' || key == 'S') {
        g_pl[0].yaw = 0.f;
        g_pl[0].pitch = 0.f;
        return;
    }
    if (g_st == ST_PLAY && (key == 'e' || key == 'E'))
        enter_exit_vehicle(0);
}

static void keyboard_up(unsigned char key, int x, int y) {
    (void)x; (void)y;
    g_keys[(int)(unsigned char)key] = 0;
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    g_special[key] = 1;
    if (g_st == ST_MENU) {
        if (key == GLUT_KEY_UP) g_menu_sel = (g_menu_sel + 2) % 3;
        if (key == GLUT_KEY_DOWN) g_menu_sel = (g_menu_sel + 1) % 3;
        if (key == GLUT_KEY_LEFT && g_menu_sel == 0) {
            g_nplayers--; if (g_nplayers < 2) g_nplayers = 4;
        }
        if (key == GLUT_KEY_RIGHT && g_menu_sel == 0) {
            g_nplayers++; if (g_nplayers > 4) g_nplayers = 2;
        }
    }
}

static void special_up(int key, int x, int y) {
    (void)x; (void)y;
    g_special[key] = 0;
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("007 GoldenEye Clysim — arrows, buildings, tank/heli");
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.06f, 0.09f, 0.14f, 1.f);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboard_up);
    glutSpecialFunc(special);
    glutSpecialUpFunc(special_up);
    /* NO mouse handlers — keyboard only */
    glutTimerFunc(16, timer, 0);
    fprintf(stderr,
            "goldeye-clysim: NO MOUSE\n"
            "  ARROWS fwd/back/strafe | A/D turn | J jump | E vehicle | F fire\n"
            "  1=1st/2=3rd cam | S reset | P pause | Ctrl+C quit\n");
    glutMainLoop();
    return 0;
}
