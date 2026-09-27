/* pc_trigger_watcher - real PERSISTENT bridge-watcher daemon
 * (EVENT-TRIGGER-LAYER-PLAN.md §3 Step 2). Same real lifecycle
 * convention as pc_clock_daemon.c (persistent, PID-file tracked,
 * launched once per world session, SIGTERM/SIGINT clean exit) - not a
 * new pattern, the second daemon in this project shaped exactly like
 * the first.
 *
 * Job: tail data/master_ledger.txt (the SAME real, already-multi-
 * writer ledger pc_menu_input.c's MOVE handler and pc_clock_daemon.c's
 * own tick_animals() already append to - EVENT-TRIGGER-LAYER-PLAN.md
 * §3's own finding, no new file). For every genuinely NEW line whose
 * action_type is "touched_npc", shell out to events-hq's own real
 * play_event.sh with trigger="player-touch" - the exact same call the
 * manual Play button already makes, so this doesn't reimplement
 * anything about how an event actually runs, only WHEN one starts.
 *
 * Real, honest scope limitation (not silently hidden): play_event.sh's
 * own common-events dispatch block (checked directly, 2026-09-14) has
 * NO per-entity scoping - it runs EVERY common event whose page
 * condition.pdl trigger matches, regardless of which $PKG was passed.
 * For this project's current single test tile that's exactly right;
 * once more than one player-touch common event exists, this daemon
 * will need real per-tile-to-event routing (reading events.pdl's own
 * x/y-to-event-path row, same lookup pc_menu_input.c's own
 * check_player_touch_trigger() already does for the DETECT side) -
 * flagged here as the real next increment, not built yet.
 *
 * Starts reading the ledger from its CURRENT end-of-file at launch,
 * not byte 0 - a fresh daemon must not replay a whole session's worth
 * of old test data the instant it starts (same real design already
 * used by palnet_peer.c's own read_outbox_new_lines(), matching that
 * proven "only genuinely new lines" convention).
 *
 * Self-contained, no shared headers.
 * Usage: pc_trigger_watcher.+x (no args, runs forever until SIGTERM/SIGINT) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include "win_posix_shim.h"

#define MAX_LINE 1024
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_should_exit = 0;

static void handle_signal(int signo) {
    (void)signo;
    g_should_exit = 1;
}

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Same real shape as pc_clock_daemon.c's own resolve_real_root() -
 * project-local copy, not a shared header, per this project's own
 * duplicate-rather-than-share convention. */
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

/* Reads whatever NEW complete lines exist past g_ledger_offset, same
 * "wait for the trailing newline, leave a partial write for next poll"
 * tolerance palnet_peer.c's own read_outbox_new_lines() already uses -
 * proven pattern for a file another process is actively appending to
 * concurrently. Returns the count found (0 if none). */
static long g_ledger_offset = -1; /* -1 = not yet initialized to EOF */
static int read_ledger_new_lines(const char *ledger_path, char out_lines[][MAX_LINE], int max_lines) {
    FILE *f = fopen(ledger_path, "r");
    if (!f) return 0;

    if (g_ledger_offset < 0) {
        /* First real poll after launch - start from current EOF so a
         * fresh daemon never replays old test data (this file's own
         * header comment). */
        fseek(f, 0, SEEK_END);
        g_ledger_offset = ftell(f);
        fclose(f);
        return 0;
    }

    if (fseek(f, g_ledger_offset, SEEK_SET) != 0) {
        g_ledger_offset = 0;
        fseek(f, 0, SEEK_SET);
    }

    int n = 0;
    long consumed = g_ledger_offset;
    char line[MAX_LINE];
    while (n < max_lines && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n') break; /* partial trailing line - wait for the rest */
        line[strcspn(line, "\r\n")] = '\0';
        consumed += (long)len;
        if (line[0] == '\0') continue;
        snprintf(out_lines[n], MAX_LINE, "%s", line);
        n++;
    }
    g_ledger_offset = consumed;
    fclose(f);
    return n;
}

/* Ledger row shape: timestamp|turn|actor|action_type|details (EVENT-
 * TRIGGER-LAYER-PLAN.md §3's own format, matching 101.lpns+map+4's
 * independently-proven ledger). Only action_type is needed here - the
 * DETECT side (check_player_touch_trigger() in pc_menu_input.c) has
 * already done the x/y-to-trigger-string matching before this line was
 * ever written; this daemon's whole job is noticing it happened. */
static int ledger_line_is_touched_npc(const char *line) {
    const char *p = line;
    int field = 0;
    while (*p) {
        if (*p == '|') {
            field++;
            if (field == 3) return strncmp(p + 1, "touched_npc", 11) == 0 &&
                                    (p[12] == '|' || p[12] == '\0');
        }
        p++;
    }
    return 0;
}

int main(void) {
    resolve_root();
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    char real_root[PATH_BUF];
    resolve_real_root(project_root, real_root, sizeof(real_root));

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/pieces/system/pc_trigger_watcher.pid", real_root);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    /* house_root.txt is a raw single-line path (not a key=value .pdl
     * row) - same real file pc_generate_chunk.c's own resolve_root()
     * reads the same way. */
    char house_root_path[PATH_BUF], house_root[PATH_BUF] = "";
    snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", real_root);
    {
        FILE *hf = fopen(house_root_path, "r");
        if (hf) {
            if (fgets(house_root, sizeof(house_root), hf)) house_root[strcspn(house_root, "\r\n")] = '\0';
            fclose(hf);
        }
    }

    char ledger_path[PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", real_root);

    while (!g_should_exit) {
        /* Real, live re-resolve every loop - same "cheap to re-check"
         * convention pc_clock_daemon.c's own main loop already uses. */
        resolve_real_root(project_root, real_root, sizeof(real_root));
        snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", real_root);

        char new_lines[16][MAX_LINE];
        int n = read_ledger_new_lines(ledger_path, new_lines, 16);
        for (int i = 0; i < n; i++) {
            if (!ledger_line_is_touched_npc(new_lines[i])) continue;
            if (!house_root[0]) continue; /* can't locate play_event.sh without it */

            char cmd[PATH_BUF * 3];
            snprintf(cmd, sizeof(cmd),
                     "'%s/&.widgits/events-hq/ops/play_event.sh' '%s/common_events/pc_hq_trigger_caller' '%s' player-touch >/dev/null 2>&1",
                     house_root, house_root, house_root);
            { int _rc = system(cmd); (void)_rc; }
        }

        usleep(300000); /* real 0.3s cadence, same real interval pc_clock_daemon.c's own loop uses */
    }

    remove(pid_path);
    return 0;
}
