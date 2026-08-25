/* map.c — load maps/pallet/map.txt, collision queries */
#include "gb.h"

static TileId char_to_tile(char c) {
    switch (c) {
    case '#': return TILE_WALL;
    case '~': return TILE_WATER;
    case ',': return TILE_GRASS;
    case 'T': return TILE_TALL;
    case 'P': return TILE_PC;
    case 'H': return TILE_HOUSE;
    case '.':
    case '@':
    case ' ':
    default:  return TILE_PATH;
    }
}

int map_load(Map *m, const char *path) {
    FILE *f;
    char line[MAP_MAX_W + 8];
    int y = 0, x, w = 0;
    int found_start = 0;

    if (!m || !path) return -1;
    memset(m, 0, sizeof(*m));
    snprintf(m->path, sizeof(m->path), "%s", path);

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "map_load: cannot open %s\n", path);
        return -1;
    }

    while (y < MAP_MAX_H && fgets(line, sizeof(line), f)) {
        /* strip CR/LF */
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n == 0) continue;
        if ((int)n > w) w = (int)n;
        for (x = 0; x < (int)n && x < MAP_MAX_W; x++) {
            if (line[x] == '@') {
                m->start_x = x;
                m->start_y = y;
                found_start = 1;
            }
            m->cells[y][x] = (unsigned char)char_to_tile(line[x]);
        }
        /* pad rest of row as wall if short (should not happen) */
        for (; x < MAP_MAX_W; x++)
            m->cells[y][x] = (unsigned char)TILE_WALL;
        y++;
    }
    fclose(f);

    m->w = w;
    m->h = y;
    if (!found_start) {
        m->start_x = m->w / 2;
        m->start_y = m->h / 2;
    }
    if (m->w < 1 || m->h < 1) return -1;
    return 0;
}

TileId map_tile(const Map *m, int x, int y) {
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h)
        return TILE_WALL;
    return (TileId)m->cells[y][x];
}

int map_walkable(const Map *m, int x, int y) {
    TileId t = map_tile(m, x, y);
    return t != TILE_WALL && t != TILE_WATER;
}
