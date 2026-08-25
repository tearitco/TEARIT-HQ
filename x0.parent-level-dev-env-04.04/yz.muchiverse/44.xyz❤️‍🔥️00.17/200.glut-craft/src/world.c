/* world.c — terrain generation + save/load */
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>

static size_t world_size(void) {
    return (size_t)WORLD_W * (size_t)WORLD_H * (size_t)WORLD_D;
}

static size_t idx(int x, int y, int z) {
    return (size_t)x + (size_t)WORLD_W * ((size_t)y + (size_t)WORLD_H * (size_t)z);
}

int world_in_bounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H && z >= 0 && z < WORLD_D;
}

void world_init(World *w, int seed) {
    memset(w, 0, sizeof(*w));
    w->seed = seed;
    snprintf(w->name, sizeof(w->name), "default");
    w->blocks = (uint8_t *)calloc(world_size(), 1);
    if (!w->blocks) {
        fprintf(stderr, "world_init: out of memory\n");
        exit(1);
    }
}

void world_free(World *w) {
    free(w->blocks);
    w->blocks = NULL;
}

uint8_t world_get(const World *w, int x, int y, int z) {
    if (!world_in_bounds(x, y, z) || !w->blocks) return BLK_AIR;
    return w->blocks[idx(x, y, z)];
}

void world_set(World *w, int x, int y, int z, uint8_t id) {
    if (!world_in_bounds(x, y, z) || !w->blocks) return;
    w->blocks[idx(x, y, z)] = id;
}

int world_is_solid(const World *w, int x, int y, int z) {
    uint8_t b = world_get(w, x, y, z);
    return b != BLK_AIR && b != BLK_LEAVES; /* leaves are soft */
}

/* --- simple value noise (hash-based) --- */
static float hash2(int x, int z, int seed) {
    unsigned n = (unsigned)(x * 374761393 + z * 668265263 + seed * 1274126177);
    n = (n ^ (n >> 13)) * 1274126177u;
    n = n ^ (n >> 16);
    return (n & 0xFFFFFFu) / (float)0xFFFFFFu;
}

static float smooth_noise(float x, float z, int seed) {
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    float fx = x - (float)x0;
    float fz = z - (float)z0;
    /* smoothstep */
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uz = fz * fz * (3.0f - 2.0f * fz);
    float a = hash2(x0,     z0,     seed);
    float b = hash2(x0 + 1, z0,     seed);
    float c = hash2(x0,     z0 + 1, seed);
    float d = hash2(x0 + 1, z0 + 1, seed);
    float ab = a + (b - a) * ux;
    float cd = c + (d - c) * ux;
    return ab + (cd - ab) * uz;
}

static float fbm(float x, float z, int seed) {
    float v = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;
    int o;
    for (o = 0; o < 4; o++) {
        v += amp * smooth_noise(x * freq, z * freq, seed + o * 101);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return v / norm;
}

static void place_tree(World *w, int tx, int base_y, int tz) {
    int h = 4 + (hash2(tx, tz, w->seed + 9) > 0.5f ? 1 : 0);
    int i, dx, dy, dz;
    for (i = 0; i < h; i++) {
        if (world_in_bounds(tx, base_y + i, tz))
            world_set(w, tx, base_y + i, tz, BLK_WOOD);
    }
    /* leaf blob */
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            for (dz = -2; dz <= 2; dz++) {
                if (dx * dx + dy * dy + dz * dz > 6) continue;
                int lx = tx + dx;
                int ly = base_y + h - 1 + dy;
                int lz = tz + dz;
                if (!world_in_bounds(lx, ly, lz)) continue;
                if (world_get(w, lx, ly, lz) == BLK_AIR)
                    world_set(w, lx, ly, lz, BLK_LEAVES);
            }
        }
    }
}

void world_generate(World *w) {
    int x, y, z;
    size_t n = world_size();
    memset(w->blocks, 0, n);

    for (z = 0; z < WORLD_D; z++) {
        for (x = 0; x < WORLD_W; x++) {
            float nx = (float)x / 24.0f;
            float nz = (float)z / 24.0f;
            float hnoise = fbm(nx, nz, w->seed);
            int height = 18 + (int)(hnoise * 16.0f); /* ~18..34 */
            if (height < 4) height = 4;
            if (height >= WORLD_H - 2) height = WORLD_H - 2;

            /* beach-ish low areas */
            int is_beach = height < 22;

            for (y = 0; y < WORLD_H; y++) {
                uint8_t id = BLK_AIR;
                if (y == 0) {
                    id = BLK_STONE; /* bedrock-ish */
                } else if (y < height - 4) {
                    id = BLK_STONE;
                } else if (y < height) {
                    id = is_beach ? BLK_SAND : BLK_DIRT;
                } else if (y == height) {
                    id = is_beach ? BLK_SAND : BLK_GRASS;
                }
                world_set(w, x, y, z, id);
            }

            /* sparse trees on grass */
            if (!is_beach && height > 20 && height < WORLD_H - 8) {
                float t = hash2(x, z, w->seed + 777);
                if (t > 0.985f && (x % 5) != 0) {
                    if (world_get(w, x, height, z) == BLK_GRASS)
                        place_tree(w, x, height + 1, z);
                }
            }
        }
    }
}

