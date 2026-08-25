/* world.h — voxel grid, gen, get/set, save/load */
#ifndef GLUT_CRAFT_WORLD_H
#define GLUT_CRAFT_WORLD_H

#include <stdint.h>
#include <stddef.h>

#define WORLD_W 128
#define WORLD_H 64
#define WORLD_D 128

/* Block IDs */
#define BLK_AIR     0
#define BLK_GRASS   1
#define BLK_DIRT    2
#define BLK_STONE   3
#define BLK_WOOD    4
#define BLK_SAND    5
#define BLK_COBBLE  6
#define BLK_LEAVES  7
#define BLK_PLANKS  8
#define BLK_COUNT   9

typedef struct {
    uint8_t *blocks; /* WORLD_W * WORLD_H * WORLD_D */
    int seed;
    char name[64];
} World;

void world_init(World *w, int seed);
void world_free(World *w);
void world_generate(World *w);

int  world_in_bounds(int x, int y, int z);
uint8_t world_get(const World *w, int x, int y, int z);
void    world_set(World *w, int x, int y, int z, uint8_t id);
int     world_is_solid(const World *w, int x, int y, int z);

/* Save/load under saves/<name>/world.bin + meta.txt */
int world_save(const World *w, const char *saves_root, const char *name);
int world_load(World *w, const char *saves_root, const char *name);

/* Block display helpers */
const char *block_name(uint8_t id);
void block_color(uint8_t id, float *r, float *g, float *b);

#endif
