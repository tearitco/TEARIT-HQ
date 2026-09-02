/* pc_compose_frame - renders whichever mutaclysm screen is current into
 * pieces/apps/player_app/view.txt (the file chtpm's own ${game_map}
 * placeholder substitutes verbatim). Modeled directly on
 * @.apps/civ-txt's own ops/civ_compose_frame.c: writes ONLY
 * view.txt, never pieces/display/current_frame.txt directly (ONE
 * WRITER RULE - that's chtpm_parser_pal.c's own exclusive job), then
 * bumps pieces/display/frame_changed.txt.
 *
 * P1 scope (CLONE ONLY): new_game (setup) + main (turn counter) screens only.
 *
 * Self-contained, no shared headers.
 * Usage: pc_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#ifdef _WIN32
/* MinGW has sys/time.h; keep CLOCK if present */
#include <sys/time.h>
#else
#include <sys/time.h>
#endif
#include "win_posix_shim.h"

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* REAL FIX 2026-08-04, direct user report ("tick didn't change time...
 * autotick isn't moving time forward"): world_01/state.txt and
 * hero_01/state.txt reads below used to go through the raw EPHEMERAL
 * session root - wrong for a session that launched BEFORE Confirm &
 * Start ever generated those files (mua_generate_chunk.c's own real
 * world-gen write already correctly uses real_root) - real "symlink
 * omission" mismatch, same real fix now applied on the pc_menu_input.c
 * side too (advance_tick()/ledger_append()/TOGGLE_AUTOTICK/
 * CYCLE_TICK_SPEED, same session). */
static void resolve_real_root(const char *proj_root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", proj_root);
    char real_root_path[PATH_BUF];
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", proj_root);
    FILE *rf = fopen(real_root_path, "r");
    if (rf) {
        char buf[PATH_BUF];
        if (fgets(buf, sizeof(buf), rf)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            if (buf[0]) snprintf(out, out_sz, "%s", buf);
        }
        fclose(rf);
    }
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            val = atoi(line + key_len + 1);
        }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(l, sizeof(l), f)) {
        if (strncmp(l, key, key_len) == 0 && l[key_len] == '=') {
            char *v = l + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "new_game");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
    FILE *f = fopen(layout_path, "r");
    if (!f) return;
    char line1[MAX_LINE];
    if (fgets(line1, sizeof(line1), f)) {
        line1[strcspn(line1, "\r\n")] = '\0';
        const char *slash = strrchr(line1, '/');
        const char *base = slash ? slash + 1 : line1;
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", base);
        char *dot = strstr(tmp, ".chtpm");
        if (dot) *dot = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
#pragma GCC diagnostic pop
    }
    fclose(f);
}

/* Forward declaration - real definition is later in this file (used
 * by several real helpers added below, defined before it structurally
 * for historical/unrelated reasons). */
static void write_kv(const char *path, const char *key, const char *value);

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static long long read_kv_ll(const char *path, const char *key, long long def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoll(buf) : def;
}

static void write_kv_ll(const char *path, const char *key, long long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", value);
    write_kv(path, key, buf);
}

/* REMOVED 2026-08-04: ledger_append_local()/tick_animals_local() used
 * to live here for the OLD reactive-advancement model - real chicken-
 * wander ticking is now exclusively mua_clock_daemon.c's own real job
 * (see read_game_clock()'s own header comment), this file no longer
 * advances anything, only reads/displays. */

/* REVISED 2026-08-04, direct instruction (real house precedent found:
 * #.ref/Mar$.$treetRace]Q]k32]4K/wsr_clock.c) - clock ADVANCEMENT is
 * no longer done here. This function used to advance the clock
 * reactively (only on a real render), which meant a session with no
 * player input had a genuinely frozen clock no matter how much real
 * time passed - the real house precedent for this exact problem is a
 * true PERSISTENT background daemon (ops/mua_clock_daemon.c, direct
 * port of wsr_clock.c's own real continuous-loop model), launched once
 * per world at CONFIRM_START/CONFIRM_START_DEBUG (pc_menu_input.c).
 * This function's real remaining job is READ-ONLY - just report
 * whatever the daemon has already written, so the "main" screen never
 * double-advances the same clock two different ways. */
