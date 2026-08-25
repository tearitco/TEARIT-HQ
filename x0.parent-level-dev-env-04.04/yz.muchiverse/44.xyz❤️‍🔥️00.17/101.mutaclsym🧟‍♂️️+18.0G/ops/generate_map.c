/* generate_map - one verb, one binary, no shared headers.
 * Usage: generate_map.+x <map_id> <seed> [width] [height] [link_map_id]
 *                         [link_x] [link_y]
 * (defaults: width=80 height=40 link_map_id=map_02 link_x=36 link_y=13)
 *
 * A one-shot AUTHORING-time tool (not part of pal/main_loop.pal's
 * per-tick loop, same spirit as save_game.c) - deterministic given the
 * same seed, so re-running with an identical seed reproduces identical
 * output. Zero AI-API calls anywhere in this file: procedural
 * generation isn't a decision_mode node (it's not an agent making a
 * choice, it's deterministic world synthesis), but it follows the
 * GAME-AI-SPEED-DOCTRINE.txt governing philosophy identically - 100%
 * local, seeded/reproducible, nothing live. See dox/02-procgen-design.txt
 * for the full write-up of this v1 slice and what's deliberately not
 * built yet (more biome flavors, non-rectangular rooms, caves).
 *
 * Pipeline of separate passes over one shared in-memory grid, kept
 * genuinely separate (not fused into one function) specifically so
 * more passes can be added later without redesigning the earlier ones:
 *   1. biome_pass    - grass base, tree border, scattered dirt/water/
 *                       tree blobs (a stochastic patch scatter, NOT a
 *                       real Perlin/simplex implementation - honestly
 *                       right-sized for v1, not claimed to be more).
 *   2. structure_pass - 3-5 non-overlapping rectangular rooms (wall
 *                       border, floor interior, one door), each
 *                       connected to the previous room by an L-shaped
 *                       corridor. Known v1 simplification: a corridor
 *                       can carve through another room's wall if their
 *                       paths cross - acceptable for this slice, not
 *                       silently pretended otherwise.
 *   3. population_pass - scatters monsters/items on walkable tiles,
 *                       away from the entry point, reusing the exact
 *                       decision_mode convention ops/tick_monsters.c
 *                       already implements (a real mix of preset/
 *                       weighted monster instances - see that file's
 *                       header comment) and item ids read straight out
 *                       of the real items.txt/monster_types.txt
 *                       registries, not hardcoded.
 *   4. emit + link   - writes map.txt/furniture.txt/state.txt/
 *                       transitions.txt/items//monsters/ in the exact
 *                       shapes compose_frame.c/move_player.c/
 *                       tick_monsters.c already read, then wires a
 *                       bidirectional stairway back into <link_map_id>
 *                       (transitions.txt entries + '<'/'>' glyphs on
 *                       both sides - matching the existing map_start
 *                       <-> map_02 '>' / '<' convention). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
/* Compile-time buffer-size caps, not a mandated real size - same
 * convention as MAX_MAP_W/MAX_MAP_H elsewhere in this project. */
#define MAX_W 160
#define MAX_H 80
#define MAX_ROOMS 8
#define MAX_IDS 24

static char project_root[MAX_PATH] = ".";
static char grid[MAX_H][MAX_W + 1];

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* --- seeded PRNG (xorshift32) - deterministic, no external deps --- */
typedef struct { unsigned int s; } rng_t;

static void rng_seed(rng_t *r, unsigned int seed) { r->s = seed ? seed : 1; }

static unsigned int rng_next(rng_t *r) {
    unsigned int x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

/* Inclusive [lo, hi]. */
static int rng_range(rng_t *r, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rng_next(r) % (unsigned int)(hi - lo + 1));
}

/* --- registry reading (id is always the first pipe-column) --- */
static int load_ids(const char *path, char ids[][32], int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        int len = (int)(p - line);
        if (len <= 0 || len >= 32) continue;
        snprintf(ids[n], 32, "%.*s", len, line);
        n++;
    }
    fclose(f);
    return n;
}

/* Reads a monster TYPE's hp column (id|name|glyph|hp|damage) from
 * monster_types.txt - spawned instances must start at their real type
 * hp, not a made-up constant, since ops/tick_monsters.c's weighted
 * tier computes hp percentage against this same registry value as
 * "max hp" (see that file's own monster_max_hp()). */
