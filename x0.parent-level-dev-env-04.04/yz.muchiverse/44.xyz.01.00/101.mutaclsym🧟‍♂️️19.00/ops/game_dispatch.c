/* game_dispatch.c - One-shot op: read ALL keys from relay, dispatch each,
 * run NPC auto-play, compose frame, signal renderer.
 * Architecture: read all -> dispatch all -> NPC -> render -> exit.
 * Matches lpns+map+4's game_dispatch.c pattern exactly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE 4096
#define PATH_BUF 2048

/* CATEGORY B FIX 2026-08-20: After symlink elimination, CWD is the
 * ephemeral session dir. Files that are WRITTEN by other processes via
 * PRISC_PROJECT_ROOT (interact_relay.txt, hero_01/state.txt) live in the
 * project root and must be accessed via absolute path. Session-specific
 * files (display, bv_state.txt) stay relative to CWD. */
static char project_root[PATH_BUF] = "";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
    } else {
        if (getcwd(project_root, sizeof(project_root)) == NULL)
            project_root[0] = '\0';
    }
}

static void build_proj_path(char *dst, size_t sz, const char *rel) {
    snprintf(dst, sz, "%s/%s", project_root, rel);
}

#define build_op_path build_proj_path

static int run_op(const char *path, const char *arg1) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        if (arg1) execl(path, path, arg1, NULL);
        else execl(path, path, NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

static void write_kv_int(const char *path, const char *key, int value) {
    char lines[128][512];
    int nlines = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (nlines < 128 && fgets(lines[nlines], sizeof(lines[0]), f)) nlines++;
        fclose(f);
    }
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    size_t klen = strlen(key);
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%d\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%d\n", key, value);
    fclose(f);
}

static void read_kv_raw(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
        }
    }
    fclose(f);
}

/* REAL BUG FIX 2026-08-17: muta_render_3d.+x (board-viewer's own real
 * voxel renderer) reads ALL camera fields (cam_yaw, cam_pitch, cam_pan_*,
 * camera_mode, cam_z_level, render_mode) from pieces/system/bv_state.txt -
 * a COMPLETELY SEPARATE file from pieces/hero_01/state.txt, which is what
 * mutaclysm's own camera_control.c (a restored +18.0G classic op) actually
 * writes to. Neither file was ever a single source of truth for camera
 * state - board-viewer's own multi-host widget design assumes bv_state.txt
 * is the SAME file the host project writes its own camera state into, but
 * mutaclysm's own camera_control.c has no idea bv_state.txt exists at all.
 * Confirmed live: cam_yaw updated correctly in hero_01/state.txt on every
 * keypress, but the rendered frame's checksum never changed, because
 * muta_render_3d always fell back to its own hardcoded default (yaw=180)
 * reading an empty bv_state.txt. Fix: mirror the real camera fields from
 * hero_01/state.txt into bv_state.txt right before every render, preserving
 * bv_state.txt's own focused_project_root line. */
static void sync_camera_to_bv_state(void) {
    char focused[512] = "";
    read_kv_raw("pieces/system/bv_state.txt", "focused_project_root", focused, sizeof(focused));

    char cam_yaw[64] = "", cam_pitch[64] = "", cam_pan_x[64] = "", cam_pan_y[64] = "",
         cam_pan_z[64] = "", cam_z_level[64] = "", camera_mode[64] = "", render_mode[64] = "",
         selector_x[64] = "", selector_y[64] = "";
    char hero_path_buf[PATH_BUF];
    build_proj_path(hero_path_buf, sizeof(hero_path_buf), "pieces/hero_01/state.txt");
    const char *hero_path = hero_path_buf;
    read_kv_raw(hero_path, "cam_yaw", cam_yaw, sizeof(cam_yaw));
    read_kv_raw(hero_path, "cam_pitch", cam_pitch, sizeof(cam_pitch));
    read_kv_raw(hero_path, "cam_pan_x", cam_pan_x, sizeof(cam_pan_x));
    read_kv_raw(hero_path, "cam_pan_y", cam_pan_y, sizeof(cam_pan_y));
    read_kv_raw(hero_path, "cam_pan_z", cam_pan_z, sizeof(cam_pan_z));
    read_kv_raw(hero_path, "cam_z_level", cam_z_level, sizeof(cam_z_level));
    read_kv_raw(hero_path, "camera_mode", camera_mode, sizeof(camera_mode));
    read_kv_raw(hero_path, "render_mode", render_mode, sizeof(render_mode));
    /* REAL BUG FIX 2026-08-18, direct user report ("arrow keys are only
     * moving the xelector in 2d emoji mode, not in 3d camera mode"):
     * muta_render_3d.c (see its own header comment, ~line 1484) reads
     * selector_x/selector_y from THIS file (bv_state.txt) - not from
     * hero_01/state.txt's own xlector_pos_x/xlector_pos_y, which is what
     * move_player.c actually updates on every arrow key. This function
     * never mirrored those two fields across, so muta_render_3d.c always
     * fell back to its own hardcoded default (board_w/2, board_h/2) on
     * every single render, regardless of where the xlector actually was -
     * the backend position was updating correctly the whole time (proven
     * via direct keyboard/history.txt injection + hero state diff), only
     * the 3D view's own selector marker (and its cam_pan_x/y fallback,
     * same lines) never moved. 2D emoji mode reads xlector_pos_x/y
     * straight from hero_01/state.txt via a separate render path, so it
     * was never affected. */
    read_kv_raw(hero_path, "xlector_pos_x", selector_x, sizeof(selector_x));
    read_kv_raw(hero_path, "xlector_pos_y", selector_y, sizeof(selector_y));

    FILE *f = fopen("pieces/system/bv_state.txt", "w");
    if (!f) return;
    if (focused[0]) fprintf(f, "focused_project_root=%s\n", focused);
    if (cam_yaw[0]) fprintf(f, "cam_yaw=%s\n", cam_yaw);
    if (cam_pitch[0]) fprintf(f, "cam_pitch=%s\n", cam_pitch);
    if (cam_pan_x[0]) fprintf(f, "cam_pan_x=%s\n", cam_pan_x);
    if (cam_pan_y[0]) fprintf(f, "cam_pan_y=%s\n", cam_pan_y);
    if (cam_pan_z[0]) fprintf(f, "cam_pan_z=%s\n", cam_pan_z);
    if (cam_z_level[0]) fprintf(f, "cam_z_level=%s\n", cam_z_level);
    if (camera_mode[0]) fprintf(f, "camera_mode=%s\n", camera_mode);
    if (render_mode[0]) fprintf(f, "render_mode=%s\n", render_mode);
    if (selector_x[0]) fprintf(f, "selector_x=%s\n", selector_x);
    if (selector_y[0]) fprintf(f, "selector_y=%s\n", selector_y);
    fclose(f);
}