/* REVISED 2026-08-04, direct user request ("add ms to the time output
 * display") - now returns real MILLISECOND precision (game_time_epoch_
 * ms), matching mua_clock_daemon.c's own real canonical clock field
 * (same session's own fix for the real "slow speeds lose all
 * fractional progress" bug that report surfaced). */
static long long read_game_clock_ms(const char *proj_root) {
    char root[PATH_BUF];
    resolve_real_root(proj_root, root, sizeof(root));
    char world_state_path[PATH_BUF];
    snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", root);
    long long ms = read_kv_ll(world_state_path, "game_time_epoch_ms", 0);
    if (ms == 0) {
        /* Real, one-time fallback for a world generated before this
         * field existed - derive from the old whole-seconds field. */
        ms = read_kv_ll(world_state_path, "game_time_epoch_sec", 0) * 1000LL;
    }
    return ms;
}

static FILE *g_view_out = NULL;

static int utf8_char_len(const char *s) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static void border(void) {
    if (g_view_out) { fputc('+', g_view_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out); fputc('+', g_view_out); fputc('\n', g_view_out); }
}
static void line(const char *content) {
    if (!g_view_out) return;
    fputc('|', g_view_out);
    int disp_w = 0;
    const char *p = content;
    while (*p && disp_w < BOX_W) {
        int clen = utf8_char_len(p);
        fwrite(p, 1, clen, g_view_out);
        p += clen;
        disp_w++;
    }
    for (int i = disp_w; i < BOX_W; i++) fputc(' ', g_view_out);
    fputc('|', g_view_out);
    fputc('\n', g_view_out);
}
static void blank(void) { line(""); }

