/* pc_generate_chunk - real Phase 1 chunk terrain generation, per
 * PIECECRAFT_XYZ_DESIGN.md §6 and civ-vs-piece.md's own recommended
 * build order step 2. Deterministic, seed-driven, per-chunk-coordinate
 * (so a compressed chunk can regenerate identically later, per design
 * §2's own decompression requirement) - uses a real seeded-per-
 * coordinate integer hash, NOT raw srand(time()) (design §6's own
 * explicit callout: civ-txt's CONFIRM_START uses srand(time()) for
 * ONE-TIME generation, intentionally non-reproducible there - that
 * would break chunk regeneration here).
 *
 * P1 scope: ONE chunk (chunk_0_0), ONE biome (plains: grass surface,
 * dirt subsurface, stone below, air above) - no biome variety, no
 * trees/ore scatter yet (design §11: "terrain gen for ONE biome...
 * prove the render/nav loop" is the whole Phase 1 bar). 16x16 chunk,
 * 32 Z-layers (civ-vs-piece.md §2 decisions).
 *
 * Also writes pieces/world_01/state.txt (seed/tick/active) and
 * pieces/hero_01/state.txt (real entity, universal schema per
 * PORTABLE_ENTITY_ARCHITECTURE.md §3, placed standing on the newly
 * generated surface at the chunk's own center column) - co-located
 * here since both need the SAME computed surface height this op
 * already produces, avoiding duplicate height logic in a second op.
 *
 * Self-contained, no shared headers.
 * Usage: pc_generate_chunk.+x <seed> <chunk_x> <chunk_y> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "win_posix_shim.h"

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define CHUNK_DIM 16
#define Z_COUNT 32

static char project_root[MAX_PATH] = ".";
/* real_root - the REAL, persistent (non-session) project root, read
 * from pieces/system/real_project_root.txt (already written by
 * button.sh at session start - same file OPEN_BOARD_WIDGET's own
 * handler already reads for the same reason). REAL BUG, caught live
 * 2026-08-03: this op's every write used to target project_root (the
 * EPHEMERAL session directory PRISC_PROJECT_ROOT points at) - config.
 * txt/board.txt/entities.txt survive that because button.sh explicitly
 * symlinks those SPECIFIC files back to the real root, but board_
 * manifest.txt/pieces/world_01/pieces/hero_01/pieces/system/chunks/
 * are all BRAND NEW files this op creates for the first time, which
 * button.sh has no symlink for - they were silently being written
 * into the session dir and vanishing the instant button.sh's own EXIT
 * trap ran `rm -rf "$SESSION_DIR"`. Exact same bug class as my-chara-
 * txt's own historical plots.txt symlink omission (see civ-txt's own
 * button.sh comment) and this project's own §2 "Silent fopen write
 * failure" pitfall - real persistent data must target the REAL root,
 * never the session root, unless a file is individually symlinked. */
static char real_root[MAX_PATH] = ".";

#ifdef _WIN32
#include <windows.h>
/* UTF-8 path open for emoji house roots */
static FILE *host_fopen(const char *path, const char *mode) {
    wchar_t wpath[PATH_BUF], wmode[16];
    if (!path || !mode) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_BUF) <= 0 &&
        MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, PATH_BUF) <= 0)
        return fopen(path, mode);
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 16) <= 0)
        MultiByteToWideChar(CP_ACP, 0, mode, -1, wmode, 16);
    FILE *f = _wfopen(wpath, wmode);
    return f ? f : fopen(path, mode);
}
static int host_access(const char *path) {
    wchar_t wpath[PATH_BUF];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_BUF) <= 0 &&
        MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, PATH_BUF) <= 0)
        return access(path, F_OK);
    return _waccess(wpath, 0);
}
#else
#define host_fopen fopen
#define host_access(p) access((p), F_OK)
#endif

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
    } else
