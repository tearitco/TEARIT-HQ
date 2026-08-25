/* pet_import - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/, see
 * shared-ops-refactor-plan.txt for the why).
 *
 * THE AGNOSTIC CROSS-GAME IMPORT SHIM - counterpart to pet_export.c
 * (read that file's own header comment first for the shared exchange/
 * location and trade_envelope.txt format). NOT wired into pal/
 * main_loop.pal's per-tick dispatch - an un-imported pet has no
 * piece.pdl in THIS game yet, so there's nothing for xlector to
 * select/dispatch to until import has already happened. Run manually
 * (same "debug/authoring tool, not part of the live tick loop"
 * precedent as this shared-ops/'s own dump_rgb_png.c), or eventually
 * from whatever visual drag-and-drop layer gets built on top.
 *
 * Reads trade_envelope.txt (falls back to a native state.txt read if
 * the directory came from THIS same game/standard, e.g. round-
 * tripping a piece that was exported and never actually left) and
 * ALWAYS writes a fresh, native piece.pdl for the imported piece -
 * "translated as best-effort into that game's own schema", per
 * mutaclsym's own platform-vision.txt §2 framing, made concrete:
 * whatever game it came from, it becomes a real, fully-controllable
 * pet the instant import succeeds, not a read-only or partially-wired
 * guest.
 *
 * Genuinely pet-specific (unlike pet_export.c, which needed zero
 * schema knowledge): writes pet-flavored state fields (hunger/
 * happiness/energy) and a piece.pdl with feed/pet/play methods.
 * Consumed by any project that wants importable pet-shaped pieces
 * with the xlector standard - not a fit for a non-game project like
 * muchipal-editor-0.0, which is correct scoping, not a gap.
 *
 * GENERICIZED (originally had `#define MAP_ID "map_zoo"` hardcoded -
 * that's what made it un-shareable): the destination map is now
 * whatever map xlector is CURRENTLY on (resolved dynamically via the
 * same world_NN/map_NN/ scan move_entity.c/xlector_input.c use, then
 * reading xlector's own state.txt) - "land the imported pet wherever
 * you're currently playing" is both more correct than a fixed map id
 * and removes the hardcoding.
 *
 * Usage: pet_import.+x [piece_id]
 *   With an argument: import exactly that directory from exchange/.
 *   With no argument: import EVERY directory currently in exchange/.
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

static void resolve_exchange_root(char *out, size_t out_sz) {
    const char *env = getenv("PRISC_EXCHANGE_ROOT");
    if (env && env[0]) { snprintf(out, out_sz, "%s", env); return; }
    char parent[MAX_PATH];
    snprintf(parent, sizeof(parent), "%s", project_root);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s/exchange", parent);
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

/* Reads a pipe-delimited "key|value" line, the trade_envelope.txt
 * shape - distinct from every OTHER file format in this project family
 * (key=value), on purpose: envelopes are meant to be read by games
 * that share nothing else about the origin game's own conventions, so
 * their shape shouldn't accidentally collide with or be assumed
 * identical to any one game's own state.txt format. */
