/* move_entity - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/,
 * see shared-ops-refactor-plan.txt for the why). Fully generic mover:
 * takes a piece_id and a direction, moves THAT piece's own
 * pos_x/pos_y one tile if the destination is walkable. Deliberately
 * has ZERO special-casing for "is this xlector or a real entity" -
 * real fuzz-op_manager.c's own route_input() calls this exact same
 * generic op (there, `move_entity.+x $active_target_id $dir`) whether
 * the current active_target_id is "xlector" or a real pet, and
 * xlector's own movement is subject to the SAME collision check as
 * anything else, not a free/uncollided cursor mode (that design
 * belongs to mutaclsym's own DIFFERENT adaptation of xlector - see
 * z0.zoo_0000's own dox/xlector-standard.md for why this file
 * intentionally does not copy that choice). Whichever piece the
 * caller names is just "the thing being moved right now".
 *
 * Originally written with a hardcoded `#define MAP_ID` (single-map
 * assumption), which is exactly what made it zoo_0000-specific and
 * un-shareable - fixed by resolving the piece's own directory (and
 * from it, its containing map's map.txt) via a dynamic
 * world_NN/map_NN/<piece_id>/ scan, the SAME pattern
 * pdl_reader.c's own resolve_piece_pdl_path() and real
 * system/prisc+x.c's own resolve_piece_state_path() already use. This
 * is a strict improvement, not just a refactor for its own sake: it
 * also removes the single-map-only limitation as a free side effect -
 * a piece on ANY map now moves correctly, not just one on a
 * hardcoded map id.
 *
 * Usage: move_entity.+x <piece_id> <up|down|left|right>
 */
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

/* Resolves piece_id to its OWN directory (the one containing
 * state.txt) by scanning world_NN/map_NN/ - same pattern
 * pdl_reader.c's own resolve_piece_pdl_path() uses, just resolving a
 * directory instead of a specific file inside it. Falls back to a
 * flat pieces/<piece_id>/ layout if no world/map scan match is found. */
static void resolve_piece_dir(const char *piece_id, char *out, size_t out_sz) {
    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/pieces", project_root);

    DIR *worlds = opendir(pieces_dir);
    if (worlds) {
        struct dirent *w;
        while ((w = readdir(worlds)) != NULL) {
            if (strncmp(w->d_name, "world_", 6) != 0) continue;
            char maps_dir[PATH_BUF + 256];
            snprintf(maps_dir, sizeof(maps_dir), "%s/%s", pieces_dir, w->d_name);
            DIR *maps = opendir(maps_dir);
            if (!maps) continue;
            struct dirent *m;
            while ((m = readdir(maps)) != NULL) {
                if (strncmp(m->d_name, "map_", 4) != 0) continue;
                char candidate[PATH_BUF + 512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(candidate, sizeof(candidate), "%s/%s/%s", maps_dir, m->d_name, piece_id);
#pragma GCC diagnostic pop
                struct stat st;
                if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
                    closedir(maps);
                    closedir(worlds);
                    snprintf(out, out_sz, "%s", candidate);
                    return;
                }
            }
            closedir(maps);
        }
        closedir(worlds);
    }

    snprintf(out, out_sz, "%s/pieces/%s", project_root, piece_id);
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

static void set_kv_int(const char *path, const char *key, int value) {
    char lines[64][MAX_LINE];
    int nlines = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    int found = 0;
    size_t klen = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        if (!found && strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%d\n", key, value);
            found = 1;
            continue;
        }
        fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%d\n", key, value);
    fclose(f);
}

/* map_dir is the piece's own CONTAINING map directory (one level up
 * from its own piece directory), resolved by the caller - this
 * function just reads map.txt from wherever it's told to look, no
 * assumptions about which map that is. */
static int is_walkable(const char *map_dir, int x, int y) {
    char map_path[PATH_BUF + 32];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
#pragma GCC diagnostic pop
    FILE *f = fopen(map_path, "r");
    if (!f) return 0;
    char line[256];
    int row = 0;
    int walkable = 0;
    while (fgets(line, sizeof(line), f)) {
        if (row == y) {
            line[strcspn(line, "\n")] = '\0';
            walkable = (x >= 0 && x < (int)strlen(line) && line[x] != '#');
            break;
        }
        row++;
    }
    fclose(f);
    return walkable;
}

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    const char *piece_id = argv[1];
    const char *dir = argv[2];
    resolve_root();

    char piece_dir[PATH_BUF + 512];
    resolve_piece_dir(piece_id, piece_dir, sizeof(piece_dir));

    char state_path[PATH_BUF + 544];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", piece_dir);

    char map_dir[PATH_BUF + 512];
    snprintf(map_dir, sizeof(map_dir), "%s", piece_dir);
    char *slash = strrchr(map_dir, '/');
    if (slash) *slash = '\0';

    int x = read_kv_int(state_path, "pos_x", 0);
    int y = read_kv_int(state_path, "pos_y", 0);
    int nx = x, ny = y;

    if (strcmp(dir, "up") == 0) ny--;
    else if (strcmp(dir, "down") == 0) ny++;
    else if (strcmp(dir, "left") == 0) nx--;
    else if (strcmp(dir, "right") == 0) nx++;
    else return 1;

    if (is_walkable(map_dir, nx, ny)) {
        set_kv_int(state_path, "pos_x", nx);
        set_kv_int(state_path, "pos_y", ny);
    }
    return 0;
}
