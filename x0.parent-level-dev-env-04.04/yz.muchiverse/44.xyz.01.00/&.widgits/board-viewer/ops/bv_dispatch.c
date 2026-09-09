/* bv_dispatch - board-viewer's one-shot game-loop op, TPMOS diamond
 * shape. Direct port of 101.mutaclsym...19.00/ops/game_dispatch.c's
 * "read ALL keys -> dispatch each -> render once -> exit" architecture
 * (pchq-vs-tpmos.md APPENDIX A, plan P-5). Replaces the pre-diamond
 * pal/main_module.pal loop that did ONE key + FOUR fork-ops + a full
 * bv_render_3d raymarch per 30ms iteration (~11-20 xelector moves/s).
 *
 * pal/main_module.pal now just:  loop: exec ./ops/+x/bv_dispatch ;
 *                                      sleep 16667 ; j loop
 *
 * Per tick this op:
 *   1. reads EVERY pending line of pieces/apps/player_app/interact_relay
 *      .txt into memory, then truncates it (same race-free drain
 *      game_dispatch.c uses),
 *   2. also notes whether pieces/display/bv_screen_changed.txt grew
 *      (host-driven board update - piece moved, map reload, etc.),
 *   3. forks ops/+x/bv_menu_input.+x <key> for each drained key (the
 *      exact per-key op the old loop called - reused, not reimplemented;
 *      key==0 is a no-op in that binary so idle ticks cost nothing),
 *   4. if any key was processed OR the screen changed, renders ONCE:
 *      bv_render_3d -> bv_compose_frame -> frame_changed marker.
 *
 * Idle tick (no keys, no screen change) = this process forks nothing,
 * does ~2 stat()s, exits. Held arrow key = N cheap bv_menu_input forks
 * + ONE raymarch per 16ms frame instead of per key.
 *
 * Self-contained, no shared headers. Usage: bv_dispatch.+x (no args).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE 512
#define PATH_BUF 4096
#define RELAY_BUF (MAX_LINE * 64)   /* plenty for a frame's worth of held-key input */

static char project_root[PATH_BUF] = "";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
    } else if (!getcwd(project_root, sizeof(project_root))) {
        project_root[0] = '\0';
    }
}

static void pj(char *dst, size_t sz, const char *rel) {
    snprintf(dst, sz, "%s/%s", project_root, rel);
}

/* fork+exec a board-viewer op with an argv vector, wait for it.
 * stdout/stderr -> /dev/null so a chatty op never pollutes the pal VM's
 * own stdout (same as game_dispatch.c's run_op). */
static int run_argv(char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        if (!freopen("/dev/null", "w", stdout)) _exit(1);
        if (!freopen("/dev/null", "w", stderr)) _exit(1);
        execv(argv[0], argv);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}
static int run_op(const char *path, const char *arg1) {
    char *av[3] = { (char *)path, (char *)arg1, NULL };
    if (!arg1) av[1] = NULL;
    return run_argv(av);
}

static long file_size(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? (long)st.st_size : 0;
}

int main(void) {
    resolve_root();
    if (!project_root[0]) return 0;

    char relay_path[PATH_BUF], screen_path[PATH_BUF], pos_path[PATH_BUF],
         marker_path[PATH_BUF], op_path[PATH_BUF];
    pj(relay_path,  sizeof(relay_path),  "pieces/apps/player_app/interact_relay.txt");
    pj(screen_path, sizeof(screen_path), "pieces/display/bv_screen_changed.txt");
    pj(pos_path,    sizeof(pos_path),    "pieces/display/.bv_dispatch_screen_pos");
    pj(marker_path, sizeof(marker_path), "pieces/display/frame_changed.txt");

    /* --- 1. drain interact_relay.txt (read all, then truncate) --- */
    char buf[RELAY_BUF];
    size_t used = 0;
    buf[0] = '\0';
    FILE *rf = fopen(relay_path, "r");
    if (rf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), rf)) {
            size_t l = strlen(line);
            if (used + l < sizeof(buf) - 1) { memcpy(buf + used, line, l); used += l; }
        }
        buf[used] = '\0';
        fclose(rf);
        FILE *tr = fopen(relay_path, "w");   /* truncate; keys after this are next tick's */
        if (tr) fclose(tr);
    }

    /* --- 2. host-driven screen-change trigger (bv_screen_changed.txt) --- */
    long screen_now = file_size(screen_path);
    long screen_last = 0;
    { FILE *pf = fopen(pos_path, "r"); if (pf) { if (fscanf(pf, "%ld", &screen_last) != 1) screen_last = 0; fclose(pf); } }
    int external_change = (screen_now != screen_last);
    if (external_change) {
        FILE *pf = fopen(pos_path, "w");
        if (pf) { fprintf(pf, "%ld\n", screen_now); fclose(pf); }
    }

    /* --- 3. dispatch ALL drained keys through ONE bv_menu_input.+x
     * (P-6: was fork+exec+wait per key). argv = [path, k1, k2, ..., NULL] --- */