static int monster_type_hp(const char *path, const char *id) {
    FILE *f = fopen(path, "r");
    if (!f) return 20;
    char line[MAX_LINE];
    int hp = 20;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(id) || strncmp(line, id, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *p2 = strchr(name, '|');
        if (!p2) continue;
        char *glyph = p2 + 1;
        char *p3 = strchr(glyph, '|');
        if (!p3) continue;
        hp = atoi(p3 + 1);
        break;
    }
    fclose(f);
    return hp > 0 ? hp : 1;
}

/* --- pass 1: biome --- */
static void biome_pass(int w, int h, rng_t *rng) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            grid[y][x] = (x == 0 || y == 0 || x == w - 1 || y == h - 1) ? 'T' : '"';
        }
        grid[y][w] = '\0';
    }
    int blobs = 4 + rng_range(rng, 0, 4); /* 4-8 patches */
    for (int b = 0; b < blobs; b++) {
        int pick = rng_range(rng, 0, 2);
        char flavor = (pick == 0) ? ':' : (pick == 1) ? '~' : 'T';
        int cx = rng_range(rng, 2, w - 3);
        int cy = rng_range(rng, 2, h - 3);
        int radius = rng_range(rng, 1, 3);
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy > radius * radius) continue;
                int x = cx + dx, y = cy + dy;
                if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1) continue;
                grid[y][x] = flavor;
            }
        }
        if (flavor == '~' && radius >= 2) grid[cy][cx] = 'W';
    }
}

/* --- pass 2: structure (rooms + corridors) --- */
typedef struct { int x, y, w, h; } room_t;

static int rooms_overlap(const room_t *a, const room_t *b) {
    return !(a->x + a->w + 1 < b->x || b->x + b->w + 1 < a->x ||
              a->y + a->h + 1 < b->y || b->y + b->h + 1 < a->y);
}

static void carve_room(const room_t *r) {
    for (int y = r->y; y < r->y + r->h; y++) {
        for (int x = r->x; x < r->x + r->w; x++) {
            int border = (x == r->x || x == r->x + r->w - 1 || y == r->y || y == r->y + r->h - 1);
            grid[y][x] = border ? '#' : '.';
        }
    }
    grid[r->y + r->h - 1][r->x + r->w / 2] = '+'; /* one door, bottom wall center */
}

static void carve_corridor(int x1, int y1, int x2, int y2) {
    int x = x1, y = y1;
    while (x != x2) { grid[y][x] = '.'; x += (x2 > x) ? 1 : -1; }
    while (y != y2) { grid[y][x] = '.'; y += (y2 > y) ? 1 : -1; }
    grid[y][x] = '.';
}

static int place_rooms(int w, int h, rng_t *rng, room_t *rooms) {
    int n = 0;
    int target = 3 + rng_range(rng, 0, 2); /* 3-5 rooms */
    int attempts = 0;
    while (n < target && attempts < 200) {
        attempts++;
        int rw = rng_range(rng, 5, 9);
        int rh = rng_range(rng, 4, 6);
        if (rw + 3 >= w || rh + 3 >= h) break;
        int rx = rng_range(rng, 1, w - rw - 2);
        int ry = rng_range(rng, 1, h - rh - 2);
        room_t cand = { rx, ry, rw, rh };
        int ok = 1;
        for (int i = 0; i < n; i++) {
            if (rooms_overlap(&cand, &rooms[i])) { ok = 0; break; }
        }
        if (!ok) continue;
        rooms[n] = cand;
        carve_room(&cand);
        if (n > 0) {
            int cx1 = rooms[n - 1].x + rooms[n - 1].w / 2, cy1 = rooms[n - 1].y + rooms[n - 1].h / 2;
            int cx2 = cand.x + cand.w / 2, cy2 = cand.y + cand.h / 2;
            carve_corridor(cx1, cy1, cx2, cy2);
        }
        n++;
    }
    return n;
}

/* --- pass 3: population --- */
static int walkable_glyph(char g) {
    return g == '"' || g == ':' || g == '.';
}

