/* pickup - one verb, one binary, no shared headers.
 * Scans pieces/world_01/<hero's map_id>/items/ for an item instance piece
 * whose pos_x/pos_y matches the hero's position, and physically MOVES
 * that piece's whole directory (rename()) into
 * pieces/world_01/map_start/hero/inventory/ - real russian-doll nesting,
 * not a location field. An item's disposition is entirely encoded by
 * which directory it's physically sitting in: dropped means the
 * directory can be dragged into a different environment's tree at any
 * time, on disk, by hand, with no other bookkeeping to keep in sync.
 * Appends a one-line result to pieces/display/message_log.txt for
 * compose_frame to display the tail of - not a single overwritten
 * hero/state.txt field (that clobbered same-turn messages from other
 * ops, e.g. a zombie's attack message silently erasing a pickup message
 * from earlier the same turn - see nav-refactor-2.txt for the full
 * writeup of why this changed). */
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

/* item_id -> display name, looked up from the registry so the pickup
 * message reads "Picked up Rock." not "Picked up rock." */
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

    char items_dir[PATH_BUF + 64];
    snprintf(items_dir, sizeof(items_dir), "%s/pieces/world_01/%s/items", project_root, map_id);
    char inventory_dir[PATH_BUF + 64];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_01/map_start/hero/inventory", project_root);

    DIR *d = opendir(items_dir);
    if (!d) { log_message("Nothing here to pick up."); return 0; }

    struct dirent *entry;
    int picked = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir, entry->d_name);

        int ix = read_kv_int(state_path, "pos_x", -1);
        int iy = read_kv_int(state_path, "pos_y", -1);
        if (ix != hero_x || iy != hero_y) continue;

        char item_id[64];
        read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");

        mkdir(inventory_dir, 0755); /* ensure it exists - harmless if already there */

        char src_dir[PATH_BUF + 320], dst_dir[PATH_BUF + 320];
        snprintf(src_dir, sizeof(src_dir), "%s/%s", items_dir, entry->d_name);
        snprintf(dst_dir, sizeof(dst_dir), "%s/%s", inventory_dir, entry->d_name);

        if (rename(src_dir, dst_dir) != 0) {
            log_message("Pickup failed (could not move piece).");
            closedir(d);
            return 1;
        }

        /* pos_x/pos_y are meaningless while carried - drop.c re-adds
         * them at the hero's position when the item leaves inventory. */
        char new_state_path[PATH_BUF + 384];
        snprintf(new_state_path, sizeof(new_state_path), "%s/state.txt", dst_dir);
        FILE *sf = fopen(new_state_path, "w");
        if (sf) { fprintf(sf, "item_id=%s\n", item_id); fclose(sf); }

        char name[64], msg[128];
        item_display_name(item_id, name, sizeof(name));
        snprintf(msg, sizeof(msg), "Picked up %s.", name);
        log_message(msg);
        picked = 1;
        break;
    }
    closedir(d);

    if (!picked) log_message("Nothing here to pick up.");
    return 0;
}