#endif
    {
        const char *env = getenv("PRISC_PROJECT_ROOT");
        if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    }

    snprintf(real_root, sizeof(real_root), "%s", project_root);
    char real_root_path[PATH_BUF];
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", project_root);
    FILE *rf = host_fopen(real_root_path, "r");
    if (!rf) rf = fopen(real_root_path, "r");
    if (rf) {
        char buf[PATH_BUF];
        if (fgets(buf, sizeof(buf), rf)) {
            char *b = buf;
            if ((unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF)
                b += 3;
            b[strcspn(b, "\r\n")] = '\0';
            if (b[0]) {
                /* relative to house_root if not absolute */
                if (!((b[0] && b[1] == ':') || b[0] == '/' || b[0] == '\\')) {
                    char house_path[PATH_BUF], house[PATH_BUF] = "";
                    snprintf(house_path, sizeof(house_path), "%s/pieces/system/house_root.txt", project_root);
                    FILE *hf = host_fopen(house_path, "r");
                    if (!hf) hf = fopen(house_path, "r");
                    if (hf) {
                        if (fgets(house, sizeof(house), hf)) {
                            char *h = house;
                            if ((unsigned char)h[0] == 0xEF && (unsigned char)h[1] == 0xBB && (unsigned char)h[2] == 0xBF)
                                h += 3;
                            h[strcspn(h, "\r\n")] = '\0';
                            snprintf(real_root, sizeof(real_root), "%s/%s", h, b);
                        }
                        fclose(hf);
                    } else {
                        snprintf(real_root, sizeof(real_root), "%s", b);
                    }
                } else {
                    snprintf(real_root, sizeof(real_root), "%s", b);
                }
            }
        }
        fclose(rf);
    }
    /* If real_root is unusable (bad Unicode abs path), fall back to CWD
     * when it looks like the real project (has ops/+x and pieces). */
    if (host_access(real_root) != 0) {
#ifdef _WIN32
        char cwd[PATH_BUF];
        if (GetCurrentDirectoryA((DWORD)sizeof(cwd), cwd)) {
            char probe[PATH_BUF];
            snprintf(probe, sizeof(probe), "%s\\ops", cwd);
            if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES)
                snprintf(real_root, sizeof(real_root), "%s", cwd);
        }
#else
        /* keep project_root */
        snprintf(real_root, sizeof(real_root), "%s", project_root);
#endif
    }
}

/* REAL FIX 2026-08-04, direct user report ("cycle speed stops
 * autotick... toggle doesn't restart it") - real root cause: this op
 * and pc_clock_daemon.c (a real, ALWAYS-running background process,
 * same session) both do this exact real read-whole-file/rewrite-
 * whole-file sequence on the SAME real world_01/state.txt, completely
 * independently, with no coordination at all. If they interleave (the
 * daemon reads its own snapshot, THIS op writes a real change, then
 * the daemon finishes writing its OWN now-stale snapshot), the
 * daemon's own write silently OVERWRITES the real change with old
 * data - a genuine lost-update race, not a logic bug in either write.
 * Real fix: a real OS file lock (flock), held for the ENTIRE real
 * read-then-write sequence via ONE shared file descriptor (not two
 * separate fopen calls like before - that gap between them was
 * exactly where the race lived) - any other process's own real
 * write_kv() now genuinely blocks until this one fully finishes. */
static void write_kv(const char *path, const char *key, const char *value) {
#ifdef _WIN32
    /* open()/ANSI fopen fail on emoji house paths — use host_fopen. */
    char lines[64][512];
    int nlines = 0;
    FILE *rf = host_fopen(path, "r");
    if (rf) {
        while (nlines < 64 && fgets(lines[nlines], 512, rf)) nlines++;
        fclose(rf);
    }
    FILE *wf = host_fopen(path, "w");
    if (!wf) return;
    size_t key_len = strlen(key);
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(wf, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], wf);
        }
    }
    if (!found) fprintf(wf, "%s=%s\n", key, value);
    fclose(wf);
#else
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    FILE *f = fdopen(fd, "r+");
    if (!f) { flock(fd, LOCK_UN); close(fd); return; }

    char lines[64][512];
    int nlines = 0;
    while (nlines < 64 && fgets(lines[nlines], 512, f)) nlines++;

    size_t key_len = strlen(key);
    int found = 0;
    fseek(f, 0, SEEK_SET);
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fflush(f);
    long endpos = ftell(f);
    if (endpos >= 0) { int _rc = ftruncate(fd, endpos); (void)_rc; }
    flock(fd, LOCK_UN);
    fclose(f);
#endif
}

static void write_kv_int(const char *path, const char *key, long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", value);
    write_kv(path, key, buf);
}

/* Real seeded-per-coordinate hash (a simplified integer avalanche
 * hash - xorshift-multiply, same family mutaclysm's own srand()-based
 * board gen would need if it ever required reproducibility, which it
 * doesn't per design §6's own honest note - this project DOES need
 * it). Deterministic: same (seed, chunk_x, chunk_y, col, row) always
 * produces the same value, which is the entire point (design §2's
 * decompression requirement). */