/* REAL FIX 2026-08-04, direct user report ("cycle speed stops
 * autotick... toggle doesn't restart it") - real root cause: this op
 * and mua_clock_daemon.c (a real, ALWAYS-running background process,
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
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    FILE *f = fdopen(fd, "r+");
    if (!f) { flock(fd, LOCK_UN); close(fd); return; }

    char lines[64][MAX_LINE];
    int nlines = 0;
    while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;

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
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/mutaclysm/pieces/mua_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "  A T L A S - E D I T O R   [%s]", active_piece);
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "new_game") == 0) {
        /* REPLACED 2026-08-03, real Phase 1 divergence per
         * civ-vs-piece.md §2: civ-txt's own Victory/Map/Combat setup
         * options are gone - a voxel sandbox has no such concepts. Seed
         * is auto-random on Confirm & Start (civ-vs-piece.md's own
         * decision - manual seed entry needs the house's real cli_io
         * text-input mechanic, flagged as separate later work).
         *
         * UPDATED 2026-08-03, real second option added (phase2-plan.md,
         * direct instruction: "premade-map that is mostly flat with
         * just a few trees, as a vanilla debug testing ground" - real
         * motivation, same session: "i still dont see player-avatar,
         * so i want to make sure he spawns on flat, clearly visible
         * ground level"). Two real, selectable world types now, not
         * hidden flags or env vars. */
        line("Choose a world type, then Confirm & Start.");
        blank();
        line("Seeded World: real 16x16x32 chunk, +/-2 height variation.");
        line("Debug Flat World: flat ground, 4 trees, easy to verify.");

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/mutaclysm/pieces/new_game/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | new_game\n\n");
            fprintf(pdl_out, "METHOD       | Confirm & Start (Seeded World)      | CONFIRM_START\n");
            fprintf(pdl_out, "METHOD       | Confirm & Start (Debug Flat World)  | CONFIRM_START_DEBUG\n");
            fclose(pdl_out);
        }
    } else if (strcmp(active_piece, "main") == 0) {
        /* REPLACED 2026-08-03, real Phase 2 divergence per phase2-
         * plan.md §6 step 1: civ-txt's own Turn/Treasury/Cities/
         * Victory readout is gone - a voxel sandbox has none of those
         * concepts. Real world/hero state instead: tick (design §5 -
         * pieces/world_01/state.txt's own real counter, NOT config.
         * txt's leftover "turn" field, which the clone-phase mutation
         * had wrongly conflated with world state once already - see
         * mutant-clone.txt), hero position + current chunk (pieces/
         * hero_01/state.txt). */
        char real_root_main[PATH_BUF];
        resolve_real_root(project_root, real_root_main, sizeof(real_root_main));
        char world_state_path[PATH_BUF];
        snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", real_root_main);
        int tick = read_kv_int(world_state_path, "tick", 0);

        char hero_state_path[PATH_BUF];
        snprintf(hero_state_path, sizeof(hero_state_path), "%s/pieces/hero_01/state.txt", real_root_main);
        int hero_x = read_kv_int(hero_state_path, "pos_x", 0);
        int hero_y = read_kv_int(hero_state_path, "pos_y", 0);
        int hero_z = read_kv_int(hero_state_path, "pos_z", 0);
        int hero_hp = read_kv_int(hero_state_path, "hp", 0);
        int hero_hunger = read_kv_int(hero_state_path, "hunger", 0);
        int hero_thirst = read_kv_int(hero_state_path, "thirst", 0);
        int hero_stamina = read_kv_int(hero_state_path, "stamina", 100);
        int chunk_x = read_kv_int(hero_state_path, "chunk_x", 0);
        int chunk_y = read_kv_int(hero_state_path, "chunk_y", 0);

        snprintf(rowbuf, sizeof(rowbuf), "  Tick: %d", tick);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Hero HP: %d   Pos: (%d,%d,%d)", hero_hp, hero_x, hero_y, hero_z);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Hunger: %d   Thirst: %d   Stamina: %d", hero_hunger, hero_thirst, hero_stamina);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Chunk: (%d,%d)", chunk_x, chunk_y);
        line(rowbuf);

        /* Sprint 4: show nearby monsters */
        {
            char monsters_dir[PATH_BUF];
            snprintf(monsters_dir, sizeof(monsters_dir), "%s/pieces/world_01/monsters", real_root_main);
            DIR *md = opendir(monsters_dir);
            if (md) {
                struct dirent *mentry;
                char nearby_buf[MAX_LINE];
                int nb_off = 0;
                int nb_first = 1;
                while ((mentry = readdir(md)) != NULL) {
                    if (mentry->d_name[0] == '.') continue;
                    char mstate[PATH_BUF + 384];
                    snprintf(mstate, sizeof(mstate), "%s/%s/state.txt", monsters_dir, mentry->d_name);
                    int mmx = read_kv_int(mstate, "pos_x", -99);
                    int mmy = read_kv_int(mstate, "pos_y", -99);
                    int mmhp = read_kv_int(mstate, "hp", 0);
                    if (mmhp <= 0) continue;
                    int dx = mmx - hero_x; if (dx < 0) dx = -dx;
                    int dy = mmy - hero_y; if (dy < 0) dy = -dy;
                    if (dx > 4 || dy > 4) continue;
                    char mtype[64];
                    read_kv_str(mstate, "monster_type", mtype, sizeof(mtype));
                    if (nb_first) { snprintf(nearby_buf + nb_off, sizeof(nearby_buf) - nb_off, "  Nearby: "); nb_off += 10; nb_first = 0; }
                    else { snprintf(nearby_buf + nb_off, sizeof(nearby_buf) - nb_off, ", "); nb_off += 2; }
                    snprintf(nearby_buf + nb_off, sizeof(nearby_buf) - nb_off, "%s(%dhp)", mtype, mmhp);
                    nb_off += (int)strlen(nearby_buf + nb_off);
                }
                closedir(md);
                if (!nb_first) line(nearby_buf);
            }
        }

        /* REAL, NEW 2026-08-04, direct instruction ("] will start or
         * stop autotick... show time/date as variable in view screen").
         * advance_game_clock() is a real, honest no-op (returns the
         * stored value unchanged) when autotick is off - this call is
         * safe/cheap every real frame regardless of autotick state. */
        long long game_epoch_ms = read_game_clock_ms(project_root);
        time_t game_time = (time_t)(game_epoch_ms / 1000);
        int game_ms_part = (int)(game_epoch_ms % 1000);
        char game_datetime_str[40];
        char game_datetime_base[32];
        strftime(game_datetime_base, sizeof(game_datetime_base), "%Y-%m-%dT%H:%M:%S", gmtime(&game_time));
        snprintf(game_datetime_str, sizeof(game_datetime_str), "%s.%03d", game_datetime_base, game_ms_part);
        int autotick_on = read_kv_int(world_state_path, "autotick_enabled", 0);
        char autotick_speed_str[16] = "min";
        read_kv_str(world_state_path, "autotick_speed", autotick_speed_str, sizeof(autotick_speed_str));
        snprintf(rowbuf, sizeof(rowbuf), "  Date: %s   Autotick: %s (%s)  [ ] to toggle ]",
                 game_datetime_str, autotick_on ? "ON" : "off", autotick_speed_str);
        line(rowbuf);
        blank();

        /* === EMOJI 2D MAP RENDERING (Mode 0) ===
         * Reads the chunk z-layer file for the hero's current z-level
         * and renders a 16x16 emoji grid. This is the "leveled up
         * mutaclysm" proof - same map as old mutaclysm but with
         * piececraft-compatible z-level data. */
        {
            /* Read render_mode and interact_mode from hero state */
            int render_mode = read_kv_int(hero_state_path, "render_mode", 0);
            int interact_mode = read_kv_int(hero_state_path, "interact_mode", 0);
            int cursor_x = read_kv_int(hero_state_path, "cursor_x", hero_x);
            int cursor_y = read_kv_int(hero_state_path, "cursor_y", hero_y);

            /* Load the z-layer chunk file */
            char chunk_path[PATH_BUF];
            snprintf(chunk_path, sizeof(chunk_path),
                     "%s/pieces/system/chunks/chunk_%d_%d/chunk_%d_%d_z%d.txt",
                     real_root_main, chunk_x, chunk_y, chunk_x, chunk_y, hero_z);
            FILE *cf = fopen(chunk_path, "r");
            if (cf) {
                char board[16][17]; /* 16 rows + null */
                int brow = 0;
                while (brow < 16 && fgets(board[brow], sizeof(board[brow]), cf))
                    brow++;
                fclose(cf);

                /* Terrain glyph -> emoji mapping */
                #define TERRAIN_AIR     "🌿"
                #define TERRAIN_SOLID   "🧱"
                #define TERRAIN_ROCK    "🪨"
                #define TERRAIN_SAND    "🟫"
                #define TERRAIN_TREE    "🌳"
                #define TERRAIN_WATER   "💧"
                #define TERRAIN_HERO    "🧙"
                #define TERRAIN_MONSTER "💀"
                #define TERRAIN_ITEM    "✨"
                #define TERRAIN_CURSOR  "◆"

                const char *glyph_emoji(char g) {
                    switch (g) {
                        case '.': return TERRAIN_AIR;
                        case '_': return TERRAIN_SOLID;
                        case ',': return TERRAIN_ROCK;
                        case 's': return TERRAIN_SAND;
                        case 't': return TERRAIN_TREE;
                        case '~': return TERRAIN_WATER;
                        default:  return TERRAIN_AIR;
                    }
                }

                /* Build a lookup for monsters and items at positions */
                int mon_exists[16][16];
                memset(mon_exists, 0, sizeof(mon_exists));
                {
                    char monsters_dir[PATH_BUF];
                    snprintf(monsters_dir, sizeof(monsters_dir), "%s/pieces/world_01/monsters", real_root_main);
                    DIR *md = opendir(monsters_dir);
                    if (md) {
                        struct dirent *me;
                        while ((me = readdir(md)) != NULL) {
                            if (me->d_name[0] == '.') continue;
                            char ms[PATH_BUF + 256];
                            snprintf(ms, sizeof(ms), "%s/%s/state.txt", monsters_dir, me->d_name);
                            int mx = read_kv_int(ms, "pos_x", -1);
                            int my = read_kv_int(ms, "pos_y", -1);
                            int mz = read_kv_int(ms, "pos_z", -1);
                            int mhp = read_kv_int(ms, "hp", 0);
                            if (mx >= 0 && mx < 16 && my >= 0 && my < 16 && mz == hero_z && mhp > 0)
                                mon_exists[my][mx] = 1;
                        }
                        closedir(md);
                    }
                }

                int item_exists[16][16];
                memset(item_exists, 0, sizeof(item_exists));
                {
                    char items_dir[PATH_BUF];
                    snprintf(items_dir, sizeof(items_dir), "%s/pieces/world_01/items", real_root_main);
                    DIR *id = opendir(items_dir);
                    if (id) {
                        struct dirent *ie;
                        while ((ie = readdir(id)) != NULL) {
                            if (ie->d_name[0] == '.') continue;
                            char is[PATH_BUF + 256];
                            snprintf(is, sizeof(is), "%s/%s/state.txt", items_dir, ie->d_name);
                            int ix = read_kv_int(is, "pos_x", -1);
                            int iy = read_kv_int(is, "pos_y", -1);
                            int iz = read_kv_int(is, "pos_z", -1);
                            if (ix >= 0 && ix < 16 && iy >= 0 && iy < 16 && iz == hero_z)
                                item_exists[iy][ix] = 1;
                        }
                        closedir(id);
                    }
                }

                /* Render header */
                blank();

                if (render_mode == 0) {
                    /* Mode 0: Emoji 2D flat view */
                    snprintf(rowbuf, sizeof(rowbuf), "  [2D] z=%d  (i=interact, 0-4=view)", hero_z);
                    line(rowbuf);

                    for (int r = 0; r < 16; r++) {
                        char row_out[256];
                        int off = 0;
                        for (int c = 0; c < 16; c++) {
                            const char *emoji = NULL;

                            if (r == hero_y && c == hero_x)
                                emoji = TERRAIN_HERO;
                            else if (interact_mode && r == cursor_y && c == cursor_x)
                                emoji = TERRAIN_CURSOR;
                            else if (mon_exists[r][c])
                                emoji = TERRAIN_MONSTER;
                            else if (item_exists[r][c])
                                emoji = TERRAIN_ITEM;
                            else
                                emoji = glyph_emoji(board[r][c]);

                            if (emoji) {
                                int elen = (int)strlen(emoji);
                                memcpy(row_out + off, emoji, elen);
                                off += elen;
                            } else {
                                row_out[off++] = ' ';
                            }
                        }
                        row_out[off] = '\0';
                        line(row_out);
                    }

                    if (interact_mode) {
                        const char *tile_at = glyph_emoji(board[cursor_y][cursor_x]);
                        snprintf(rowbuf, sizeof(rowbuf), "  Cursor: (%d,%d) tile=%s", cursor_x, cursor_y, tile_at ? tile_at : "?");
                        line(rowbuf);
                    }
                } else {
                    /* Modes 1-4: text-based 3D views, all rendered
                     * directly in the CHTPM frame (no GL needed).
                     * Proves z-levels work by showing wall height. */

                    /* Load z-layers below and above hero for depth */
                    char chunk_below[16][17], chunk_above[16][17];
                    int have_below = 0, have_above = 0;
                    if (hero_z > 0) {
                        char p[PATH_BUF];
                        snprintf(p, sizeof(p), "%s/pieces/system/chunks/chunk_%d_%d/chunk_%d_%d_z%d.txt",
                                 real_root_main, chunk_x, chunk_y, chunk_x, chunk_y, hero_z - 1);
                        FILE *f = fopen(p, "r");
                        if (f) { int r = 0; while (r < 16 && fgets(chunk_below[r], 17, f)) r++; fclose(f); have_below = 1; }
                    }
                    if (hero_z < 31) {
                        char p[PATH_BUF];
                        snprintf(p, sizeof(p), "%s/pieces/system/chunks/chunk_%d_%d/chunk_%d_%d_z%d.txt",
                                 real_root_main, chunk_x, chunk_y, chunk_x, chunk_y, hero_z + 1);
                        FILE *f = fopen(p, "r");
                        if (f) { int r = 0; while (r < 16 && fgets(chunk_above[r], 17, f)) r++; fclose(f); have_above = 1; }
                    }

                    if (render_mode == 1) {
                        /* Mode 1: First-person ASCII raymarch.
                         * Cast rays forward from hero facing north (-Y).
                         * Show wall/floor/ceiling with depth shading. */
                        snprintf(rowbuf, sizeof(rowbuf), "  [1ST] z=%d  facing N  (i=interact, 0=2D)", hero_z);
                        line(rowbuf);

                        for (int row = 0; row < 10; row++) {
                            char fov[65];
                            int foff = 0;
                            for (int col = -7; col <= 7; col++) {
                                int wx = hero_x + col;
                                int wy = hero_y - row;
                                char glyph = '.';
                                if (wx >= 0 && wx < 16 && wy >= 0 && wy < 16)
                                    glyph = board[wy][wx];

                                char c;
                                if (glyph == '_' || glyph == ',') {
                                    /* Wall: closer = brighter */
                                    c = (row < 3) ? '#' : (row < 6) ? '%' : '.';
                                } else if (glyph == '~') {
                                    c = '~';
                                } else {
                                    /* Air: show ceiling/floor at distance */
                                    if (have_above && wx >= 0 && wx < 16 && wy >= 0 && wy < 16) {
                                        char above = chunk_above[wy][wx];
                                        if (above == '_' || above == ',') c = '"';
                                        else c = ' ';
                                    } else {
                                        c = ' ';
                                    }
                                }

                                /* Entity overlays */
                                if (wx == hero_x && wy == hero_y && row == 0)
                                    c = '@';
                                else if (mon_exists[wy][wx] && wy >= 0 && wy < 16 && wx >= 0 && wx < 16)
                                    c = 'M';
                                else if (item_exists[wy][wx] && wy >= 0 && wy < 16 && wx >= 0 && wx < 16)
                                    c = '*';

                                /* Cursor highlight */
                                if (interact_mode && wx == cursor_x && wy == cursor_y)
                                    c = '+';

                                fov[foff++] = c;
                            }
                            fov[foff] = '\0';
                            line(fov);
                        }

                    } else if (render_mode == 2) {
                        /* Mode 2: Isometric view. Show chunk from
                         * above at an angle, with z-depth shading. */
                        snprintf(rowbuf, sizeof(rowbuf), "  [ISO] z=%d  (i=interact, 0=2D)", hero_z);
                        line(rowbuf);

                        for (int dy = 0; dy < 12; dy++) {
                            char iso_row[65];
                            int ioff = 0;
                            for (int dx = 0; dx < 16; dx++) {
                                int sx = dx;
                                int sy = dy - 4 + hero_y;
                                if (sy < 0 || sy >= 16 || sx < 0 || sx >= 16) {
                                    iso_row[ioff++] = ' ';
                                    continue;
                                }
                                char glyph = board[sy][sx];
                                char c;
                                if (glyph == '_' || glyph == ',') {
                                    /* Show wall height: check above */
                                    int wall_h = 1;
                                    if (have_above && chunk_above[sy][sx] == '_') wall_h = 2;
                                    c = (dy % 4 < wall_h * 2) ? '#' : ' ';
                                } else {
                                    c = '.';
                                }
                                if (sy == hero_y && sx == hero_x) c = '@';
                                else if (mon_exists[sy][sx]) c = 'M';
                                else if (item_exists[sy][sx]) c = '*';
                                else if (interact_mode && sy == cursor_y && sx == cursor_x) c = '+';
                                iso_row[ioff++] = c;
                            }
                            iso_row[ioff] = '\0';
                            line(iso_row);
                        }

                    } else if (render_mode == 3) {
                        /* Mode 3: Free camera - centered on cursor position,
                         * shows surrounding tiles with z-depth. */
                        snprintf(rowbuf, sizeof(rowbuf), "  [FREE] z=%d  cursor=(%d,%d)  (0=2D)", hero_z, cursor_x, cursor_y);
                        line(rowbuf);

                        int cam_cx = interact_mode ? cursor_x : hero_x;
                        int cam_cy = interact_mode ? cursor_y : hero_y;
                        for (int dy = -5; dy <= 5; dy++) {
                            char free_row[65];
                            int foff = 0;
                            for (int dx = -7; dx <= 7; dx++) {
                                int wx = cam_cx + dx;
                                int wy = cam_cy + dy;
                                char c = ' ';
                                if (wx >= 0 && wx < 16 && wy >= 0 && wy < 16) {
                                    char glyph = board[wy][wx];
                                    if (glyph == '_' || glyph == ',') {
                                        int dist = dx * dx + dy * dy;
                                        c = (dist < 4) ? '#' : (dist < 16) ? '%' : '.';
                                    } else if (glyph == '~') c = '~';
                                    else c = ' ';
                                    if (wy == hero_y && wx == hero_x) c = '@';
                                    else if (mon_exists[wy][wx]) c = 'M';
                                    else if (item_exists[wy][wx]) c = '*';
                                    else if (interact_mode && wy == cursor_y && wx == cursor_x) c = '+';
                                }
                                free_row[foff++] = c;
                            }
                            free_row[foff] = '\0';
                            line(free_row);
                        }

                    } else if (render_mode == 4) {
                        /* Mode 4: Bird's-eye with depth shading.
                         * Top-down view with z-layer visualization.
                         * Brighter = higher, darker = lower. Shows
                         * walls as taller by looking at z+1. */
                        snprintf(rowbuf, sizeof(rowbuf), "  [BIRD] z=%d  (i=interact, 0=2D)", hero_z);
                        line(rowbuf);

                        for (int r = 0; r < 16; r++) {
                            char bird_row[65];
                            int boff = 0;
                            for (int c = 0; c < 16; c++) {
                                char glyph = board[r][c];
                                char ch;
                                if (glyph == '_' || glyph == ',') {
                                    /* Wall: check if it extends above */
                                    if (have_above && chunk_above[r][c] == '_')
                                        ch = '@';  /* Tall wall (2+ z-layers) */
                                    else
                                        ch = '#';  /* Normal wall */
                                } else if (glyph == '~') ch = '~';
                                else if (have_below && chunk_below[r][c] == '_')
                                    ch = ',';  /* Floor above solid (underground) */
                                else
                                    ch = '.';  /* Air */

                                if (r == hero_y && c == hero_x) ch = 'H';
                                else if (mon_exists[r][c]) ch = 'M';
                                else if (item_exists[r][c]) ch = '*';
                                else if (interact_mode && r == cursor_y && c == cursor_x) ch = '+';
                                bird_row[boff++] = ch;
                            }
                            bird_row[boff] = '\0';
                            line(bird_row);
                        }

                        /* Legend */
                        line("  Legend: H=hero M=monster *=item +=cursor");
                        line("  #=wall @=tall-wall .=air ~=water ,=under");
                    }

                    if (interact_mode) {
                        const char *tile_at = glyph_emoji(board[cursor_y][cursor_x]);
                        snprintf(rowbuf, sizeof(rowbuf), "  Cursor: (%d,%d) tile=%s", cursor_x, cursor_y, tile_at ? tile_at : "?");
                        line(rowbuf);
                    }
                }
            } else {
                blank();
                line("  (no map data - move to generate chunks)");
            }
        }

        /* Real "how to add a variable to layout+backend" demonstration
         * (direct question asked this same session): writing a real
         * key=value line here to player_app/state.txt makes
         * "${game_datetime}" usable in ANY .chtpm layout - main.chtpm
         * doesn't reference it yet (the line above already shows it
         * inline), but this proves the mechanism for later real use
         * (e.g. a title-bar clock separate from the main text block). */
        char player_app_state_path[PATH_BUF];
        snprintf(player_app_state_path, sizeof(player_app_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        write_kv(player_app_state_path, "game_datetime", game_datetime_str);

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/mutaclysm/pieces/main/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | main\n\n");
            fprintf(pdl_out, "METHOD       | End Turn                            | END_TURN\n");
            fprintf(pdl_out, "METHOD       | Eat (from inventory)                 | EAT\n");
            fprintf(pdl_out, "METHOD       | Pick Up                             | PICKUP\n");
            fprintf(pdl_out, "METHOD       | Drop                               | DROP\n");
            fprintf(pdl_out, "METHOD       | View Board (opens separate GL window) | OPEN_BOARD_WIDGET\n");
            fprintf(pdl_out, "METHOD       | View Editor (opens separate GL window) | OPEN_VIEW_EDITOR\n");
            fclose(pdl_out);
        }
    }

    blank();
    if (last_message[0]) {
        snprintf(rowbuf, sizeof(rowbuf), "  %s", last_message);
        line(rowbuf);
    }

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);

    return 0;
}
