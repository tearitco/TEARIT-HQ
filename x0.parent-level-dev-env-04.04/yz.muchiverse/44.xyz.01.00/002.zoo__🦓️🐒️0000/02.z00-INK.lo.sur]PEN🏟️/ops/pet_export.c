/* pet_export - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/, see
 * shared-ops-refactor-plan.txt for the why).
 *
 * THE AGNOSTIC CROSS-GAME EXPORT SHIM - the file-level half of the
 * "import/export by drag and drop" feature (see z0.zoo_0000's own
 * dox/pet-import-export-standard.md for the full design, including
 * what's still a genuinely separate visual-drag-and-drop layer vs.
 * what's real and working right now: this op IS the real, working,
 * testable mechanism; the OS-level drag gesture that would eventually
 * call it is a documented next step, not built this pass).
 *
 * Mechanism: physically rename() the piece's WHOLE directory out of
 * this game's own pieces/ tree into a shared `exchange/` directory
 * that lives ONE LEVEL ABOVE any single game's project root (a
 * sibling of every game that opts into this standard - override with
 * PRISC_EXCHANGE_ROOT if a different layout is needed) - the exact
 * same "location IS containment" philosophy mutaclsym's own
 * pickup.c/drop.c already use for hero inventory, just one level
 * further out: moving a piece OUT of any single game's world
 * entirely.
 *
 * Before moving, a `trade_envelope.txt` (the neutral, engine-agnostic
 * format first proposed in mutaclsym's own platform-vision.txt §2) is
 * written INTO the piece's own directory alongside its native
 * state.txt, so ANY other game's own import op (which may know
 * nothing about the origin game's specific state.txt field names) can
 * read just the envelope and translate it into that game's own
 * schema, best-effort.
 *
 * Zero-sum, not a copy - matches platform-vision.txt's own explicit
 * recommendation and every other piece-transfer op in this family
 * (pickup/drop): the piece is GONE from this game's own world the
 * instant export succeeds, not duplicated.
 *
 * GENERICIZED (originally had `#define MAP_ID`/`#define GAME_ID`
 * hardcoded to zoo_0000 specifically - that's what made it
 * un-shareable): the piece's own directory is now resolved via a
 * dynamic world_NN/map_NN/<piece_id>/ scan, same pattern as
 * move_entity.c/xlector_input.c. `origin_game` in the envelope is read
 * from the `PRISC_PROJECT_ID` env var every project's own button.sh
 * already exports (confirmed: mutaclsym's and zoo_0000's button.sh
 * both already `export PRISC_PROJECT_ID="<name>"` - this was already
 * real, existing per-project plumbing, not a new convention invented
 * for this refactor), falling back to "unknown_game" if unset.
 *
 * Usage: pet_export.+x <piece_id>
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

    /* If this piece is currently under xlector's manual control,
     * release control back to xlector first - about to delete the
     * very directory active_target_id points at would otherwise leave
     * a dangling reference. Resolved the same dynamic way, not a
     * hardcoded map path - xlector may be on a different map than this
     * piece in a multi-map project, so this deliberately looks up
     * xlector's OWN directory rather than assuming it's a sibling of
     * src_dir. */
    char xlector_dir[PATH_BUF + 512];
    resolve_piece_dir("xlector", xlector_dir, sizeof(xlector_dir));
    char xlector_path[PATH_BUF + 544];
    snprintf(xlector_path, sizeof(xlector_path), "%s/state.txt", xlector_dir);
    char active_target[64];
    read_kv_str(xlector_path, "active_target_id", active_target, sizeof(active_target), "xlector");
    if (strcmp(active_target, piece_id) == 0) {
        set_kv_str(xlector_path, "active_target_id", "xlector");
    }

    char name[64], species[16];
    read_kv_str(state_path, "name", name, sizeof(name), piece_id);
    read_kv_str(state_path, "species", species, sizeof(species), "unknown");
    int hunger = read_kv_int(state_path, "hunger", 50);
    int happiness = read_kv_int(state_path, "happiness", 50);
    int energy = read_kv_int(state_path, "energy", 50);

    char game_id[64];
    resolve_game_id(game_id, sizeof(game_id));

    /* Neutral trade envelope - proposed shape from mutaclsym's own
     * platform-vision.txt §2, now actually implemented. Written INTO
     * the piece's own directory before the move, so it travels WITH
     * the directory - any importing game's own op reads this file,
     * not this game's own state.txt field names. */
    char envelope_path[PATH_BUF + 544];
    snprintf(envelope_path, sizeof(envelope_path), "%s/trade_envelope.txt", src_dir);
    FILE *ef = fopen(envelope_path, "w");
    if (ef) {
        fprintf(ef, "origin_game|%s\n", game_id);
        fprintf(ef, "kind|pet\n");
        fprintf(ef, "payload_id|%s\n", piece_id);
        fprintf(ef, "display_name|%s\n", name);
        fprintf(ef, "species|%s\n", species);
        fprintf(ef, "hunger|%d\n", hunger);
        fprintf(ef, "happiness|%d\n", happiness);
        fprintf(ef, "energy|%d\n", energy);
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

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) { fprintf(lf, "%s was exported to the exchange.\n", name); fclose(lf); }
    return 0;
}
