/* tick_monsters - one verb, one binary, no shared headers.
 * Runs once per hero turn (called from the pal script right after
 * end_turn): every monster piece physically nested under the hero's
 * CURRENT map's monsters/ directory takes one action - step one tile
 * toward or away from the hero (diagonal moves allowed, blocked by the
 * same terrain/furniture rules as the hero and by other monsters), or,
 * if a toward-step would land on the hero's own tile, attack instead of
 * moving (reduces hero hp by the monster's damage). Monsters on a map
 * the hero isn't currently on don't tick - same "only what's being
 * observed advances" rule the per-map turn counters already follow.
 *
 * decision_mode (per-instance state.txt field, per-instance not a
 * registry default - see GAME-AI-SPEED-DOCTRINE.txt and the family's
 * wsr-pal/corp_decide.c + muchi-evo-pal/bot_choose.c precedent):
 *   0 (preset, default when the field is absent - existing pieces with
 *      no such field keep behaving byte-identically) - always chase.
 *   1 (weighted) - reads this instance's own flee_hp_pct field (same
 *      state.txt, no separate weights file - matches wsr-pal's
 *      risk_bias precedent). Below that hp percentage, steps AWAY from
 *      the hero instead of toward. Otherwise identical to preset.
 * Deliberately no 2/3/4 (rl/llm/human) branches - a monster's
 * chase-or-flee decision is a small, fixed-shape node (doctrine §3);
 * it doesn't need GOAP/BT/rl/llm/human the way a corp or creature-
 * lineage piece does. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
/* Generous compile-time buffer-size caps, NOT the real per-map
 * dimensions - see move_player.c's identical comment / dox/
 * 01-cdda-architecture.md §5a for why. */
#define MAX_MAP_W 256
#define MAX_MAP_H 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_config_int(const char *key, int def) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/config.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void append_ledger(const char *actor, int x, int y, const char *action_type) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);
    int epoch = read_config_int("current_epoch", 1);
    int turn = read_config_int("current_turn", 0);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s|%d|%s|%d|x:%d,y:%d|%s\n", ts, epoch, actor, turn, x, y, action_type);
        fclose(f);
    }
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

static int glyph_walkable(const char *registry_rel_path, char glyph) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return glyph == '.';
    char line[MAX_LINE];
    int result = 0;
    while (fgets(line, sizeof(line), f)) {
        /* See ops/move_player.c's own copy of this function for why
         * line[1]=='|' (not just line[0]=='#') is the real comment
         * test - '#' is itself a valid glyph (t_wall). */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        result = atoi(p + 1);
        break;
    }
    fclose(f);
    return result;
}

static char file_glyph_at(const char *abs_path, int x, int y, char default_glyph, int map_w, int map_h) {
    if (x < 0 || y < 0 || x >= map_w || y >= map_h) return default_glyph;
    FILE *f = fopen(abs_path, "r");
    if (!f) return default_glyph;
    char line[MAX_MAP_W + 4];
    char glyph = default_glyph;
    for (int row = 0; row <= y; row++) {
        if (!fgets(line, sizeof(line), f)) { glyph = default_glyph; break; }
        if (row == y) glyph = (x < (int)strlen(line)) ? line[x] : default_glyph;
    }
    fclose(f);
    return glyph;
}

static int furniture_walkable(char glyph) {
    if (glyph == ' ') return 1;
    return glyph_walkable("pieces/registry/furniture/furniture_types.txt", glyph);
}

static int monster_damage(const char *monster_type) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int dmg = 1;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *p2 = strchr(name, '|');
        if (!p2) continue;
        char *glyph = p2 + 1;
        char *p3 = strchr(glyph, '|');
        if (!p3) continue;
        char *hp = p3 + 1;
        char *p4 = strchr(hp, '|');
        if (!p4) continue;
        dmg = atoi(p4 + 1);
        break;
    }
    fclose(f);
    return dmg;
}