static void read_envelope_field(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *bar = strchr(line, '|');
        if (!bar) continue;
        if ((size_t)(bar - line) == strlen(key) && strncmp(line, key, bar - line) == 0) {
            snprintf(out, out_sz, "%s", bar + 1);
            break;
        }
    }
    fclose(f);
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

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void import_one(const char *name, const char *dest_map_id) {
    char exchange_root[PATH_BUF];
    resolve_exchange_root(exchange_root, sizeof(exchange_root));

    char src_dir[PATH_BUF + 64];
    snprintf(src_dir, sizeof(src_dir), "%s/%s", exchange_root, name);
    if (!path_exists(src_dir)) return;

    char envelope_path[PATH_BUF + 96];
    snprintf(envelope_path, sizeof(envelope_path), "%s/trade_envelope.txt", src_dir);
    char native_state_path[PATH_BUF + 96];
    snprintf(native_state_path, sizeof(native_state_path), "%s/state.txt", src_dir);

    char display_name[64], species[16];
    int hunger, happiness, energy;
    int have_envelope = path_exists(envelope_path);

    if (have_envelope) {
        read_envelope_field(envelope_path, "display_name", display_name, sizeof(display_name), name);
        read_envelope_field(envelope_path, "species", species, sizeof(species), "unknown");
        char buf[16];
        read_envelope_field(envelope_path, "hunger", buf, sizeof(buf), "50"); hunger = atoi(buf);
        read_envelope_field(envelope_path, "happiness", buf, sizeof(buf), "50"); happiness = atoi(buf);
        read_envelope_field(envelope_path, "energy", buf, sizeof(buf), "80"); energy = atoi(buf);
    } else if (path_exists(native_state_path)) {
        /* No envelope - fall back to reading the origin's own
         * state.txt shape directly (a round-trip within the same
         * game/standard, or a game close enough in shape to this
         * one's own fields - best-effort, matches platform-vision.txt's
         * own "translated as best-effort" framing, not a strict
         * requirement that every source always provide a proper
         * envelope). */
        char line[MAX_LINE];
        FILE *sf = fopen(native_state_path, "r");
        snprintf(display_name, sizeof(display_name), "%s", name);
        snprintf(species, sizeof(species), "unknown");
        if (sf) {
            while (fgets(line, sizeof(line), sf)) {
                line[strcspn(line, "\n")] = '\0';
                char *eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                if (strcmp(line, "name") == 0) snprintf(display_name, sizeof(display_name), "%s", eq + 1);
                else if (strcmp(line, "species") == 0) snprintf(species, sizeof(species), "%s", eq + 1);
            }
            fclose(sf);
        }
        hunger = read_kv_int(native_state_path, "hunger", 50);
        happiness = read_kv_int(native_state_path, "happiness", 50);
        energy = read_kv_int(native_state_path, "energy", 80);
    } else {
        snprintf(display_name, sizeof(display_name), "%s", name);
        snprintf(species, sizeof(species), "unknown");
        hunger = 50; happiness = 50; energy = 80;
    }

    char dst_dir[PATH_BUF + 128];
    snprintf(dst_dir, sizeof(dst_dir), "%s/pieces/world_01/%s/%s", project_root, dest_map_id, name);
    if (rename(src_dir, dst_dir) != 0) return;

    /* ALWAYS (re)write a fresh, native state.txt/piece.pdl - whatever
     * game this pet came from, it becomes a real, fully-controllable
     * pet the instant import succeeds. A fixed, always-open import
     * point (2,2) - real placement/landing-spot logic (avoid overlap,
     * spawn near xlector, etc.) is a reasonable next step, not built
     * this pass. */
    char state_path[PATH_BUF + 160];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", dst_dir);
    FILE *sf = fopen(state_path, "w");
    if (sf) {
        fprintf(sf, "name=%s\n", display_name);
        fprintf(sf, "type=pet\n");
        fprintf(sf, "species=%s\n", species);
        fprintf(sf, "pos_x=2\n");
        fprintf(sf, "pos_y=2\n");
        fprintf(sf, "pos_z=0\n");
        fprintf(sf, "on_map=1\n");
        fprintf(sf, "map_id=%s\n", dest_map_id);
        fprintf(sf, "hunger=%d\n", hunger);
        fprintf(sf, "happiness=%d\n", happiness);
        fprintf(sf, "energy=%d\n", energy);
        fprintf(sf, "decision_mode=0\n");
        fprintf(sf, "digit_accum=0\n");
        fprintf(sf, "action_cursor=-1\n");
        fprintf(sf, "last_key=0\n");
        fclose(sf);
    }

    char pdl_path[PATH_BUF + 160];
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", dst_dir);
    FILE *pf = fopen(pdl_path, "w");
    if (pf) {
        fprintf(pf, "SECTION      | KEY                | VALUE\n");
        fprintf(pf, "----------------------------------------\n");
        fprintf(pf, "META         | piece_id           | %s\n", name);
        fprintf(pf, "META         | version            | 1.0\n\n");
        fprintf(pf, "STATE        | name                 | %s\n", display_name);
        fprintf(pf, "STATE        | species              | %s\n", species);
        fprintf(pf, "STATE        | map_id               | %s\n\n", dest_map_id);
        fprintf(pf, "METHOD       | move                 | ops/+x/move_entity.+x\n");
        fprintf(pf, "METHOD       | select               | ops/+x/xlector_input.+x\n");
        fprintf(pf, "METHOD       | feed                 | ops/+x/feed_pet.+x\n");
        fprintf(pf, "METHOD       | pet                  | ops/+x/pet_pet.+x\n");
        fprintf(pf, "METHOD       | play                 | ops/+x/play_pet.+x\n");
        fprintf(pf, "METHOD       | export               | ops/+x/pet_export.+x\n\n");
        fprintf(pf, "RESPONSE     | default              | *%s settles in*\n", display_name);
        fclose(pf);
    }

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) { fprintf(lf, "%s was imported from the exchange.\n", display_name); fclose(lf); }
}

int main(int argc, char **argv) {
    resolve_root();

    /* Destination map: wherever xlector is CURRENTLY on, resolved
     * dynamically - see this file's own header comment for why this
     * replaced a hardcoded MAP_ID. xlector_dir looks like
     * ".../world_01/map_zoo/xlector" - strip the trailing "/xlector"
     * to get the map directory, then take THAT directory's own last
     * path component ("map_zoo") as the destination map id. */
    char xlector_dir[PATH_BUF + 512];
    resolve_piece_dir("xlector", xlector_dir, sizeof(xlector_dir));
    char map_dir[PATH_BUF + 512];
    snprintf(map_dir, sizeof(map_dir), "%s", xlector_dir);
    char *slash = strrchr(map_dir, '/');
    if (slash) *slash = '\0';
    char *map_name = strrchr(map_dir, '/');
    char dest_map_id[128];
    snprintf(dest_map_id, sizeof(dest_map_id), "%s", map_name ? map_name + 1 : "map_start");

    if (argc >= 2) {
        import_one(argv[1], dest_map_id);
        return 0;
    }

    char exchange_root[PATH_BUF];
    resolve_exchange_root(exchange_root, sizeof(exchange_root));
    DIR *d = opendir(exchange_root);
    if (!d) return 0;
    struct dirent *entry;
    char names[128][256];
    int n = 0;
    while (n < 128 && (entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(names[n], sizeof(names[0]), "%s", entry->d_name);
        n++;
    }
    closedir(d);
    for (int i = 0; i < n; i++) import_one(names[i], dest_map_id);
    return 0;
}
