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
    case 'G': return TILE_GYM;
    case '.':
    case '@':
    case ' ':
    default:  return TILE_PATH;
    }
}

static int parse_gym_line(Map *m, const char *line) {
    GymLeader *gym = &m->gym;
    int badge_id, reward, species, level, n;

    /* format: gym <badge_id> <leader_name> <reward_item_id> <species_lv_pairs...> */
    char name[NAME_LEN], buf[256];
    if (sscanf(line, "gym %d %15s %d", &badge_id, name, &reward) < 3)
        return -1;
    memset(gym, 0, sizeof(*gym));
    gym->badge_id = badge_id;
    snprintf(gym->name, sizeof(gym->name), "%s", name);
    gym->reward_item_id = reward;
    gym->party_n = 0;

    snprintf(buf, sizeof(buf), "%s", line);
    char *p = buf;
    int tok = 0;
    while (*p && tok < 3) { if (*p == ' ') tok++; p++; }
    while (*p && gym->party_n < PARTY_MAX) {
        if (sscanf(p, "%d %d%n", &species, &level, &n) >= 2 && species > 0 && level > 0) {
            const Species *sp = mon_species(&g, species);
            if (sp) {
                PartyMon *pm = &gym->party[gym->party_n++];
                mon_init_from_species(pm, sp, level);
                pm->move_id[1] = sp->type == TYPE_GRASS ? 3
                               : sp->type == TYPE_FIRE ? 4
                               : sp->type == TYPE_WATER ? 5 : 6;
            }
            p += n;
        } else break;
    }
    m->has_gym = 1;
    return 0;
}

static int parse_trainer_line(Map *m, const char *line) {
    Trainer *t;
    char name[NAME_LEN];
    int x, y, sight, species, level, i, n = 0;

    /* format: trainer <name> <x> <y> <sight> <species_lv_pairs...> */
    if (sscanf(line, "trainer %15s %d %d %d", name, &x, &y, &sight) < 4)
        return -1;
    if (m->trainers_n >= MAX_TRAINERS) return -1;
    t = &m->trainers[m->trainers_n];
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name);
    t->x = x; t->y = y; t->sight = sight; t->defeated = 0;
    t->party_n = 0;

    /* parse species/level pairs after the first 4 tokens */
    {
        char buf[256];
        int pos = 0, tok = 0;
        snprintf(buf, sizeof(buf), "%s", line);
        /* skip first 4 whitespace-delimited tokens */
        char *p = buf;
        while (*p && tok < 4) { if (*p == ' ') tok++; p++; }
        while (*p && t->party_n < PARTY_MAX) {
            if (sscanf(p, "%d %d%n", &species, &level, &n) >= 2 && species > 0 && level > 0) {
                const Species *sp = mon_species(&g, species);
                if (sp) {
                    PartyMon *pm = &t->party[t->party_n++];
                    mon_init_from_species(pm, sp, level);
                    pm->move_id[1] = sp->type == TYPE_GRASS ? 3
                                   : sp->type == TYPE_FIRE ? 4
                                   : sp->type == TYPE_WATER ? 5 : 6;
                }
                p += n;
            } else break;
        }
    }
    m->trainers_n++;
    return 0;
}

int map_find_conn(const Map *m, int x, int y, MapConn *out) {
    int i;
    if (!m || !out) return -1;
    for (i = 0; i < m->conns_n; i++) {
        if (m->conns[i].sx == x && m->conns[i].sy == y) {
            *out = m->conns[i];
            return 0;
        }
    }
    return -1;
}

static int parse_header_line(Map *m, const char *line) {
    int id;
    char name[NAME_LEN], dest_path[PATH_LEN];
    if (sscanf(line, "id %d", &id) == 1) {
        m->id = id; return 0;
    }
    if (sscanf(line, "name %15[^\n]", name) == 1) {
        snprintf(m->name, sizeof(m->name), "%s", name); return 0;
    }
    if (strncmp(line, "gym ", 4) == 0) {
        return parse_gym_line(m, line);
    }
    /* conn <sx> <sy> <dest_path> <dx> <dy> <dfacing> */
    {
        int sx, sy, dx, dy, df;
        char dp[PATH_LEN];
        if (sscanf(line, "conn %d %d %511s %d %d %d", &sx, &sy, dp, &dx, &dy, &df) >= 5) {
            if (m->conns_n < MAX_CONNS) {
                MapConn *c = &m->conns[m->conns_n++];
                c->sx = sx; c->sy = sy;
                snprintf(c->dest_path, sizeof(c->dest_path), "%s", dp);
                c->dx = dx; c->dy = dy;
                c->dfacing = (sscanf(line + 5, "%*s %*s %*s %*s %*s %d", &df) == 1) ? df : 2;
            }
            return 0;
        }
    }
    return -1; /* not a header line */
}

int map_load(Map *m, const char *path) {
    FILE *f;
    char line[MAP_MAX_W + 64];
    int y = 0, x, w = 0;
    int found_start = 0;
    int header_done = 0;

    if (!m || !path) return -1;
    memset(m, 0, sizeof(*m));
    snprintf(m->path, sizeof(m->path), "%s", path);
    m->id = 0;
    snprintf(m->name, sizeof(m->name), "Unknown");

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "map_load: cannot open %s\n", path);
        return -1;
    }

    while (y < MAP_MAX_H && fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0) continue;

        /* header lines before the grid — detect by non-grid chars */
        if (!header_done) {
            if (line[0] == '#' || line[0] == '~' || line[0] == '.' ||
                line[0] == '@' || line[0] == 'T' || line[0] == ',' ||
                line[0] == 'P' || line[0] == 'H' || line[0] == 'G') {
                header_done = 1;
            } else if (strncmp(line, "trainer ", 8) == 0) {
                parse_trainer_line(m, line);
                continue;
            } else {
                parse_header_line(m, line);
                continue;
            }
        }

        if (strncmp(line, "trainer ", 8) == 0) {
            parse_trainer_line(m, line);
            continue;
        }

        if ((int)n > w) w = (int)n;
        for (x = 0; x < (int)n && x < MAP_MAX_W; x++) {
            if (line[x] == '@') {
                m->start_x = x;
                m->start_y = y;
                found_start = 1;
            }
            m->cells[y][x] = (unsigned char)char_to_tile(line[x]);
        }
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
