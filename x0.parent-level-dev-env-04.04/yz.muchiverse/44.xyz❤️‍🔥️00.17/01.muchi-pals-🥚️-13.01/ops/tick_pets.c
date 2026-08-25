/* tick_pets - the epoch op, one verb, one binary, no shared headers.
 * Modeled directly on Moke-Pet's manager.c pattern (mutaclsym/dox/
 * 01-cdda-architecture.md sec.4): scans pieces/world_01/map_lobby/ for
 * every piece of type=pet and advances its metabolism, regardless of
 * what any menu action did this turn.
 *
 * Two independent callers, both allowed to be active at once, controlled
 * by pieces/system/tick_config.txt:
 *   - "world tick": pal/main_loop.pal's turn loop bareword-dispatches
 *     this op with no args every ~100 iterations, sweeping every pet.
 *     Gated by tick_config.txt's world_tick=0/1 - a no-op scan if 0.
 *   - "self tick": each pet's own egg_window process, once opened,
 *     invokes `tick_pets.+x <pet_id>` on its own timer, independent of
 *     any terminal - gated by egg_window.c reading self_tick=0/1 itself
 *     before ever calling this.
 * These two are made safe to run together (not mutually exclusive) by a
 * shared `last_tick_ts` (unix seconds) field on each pet: tick_one_pet()
 * skips a pet entirely if less than tick_interval_sec has passed since
 * its own last tick, regardless of which caller is asking - so whichever
 * of world-tick/self-tick reaches a given pet first in an interval wins
 * that interval, and the other becomes a harmless no-op instead of
 * double-ticking it. One pet, one advance per interval, no matter how
 * many things are allowed to try.
 *
 * Fields it owns: hunger, energy, poop_count, poop_timer, asleep,
 * grid_x/grid_y/z (absolute position on a fixed logical
 * WORLD_GRID_W x WORLD_GRID_H grid - this op is the sole owner of
 * position so it's always displayable in the terminal even with no
 * egg_window open; egg_window only converts these to screen pixels and
 * clamps to the real display, it never decides where the pet goes),
 * facing (left/right, updated only when the horizontal step is nonzero
 * so it holds steady when the pet isn't moving horizontally), tick_seq
 * (bumped every applied tick so a poller can detect a fresh one),
 * last_tick_ts (the compatibility gate above). Any pet missing these
 * fields (hatched before this op existed) gets them appended with
 * defaults threaded through this tick, rather than requiring a migration.
 *
 * z is bounded by the pet's own `movement` trait (copied from the
 * species registry at generate_egg.c time): ground pets stay pinned at
 * z=0, flying roams above ground, digging/aquatic roams below it - see
 * z_bounds_for_movement() below.
 *
 * Usage: tick_pets.+x [pet_id]
 *   no args    - world-tick mode: sweep every pet in map_lobby (subject
 *                to world_tick config and each pet's own interval gate).
 *   <pet_id>   - self-tick mode: advance just that one pet (subject only
 *                to its own interval gate - the caller, egg_window.c, is
 *                the one that checks self_tick config before invoking
 *                this at all).
 * Prints nothing on success (silent, matches a background epoch tick);
 * only stderr on real errors. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINES 48

/* Logical world grid - fixed and independent of any actual display;
 * egg_window.c defensively re-clamps to the real screen when converting
 * to pixels. hatch_egg.c's grid_x=20/grid_y=15 defaults are this grid's
 * center - keep both in sync if these ever change. */
#define WORLD_GRID_W 40
#define WORLD_GRID_H 30

static char project_root[MAX_PATH] = ".";
static int g_world_tick_enabled = 1;
static int g_tick_interval_sec = 3;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* pieces/system/tick_config.txt - see this file's header comment for how
 * world_tick/self_tick/tick_interval_sec make the two ticking modes
 * compatible instead of mutually exclusive. Missing file/keys just keep
 * the defaults above (both modes on, 3s interval) - a fresh checkout
 * works without anyone having to create this file first. */
static void read_tick_config(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/tick_config.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(line, "world_tick") == 0) g_world_tick_enabled = val;
        else if (strcmp(line, "tick_interval_sec") == 0 && val > 0) g_tick_interval_sec = val;
    }
    fclose(f);
}

static unsigned int random_seed(void) {
    unsigned int seed;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(&seed, sizeof(seed), 1, f);
        fclose(f);
        if (n == 1) return seed;
    }
    return (unsigned int)(time(NULL) ^ getpid());
}

static void append_ledger(const char *pet_id, int hunger, int energy, int poop_count,
                           int asleep, int grid_x, int grid_y, int z, const char *facing) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] StateChange: %s tick hunger=%d energy=%d poop_count=%d asleep=%d grid_x=%d grid_y=%d z=%d facing=%s | Trigger: tick_pets\n",
            ts, pet_id, hunger, energy, poop_count, asleep, grid_x, grid_y, z, facing);
    fclose(f);
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* z-axis band a species is allowed to wander, keyed by its movement
 * trait - plain data-driven behavior, no per-species special-casing. */