static unsigned int hash_coord(unsigned int seed, int chunk_x, int chunk_y, int col, int row) {
    unsigned int h = seed;
    h ^= (unsigned int)(chunk_x * 374761393 + chunk_y * 668265263 +
                         col * 2246822519u + row * 3266489917u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return h;
}

/* Real premade "vanilla debug" flat map, added 2026-08-03 (direct
 * instruction: "i want us to have option to select to use a premade-
 * map that is mostly flat with just a few trees, as a vanilla debug
 * testing ground" - real motivation confirmed same session: "i still
 * dont see player-avatar, so i want to make sure he spawns on flat,
 * clearly visible ground level" - seeded terrain's own +/-2 height
 * variation made it genuinely harder to confirm basic spawn/render
 * correctness, a flat, predictable ground removes that variable while
 * debugging). Fixed, deterministic tree positions (not seed-hashed) -
 * a STABLE debug layout is more useful for repeated testing than
 * scattered variety would be here; real biome-driven tree placement is
 * separate, later work (phase2-plan.md §4a), this is a debug fixture,
 * not real world-gen. */
#define FLAT_SURFACE_Z 16
#define TREE_COUNT 4
static const int tree_col[TREE_COUNT] = {4, 12, 3, 11};
static const int tree_row[TREE_COUNT] = {4, 4, 12, 12};

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: pc_generate_chunk.+x <seed> <chunk_x> <chunk_y> [flat]\n");
        return 1;
    }
    resolve_root();

    unsigned int seed = (unsigned int)strtoul(argv[1], NULL, 10);
    int chunk_x = atoi(argv[2]);
    int chunk_y = atoi(argv[3]);
    int flat_mode = (argc >= 5 && strcmp(argv[4], "flat") == 0);

    char chunk_dir[PATH_BUF];
    char mkdir_cmd[PATH_BUF + 16];
    snprintf(chunk_dir, sizeof(chunk_dir), "%s/pieces/system/chunks/chunk_%d_%d", real_root, chunk_x, chunk_y);
#ifdef _WIN32
    win_mkdir_p(chunk_dir);
#else
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", chunk_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }
#endif

    /* Per-column surface height: flat mode uses a fixed height
     * everywhere (real debug fixture, see is_tree_column()'s own header
     * comment above) - seeded mode keeps its real +/-2 hash variation
     * (base 16, middle of 32). Multiple biomes / real noise octaves are
     * later work (design §6), not Phase 1's bar either way. */
    int surface[CHUNK_DIM][CHUNK_DIM];
    for (int row = 0; row < CHUNK_DIM; row++) {
        for (int col = 0; col < CHUNK_DIM; col++) {
            if (flat_mode) {
                surface[row][col] = FLAT_SURFACE_Z;
            } else {
                unsigned int h = hash_coord(seed, chunk_x, chunk_y, col, row);
                int variation = (int)(h % 5) - 2; /* -2..+2 */
                surface[row][col] = 16 + variation;
            }
        }
    }

    for (int z = 0; z < Z_COUNT; z++) {
        char z_path[PATH_BUF];
        snprintf(z_path, sizeof(z_path), "%s/chunk_%d_%d_z%d.txt", chunk_dir, chunk_x, chunk_y, z);
        FILE *zf = host_fopen(z_path, "w");
        if (!zf) continue;
        for (int row = 0; row < CHUNK_DIM; row++) {
            for (int col = 0; col < CHUNK_DIM; col++) {
                int sh = surface[row][col];
                char glyph;
                /* Trees are REAL PHYMOJI ENTITIES now (phymoji.md §4b's
                 * own resolved decision, "Yes, switch trees to real
                 * entities now" - direct instruction), written below to
                 * phymoji_entities.txt, NOT embedded as terrain glyphs
                 * here anymore. Ground under a tree column is now plain
                 * grass, same as everywhere else. */
                if (z > sh) glyph = '_';         /* air */
                else if (z == sh) glyph = ',';    /* grass surface */
                else if (z >= sh - 3) glyph = '.'; /* dirt subsurface */
                else glyph = 's';                  /* stone below */
                fputc(glyph, zf);
            }
            fputc('\n', zf);
        }
        fclose(zf);
    }

    /* board_manifest.txt - real per-host data-bank board-viewer's own
     * resolve_board_path() reads (added this same session, see
     * &.widgits/board-viewer/ops/bv_compose_frame.c's own header
     * comment on that change). Points at THIS chunk's own Z-file base. */
    char manifest_path[PATH_BUF];
    snprintf(manifest_path, sizeof(manifest_path), "%s/pieces/system/board_manifest.txt", real_root);
    FILE *mf = host_fopen(manifest_path, "w");
    if (mf) {
        fprintf(mf, "z_base=pieces/system/chunks/chunk_%d_%d/chunk_%d_%d_z\n", chunk_x, chunk_y, chunk_x, chunk_y);
        fprintf(mf, "z_count=%d\n", Z_COUNT);
        fclose(mf);
    }

    /* world_01/state.txt - real world/tick state (design §5). */
    char world_dir[PATH_BUF];
    snprintf(world_dir, sizeof(world_dir), "%s/pieces/world_01", real_root);