/* REAL BUG FIX 2026-08-18, direct user report ("keys ment for interact
 * mode are still passsing thru menu when interact isn't active"), ported
 * from the real, canonical fuzz-op_manager.c reference
 * (1.TPMOS_c_+rmmp.0103.0001/projects/fuzz-op/manager/fuzz-op_manager.c,
 * its own real is_active_layout() function, read in full before writing
 * this): the canonical reference NEVER processes ANY relayed key unless
 * pieces/display/current_layout.txt is genuinely the real game screen -
 * mutaclysm's own game_dispatch.c had NO equivalent check at all, and
 * would unconditionally drain+process interact_relay.txt on every single
 * tick regardless of which chtpm layout was actually being shown. Matches
 * the reference's own exact behavior (not just the same spirit): if the
 * layout doesn't match, this returns immediately WITHOUT even opening
 * interact_relay.txt - any keys already queued there stay queued,
 * untouched, until the layout becomes active again (fuzz-op's own real
 * choice - it does not drain-and-discard on a mismatched layout, it
 * simply skips the whole cycle and retries next tick). mutaclysm only
 * has ONE real game layout (main.chtpm, confirmed via direct read of
 * pieces/display/current_layout.txt this same session) - unlike
 * fuzz-op's own multi-name check (game.chtpm/fuzz-op.chtpm/gl_os), a
 * single substring match is sufficient here. */
