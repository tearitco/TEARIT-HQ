/* pc_clock_daemon - real PERSISTENT game-clock process, direct port of
 * the house's own real precedent: #.ref/Mar$.$treetRace]Q]k32]4K/
 * wsr_clock.c (read in full 2026-08-04, direct instruction). Same real
 * per-real-millisecond rate table, same real continuous background
 * loop (CLOCK_MONOTONIC + usleep, NOT tied to any per-render/per-
 * keypress trigger) - the REAL fix for "time isn't ticking when I'm
 * not pressing keys": pc_compose_frame.c's own advance_game_clock()
 * only ever ran REACTIVELY (on a render), so a session with no input
 * had a genuinely frozen clock no matter how much real time passed.
 * This daemon ticks on its own, exactly like the real reference.
 *
 * Adapted to piececraft-xyz's own real schema (world_01/state.txt's
 * own game_time_epoch_sec/autotick_enabled/autotick_speed fields,
 * whole-seconds epoch instead of the reference's own separate y/m/d/h/
 * m/s/cs fields - this project's own real display already derives the
 * calendar breakdown via strftime, no need to track it separately)
 * rather than a byte-for-byte port - real precedent for the TIMING
 * MODEL (continuous daemon, same rate table), not a literal copy of
 * every field.
 *
 * Real, additional piece the reference doesn't have: this daemon ALSO
 * advances the real world tick counter + calls tick_animals() (same
 * mechanic every player action already triggers) once per real GAME
 * MINUTE crossed - the reference's own file has no concept of "world
 * tick"/NPCs at all, that's piececraft-xyz's own real addition on top.
 *
 * Self-contained, no shared headers.
 * Usage: pc_clock_daemon.+x (no args, runs forever until SIGTERM/SIGINT) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include "win_posix_shim.h"

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define M_PI_LOCAL 3.14159265358979323846

static char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_should_exit = 0;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

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

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static long long read_kv_ll(const char *path, const char *key, long long def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoll(buf) : def;
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

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static void write_kv_ll(const char *path, const char *key, long long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", value);
    write_kv(path, key, buf);
}

static void ledger_append(const char *root, int turn, const char *actor, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%s|%s|%s\n", ts, turn, actor, action_type, details);
    fclose(f);
}

/* Same real chicken-wander mechanic every other real tick source in
 * this project already uses (pc_menu_input.c's own tick_animals(),
 * pc_compose_frame.c's own tick_animals_local() - a real THIRD copy,
 * per this house's own no-shared-headers convention). */
static void tick_animals(const char *root, int tick) {
    char animals_path[PATH_BUF];
    snprintf(animals_path, sizeof(animals_path), "%s/pieces/world_01/animals.txt", root);
    FILE *f = fopen(animals_path, "r");
    if (!f) return;
    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL) ^ (unsigned)getpid()); seeded = 1; }
    char rewritten[64][MAX_LINE];
    int n = 0;
    char line[MAX_LINE];
    while (n < 64 && fgets(line, sizeof(line), f)) {
        char entity_id[64];
        int x, y, z;
        if (sscanf(line, "%63[^,],%d,%d,%d", entity_id, &x, &y, &z) == 4) {
            int dx = (rand() % 3) - 1, dy = (rand() % 3) - 1;
            int nx = x + dx, ny = y + dy;
            if (nx < 0) nx = 0; if (nx > 15) nx = 15;
            if (ny < 0) ny = 0; if (ny > 15) ny = 15;
            snprintf(rewritten[n], MAX_LINE, "%s,%d,%d,%d\n", entity_id, nx, ny, z);
            if (nx != x || ny != y) {
                char details[64];
                snprintf(details, sizeof(details), "x:%d,y:%d", nx, ny);
                ledger_append(root, tick, entity_id, "wander", details);
            }
        } else {
            snprintf(rewritten[n], MAX_LINE, "%s", line);
        }
        n++;
    }
    fclose(f);
    FILE *wf = fopen(animals_path, "w");
    if (wf) { for (int i = 0; i < n; i++) fputs(rewritten[i], wf); fclose(wf); }
}