static void population_pass(int w, int h, rng_t *rng, int entry_x, int entry_y,
                             const char *map_dir) {
    char items_path[PATH_BUF], monsters_path[PATH_BUF];
    snprintf(items_path, sizeof(items_path), "%s/pieces/registry/items/items.txt", project_root);
    snprintf(monsters_path, sizeof(monsters_path), "%s/pieces/registry/monsters/monster_types.txt", project_root);

    char item_ids[MAX_IDS][32], monster_ids[MAX_IDS][32];
    int n_item_ids = load_ids(items_path, item_ids, MAX_IDS);
    int n_monster_ids = load_ids(monsters_path, monster_ids, MAX_IDS);
    if (n_item_ids <= 0 || n_monster_ids <= 0) return; /* nothing registered - skip population */

    int spawn_x[64], spawn_y[64];
    int n_spawned = 0;
    int n_items = 5 + rng_range(rng, 0, 3);
    int n_monsters = 3 + rng_range(rng, 0, 2);
    int total = n_items + n_monsters;

    char items_dir[PATH_BUF], monsters_dir[PATH_BUF];
    /* gcc can't prove map_dir (itself sized off project_root, a
     * runtime env var) leaves enough room for "/items"/"/monsters" -
     * same class of warning narrowly suppressed elsewhere in this
     * project (tick_monsters.c, prisc+x.c, generate_egg.c) rather than
     * widened indefinitely. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(items_dir, sizeof(items_dir), "%s/items", map_dir);
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
#pragma GCC diagnostic pop
    mkdir(items_dir, 0755);
    mkdir(monsters_dir, 0755);

    for (int i = 0; i < total; i++) {
        int x = 0, y = 0, attempts = 0, placed = 0;
        while (attempts < 100 && !placed) {
            attempts++;
            x = rng_range(rng, 1, w - 2);
            y = rng_range(rng, 1, h - 2);
            if (!walkable_glyph(grid[y][x])) continue;
            int dx = x - entry_x, dy = y - entry_y;
            if (dx * dx + dy * dy < 16) continue; /* keep clear of the entry point */
            int collide = 0;
            for (int j = 0; j < n_spawned; j++) {
                if (spawn_x[j] == x && spawn_y[j] == y) { collide = 1; break; }
            }
            if (collide) continue;
            placed = 1;
        }
        if (!placed) continue;
        spawn_x[n_spawned] = x; spawn_y[n_spawned] = y; n_spawned++;

        if (i < n_items) {
            const char *id = item_ids[rng_range(rng, 0, n_item_ids - 1)];
            char dir_path[PATH_BUF + 64], state_path[PATH_BUF + 96];
            snprintf(dir_path, sizeof(dir_path), "%s/item_gen_%02d", items_dir, i);
            mkdir(dir_path, 0755);
            snprintf(state_path, sizeof(state_path), "%s/state.txt", dir_path);
            FILE *f = fopen(state_path, "w");
            if (f) { fprintf(f, "item_id=%s\npos_x=%d\npos_y=%d\n", id, x, y); fclose(f); }
        } else {
            int mi = i - n_items;
            const char *id = monster_ids[rng_range(rng, 0, n_monster_ids - 1)];
            int hp = monster_type_hp(monsters_path, id);
            int weighted = (mi % 2) == 1; /* alternate preset/weighted, same mix as map_02's hand-authored spawns */
            char dir_path[PATH_BUF + 64], state_path[PATH_BUF + 96];
            snprintf(dir_path, sizeof(dir_path), "%s/monster_gen_%02d", monsters_dir, mi);
            mkdir(dir_path, 0755);
            snprintf(state_path, sizeof(state_path), "%s/state.txt", dir_path);
            FILE *f = fopen(state_path, "w");
            if (f) {
                fprintf(f, "monster_type=%s\npos_x=%d\npos_y=%d\nhp=%d\n", id, x, y, hp);
                if (weighted) fprintf(f, "decision_mode=1\nflee_hp_pct=60\n");
                fclose(f);
            }
        }
    }
}