static void z_bounds_for_movement(const char *movement, int *lo, int *hi) {
    if (strcmp(movement, "flying") == 0) { *lo = 1; *hi = 4; }
    else if (strcmp(movement, "digging") == 0) { *lo = -4; *hi = 0; }
    else if (strcmp(movement, "aquatic") == 0) { *lo = -4; *hi = 0; }
    else { *lo = 0; *hi = 0; } /* ground (default) */
}

/* Reads one pet's state.txt, advances its metabolism, rewrites it. */
static void tick_one_pet(const char *pet_id, const char *state_path) {
    FILE *f = fopen(state_path, "r");
    if (!f) return;

    char lines[MAX_LINES][MAX_LINE];
    int nlines = 0;
    int type_is_pet = 0;
    int hunger = 0, energy = 100, poop_count = 0, poop_timer = 0, asleep = 0, tick_seq = 0;
    int grid_x = WORLD_GRID_W / 2, grid_y = WORLD_GRID_H / 2, z = 0;
    long last_tick_ts = 0;
    char facing[8] = "right";
    char movement[16] = "ground";
    int seen_hunger = 0, seen_energy = 0, seen_poop_count = 0, seen_poop_timer = 0;
    int seen_asleep = 0, seen_grid_x = 0, seen_grid_y = 0, seen_z = 0;
    int seen_tick_seq = 0, seen_facing = 0, seen_last_tick_ts = 0;

    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            const char *key = lines[nlines];
            if (strcmp(key, "type") == 0) {
                /* Strip both \r and \n - a state.txt touched by a Windows
                 * editor/tool can have CRLF line endings, and comparing
                 * against a bare "pet\n" would silently never match a
                 * "pet\r\n" value, making every pet in that file
                 * permanently un-tickable (hit this for real - egg_1's
                 * state.txt had CRLF endings after being touched cross-
                 * platform, and tick_one_pet returned immediately every
                 * time without changing or logging anything). */
                char v[8];
                snprintf(v, sizeof(v), "%s", eq + 1);
                v[strcspn(v, "\r\n")] = '\0';
                if (strcmp(v, "pet") == 0) type_is_pet = 1;
            }
            else if (strcmp(key, "hunger") == 0) { hunger = atoi(eq + 1); seen_hunger = 1; }
            else if (strcmp(key, "energy") == 0) { energy = atoi(eq + 1); seen_energy = 1; }
            else if (strcmp(key, "poop_count") == 0) { poop_count = atoi(eq + 1); seen_poop_count = 1; }
            else if (strcmp(key, "poop_timer") == 0) { poop_timer = atoi(eq + 1); seen_poop_timer = 1; }
            else if (strcmp(key, "asleep") == 0) { asleep = atoi(eq + 1); seen_asleep = 1; }
            else if (strcmp(key, "grid_x") == 0) { grid_x = atoi(eq + 1); seen_grid_x = 1; }
            else if (strcmp(key, "grid_y") == 0) { grid_y = atoi(eq + 1); seen_grid_y = 1; }
            else if (strcmp(key, "z") == 0) { z = atoi(eq + 1); seen_z = 1; }
            else if (strcmp(key, "movement") == 0) {
                char v[16];
                snprintf(v, sizeof(v), "%s", eq + 1);
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(movement, sizeof(movement), "%s", v);
                /* not marked "seen" for write-back purposes - movement is
                 * read-only here, copied verbatim like species_* fields */
            }
            else if (strcmp(key, "facing") == 0) {
                char v[8];
                snprintf(v, sizeof(v), "%s", eq + 1);
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(facing, sizeof(facing), "%s", v);
                seen_facing = 1;
            }
            else if (strcmp(key, "tick_seq") == 0) { tick_seq = atoi(eq + 1); seen_tick_seq = 1; }
            else if (strcmp(key, "last_tick_ts") == 0) { last_tick_ts = atol(eq + 1); seen_last_tick_ts = 1; }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (!type_is_pet) return;

    /* The compatibility gate: whichever of world-tick/self-tick reaches
     * this pet first in an interval wins it, the other is a no-op - see
     * this file's header comment. seen_last_tick_ts==0 means this pet
     * has never been ticked before, so it always applies immediately. */
    time_t now = time(NULL);
    if (seen_last_tick_ts && (long)now - last_tick_ts < g_tick_interval_sec) return;
    last_tick_ts = (long)now;

    if (!asleep) {
        energy = clampi(energy - 2, 0, 100);
        hunger = clampi(hunger + 3, 0, 100);

        int step_x = (rand() % 3) - 1; /* -1, 0, or 1 grid cell */
        int step_y = (rand() % 3) - 1;
        int step_z = (rand() % 3) - 1;

        if (step_x > 0) snprintf(facing, sizeof(facing), "right");
        else if (step_x < 0) snprintf(facing, sizeof(facing), "left");
        /* step_x == 0 leaves facing as whatever it already was */

        grid_x = clampi(grid_x + step_x, 0, WORLD_GRID_W - 1);
        grid_y = clampi(grid_y + step_y, 0, WORLD_GRID_H - 1);

        int zlo, zhi;
        z_bounds_for_movement(movement, &zlo, &zhi);
        z = clampi(z + step_z, zlo, zhi);
    } else {
        energy = clampi(energy + 8, 0, 100);
        hunger = clampi(hunger + 1, 0, 100);
        /* asleep: no movement at all, grid_x/grid_y/z/facing hold steady */
    }

    poop_timer += 1;
    if (poop_timer >= 12 && hunger <= 60) {
        poop_count += 1;
        poop_timer = 0;
    }

    if (energy <= 0) asleep = 1; /* collapses from exhaustion */

    tick_seq += 1;

    FILE *out = fopen(state_path, "w");
    if (!out) return;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            const char *key = lines[i];
            int handled = 1;
            if (strcmp(key, "hunger") == 0) fprintf(out, "hunger=%d\n", hunger);
            else if (strcmp(key, "energy") == 0) fprintf(out, "energy=%d\n", energy);
            else if (strcmp(key, "poop_count") == 0) fprintf(out, "poop_count=%d\n", poop_count);
            else if (strcmp(key, "poop_timer") == 0) fprintf(out, "poop_timer=%d\n", poop_timer);
            else if (strcmp(key, "asleep") == 0) fprintf(out, "asleep=%d\n", asleep);
            else if (strcmp(key, "grid_x") == 0) fprintf(out, "grid_x=%d\n", grid_x);
            else if (strcmp(key, "grid_y") == 0) fprintf(out, "grid_y=%d\n", grid_y);
            else if (strcmp(key, "z") == 0) fprintf(out, "z=%d\n", z);
            else if (strcmp(key, "facing") == 0) fprintf(out, "facing=%s\n", facing);
            else if (strcmp(key, "tick_seq") == 0) fprintf(out, "tick_seq=%d\n", tick_seq);
            else if (strcmp(key, "last_tick_ts") == 0) fprintf(out, "last_tick_ts=%ld\n", last_tick_ts);
            else handled = 0;
            *eq = '=';
            if (!handled) fputs(lines[i], out);
        } else {
            fputs(lines[i], out);
        }
    }
    if (!seen_hunger) fprintf(out, "hunger=%d\n", hunger);
    if (!seen_energy) fprintf(out, "energy=%d\n", energy);
    if (!seen_poop_count) fprintf(out, "poop_count=%d\n", poop_count);
    if (!seen_poop_timer) fprintf(out, "poop_timer=%d\n", poop_timer);
    if (!seen_asleep) fprintf(out, "asleep=%d\n", asleep);
    if (!seen_grid_x) fprintf(out, "grid_x=%d\n", grid_x);
    if (!seen_grid_y) fprintf(out, "grid_y=%d\n", grid_y);
    if (!seen_z) fprintf(out, "z=%d\n", z);
    if (!seen_facing) fprintf(out, "facing=%s\n", facing);
    if (!seen_tick_seq) fprintf(out, "tick_seq=%d\n", tick_seq);
    if (!seen_last_tick_ts) fprintf(out, "last_tick_ts=%ld\n", last_tick_ts);
    fclose(out);

    append_ledger(pet_id, hunger, energy, poop_count, asleep, grid_x, grid_y, z, facing);
}