/* Reads the TYPE's hp column from monster_types.txt - used as the
 * "max hp" reference for the weighted tier's flee-percentage calc.
 * The live instance's own hp (its state.txt field) is the "current hp"
 * side of that ratio. */
static int monster_max_hp(const char *monster_type) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int hp = 1;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
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

static void monster_name(const char *monster_type, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", monster_type);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
    }
    fclose(f);
}

/* Monster spawners (e.g. a grave/headstone room): reads the current
 * map's own spawners.txt (optional - most maps have none, a missing
 * file is a silent no-op) and tops every listed spawner back up to 4
 * living monsters tagged spawner_id=<its id> in their own state.txt,
 * same way a grave keeps producing zombies for the hero to fight
 * without a map edit every time one dies. One line per spawner:
 * spawner_id|map_id|x|y|room_x0|room_y0|room_x1|room_y1 - x/y is the
 * grave tile itself (never spawned on); room_x0/y0/x1/y1 is the room's
 * OUTER wall rectangle, so the open interior spawn area is one tile in
 * from every edge. map_id is read but not checked against the current
 * map - this file already lives inside that map's own dir, same as
 * monsters/items/hero, so the field is just future-proofing for a
 * later op that might aggregate spawners across maps, not something
 * this op needs to verify itself. */
static void run_spawners(const char *map_dir, const char *monsters_dir, int hero_x, int hero_y) {
    char spawners_path[PATH_BUF + 32];
    snprintf(spawners_path, sizeof(spawners_path), "%s/spawners.txt", map_dir);
    FILE *spf = fopen(spawners_path, "r");
    if (!spf) return;

    char sline[MAX_LINE];
    while (fgets(sline, sizeof(sline), spf)) {
        if (sline[0] == '#' || sline[0] == '\n') continue;
        sline[strcspn(sline, "\n")] = '\0';

        char *f0 = sline;
        char *bar1 = strchr(f0, '|'); if (!bar1) continue; *bar1 = '\0';
        char *f1 = bar1 + 1; /* map_id - consumed, not checked, see header comment */
        char *bar2 = strchr(f1, '|'); if (!bar2) continue; *bar2 = '\0';
        char *f2 = bar2 + 1;
        char *bar3 = strchr(f2, '|'); if (!bar3) continue; *bar3 = '\0';
        char *f3 = bar3 + 1;
        char *bar4 = strchr(f3, '|'); if (!bar4) continue; *bar4 = '\0';
        char *f4 = bar4 + 1;
        char *bar5 = strchr(f4, '|'); if (!bar5) continue; *bar5 = '\0';
        char *f5 = bar5 + 1;
        char *bar6 = strchr(f5, '|'); if (!bar6) continue; *bar6 = '\0';
        char *f6 = bar6 + 1;
        char *bar7 = strchr(f6, '|'); if (!bar7) continue; *bar7 = '\0';
        char *f7 = bar7 + 1;

        char spawner_id[64];
        /* f0 points into sline (MAX_LINE=512) so gcc can't prove it fits
         * spawner_id's 64 bytes from static sizes alone - same class of
         * warning suppressed narrowly elsewhere in this project (see
         * the state_path snprintf above in main()), not widened
         * indefinitely just to silence it. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(spawner_id, sizeof(spawner_id), "%s", f0);
#pragma GCC diagnostic pop
        int gx = atoi(f2), gy = atoi(f3);
        int rx0 = atoi(f4), ry0 = atoi(f5), rx1 = atoi(f6), ry1 = atoi(f7);

        /* Occupied cells: hero + every currently-alive monster on this
         * map, re-scanned fresh (not the move loop's pre-move snapshot
         * in main()) since monsters may have just stepped this tick. */
        int ox[128], oy[128], on = 0;
        ox[on] = hero_x; oy[on] = hero_y; on++;

        int alive = 0;
        DIR *d2 = opendir(monsters_dir);
        if (d2) {
            struct dirent *e2;
            while ((e2 = readdir(d2)) != NULL) {
                if (e2->d_name[0] == '.') continue;
                char sp2[PATH_BUF + 640];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(sp2, sizeof(sp2), "%s/%s/state.txt", monsters_dir, e2->d_name);
#pragma GCC diagnostic pop
                if (on < 128) {
                    ox[on] = read_kv_int(sp2, "pos_x", -1);
                    oy[on] = read_kv_int(sp2, "pos_y", -1);
                    on++;
                }
                char tag[64];
                read_kv_str(sp2, "spawner_id", tag, sizeof(tag), "");
                if (strcmp(tag, spawner_id) == 0) alive++;
            }
            closedir(d2);
        }

        int need = 4 - alive;
        static const char *spawn_types[2] = { "slime", "slime_pup" };
        for (int i = 0; i < need; i++) {
            int fx = -1, fy = -1;
            for (int y = ry0 + 1; y < ry1 && fx < 0; y++) {
                for (int x = rx0 + 1; x < rx1; x++) {
                    if (x == gx && y == gy) continue;
                    int occ = 0;
                    for (int k = 0; k < on; k++) if (ox[k] == x && oy[k] == y) { occ = 1; break; }
                    if (occ) continue;
                    fx = x; fy = y;
                    break;
                }
            }
            if (fx < 0) break; /* room's full this tick - top off more next tick */

            const char *mtype = spawn_types[i % 2];
            int mhp = monster_max_hp(mtype);

            char new_dir[PATH_BUF + 96];
            int suffix;
            for (suffix = 1; suffix <= 999; suffix++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(new_dir, sizeof(new_dir), "%s/%s_%02d", monsters_dir, spawner_id, suffix);
#pragma GCC diagnostic pop
                if (mkdir(new_dir, 0755) == 0) break;
            }
            if (suffix > 999) break; /* every suffix taken - shouldn't happen, don't spin */

            char new_state[PATH_BUF + 128];
            snprintf(new_state, sizeof(new_state), "%s/state.txt", new_dir);
            FILE *nf = fopen(new_state, "w");
            if (nf) {
                fprintf(nf, "monster_type=%s\n", mtype);
                fprintf(nf, "pos_x=%d\n", fx);
                fprintf(nf, "pos_y=%d\n", fy);
                fprintf(nf, "hp=%d\n", mhp);
                fprintf(nf, "spawner_id=%s\n", spawner_id);
                fclose(nf);
            }
            append_ledger(new_dir + strlen(monsters_dir) + 1, fx, fy, "monster_spawn");

            if (on < 128) { ox[on] = fx; oy[on] = fy; on++; }
        }
    }
    fclose(spf);
}