/* REAL, NEW 2026-08-04, direct instruction ("sun and moon will have
 * their own directory as pieces/entities/maps") - sun/moon are now
 * REAL PERSISTENT PIECES (pieces/sun_01/state.txt, pieces/moon_01/
 * state.txt), same real convention hero_01/xelector_01 already use,
 * written continuously by this SAME daemon (single real source of
 * truth, matching how it already owns game_time_epoch_sec) - any
 * renderer just reads their real pos_x/y/z, no orbit math duplicated
 * elsewhere. Shared function, real per-body period/orbit/phase
 * parameters (xyz-ngn-plan.md §2d's own "add a row to the table, not
 * rewrite the math" design) - moon's own real phase is offset by half
 * a cycle (classic sun/moon opposition: moon is high when sun is low,
 * a real, simple astronomical relationship for a first pass). */
#define PLANET_CENTER_X 8.0
#define PLANET_CENTER_Y 20.0
#define PLANET_CENTER_Z 8.0
#define ORBIT_A 40.0
#define ORBIT_B 6.0
#define ORBIT_HEIGHT 30.0

static void update_celestial_body(const char *root, const char *entity_id,
                                   long long game_epoch_sec, double period_seconds, double phase_offset_rad) {
    double t = fmod((double)game_epoch_sec, period_seconds);
    if (t < 0) t += period_seconds;
    double angle = (t / period_seconds) * 2.0 * M_PI_LOCAL - (M_PI_LOCAL / 2.0) + phase_offset_rad;

    double world_x = PLANET_CENTER_X + cos(angle) * ORBIT_A;
    double world_z = PLANET_CENTER_Z + sin(angle) * ORBIT_B;
    double world_y = PLANET_CENTER_Y + sin(angle) * ORBIT_HEIGHT;

    char dir_path[PATH_BUF];
    snprintf(dir_path, sizeof(dir_path), "%s/pieces/%s", root, entity_id);
#ifdef _WIN32
    win_mkdir_p(dir_path);
#else
    char mkdir_cmd[PATH_BUF + 16];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", dir_path);
    { int _rc = system(mkdir_cmd); (void)_rc; }
#endif

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", dir_path);
    write_kv(state_path, "entity_type", "celestial_body");
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", world_x); write_kv(state_path, "pos_x", buf);
    snprintf(buf, sizeof(buf), "%.2f", world_y); write_kv(state_path, "pos_y", buf);
    snprintf(buf, sizeof(buf), "%.2f", world_z); write_kv(state_path, "pos_z", buf);
}

static void handle_signal(int signo) {
    (void)signo;
    g_should_exit = 1;
}

