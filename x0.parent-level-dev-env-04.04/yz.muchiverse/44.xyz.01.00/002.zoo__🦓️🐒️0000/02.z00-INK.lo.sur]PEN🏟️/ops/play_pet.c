/* play_pet - one verb, one binary, no shared headers.
 * Playing with a pet: happiness up, energy down (costs stamina, unlike
 * a plain pet-on-the-head). Same generic <piece_id>-argument shape as
 * feed_pet.c/pet_pet.c - see that file's own header comment.
 * Usage: play_pet.+x <piece_id>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAP_ID "map_zoo"

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

static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *piece_id = argv[1];
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/%s/%s/state.txt", project_root, MAP_ID, piece_id);

    int happiness = read_kv_int(state_path, "happiness", 50);
    int energy = read_kv_int(state_path, "energy", 50);
    set_kv_int(state_path, "happiness", clamp(happiness + 15, 0, 100));
    set_kv_int(state_path, "energy", clamp(energy - 15, 0, 100));

    char name[64];
    read_kv_str(state_path, "name", name, sizeof(name), piece_id);
    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) { fprintf(lf, "%s plays energetically!\n", name); fclose(lf); }
    return 0;
}