static void log_message(const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

static void hero_take_damage(const char *hero_path, int dmg) {
    int hp = read_kv_int(hero_path, "hp", 100);
    hp -= dmg;
    if (hp < 0) hp = 0;

    FILE *f = fopen(hero_path, "r");
    if (!f) return;
    char lines[32][MAX_LINE];
    int nlines = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
    fclose(f);

    f = fopen(hero_path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], "hp", 2) == 0 && lines[i][2] == '=') {
            fprintf(f, "hp=%d\n", hp);
            continue;
        }
        fputs(lines[i], f);
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    int hero_x = read_kv_int(hero_path, "pos_x", 0);
    int hero_y = read_kv_int(hero_path, "pos_y", 0);
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, map_id);
    char monsters_dir[PATH_BUF + 32];
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
    char map_path[PATH_BUF + 32], furniture_path[PATH_BUF + 32], map_state_path[PATH_BUF + 32];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
    snprintf(furniture_path, sizeof(furniture_path), "%s/furniture.txt", map_dir);
    snprintf(map_state_path, sizeof(map_state_path), "%s/state.txt", map_dir);
    int map_w = read_kv_int(map_state_path, "width", 40);
    int map_h = read_kv_int(map_state_path, "height", 16);

    DIR *d = opendir(monsters_dir);
    if (!d) return 0;

    /* First pass: read every monster's current position, so a later
     * monster in iteration order doesn't see an earlier one's ALREADY
     * updated position and get confused about occupancy - all monsters
     * act simultaneously based on where everyone was at the start of
     * this tick. */
    char names[64][320]; /* headroom for dirent's up-to-256-byte d_name */
    int mx[64], my[64];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < 64) {
        if (entry->d_name[0] == '.') continue;
        snprintf(names[count], sizeof(names[count]), "%s", entry->d_name);
        char state_path[PATH_BUF + 640];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir, entry->d_name);
        mx[count] = read_kv_int(state_path, "pos_x", 0);
        my[count] = read_kv_int(state_path, "pos_y", 0);
        count++;
    }
    closedir(d);

    for (int i = 0; i < count; i++) {
        char state_path[PATH_BUF + 640];
        /* names[i] is genuinely a short piece-directory name ("zombie_01")
         * despite being declared with 256-byte-dirent headroom, so gcc
         * can't prove state_path is big enough from static sizes alone;
         * same class of warning suppressed narrowly elsewhere in this
         * project (mutaclsym/system/prisc+x.c, generate_egg.c,
         * menu_input.c) rather than widened indefinitely. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir, names[i]);
#pragma GCC diagnostic pop
        char monster_type[64];
        read_kv_str(state_path, "monster_type", monster_type, sizeof(monster_type), "slime");
        int decision_mode = read_kv_int(state_path, "decision_mode", 0);

        int dx = (hero_x > mx[i]) - (hero_x < mx[i]);
        int dy = (hero_y > my[i]) - (hero_y < my[i]);
        if (dx == 0 && dy == 0) continue; /* already on the hero - shouldn't happen, skip */

        int fleeing = 0;
        if (decision_mode == 1) {
            int flee_hp_pct = read_kv_int(state_path, "flee_hp_pct", 50);
            int hp = read_kv_int(state_path, "hp", 1);
            int max_hp = monster_max_hp(monster_type);
            int hp_pct = hp * 100 / max_hp;
            if (hp_pct < flee_hp_pct) fleeing = 1;
        }
        if (fleeing) { dx = -dx; dy = -dy; }

        int nx = mx[i] + dx, ny = my[i] + dy;

        if (nx == hero_x && ny == hero_y) {
            int dmg = monster_damage(monster_type);
            char name[64], msg[128];
            monster_name(monster_type, name, sizeof(name));
            snprintf(msg, sizeof(msg), "%s hits you for %d!", name, dmg);
            hero_take_damage(hero_path, dmg);
            log_message(msg);
            continue;
        }

        char terrain_glyph = file_glyph_at(map_path, nx, ny, '#', map_w, map_h);
        char furniture_glyph = file_glyph_at(furniture_path, nx, ny, ' ', map_w, map_h);
        if (!glyph_walkable("pieces/registry/terrain/terrain_types.txt", terrain_glyph) ||
            !furniture_walkable(furniture_glyph)) continue;

        int occupied = 0;
        for (int j = 0; j < count; j++) {
            if (j != i && mx[j] == nx && my[j] == ny) { occupied = 1; break; }
        }
        if (occupied) continue;

        FILE *sf = fopen(state_path, "r");
        char lines[16][MAX_LINE];
        int nlines = 0;
        if (sf) { while (nlines < 16 && fgets(lines[nlines], MAX_LINE, sf)) nlines++; fclose(sf); }

        sf = fopen(state_path, "w");
        if (sf) {
            for (int k = 0; k < nlines; k++) {
                if (strncmp(lines[k], "pos_x", 5) == 0 && lines[k][5] == '=') { fprintf(sf, "pos_x=%d\n", nx); continue; }
                if (strncmp(lines[k], "pos_y", 5) == 0 && lines[k][5] == '=') { fprintf(sf, "pos_y=%d\n", ny); continue; }
                fputs(lines[k], sf);
            }
            fclose(sf);
        }
        append_ledger(names[i], nx, ny, "monster_move");
    }

    run_spawners(map_dir, monsters_dir, hero_x, hero_y);

    return 0;
}