int main(void) {
    resolve_root();
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    char real_root[PATH_BUF];
    resolve_real_root(project_root, real_root, sizeof(real_root));

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/pieces/system/pc_clock_daemon.pid", real_root);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    char world_state_path[PATH_BUF];
    snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", real_root);

    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    while (!g_should_exit) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_ms = (current_time.tv_sec - last_time.tv_sec) * 1000 +
                          (current_time.tv_nsec - last_time.tv_nsec) / 1000000;
        last_time = current_time;

        /* Real, live re-resolve every loop - real_project_root.txt
         * could theoretically change if this daemon somehow outlives
         * its own session (it shouldn't, but resolving fresh is cheap
         * and matches this house's own "don't cache what's cheap to
         * re-check" convention elsewhere). */
        resolve_real_root(project_root, real_root, sizeof(real_root));
        snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", real_root);

        /* Real sun/moon position update - runs every loop regardless
         * of autotick_enabled (their real position is a pure function
         * of the CURRENT game_time_epoch_sec, not of whether the clock
         * is actively advancing right now - a paused clock should
         * still show sun/moon at their real, correct current spot). */
        {
            long long cur_epoch = read_kv_ll(world_state_path, "game_time_epoch_sec", 0);
            update_celestial_body(real_root, "sun_01", cur_epoch, 86400.0, 0.0);
            update_celestial_body(real_root, "moon_01", cur_epoch, 86400.0, M_PI_LOCAL);
        }

        int autotick_enabled = read_kv_int(world_state_path, "autotick_enabled", 0);
        if (autotick_enabled && elapsed_ms > 0) {
            char speed_str[16] = "min";
            read_kv_str(world_state_path, "autotick_speed", speed_str, sizeof(speed_str));

            double game_centiseconds_per_real_millisecond = 6.0;
            if (strcmp(speed_str, "cent") == 0) game_centiseconds_per_real_millisecond = 36000.0;
            else if (strcmp(speed_str, "sec") == 0) game_centiseconds_per_real_millisecond = 360.0;
            else if (strcmp(speed_str, "min") == 0) game_centiseconds_per_real_millisecond = 6.0;
            else if (strcmp(speed_str, "hour") == 0) game_centiseconds_per_real_millisecond = 0.1;
            else if (strcmp(speed_str, "day") == 0) game_centiseconds_per_real_millisecond = 0.004166666666666667;

            /* REAL FIX 2026-08-04, direct user report ("add ms to the
             * time output display") - a real, DEEPER bug this surfaced:
             * the OLD code tracked whole GAME SECONDS only, truncating
             * every single 300ms loop - at slow speeds (hour/day), one
             * loop's own real fractional progress (e.g. ~0.0125 game-
             * sec at "day" speed) rounds down to a whole 0 and was
             * simply DISCARDED, never carried to the next loop. The
             * clock could genuinely never advance at all at those
             * speeds, not just display coarsely. Real fix: the
             * canonical stored clock is now real MILLISECOND precision
             * (game_time_epoch_ms) - the same real fractional progress
             * that used to vanish now correctly accumulates loop over
             * loop (12ms this loop + 12ms next loop + ... eventually
             * crosses a whole second, same real principle as a proper
             * accumulator, not truncate-and-discard). game_time_epoch_
             * sec is still written too (whole seconds, derived) so
             * every other real reader (bv_render_3d.c's own sun orbit
             * math) needs zero changes. */
            long long game_epoch_ms = read_kv_ll(world_state_path, "game_time_epoch_ms", 0);
            if (game_epoch_ms == 0) {
                long long legacy_sec = read_kv_ll(world_state_path, "game_time_epoch_sec", 0);
                game_epoch_ms = legacy_sec * 1000LL; /* real, one-time migration from an older world with no ms field yet */
            }
            long long old_minute = game_epoch_ms / 60000LL;

            double delta_game_cs = (double)elapsed_ms * game_centiseconds_per_real_millisecond;
            long long delta_game_ms = (long long)(delta_game_cs * 10.0); /* 1 centisecond = 10 real milliseconds */
            if (delta_game_ms > 0) {
                game_epoch_ms += delta_game_ms;
                write_kv_ll(world_state_path, "game_time_epoch_ms", game_epoch_ms);
                write_kv_ll(world_state_path, "game_time_epoch_sec", game_epoch_ms / 1000LL);

                long long new_minute = game_epoch_ms / 60000LL;
                if (new_minute != old_minute) {
                    int tick = read_kv_int(world_state_path, "tick", 0);
                    tick += 1;
                    write_kv_int(world_state_path, "tick", tick);
                    tick_animals(real_root, tick);
                }

                /* REAL FIX 2026-08-04, direct user report ("autotick
                 * should make time move on its own and RERENDER... tick
                 * isn't moving time anymore"): writing game_time_epoch_
                 * sec here was never enough on its own - the LIVE pal
                 * script's own main loop (pal/main_module.pal) only
                 * ever calls pc_compose_frame again when THIS SESSION's
                 * own pieces/display/pc_screen_changed.txt marker
                 * changes (real, existing convention - see pc_menu_
                 * input.c's own identical bump on every other real
                 * state change). Without bumping it here too, the
                 * screen genuinely never refreshes on its own no matter
                 * how much the clock advances in the background - real
                 * data was moving, the visible text just never redrew
                 * to show it. Bumped in the SESSION root (project_root,
                 * not real_root) - that marker is real, per-SESSION
                 * state, not part of the persistent world. */
                char screen_marker_path[PATH_BUF];
                snprintf(screen_marker_path, sizeof(screen_marker_path), "%s/pieces/display/pc_screen_changed.txt", project_root);
                FILE *smf = fopen(screen_marker_path, "a");
                if (smf) { fputc('.', smf); fclose(smf); }

                /* REAL, NEW 2026-08-04, direct user request ("live
                 * auto-refresh... both screens" - the 3D/2D map window
                 * is a genuinely SEPARATE process/session from
                 * piececraft-xyz's own text screen, with its own real
                 * refresh marker (bv_screen_changed.txt) - bumping THIS
                 * project's own marker above does nothing for it. Same
                 * real live-widget lookup pc_menu_input.c's own
                 * OPEN_BOARD_WIDGET handler already uses (house_root.txt
                 * + ledger_peers.+x, scanning for THIS host's own real
                 * "board-viewer:piececraft-xyz" scoped entry) - a real,
                 * honest no-op if no board-viewer window is currently
                 * open (nothing to refresh). */
                /* REAL FIX 2026-08-04, direct user report ("something
                 * was throttling my cpu after leaving it running...
                 * doesn't even happen in the python version"): this
                 * whole block spawns a REAL subprocess (fork+exec,
                 * ledger_peers.+x) - fine occasionally, genuinely
                 * expensive done every real loop iteration forever (at
                 * fast speeds like cent/sec, that's ~3x/real-second,
                 * CONTINUOUSLY, for as long as autotick stays on - real,
                 * sustained CPU churn from constant process spawning,
                 * not a one-time cost). The map window doesn't need
                 * sub-second refresh precision anyway - real, simple
                 * rate limit: at most once per real second, tracked via
                 * a real static timestamp that survives across loop
                 * iterations. */
                static long long last_peer_lookup_ms = 0;
                long long now_ms_rl = (long long)current_time.tv_sec * 1000LL + current_time.tv_nsec / 1000000LL;
                char house_root_path[PATH_BUF], house_root[PATH_BUF] = "";
                if (now_ms_rl - last_peer_lookup_ms >= 1000) {
                last_peer_lookup_ms = now_ms_rl;
                snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
                FILE *hf = fopen(house_root_path, "r");
                if (hf) {
                    if (fgets(house_root, sizeof(house_root), hf)) house_root[strcspn(house_root, "\r\n")] = '\0';
                    fclose(hf);
                }
                if (house_root[0]) {
                    char peer_cmd[PATH_BUF * 2];
                    snprintf(peer_cmd, sizeof(peer_cmd),
                             "PRISC_PROJECT_ROOT='%s' '%s/&.widgits/board-viewer/ops/+x/ledger_peers.+x' widget 2>/dev/null",
                             project_root, house_root);
                    FILE *pf = popen(peer_cmd, "r");
                    if (pf) {
                        char peer_line[MAX_LINE];
                        while (fgets(peer_line, sizeof(peer_line), pf)) {
                            peer_line[strcspn(peer_line, "\r\n")] = '\0';
                            char *save = NULL;
                            char *sess_tok = strtok_r(peer_line, "|", &save);
                            strtok_r(NULL, "|", &save);
                            strtok_r(NULL, "|", &save);
                            char *proj_tok = strtok_r(NULL, "|", &save);
                            if (proj_tok && sess_tok && strcmp(proj_tok, "board-viewer:piececraft-xyz") == 0) {
                                char bv_marker_path[PATH_BUF];
                                snprintf(bv_marker_path, sizeof(bv_marker_path), "%s/pieces/display/bv_screen_changed.txt", sess_tok);
                                FILE *bvmf = fopen(bv_marker_path, "a");
                                if (bvmf) { fputc('.', bvmf); fclose(bvmf); }
                                break;
                            }
                        }
                        pclose(pf);
                    }
                }
                } /* end real 1-real-second rate limit */
            }
        }

        usleep(300000); /* real 0.3s cadence, same real interval wsr_clock.c's own loop uses */
    }

    remove(pid_path);
    return 0;
}