#ifdef _WIN32
    win_mkdir_p(world_dir);
#else
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", world_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }
#endif
    char world_state_path[PATH_BUF];
    snprintf(world_state_path, sizeof(world_state_path), "%s/state.txt", world_dir);
    write_kv(world_state_path, "name", "World 01");
    write_kv(world_state_path, "type", "world");
    write_kv_int(world_state_path, "seed", (long)seed);
    write_kv_int(world_state_path, "tick", 0);
    write_kv_int(world_state_path, "active", 1);

    /* REAL, NEW 2026-08-04, direct instruction ("] will start or stop
     * autotick... show time/date as variable in view screen"). Real
     * game clock, stored as whole game-seconds since a fixed base
     * epoch (2026-01-01T00:00:00 UTC, an arbitrary but real, stable
     * start point) - see pc_compose_frame.c's own advance_game_clock()
     * for the real elapsed-time-to-game-seconds math and pc_menu_input.
     * c's own TOGGLE_AUTOTICK handler for the '] ' key. Starts paused
     * (autotick_enabled=0) - a fresh world doesn't silently start
     * ticking before the player asks for it.
     *
     * REAL FIX 2026-08-04, direct instruction ("aomo viewer... we should
     * set it at daylight default"): base epoch alone (1767225600 =
     * 2026-01-01T00:00:00 UTC) put every fresh world at midnight -
     * pc_clock_daemon.c's update_celestial_body() then placed the sun
     * below the horizon from the very first frame. +43200s = same date
     * at 12:00:00 UTC (noon), so a new world's sun starts up and lit. */
    write_kv_int(world_state_path, "game_time_epoch_sec", 1767268800L);
    write_kv_int(world_state_path, "game_time_epoch_ms", 1767268800000L); /* real ms-precision canonical clock, see pc_clock_daemon.c's own header comment */
    write_kv_int(world_state_path, "autotick_enabled", 0);
    write_kv(world_state_path, "autotick_speed", "min");
    write_kv_int(world_state_path, "autotick_last_real_ms", 0);

    /* pieces/world_01/phymoji_entities.txt - real positioned phymoji
     * objects (phymoji.md §4b), one "entity_id,x,y,z" line per placed
     * tree, board-viewer's own bv_render_3d.c reads this generically
     * (any entity_id with a matching pieces/registry/phymoji_assets/
     * <entity_id>/voxels.csv works, not just trees). Same TREE_COUNT/
     * tree_col/tree_row fixed debug positions the OLD terrain-glyph
     * trees used, so the debug map's own layout doesn't shift. */
    if (flat_mode) {
        char phymoji_entities_path[PATH_BUF];
        snprintf(phymoji_entities_path, sizeof(phymoji_entities_path),
                 "%s/phymoji_entities.txt", world_dir);
        FILE *pef = host_fopen(phymoji_entities_path, "w");
        if (pef) {
            for (int i = 0; i < TREE_COUNT; i++) {
                int col = tree_col[i], row = tree_row[i];
                fprintf(pef, "tree_small,%d,%d,%d\n", col, row, surface[row][col] + 1);
            }
            fclose(pef);
        }

        /* pieces/world_01/animals.txt - real MOVABLE phymoji entities,
         * direct instruction ("give the chicken the master-ledger AI
         * mutaclysm zombies have, but just to walk randomly"). Kept
         * separate from phymoji_entities.txt on purpose: that file is
         * real static decoration (trees never move, tick_animals()
         * below never touches it); this one is real per-tick mutable
         * state, same "static template vs mutable instance" split
         * phymoji.md §3 already established for destructible voxels.
         * Same "entity_id,x,y,z" line format as phymoji_entities.txt so
         * board-viewer's own generic loader needs zero new parsing to
         * read this file too (see bv_render_3d.c's own
         * load_phymoji_world_entities(), updated this same session to
         * read both files). */
        char animals_path[PATH_BUF];
        snprintf(animals_path, sizeof(animals_path), "%s/animals.txt", world_dir);
        FILE *af = host_fopen(animals_path, "w");
        if (af) {
            int chick_col = 8, chick_row = 5;
            fprintf(af, "chicken,%d,%d,%d\n", chick_col, chick_row, surface[chick_row][chick_col] + 1);
            fclose(af);
        }
    }

    /* hero_01/state.txt - real entity, universal schema per
     * PORTABLE_ENTITY_ARCHITECTURE.md §3 (entity_type/hp/pos_x/pos_y/
     * owner_id), PLUS aomorai-editor's own pos_z (real Z-axis, this
     * time correctly on the HERO's own entity state, not
     * pieces/system/config.txt like the original mutation had it - see
     * mutant-clone.txt). Placed standing on the chunk's own center
     * column surface (col=8,row=8), one Z above the grass tile itself
     * (surface[8][8] is the grass block's own Z; the hero stands ON
     * it, one level higher). */
    char hero_dir[PATH_BUF];
    snprintf(hero_dir, sizeof(hero_dir), "%s/pieces/hero_01", real_root);