#define BVD_MAX_KEYS 512
    int any_key = 0;
    pj(op_path, sizeof(op_path), "ops/+x/bv_menu_input.+x");
    char keystr[BVD_MAX_KEYS][16];
    char *av[BVD_MAX_KEYS + 2];
    int nk = 0;
    av[0] = op_path;
    for (char *p = buf; *p && nk < BVD_MAX_KEYS; ) {
        int keycode = 0;
        if (sscanf(p, "%d", &keycode) == 1 && keycode != 0) {
            snprintf(keystr[nk], sizeof(keystr[0]), "%d", keycode);
            av[1 + nk] = keystr[nk];
            nk++;
            any_key = 1;
        }
        char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    if (nk > 0) { av[1 + nk] = NULL; run_argv(av); }

    /* --- 4. render --- (pchq-vs-tpmos.md P-6/P-7)
     * bv_render_3d.+x is a ~0.3s 640x480 DDA raymarch - the single most
     * expensive step. Rendering it on every tick that had a key gives
     * ~3 fps while an arrow is held (each tick blocks the whole loop for
     * 0.3s). So:
     *   - bv_compose_frame (cheap text/2D + marker) runs on EVERY change
     *     tick, so xelector position / state stays live and immediate;
     *   - bv_render_3d is COALESCED: it runs only when the input burst
     *     has settled (relay came up empty after the drain) OR at most
     *     once per BV3D_MIN_MS while a burst is ongoing. Holding an arrow
     *     now streams smooth 2D updates + a 3D refresh every ~150ms
     *     instead of a 0.3s-blocked ~3 fps, and you get one crisp final
     *     3D frame the instant you let go. */
#define BV3D_MIN_MS 150
    if (any_key || external_change) {
        /* did more keys already queue while we were dispatching? */
        long still = file_size(relay_path);
        int burst_ongoing = (still > 0);

        char t_path[PATH_BUF];
        pj(t_path, sizeof(t_path), "pieces/display/.bv_dispatch_3d_ms");
        long long now_ms;
        { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
          now_ms = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
        long long last_ms = 0;
        { FILE *tf = fopen(t_path, "r"); if (tf) { if (fscanf(tf, "%lld", &last_ms) != 1) last_ms = 0; fclose(tf); } }

        int do_3d = external_change
                 || !burst_ongoing                       /* burst settled */
                 || (now_ms - last_ms) >= BV3D_MIN_MS;    /* or rate-limit tick */

        if (do_3d) {
            pj(op_path, sizeof(op_path), "ops/+x/bv_render_3d.+x");
            run_op(op_path, NULL);
            FILE *tf = fopen(t_path, "w");
            if (tf) { fprintf(tf, "%lld\n", now_ms); fclose(tf); }
        }
        pj(op_path, sizeof(op_path), "ops/+x/bv_compose_frame.+x");
        run_op(op_path, NULL);
        FILE *mk = fopen(marker_path, "a");   /* == prisc `hit_frame` */
        if (mk) { fputc('F', mk); fputc('\n', mk); fclose(mk); }
    }

    /* Record bv_screen_changed.txt's size AFTER our own bv_menu_input
     * calls (each does bump_screen_changed()) so those self-caused bumps
     * don't read back as an "external" host change on the next tick -
     * only a bump from the host game itself counts. */
    {
        long after = file_size(screen_path);
        FILE *pf = fopen(pos_path, "w");
        if (pf) { fprintf(pf, "%ld\n", after); fclose(pf); }
    }

    return 0;
}
