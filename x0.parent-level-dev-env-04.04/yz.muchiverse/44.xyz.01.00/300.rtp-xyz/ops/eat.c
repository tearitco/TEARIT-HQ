/* eat - one verb, one binary, no shared headers.
 * Scans hero/inventory/ for the first item whose registry category is
 * food or drink, restores hunger (food) or thirst (drink) by the item's
 * power, and CONSUMES it - unlike pickup/drop, which move a piece's
 * directory around, this is the first op that deletes one outright
 * (remove() the state.txt, then rmdir() the now-empty piece directory).
 * Non-edible items in inventory are skipped, not blocking, so the first
 * apple behind a rock in inventory order still gets found and eaten. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

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

/* field_index: 1=name, 2=category, 5=power (fields are
 * id|name|category|glyph|weight|power). */
static void item_registry_field(const char *item_id, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void log_message(const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

static void write_hero_stat(const char *hero_path, const char *stat_key, int new_val) {
    if (!stat_key) return;
    FILE *f = fopen(hero_path, "r");
    if (!f) return;
    char lines[32][MAX_LINE];
    int nlines = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
    fclose(f);

    f = fopen(hero_path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], stat_key, strlen(stat_key)) == 0 && lines[i][strlen(stat_key)] == '=') {
            fprintf(f, "%s=%d\n", stat_key, new_val);
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
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_01/map_start/hero/inventory", project_root);

    DIR *d = opendir(inventory_dir);
    if (!d) { log_message("You have nothing to eat or drink."); return 0; }

    struct dirent *entry;
    int consumed = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char item_id[64];
        read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");

        char category[16], name[64];
        item_registry_field(item_id, 2, category, sizeof(category), "?");
        int is_food = (strcmp(category, "food") == 0);
        int is_drink = (strcmp(category, "drink") == 0);
        if (!is_food && !is_drink) continue;

        char power_str[16];
        item_registry_field(item_id, 5, power_str, sizeof(power_str), "0");
        int power = atoi(power_str);
        item_registry_field(item_id, 1, name, sizeof(name), item_id);

        if (is_food) {
            int hunger = read_kv_int(hero_path, "hunger", 0);
            hunger = clamp(hunger - power, 0, 200);
            char msg[128];
            snprintf(msg, sizeof(msg), "Ate %s.", name);
            write_hero_stat(hero_path, "hunger", hunger);
            log_message(msg);
        } else {
            int thirst = read_kv_int(hero_path, "thirst", 0);
            thirst = clamp(thirst - power, 0, 200);
            char msg[128];
            snprintf(msg, sizeof(msg), "Drank %s.", name);
            write_hero_stat(hero_path, "thirst", thirst);
            log_message(msg);
        }

        /* Consume: delete the piece outright, not move it. */
        char item_dir[PATH_BUF + 320];
        snprintf(item_dir, sizeof(item_dir), "%s/%s", inventory_dir, entry->d_name);
        remove(state_path);
        rmdir(item_dir);

        consumed = 1;
        break;
    }
    closedir(d);

    if (!consumed) log_message("You have nothing to eat or drink.");
    return 0;
}
