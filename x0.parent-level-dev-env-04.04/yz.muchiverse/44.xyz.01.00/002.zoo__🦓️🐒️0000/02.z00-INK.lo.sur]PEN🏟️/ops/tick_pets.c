/* tick_pets - one verb, one binary, no shared headers.
 * Runs every tick, after xlector_input - simple decision_mode=0
 * ("wander") AI: each pet on the map, EXCEPT whichever one is
 * currently xlector's active_target_id (a pet under manual control
 * shouldn't also be fighting the player for its own movement that same
 * tick - resumes autonomous wandering the instant control is
 * relinquished back to xlector), has a small per-tick chance of
 * stepping one random tile via the same generic move_entity.+x any
 * other movement in this project goes through. This is the family's
 * usual weighted/preset AI tier (see mutaclsym's own decision_mode
 * convention, GAME-AI-SPEED-DOCTRINE.txt) - deliberately NOT an LLM/
 * API call: AI APIs are training-only, never live in the game loop,
 * per this project family's own hard rule.
 * Usage: tick_pets.+x (no args - scans the whole map itself)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAP_ID "map_zoo"
/* 1-in-3 chance per tick to take a step - frequent enough to look
 * alive within a few keypresses, not so frequent it's frantic. */
#define WANDER_CHANCE_DENOM 3

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

int main(void) {
    resolve_root();
    srand((unsigned)(time(NULL) ^ getpid()));

    char xlector_path[PATH_BUF];
    snprintf(xlector_path, sizeof(xlector_path), "%s/pieces/world_01/%s/xlector/state.txt", project_root, MAP_ID);
    char active_target[64];
    read_kv_str(xlector_path, "active_target_id", active_target, sizeof(active_target), "xlector");

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, MAP_ID);
    DIR *d = opendir(map_dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, active_target) == 0) continue; /* under manual control right now */

        char state_path[PATH_BUF + 256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", map_dir, entry->d_name);
#pragma GCC diagnostic pop
        char type[16];
        read_kv_str(state_path, "type", type, sizeof(type), "");
        if (strcmp(type, "pet") != 0) continue;

        int decision_mode = read_kv_int(state_path, "decision_mode", 0);
        if (decision_mode != 0) continue; /* only "wander" implemented this pass */

        if (rand() % WANDER_CHANCE_DENOM != 0) continue;

        const char *dirs[4] = { "up", "down", "left", "right" };
        const char *dir = dirs[rand() % 4];

        char cmd[PATH_BUF + 128];
        snprintf(cmd, sizeof(cmd), "'%s/ops/+x/move_entity.+x' %s %s", project_root, entry->d_name, dir);
        int rc = system(cmd);
        (void)rc; /* move_entity's own exit status isn't consulted here, matching every other op-calling-op site in this project family */
    }
    closedir(d);
    return 0;
}
