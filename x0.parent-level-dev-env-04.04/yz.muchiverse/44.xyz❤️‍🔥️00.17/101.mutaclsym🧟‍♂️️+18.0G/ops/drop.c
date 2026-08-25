/* drop - one verb, one binary, no shared headers.
 * Drops one item (the first one found in hero/inventory/, v1 - no
 * item-selection UI yet) by physically MOVING its directory (rename())
 * from pieces/world_01/map_start/hero/inventory/ back out into
 * pieces/world_01/<hero's current map_id>/items/, and stamping the
 * hero's current pos_x/pos_y into its state.txt so it lands where the
 * hero is standing. Same real russian-doll nesting as pickup.c's
 * reverse direction. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

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
        char *p2 = strchr(name, '|');
        if (p2) *p2 = '\0';
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

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    int hero_x = read_kv_int(hero_path, "pos_x", 0);
    int hero_y = read_kv_int(hero_path, "pos_y", 0);
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char inventory_dir[PATH_BUF + 64];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_01/map_start/hero/inventory", project_root);

    DIR *d = opendir(inventory_dir);
    if (!d) { log_message("Nothing to drop."); return 0; }

    struct dirent *entry;
    char picked_name[256] = "";
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(picked_name, sizeof(picked_name), "%s", entry->d_name);
        break;
    }
    closedir(d);

    if (!picked_name[0]) { log_message("Nothing to drop."); return 0; }

    char items_dir[PATH_BUF + 64];
    snprintf(items_dir, sizeof(items_dir), "%s/pieces/world_01/%s/items", project_root, map_id);
    mkdir(items_dir, 0755); /* ensure it exists - harmless if already there */

    char src_dir[PATH_BUF + 320], dst_dir[PATH_BUF + 320];
    snprintf(src_dir, sizeof(src_dir), "%s/%s", inventory_dir, picked_name);
    snprintf(dst_dir, sizeof(dst_dir), "%s/%s", items_dir, picked_name);

    char item_id[64];
    char src_state[PATH_BUF + 384];
    snprintf(src_state, sizeof(src_state), "%s/state.txt", src_dir);
    read_kv_str(src_state, "item_id", item_id, sizeof(item_id), "?");

    if (rename(src_dir, dst_dir) != 0) {
        log_message("Drop failed (could not move piece).");
        return 1;
    }

    char dst_state[PATH_BUF + 384];
    snprintf(dst_state, sizeof(dst_state), "%s/state.txt", dst_dir);
    FILE *sf = fopen(dst_state, "w");
    if (sf) {
        fprintf(sf, "item_id=%s\n", item_id);
        fprintf(sf, "pos_x=%d\n", hero_x);
        fprintf(sf, "pos_y=%d\n", hero_y);
        fclose(sf);
    }

    char name[64], msg[128];
    item_display_name(item_id, name, sizeof(name));
    snprintf(msg, sizeof(msg), "Dropped %s.", name);
    log_message(msg);
    return 0;
}
