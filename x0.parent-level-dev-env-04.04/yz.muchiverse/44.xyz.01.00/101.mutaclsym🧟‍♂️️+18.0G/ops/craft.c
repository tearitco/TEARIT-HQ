/* craft - one verb, one binary, no shared headers.
 * Usage: craft.+x [recipe_id]
 * With a recipe_id (as chosen from the crafting overlay panel - see
 * ops/choice.c's panel-mode handling and compose_frame.c's panel
 * renderer), crafts exactly that recipe if its requirements are still
 * satisfied. With no argument (legacy CLI/testing path, no longer
 * reachable from normal play now that craft always opens the panel
 * first), scans pieces/registry/recipes/recipes.txt top to bottom and
 * crafts the FIRST recipe whose requirements are fully satisfied -
 * same "first match wins" precedent as pickup.c/eat.c. Either way,
 * consumes (remove()+rmdir(), same as eat.c) the exact required
 * quantity of each ingredient piece, then creates a brand new piece for
 * the result directly inside hero/inventory/ - crafting goes straight
 * into the hero's hands, matching CDDA convention, using a fresh
 * serial-numbered directory name (pieces/system/item_serial_counter.txt)
 * so it never collides with a hand-placed or previously crafted item's
 * id. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_REQS 8

typedef struct {
    char item_id[64];
    int qty;
} Requirement;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

static void item_display_name(const char *item_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", item_id);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
    }
    fclose(f);
}

static void log_message(const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

/* Counts how many inventory pieces have the given item_id. */
static int count_in_inventory(const char *inventory_dir, const char *item_id) {
    DIR *d = opendir(inventory_dir);
    if (!d) return 0;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char id[64];
        read_kv_str(state_path, "item_id", id, sizeof(id), "?");
        if (strcmp(id, item_id) == 0) count++;
    }
    closedir(d);
    return count;
}

/* Deletes exactly `qty` inventory pieces with the given item_id. */
static void consume_from_inventory(const char *inventory_dir, const char *item_id, int qty) {
    DIR *d = opendir(inventory_dir);
    if (!d) return;
    struct dirent *entry;
    int removed = 0;
    while (removed < qty && (entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char id[64];
        read_kv_str(state_path, "item_id", id, sizeof(id), "?");
        if (strcmp(id, item_id) != 0) continue;
        char dir_path[PATH_BUF + 320];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", inventory_dir, entry->d_name);
        remove(state_path);
        rmdir(dir_path);
        removed++;
    }
    closedir(d);
}

static int next_item_serial(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/item_serial_counter.txt", project_root);
    int serial = 0;
    FILE *f = fopen(path, "r");
    if (f) { if (fscanf(f, "%d", &serial) != 1) serial = 0; fclose(f); }
    serial++;
    f = fopen(path, "w");
    if (f) { fprintf(f, "%d\n", serial); fclose(f); }
    return serial;
}

int main(int argc, char **argv) {
    const char *want_recipe_id = (argc >= 2) ? argv[1] : NULL;
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_01/map_start/hero/inventory", project_root);

    char recipes_path[PATH_BUF];
    snprintf(recipes_path, sizeof(recipes_path), "%s/pieces/registry/recipes/recipes.txt", project_root);
    FILE *rf = fopen(recipes_path, "r");
    if (!rf) { log_message("No recipes known."); return 0; }

    char line[MAX_LINE];
    int crafted = 0;
    while (!crafted && fgets(line, sizeof(line), rf)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';

        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        const char *recipe_id = line; /* line is now nul-terminated exactly at the id */
        if (want_recipe_id && strcmp(recipe_id, want_recipe_id) != 0) continue;
        char *name = p1 + 1;
        char *p2 = strchr(name, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *result = p2 + 1;
        char *p3 = strchr(result, '|');
        if (!p3) continue;
        *p3 = '\0';
        char *reqs_str = p3 + 1;

        Requirement reqs[MAX_REQS];
        int nreqs = 0;
        char reqs_copy[MAX_LINE];
        snprintf(reqs_copy, sizeof(reqs_copy), "%s", reqs_str);
        char *tok = strtok(reqs_copy, ",");
        while (tok && nreqs < MAX_REQS) {
            char *colon = strchr(tok, ':');
            if (colon) {
                *colon = '\0';
                snprintf(reqs[nreqs].item_id, sizeof(reqs[nreqs].item_id), "%s", tok);
                reqs[nreqs].qty = atoi(colon + 1);
                nreqs++;
            }
            tok = strtok(NULL, ",");
        }

        int satisfied = 1;
        for (int i = 0; i < nreqs; i++) {
            if (count_in_inventory(inventory_dir, reqs[i].item_id) < reqs[i].qty) { satisfied = 0; break; }
        }
        if (!satisfied) continue;

        for (int i = 0; i < nreqs; i++) consume_from_inventory(inventory_dir, reqs[i].item_id, reqs[i].qty);

        int serial = next_item_serial();
        char new_dir[PATH_BUF + 64];
        snprintf(new_dir, sizeof(new_dir), "%s/crafted_%d", inventory_dir, serial);
        mkdir(new_dir, 0755);
        char new_state[PATH_BUF + 128];
        snprintf(new_state, sizeof(new_state), "%s/state.txt", new_dir);
        FILE *sf = fopen(new_state, "w");
        if (sf) { fprintf(sf, "item_id=%s\n", result); fclose(sf); }

        char result_name[64], msg[128];
        item_display_name(result, result_name, sizeof(result_name));
        snprintf(msg, sizeof(msg), "Crafted %s.", result_name);
        log_message(msg);
        crafted = 1;
    }
    fclose(rf);

    if (!crafted) {
        log_message(want_recipe_id ? "You don't have the materials for that."
                                                   : "You don't have the materials to craft anything.");
    }
    return 0;
}