static int is_active_layout(void) {
    FILE *f = fopen("pieces/display/current_layout.txt", "r");
    if (!f) return 1; /* fail-open, matches fuzz-op_manager.c's own real default */
    char line[256];
    int result = 1;
    if (fgets(line, sizeof(line), f)) {
        result = (strstr(line, "main.chtpm") != NULL);
    }
    fclose(f);
    return result;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    resolve_root();

    if (!is_active_layout()) return 0;

    /* Step 1: Read ALL keys into memory, then truncate immediately.
     * This eliminates the race where parser writes a key between
     * our read and truncation.
     * CATEGORY B: interact_relay.txt is written by chtpm_parser_pal's
     * inject_raw_key() via build_path_malloc() (project root), so we
     * must read it from project root too — session dir copy is empty. */
    char relay_path[PATH_BUF];
    build_proj_path(relay_path, sizeof(relay_path), "pieces/apps/player_app/interact_relay.txt");

    char hero_path[PATH_BUF];
    build_proj_path(hero_path, sizeof(hero_path), "pieces/hero_01/state.txt");

    FILE *hf = fopen(relay_path, "r");
    if (!hf) return 0;

    char buf[MAX_LINE * 16];
    int buf_used = 0;
    buf[0] = '\0';
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), hf)) {
        int len = strlen(line);
        if (buf_used + len < (int)sizeof(buf)) {
            strcpy(buf + buf_used, line);
            buf_used += len;
        }
    }
    fclose(hf);

    /* Truncate immediately - any keys written after this point
     * are safe for next cycle */
    FILE *trunc = fopen(relay_path, "w");
    if (trunc) fclose(trunc);

    /* Step 2: Process buffered keys - dispatch each to move_player + choice.
     * Two separate signals:
     *  any_key   = any key was processed → frame changed, recompose needed
     *  hero_moved = hero actually acted (not just xlector cursor) → advance turns */
    int any_key = 0, hero_moved = 0;
    char *p = buf;
    char op_path[PATH_BUF];
    while (*p) {
        int keycode = -1;
        if (sscanf(p, "%d", &keycode) == 1) {
            char arg[16];
            snprintf(arg, sizeof(arg), "%d", keycode);
            /* REAL BUG FIX 2026-08-17: these run_op() calls were all
             * missing the ".+x" suffix that build.sh actually names every
             * compiled binary with (e.g. real file is
             * "camera_control.+x", not "camera_control") - execl() uses
             * the path literally, no suffix appended, so every single
             * call here silently failed (ENOENT) via run_op()'s own
             * stderr-to-/dev/null redirect, meaning move_player/choice/
             * camera_control NEVER actually ran, no matter what key was
             * relayed. This exact same bug is present verbatim in
             * +18.0G's own original game_dispatch.c too (confirmed by
             * direct comparison) - it was simply never live-tested there
             * this session (the earlier +18.0G test only observed a
             * static rendered frame from a pre-existing save, never
             * confirmed a NEW key actually produced a NEW state change
             * through this dispatch path). */
            build_op_path(op_path, sizeof(op_path), "ops/+x/move_player.+x");
            int ret = run_op(op_path, arg);
            if (ret == 0) hero_moved = 1;  /* hero acted */
            else if (ret == 2) any_key = 1; /* xlector moved */
            build_op_path(op_path, sizeof(op_path), "ops/+x/choice.+x");
            run_op(op_path, arg);
            build_op_path(op_path, sizeof(op_path), "ops/+x/camera_control.+x");
            run_op(op_path, arg);
            /* Written AFTER the three dispatch ops (not before) so their
             * own read-rewrite passes over hero_01/state.txt can't race
             * and clobber this - compose_frame.c already reads+displays
             * "Last key: N" from hero_path, it just never had a writer. */
            write_kv_int(hero_path, "last_key", keycode);
            any_key = 1;
        }
        char *nl = strchr(p, '\n');
        if (nl) p = nl + 1;
        else break;
    }

    /* Step 3a: Recompose on any keypress (hero or xlector). */
    if (any_key) {
        /* compose_frame writes current_frame.txt, including the
         * MAP3D_MARKER (0x01) byte when render_mode==1. muta_render_3d
         * (board-viewer's real voxel raymarcher) writes the actual pixel
         * overlay. The PERSISTENT system/chtpm_rgb_render daemon (already
         * running, launched by button.sh) watches current_frame.txt for
         * that marker and composites the overlay into rgb_frame.raw itself
         * - classic compose_rgb_frame.c is NOT part of this path (it writes
         * rgb_frame.raw directly from old map.txt-format data and has zero
         * knowledge of the marker/overlay convention; calling it here would
         * silently clobber whatever chtpm_rgb_render just composited). */
        build_op_path(op_path, sizeof(op_path), "ops/+x/compose_frame.+x");
        run_op(op_path, NULL);
        sync_camera_to_bv_state();
        build_op_path(op_path, sizeof(op_path), "ops/+x/muta_render_3d.+x");
        run_op(op_path, NULL);
        {
            FILE *mf = fopen("pieces/display/frame_changed.txt", "a");
            if (mf) { fprintf(mf, "E\n"); fclose(mf); }
        }
        /* REAL BUG FIX 2026-08-18, direct user report ("nav frames
         * change but not player movement on map [in terminal], like it
         * does in x11"): system/renderer.c (the terminal ASCII output,
         * see its own header comment - "Watch ONLY renderer_pulse.txt")
         * only ever wakes up on pieces/display/renderer_pulse.txt
         * growing. chtpm_parser_pal.c's own internal compose path (nav
         * clicks/menu screens) bumps that file itself
         * (chtpm_parser_pal.c ~line 3249-3250), which is why nav frames
         * already worked in the terminal - but this player-movement path
         * only ever bumped frame_changed.txt above (the marker
         * chtpm_rgb_render/the X11 GUI compositor watches), never
         * renderer_pulse.txt, so the terminal renderer had no way to
         * know a new frame existed after an arrow key. compose_frame.+x
         * itself (the standalone op just run above, confirmed via direct
         * read of ops/compose_frame.c) doesn't touch renderer_pulse.txt
         * either - nothing in the movement path did, until now. */
        {
            FILE *rp = fopen("pieces/display/renderer_pulse.txt", "a");
            if (rp) { fprintf(rp, "P\n"); fclose(rp); }
        }
    }

    /* Step 3b: Advance turns only when the hero actually acted.
     * Xlector cursor movement doesn't consume turns. */
    if (hero_moved) {
        build_op_path(op_path, sizeof(op_path), "ops/+x/end_turn.+x");
        run_op(op_path, NULL);
        build_op_path(op_path, sizeof(op_path), "ops/+x/tick_monsters.+x");
        run_op(op_path, NULL);
    }

    return 0;
}