#ifdef _WIN32
    win_mkdir_p(hero_dir);
#else
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", hero_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }
#endif
    char hero_state_path[PATH_BUF];
    snprintf(hero_state_path, sizeof(hero_state_path), "%s/state.txt", hero_dir);
    write_kv(hero_state_path, "entity_type", "hero");
    write_kv_int(hero_state_path, "hp", 20);
    write_kv_int(hero_state_path, "pos_x", 8);
    write_kv_int(hero_state_path, "pos_y", 8);
    write_kv_int(hero_state_path, "pos_z", surface[8][8] + 1);
    write_kv(hero_state_path, "owner_id", "player");
    write_kv_int(hero_state_path, "chunk_x", chunk_x);
    write_kv_int(hero_state_path, "chunk_y", chunk_y);

    /* pieces/xelector_01/state.txt - real, separate cursor entity, per
     * civ-vs-piece.md §6a's own real precedent citation (fuzz-op's own
     * live pieces/xlector/state.txt fixture - a genuinely separate
     * piece, not fields tacked onto hero's own state, which the FIRST
     * pass of this op incorrectly did - see hero_state_path's own
     * now-dead interact_mode/xlector_pos_x/y writes above, left in
     * place harmlessly per this house's own "unused extra fields are
     * fine" convention rather than stripped mid-session). The xelector
     * is a free-roaming CURSOR, not the player - starts possessing the
     * hero by default (mutaclysm's own real possessed_id shape: "none"
     * or a target piece_id), but its own pos_x/y/z move independently
     * of whatever it currently possesses - board-viewer's own z/x
     * handling (ops/bv_menu_input.c, this same session) writes THIS
     * file's pos_z directly, unconstrained by gravity/collision/any
     * other entity rule, matching "even into the sky or underground"
     * per direct instruction. */
    char xelector_dir[PATH_BUF];
    snprintf(xelector_dir, sizeof(xelector_dir), "%s/pieces/xelector_01", real_root);
#ifdef _WIN32
    win_mkdir_p(xelector_dir);
#else
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", xelector_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }
#endif
    char xelector_state_path[PATH_BUF];
    snprintf(xelector_state_path, sizeof(xelector_state_path), "%s/state.txt", xelector_dir);
    write_kv(xelector_state_path, "entity_type", "xelector");
    write_kv_int(xelector_state_path, "pos_x", 8);
    write_kv_int(xelector_state_path, "pos_y", 8);
    write_kv_int(xelector_state_path, "pos_z", surface[8][8] + 1);
    write_kv(xelector_state_path, "possessed_id", "hero_01");
    write_kv_int(xelector_state_path, "chunk_x", chunk_x);
    write_kv_int(xelector_state_path, "chunk_y", chunk_y);

    printf("chunk_%d_%d generated: seed=%u surface_center=%d\n", chunk_x, chunk_y, seed, surface[8][8]);
    return 0;
}