int main(int argc, char **argv) {
    resolve_root();
    read_tick_config();
    srand(random_seed());

    if (argc >= 2) {
        /* Self-tick mode: advance just this one pet, gated only by its
         * own last_tick_ts interval - world_tick config doesn't apply
         * here, the caller (egg_window.c) already checked self_tick
         * before ever invoking this. */
        char state_path[PATH_BUF + 32];
        snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, argv[1]);
        tick_one_pet(argv[1], state_path);
        return 0;
    }

    /* World-tick mode: sweep every pet, but only if enabled - a no-op
     * scan otherwise so pal/main_loop.pal doesn't need its own
     * conditional to skip calling this. */
    if (!g_world_tick_enabled) return 0;

    char map_path[PATH_BUF];
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_01/map_lobby", project_root);

    DIR *d = opendir(map_path);
    if (!d) return 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        /* d_name is genuinely a short piece id despite gcc only being able
         * to statically prove it no longer than NAME_MAX - same class of
         * warning already suppressed narrowly in menu_input.c/
         * generate_egg.c rather than widening state_path to match. */
        char state_path[PATH_BUF + 32];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", map_path, ent->d_name);
#pragma GCC diagnostic pop

        struct stat st;
        if (stat(state_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        tick_one_pet(ent->d_name, state_path);
    }
    closedir(d);
    return 0;
}
