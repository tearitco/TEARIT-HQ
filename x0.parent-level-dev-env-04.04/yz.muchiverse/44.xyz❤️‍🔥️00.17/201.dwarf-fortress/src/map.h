/* map.h */
#ifndef DF_MAP_H
#define DF_MAP_H

#include "fort.h"

void map_generate(Fort *f, int seed);
int  map_in_bounds(int x, int y);
int  map_walkable(const Fort *f, int x, int y);
int  map_blocks_sight(const Fort *f, int x, int y);
void map_tile_color(const Fort *f, int x, int y, float *r, float *g, float *b);
char map_tile_glyph(const Fort *f, int x, int y);
const char *terrain_name(int t);
const char *desig_name(int d);
const char *item_name(int k);

int  item_add(Fort *f, int kind, int x, int y, int count);
int  item_at(const Fort *f, int x, int y, int kind);
int  item_take(Fort *f, int idx, int n);
void item_count_stocks(Fort *f);

void map_designate_rect(Fort *f, int x0, int y0, int x1, int y1, int desig);
void map_clear_desig_at(Fort *f, int x, int y);

#endif
