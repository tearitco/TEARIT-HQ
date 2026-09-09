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

/* fork+exec a board-viewer op, wait for it. Mirrors game_dispatch.c's
 * run_op() exactly (stdout/stderr -> /dev/null so a chatty op never
 * pollutes the pal VM's own stdout). */
static int run_op(const char *path, const char *arg1) {
    pid_t pid = fork();
    if (pid == 0) {
        if (!freopen("/dev/null", "w", stdout)) _exit(1);
        if (!freopen("/dev/null", "w", stderr)) _exit(1);
        if (arg1) execl(path, path, arg1, (char *)NULL);
        else      execl(path, path, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
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

    /* --- 3. dispatch every drained key through bv_menu_input.+x --- */
    int any_key = 0;
    pj(op_path, sizeof(op_path), "ops/+x/bv_menu_input.+x");
    for (char *p = buf; *p; ) {
        int keycode = 0;
        if (sscanf(p, "%d", &keycode) == 1 && keycode != 0) {
            char arg[16];
            snprintf(arg, sizeof(arg), "%d", keycode);
            run_op(op_path, arg);
            any_key = 1;
        }
        char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }

    /* --- 4. render ONCE if anything changed --- */
    if (any_key || external_change) {
        pj(op_path, sizeof(op_path), "ops/+x/bv_render_3d.+x");
        run_op(op_path, NULL);
        pj(op_path, sizeof(op_path), "ops/+x/bv_compose_frame.+x");
        run_op(op_path, NULL);
        FILE *mk = fopen(marker_path, "a");   /* == prisc `hit_frame` */
        if (mk) { fputc('F', mk); fputc('\n', mk); fclose(mk); }
    }

    return 0;
}