/* --- pass 4: emit + bidirectional link back into the source map --- */
static void write_map_files(const char *map_dir, const char *map_id, int w, int h) {
    mkdir(map_dir, 0755);
    char path[PATH_BUF];

    /* gcc can't prove map_dir leaves enough room for these fixed
     * suffixes - same narrowly-suppressed warning class as
     * tick_monsters.c/prisc+x.c/generate_egg.c elsewhere in this
     * project. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(path, sizeof(path), "%s/map.txt", map_dir);
    FILE *f = fopen(path, "w");
    if (f) {
        for (int y = 0; y < h; y++) fprintf(f, "%s\n", grid[y]);
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/furniture.txt", map_dir);
    f = fopen(path, "w");
    if (f) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) fputc(' ', f);
            fputc('\n', f);
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/state.txt", map_dir);
    f = fopen(path, "w");
    if (f) { fprintf(f, "id=%s\nwidth=%d\nheight=%d\nturn=0\n", map_id, w, h); fclose(f); }
#pragma GCC diagnostic pop
}

/* Overwrites the single glyph at (x,y) in an existing map's map.txt -
 * used to stamp the '>' stairs-down glyph at the link point in the
 * SOURCE map, matching the existing map_start<->map_02 convention
 * (that transition sits on a real '>' / '<' pair, not a plain tile). */
static void set_glyph(const char *map_dir, int x, int y, char glyph) {
    char path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(path, sizeof(path), "%s/map.txt", map_dir);
#pragma GCC diagnostic pop
    FILE *f = fopen(path, "r");
    if (!f) return;
    char lines[MAX_H][MAX_W + 4];
    int n = 0;
    while (n < MAX_H && fgets(lines[n], sizeof(lines[n]), f)) n++;
    fclose(f);
    if (y >= 0 && y < n) {
        int len = (int)strlen(lines[y]);
        if (x >= 0 && x < len && lines[y][x] != '\n') lines[y][x] = glyph;
    }
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < n; i++) fputs(lines[i], f);
        fclose(f);
    }
}

static void append_transition(const char *map_dir, int x, int y, const char *dest_map_id, int dest_x, int dest_y) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/transitions.txt", map_dir);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%d|%d|%s|%d|%d\n", x, y, dest_map_id, dest_x, dest_y); fclose(f); }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: generate_map <map_id> <seed> [width] [height] [link_map_id] [link_x] [link_y]\n");
        return 1;
    }
    resolve_root();

    const char *map_id = argv[1];
    unsigned int seed = (unsigned int)strtoul(argv[2], NULL, 10);
    int w = (argc >= 4) ? atoi(argv[3]) : 80;
    int h = (argc >= 5) ? atoi(argv[4]) : 40;
    const char *link_map_id = (argc >= 6) ? argv[5] : "map_02";
    int link_x = (argc >= 7) ? atoi(argv[6]) : 36;
    int link_y = (argc >= 8) ? atoi(argv[7]) : 13;

    if (w < 20) w = 20;
    if (w > MAX_W - 1) w = MAX_W - 1;
    if (h < 12) h = 12;
    if (h > MAX_H - 1) h = MAX_H - 1;

    rng_t rng;
    rng_seed(&rng, seed);

    biome_pass(w, h, &rng);
    room_t rooms[MAX_ROOMS];
    place_rooms(w, h, &rng, rooms);

    /* Entry point: fixed interior corner, force-cleared so it's always
     * walkable regardless of what the earlier passes put there. */
    int entry_x = 2, entry_y = 2;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int x = entry_x + dx, y = entry_y + dy;
            if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1) continue;
            grid[y][x] = '"';
        }
    }
    grid[entry_y][entry_x] = '<';

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, map_id);
    /* write_map_files() creates map_dir itself (mkdir) - must run
     * before population_pass(), which mkdir()s items//monsters/ AS
     * SUBDIRECTORIES of map_dir. */
    write_map_files(map_dir, map_id, w, h);
    population_pass(w, h, &rng, entry_x, entry_y, map_dir);
    append_transition(map_dir, entry_x, entry_y, link_map_id, link_x, link_y);

    char link_dir[PATH_BUF];
    snprintf(link_dir, sizeof(link_dir), "%s/pieces/world_01/%s", project_root, link_map_id);
    set_glyph(link_dir, link_x, link_y, '>');
    append_transition(link_dir, link_x, link_y, map_id, entry_x, entry_y);

    printf("generated %s (%dx%d, seed=%u) - linked from %s (%d,%d) to entry (%d,%d)\n",
           map_id, w, h, seed, link_map_id, link_x, link_y, entry_x, entry_y);
    return 0;
}