int world_save(const World *w, const char *saves_root, const char *name) {
    char dir[512], path[560], meta[560];
    FILE *f;
    size_t n;

    if (!w || !w->blocks || !name || !name[0]) return -1;
    snprintf(dir, sizeof(dir), "%s/%s", saves_root, name);
    if (mkdir(saves_root, 0755) != 0 && errno != EEXIST) {
        /* parent may already exist */
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        perror("mkdir saves");
        return -1;
    }

    snprintf(path, sizeof(path), "%s/world.bin", dir);
    f = fopen(path, "wb");
    if (!f) {
        perror("fopen world.bin");
        return -1;
    }
    n = world_size();
    if (fwrite(w->blocks, 1, n, f) != n) {
        fclose(f);
        return -1;
    }
    fclose(f);

    snprintf(meta, sizeof(meta), "%s/meta.txt", dir);
    f = fopen(meta, "w");
    if (!f) return -1;
    fprintf(f, "name=%s\n", name);
    fprintf(f, "seed=%d\n", w->seed);
    fprintf(f, "w=%d\n", WORLD_W);
    fprintf(f, "h=%d\n", WORLD_H);
    fprintf(f, "d=%d\n", WORLD_D);
    fprintf(f, "format=raw_u8_xyz\n");
    fclose(f);
    return 0;
}

int world_load(World *w, const char *saves_root, const char *name) {
    char path[560], meta[560];
    FILE *f;
    size_t n, got;
    int seed = 42;

    if (!w || !name || !name[0]) return -1;
    snprintf(meta, sizeof(meta), "%s/%s/meta.txt", saves_root, name);
    f = fopen(meta, "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "seed=%d", &seed) == 1) { /* ok */ }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/%s/world.bin", saves_root, name);
    f = fopen(path, "rb");
    if (!f) {
        /* missing save is normal on first run — silent fail */
        return -1;
    }
    if (!w->blocks) {
        w->blocks = (uint8_t *)calloc(world_size(), 1);
        if (!w->blocks) { fclose(f); return -1; }
    }
    n = world_size();
    got = fread(w->blocks, 1, n, f);
    fclose(f);
    if (got != n) {
        fprintf(stderr, "world_load: short read %zu / %zu\n", got, n);
        return -1;
    }
    w->seed = seed;
    snprintf(w->name, sizeof(w->name), "%s", name);
    return 0;
}

const char *block_name(uint8_t id) {
    switch (id) {
    case BLK_AIR:    return "Air";
    case BLK_GRASS:  return "Grass";
    case BLK_DIRT:   return "Dirt";
    case BLK_STONE:  return "Stone";
    case BLK_WOOD:   return "Wood";
    case BLK_SAND:   return "Sand";
    case BLK_COBBLE: return "Cobble";
    case BLK_LEAVES: return "Leaves";
    case BLK_PLANKS: return "Planks";
    default:         return "?";
    }
}

void block_color(uint8_t id, float *r, float *g, float *b) {
    switch (id) {
    case BLK_GRASS:  *r = 0.30f; *g = 0.72f; *b = 0.28f; break;
    case BLK_DIRT:   *r = 0.55f; *g = 0.36f; *b = 0.20f; break;
    case BLK_STONE:  *r = 0.55f; *g = 0.55f; *b = 0.58f; break;
    case BLK_WOOD:   *r = 0.45f; *g = 0.30f; *b = 0.14f; break;
    case BLK_SAND:   *r = 0.90f; *g = 0.85f; *b = 0.55f; break;
    case BLK_COBBLE: *r = 0.42f; *g = 0.42f; *b = 0.45f; break;
    case BLK_LEAVES: *r = 0.20f; *g = 0.55f; *b = 0.18f; break;
    case BLK_PLANKS: *r = 0.72f; *g = 0.58f; *b = 0.32f; break;
    default:         *r = 1.0f;  *g = 0.0f;  *b = 1.0f;  break;
    }
}
