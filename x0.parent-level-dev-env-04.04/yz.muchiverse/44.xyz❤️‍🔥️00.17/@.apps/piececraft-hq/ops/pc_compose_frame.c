/* pc_compose_frame - renders whichever piececraft-hq screen is current into
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
#include <sys/stat.h>
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
 * Start ever generated those files (pc_generate_chunk.c's own real
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
 * wander ticking is now exclusively pc_clock_daemon.c's own real job
 * (see read_game_clock()'s own header comment), this file no longer
 * advances anything, only reads/displays. */

/* REVISED 2026-08-04, direct instruction (real house precedent found:
 * #.ref/Mar$.$treetRace]Q]k32]4K/wsr_clock.c) - clock ADVANCEMENT is
 * no longer done here. This function used to advance the clock
 * reactively (only on a real render), which meant a session with no
 * player input had a genuinely frozen clock no matter how much real
 * time passed - the real house precedent for this exact problem is a
 * true PERSISTENT background daemon (ops/pc_clock_daemon.c, direct
 * port of wsr_clock.c's own real continuous-loop model), launched once
 * per world at CONFIRM_START/CONFIRM_START_DEBUG (pc_menu_input.c).
 * This function's real remaining job is READ-ONLY - just report
 * whatever the daemon has already written, so the "main" screen never
 * double-advances the same clock two different ways. */
/* REVISED 2026-08-04, direct user request ("add ms to the time output
 * display") - now returns real MILLISECOND precision (game_time_epoch_
 * ms), matching pc_clock_daemon.c's own real canonical clock field
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
static void border(void) {
    if (g_view_out) { fputc('+', g_view_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out); fputc('+', g_view_out); fputc('\n', g_view_out); }
}
static void line(const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    if (g_view_out) {
        fprintf(g_view_out, "|%.*s", len, content);
        for (int i = len; i < BOX_W; i++) fputc(' ', g_view_out);
        fputc('|', g_view_out);
        fputc('\n', g_view_out);
    }
}
static void blank(void) { line(""); }

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
    snprintf(state_path, sizeof(state_path), "%s/projects/piececraft-hq/pieces/pc_menu/state.txt", project_root);
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
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/piececraft-hq/pieces/new_game/piece.pdl", project_root);
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
        int chunk_x = read_kv_int(hero_state_path, "chunk_x", 0);
        int chunk_y = read_kv_int(hero_state_path, "chunk_y", 0);

        snprintf(rowbuf, sizeof(rowbuf), "  Tick: %d", tick);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Hero HP: %d   Pos: (%d,%d,%d)", hero_hp, hero_x, hero_y, hero_z);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Chunk: (%d,%d)", chunk_x, chunk_y);
        line(rowbuf);

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
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/piececraft-hq/pieces/main/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | main\n\n");
            fprintf(pdl_out, "METHOD       | Switch World                        | SWITCH_WORLD\n");
            fprintf(pdl_out, "METHOD       | End Turn                            | END_TURN\n");
            fprintf(pdl_out, "METHOD       | View Board (opens separate GL window) | OPEN_BOARD_WIDGET\n");
            fprintf(pdl_out, "METHOD       | View Editor (opens separate GL window) | OPEN_VIEW_EDITOR\n");
            fclose(pdl_out);
        }
    } else if (strcmp(active_piece, "select_world") == 0) {
        /* REAL, NEW 2026-08-30 (Piece 1 implementation): in-scene world
         * selection screen. Replaces the blocking new_game setup screen.
         * Lists available saved worlds from pieces/piececraft-desks/ and
         * allows the user to load one, or create a new one. Similar
         * conceptual pattern to livedesk_spawn_desk() on the desktop side,
         * but for piececraft's own world storage format (chunks, not DESK
         * rows). */
        line("Available Worlds:");
        blank();

        char real_root_sw[PATH_BUF];
        resolve_real_root(project_root, real_root_sw, sizeof(real_root_sw));
        char desks_dir[PATH_BUF];
        snprintf(desks_dir, sizeof(desks_dir), "%s/pieces/piececraft-desks", real_root_sw);

        DIR *d = opendir(desks_dir);
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                    continue;

                char world_path[PATH_BUF];
                snprintf(world_path, sizeof(world_path), "%s/%s", desks_dir, ent->d_name);

                struct stat st;
                if (stat(world_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                    char world_type[32] = "unknown";
                    char state_path[PATH_BUF];
                    snprintf(state_path, sizeof(state_path), "%s/state.txt", world_path);
                    FILE *sf = fopen(state_path, "r");
                    if (sf) {
                        char sline[MAX_LINE];
                        while (fgets(sline, sizeof(sline), sf)) {
                            if (strncmp(sline, "world_type=", 11) == 0) {
                                char *v = sline + 11;
                                v[strcspn(v, "\r\n")] = '\0';
                                snprintf(world_type, sizeof(world_type), "%s", v);
                            }
                        }
                        fclose(sf);
                    }
                    snprintf(rowbuf, sizeof(rowbuf), "  %s (%s)", ent->d_name, world_type);
                    line(rowbuf);
                }
            }
            closedir(d);
        }

        blank();
        line("Choose a world to load, or create a new one:");

        char select_pdl_path[PATH_BUF];
        snprintf(select_pdl_path, sizeof(select_pdl_path), "%s/projects/piececraft-hq/pieces/select_world/piece.pdl", project_root);
        FILE *select_pdl = fopen(select_pdl_path, "w");
        if (select_pdl) {
            fprintf(select_pdl, "SECTION      | KEY                | VALUE\n");
            fprintf(select_pdl, "----------------------------------------\n");
            fprintf(select_pdl, "META         | piece_id           | select_world\n\n");

            /* Add LOAD_WORLD methods for each existing world */
            d = opendir(desks_dir);
            if (d) {
                struct dirent *ent;
                while ((ent = readdir(d)) != NULL) {
                    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                        continue;

                    char world_path[PATH_BUF];
                    snprintf(world_path, sizeof(world_path), "%s/%s", desks_dir, ent->d_name);

                    struct stat st;
                    if (stat(world_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        fprintf(select_pdl, "METHOD       | Load %s                     | LOAD_WORLD:%s\n",
                                ent->d_name, ent->d_name);
                    }
                }
                closedir(d);
            }

            fprintf(select_pdl, "METHOD       | Create New Seeded World    | CREATE_WORLD_SEEDED\n");
            fprintf(select_pdl, "METHOD       | Create New Debug Flat      | CREATE_WORLD_DEBUG\n");
            fclose(select_pdl);
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
