/* Procedural RMMV/MZ-style tileset: tabs A B C D R, 8x8 cells per page */
#ifndef TILESET_H
#define TILESET_H

#define TILE_PX 32
#define TS_COLS 8
#define TS_ROWS 8
#define TS_PAGES 5  /* A B C D R */

/* tile id: high nibble page 0-4, low = 0-63 index */
#define TILE_ID(page, idx) ((((page) & 7) << 6) | ((idx) & 63))
#define TILE_PAGE(id) (((id) >> 6) & 7)
#define TILE_IDX(id) ((id) & 63)

void tileset_init(void);
void tileset_set_tick(int tick); /* animation phase (~60fps counter) */

/* draw one tile at screen pixel (sx,sy); scale 1 or 2; size = TILE_PX * scale */
void tileset_draw(unsigned char id, int sx, int sy, int scale);

/* checkerboard under transparent tiles (MZ palette style) */
void tileset_draw_checker(int sx, int sy, int s);

/* draw palette grid for page at (ox, oy top); highlight selected */
void tileset_draw_palette(int page, int ox, int oy, int selected);

/* terrain passability: 1 walkable */
int tileset_walkable(unsigned char id);
const char *tileset_page_name(int page);
const char *tileset_tile_hint(unsigned char id);

#endif
