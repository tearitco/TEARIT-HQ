/* end_turn - one verb, one binary, no shared headers.
 * Increments the turn counter on the hero's CURRENT map (read from
 * hero/state.txt's map_id field - turn counters are per-map, so a map
 * the hero isn't on doesn't advance while they're elsewhere), and ticks
 * the hero's own metabolism: hunger/thirst rise every turn, stamina
 * regenerates, and crossing the starvation/dehydration threshold costs
 * HP each turn until food/drink brings the level back down (see
 * ops/eat.c). Capped so they don't grow without bound while still
 * being clearly worse the longer they're ignored. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)

#define HUNGER_PER_TURN 1
#define THIRST_PER_TURN 2
#define STAMINA_REGEN_PER_TURN 5
#define METABOLISM_CAP 200
#define STARVING_THRESHOLD 100
#define DEHYDRATED_THRESHOLD 100
#define STARVING_HP_LOSS 1
#define DEHYDRATED_HP_LOSS 2

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_hero_map_id(const char *hero_path, char *out, size_t out_sz) {
    snprintf(out, out_sz, "map_start");
    FILE *f = fopen(hero_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "map_id") == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

static void tick_map_turn(const char *map_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/%s/state.txt", project_root, map_id);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int turn = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "turn") == 0) turn = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    turn++;

    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "turn") == 0) { fprintf(f, "turn=%d\n", turn); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void tick_hero_metabolism(const char *hero_path) {
    FILE *f = fopen(hero_path, "r");
    if (!f) return;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int hp = 100, hunger = 0, thirst = 0, stamina = 100;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "hp") == 0) hp = atoi(eq + 1);
            else if (strcmp(lines[nlines], "hunger") == 0) hunger = atoi(eq + 1);
            else if (strcmp(lines[nlines], "thirst") == 0) thirst = atoi(eq + 1);
            else if (strcmp(lines[nlines], "stamina") == 0) stamina = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    hunger = clamp(hunger + HUNGER_PER_TURN, 0, METABOLISM_CAP);
    thirst = clamp(thirst + THIRST_PER_TURN, 0, METABOLISM_CAP);
    stamina = clamp(stamina + STAMINA_REGEN_PER_TURN, 0, 100);
    if (hunger >= STARVING_THRESHOLD) hp = clamp(hp - STARVING_HP_LOSS, 0, 100);
    if (thirst >= DEHYDRATED_THRESHOLD) hp = clamp(hp - DEHYDRATED_HP_LOSS, 0, 100);

    f = fopen(hero_path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "hp") == 0) { fprintf(f, "hp=%d\n", hp); *eq = '='; continue; }
            if (strcmp(lines[i], "hunger") == 0) { fprintf(f, "hunger=%d\n", hunger); *eq = '='; continue; }
            if (strcmp(lines[i], "thirst") == 0) { fprintf(f, "thirst=%d\n", thirst); *eq = '='; continue; }
            if (strcmp(lines[i], "stamina") == 0) { fprintf(f, "stamina=%d\n", stamina); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);
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

static void increment_config_turn(void) {
    int current_turn = read_config_int("current_turn", 0);
    int current_epoch = read_config_int("current_epoch", 1);
    int new_turn = current_turn + 1;

    char cfg_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.txt", project_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/config.txt.tmp", project_root);
    FILE *fin = fopen(cfg_path, "r");
    FILE *fout = fopen(tmp_path, "w");
    if (fin && fout) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fin)) {
            if (strncmp(line, "current_turn=", 13) == 0)
                fprintf(fout, "current_turn=%d\n", new_turn);
            else if (strncmp(line, "current_epoch=", 14) == 0)
                fprintf(fout, "current_epoch=%d\n", current_epoch);
            else
                fputs(line, fout);
        }
        fclose(fin);
        fclose(fout);
        rename(tmp_path, cfg_path);
    } else {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
    }
}

static void append_end_turn_ledger(void) {
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
        fprintf(f, "%s|%d|hero|%d|N/A|end_turn\n", ts, epoch, turn);
        fclose(f);
    }
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/hero_01/state.txt", project_root);

    char map_id[64];
    read_hero_map_id(hero_path, map_id, sizeof(map_id));

    tick_map_turn(map_id);
    tick_hero_metabolism(hero_path);

    /* Ledger tracking: append end_turn event, increment global turn */
    append_end_turn_ledger();
    increment_config_turn();

    return 0;
}
