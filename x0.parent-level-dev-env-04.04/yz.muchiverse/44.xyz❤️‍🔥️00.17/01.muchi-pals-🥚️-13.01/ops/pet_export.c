/* pet_export - Export pet from muchi-pals to exchange directory.
 *
 * Adapted from zoo's pet_export.c for muchi-pals' directory structure.
 * Exports a pet to the shared exchange directory so it can be imported
 * by other games (muchaclsym, zoo, etc.).
 *
 * muchi-pals structure: pieces/world_01/map_lobby/<pet_id>/
 *   - state.txt: pet stats (hunger, energy, hp, mp, skills, etc.)
 *   - piece.pdl: behavior definition
 *   - sprite.csv: animation data
 *   - atlas.png: sprite sheet
 *   - window.pid: GL window process ID
 *
 * The pet is moved (not copied) to the exchange directory.
 * A trade_envelope.txt is written with cross-game compatible format.
 *
 * Usage: pet_export <pet_id>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void resolve_game_id(char *out, size_t out_sz) {
    const char *env = getenv("PRISC_PROJECT_ID");
    snprintf(out, out_sz, "%s", (env && env[0]) ? env : "unknown_game");
}

/* Same pattern as move_entity.c's/xlector_input.c's own
 * resolve_piece_dir() - scan world_NN/map_NN/<piece_id>/ for the
 * piece's own directory, falling back to a flat pieces/<piece_id>/
 * layout. */
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

/* The one piece of "agnostic" plumbing every adopting project needs:
 * a shared exchange/ location siblings can all find. Default is one
 * level above this project's own root; PRISC_EXCHANGE_ROOT overrides
 * it for a different layout (e.g. a project that isn't a sibling
 * under the same parent folder). */
static void resolve_exchange_root(char *out, size_t out_sz) {
    const char *env = getenv("PRISC_EXCHANGE_ROOT");
    if (env && env[0]) { snprintf(out, out_sz, "%s", env); return; }
    char parent[MAX_PATH];
    snprintf(parent, sizeof(parent), "%s", project_root);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s/exchange", parent);
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

static void set_kv_str(const char *path, const char *key, const char *value) {
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
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
            continue;
        }
        fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *piece_id = argv[1];
    resolve_root();

    char src_dir[PATH_BUF + 512];
    resolve_piece_dir(piece_id, src_dir, sizeof(src_dir));
    char state_path[PATH_BUF + 544];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", src_dir);

    FILE *check = fopen(state_path, "r");
    if (!check) return 1;
    fclose(check);

    /* muchi-pals doesn't have xlector - pets are selected via
     * egg_window's GL windows or keyboard navigation.
     * No selection state to clear before export. */

    char name[64], species_id[32], species_name[32], species_emoji[16], skills[128];
    read_kv_str(state_path, "name", name, sizeof(name), piece_id);
    read_kv_str(state_path, "species_id", species_id, sizeof(species_id), "unknown");
    read_kv_str(state_path, "species_name", species_name, sizeof(species_name), "Unknown");
    /* Carried through so an importing game can actually render this
     * pet's real species glyph instead of a generic marker - see
     * 0.a-z-pets-plan/a-z-fix-report.txt's own note on why this was
     * missing (mutaclsym's compose_frame.c had nothing that read it,
     * because nothing ever sent it). */
    read_kv_str(state_path, "species_emoji", species_emoji, sizeof(species_emoji), "");
    read_kv_str(state_path, "skills", skills, sizeof(skills), "");
    int hunger = read_kv_int(state_path, "hunger", 100);
    int energy = read_kv_int(state_path, "energy", 100);
    int hp = read_kv_int(state_path, "hp", 25);
    int hp_max = read_kv_int(state_path, "hp_max", 25);
    int mp = read_kv_int(state_path, "mp", 13);
    int mp_max = read_kv_int(state_path, "mp_max", 13);
    int level = read_kv_int(state_path, "level", 1);
    int xp = read_kv_int(state_path, "xp", 0);

    char game_id[64];
    resolve_game_id(game_id, sizeof(game_id));

    /* Neutral trade envelope - cross-game compatible format.
     * Written INTO the pet's directory before the move, so it travels
     * with the directory. Any importing game reads this file, not
     * muchi-pals' state.txt field names. */
    char envelope_path[PATH_BUF + 544];
    snprintf(envelope_path, sizeof(envelope_path), "%s/trade_envelope.txt", src_dir);
    FILE *ef = fopen(envelope_path, "w");
    if (ef) {
        fprintf(ef, "origin_game|%s\n", game_id);
        fprintf(ef, "kind|pet\n");
        fprintf(ef, "payload_id|%s\n", piece_id);
        fprintf(ef, "display_name|%s\n", name);
        fprintf(ef, "species_id|%s\n", species_id);
        fprintf(ef, "species_name|%s\n", species_name);
        fprintf(ef, "species_emoji|%s\n", species_emoji);
        fprintf(ef, "skills|%s\n", skills);
        fprintf(ef, "hunger|%d\n", hunger);
        fprintf(ef, "energy|%d\n", energy);
        fprintf(ef, "hp|%d\n", hp);
        fprintf(ef, "hp_max|%d\n", hp_max);
        fprintf(ef, "mp|%d\n", mp);
        fprintf(ef, "mp_max|%d\n", mp_max);
        fprintf(ef, "level|%d\n", level);
        fprintf(ef, "xp|%d\n", xp);
        fprintf(ef, "exported_at|%ld\n", (long)time(NULL));
        fclose(ef);
    }

    char exchange_root[PATH_BUF];
    resolve_exchange_root(exchange_root, sizeof(exchange_root));
    mkdir(exchange_root, 0755);

    char dst_dir[PATH_BUF + 64];
    snprintf(dst_dir, sizeof(dst_dir), "%s/%s", exchange_root, piece_id);

    if (rename(src_dir, dst_dir) != 0) {
        return 1;
    }

    /* Log export to console - muchi-pals doesn't have message_log.txt */
    printf("Exported pet '%s' (%s) to exchange directory\n", name, piece_id);
    return 0;
}
