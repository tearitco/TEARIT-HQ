/* bv_menu_input - board-viewer's own input dispatcher.
 *
 * REWRITE 2026-08-02 (see &.widgits/interact-fix-widget.txt for the
 * full investigation): the P6 pass above was WRONG about how
 * mutaclysm's real xlector-entry mechanism works. It is NOT a plain
 * numbered METHOD row dispatched by this op - "INTERACT" is a
 * RESERVED command string chtpm_parser_pal.c itself special-cases
 * (confirmed via direct code read: onClick="INTERACT" on a real,
 * hand-written <button> element, exactly like mutaclysm's own
 * game.chtpm:7 "Control Hero" button - now mirrored in board_viewer.
 * chtpm). The engine ITSELF owns the entire mode lifecycle: setting
 * active_index on click, remapping+relaying raw arrow keys (1000-1003)
 * into interact_relay.txt ONLY while that element is engaged, handling
 * ESC-exit before any project op ever runs, and swapping the [>]/[^]
 * focus glyph - none of that is this op's job anymore, and a plain
 * piece.pdl METHOD row can NEVER trigger it (${piece_methods} rows
 * always become onClick="KEY:N", never the literal reserved string).
 *
 * Consequence: this op no longer needs (or has) any nav_mode flag,
 * gating, or ESC handling of its own. It is only ever invoked with an
 * arrow-key code (1000-1003) while genuinely inside real INTERACT mode
 * (the engine guarantees that), so arrow handling here is
 * unconditional - the gate already happened upstream.
 *
 * ADDED 2026-08-03, direct user correction ("controls like [hero
 * actions] shouldn't be available till interact mode is activated in
 * gl map view like civ etc"): any key that reaches this op AT ALL is
 * already, by construction (per this file's own header above),
 * genuinely inside real INTERACT/nav mode - render_mode==1 is also
 * checked explicitly below before this new dispatch, so it can never
 * fire from the flat 2D pre-nav-mode state either. Any key not
 * consumed by camera controls above now falls through to a real
 * widget->host verb send: looks up the FOCUSED host's own real
 * pieces/system/keybinds.txt (same file @.apps/piececraft-xyz/ops/
 * pc_menu_input.c's own header comment describes - this is the
 * board-viewer-side half of that mechanism) for this keycode, and if
 * found, appends the resolved ACTION NAME to that host's own real
 * inbox (declared via its own pieces/system/board_widget_bridge.txt,
 * same bridge/inbox mechanism civ-txt/tactics-txt already use for
 * OPEN_BOARD_WIDGET reuse). civ-txt/tactics-txt don't ship a
 * keybinds.txt, so this is a real no-op for them - only a host that
 * publishes one (piececraft-xyz) ever gets anything sent.
 *
 * Self-contained, no shared headers.
 * Usage: bv_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_BOARD_DIM 64

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

/* Camera key scheme, direct port of mutaclysm's real ops/camera_control.c
 * (full citation + per-mode table in &.widgits/5-pov-widgit.md §2e) -
 * plain ASCII letter keys pass through inject_raw_key() unchanged
 * (only arrows/joystick get remapped), so these arrive here as their
 * own literal ASCII codes, same as any other raw key. */
#define YAW_STEP   10
#define PITCH_STEP 10
#define PAN_STEP   1

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

/* Forward declaration - has_z_manifest() is defined further down this
 * same file, needed here by default_render_mode() below. */
static int has_z_manifest(const char *root, char *z_base_out, size_t z_base_sz, int *z_count_out);

/* Same real fix as bv_render_3d.c's own default_current_z() (2026-08-04,
 * direct user report about the camera starting underground) - kept as
 * a real, separate copy per this house's own no-shared-headers
 * convention, not a cross-file include. */
static int default_current_z(const char *root) {
    if (!root || !root[0]) return 0;
    char hero_state_path[PATH_BUF];
    snprintf(hero_state_path, sizeof(hero_state_path), "%s/pieces/hero_01/state.txt", root);
    int hero_pos_z = read_kv_int(hero_state_path, "pos_z", 0);
    if (!hero_pos_z) return 0;
    /* REAL FOLLOW-UP FIX 2026-08-04, direct user report ("broke the map
     * renders for 2d and 3d - blank/no emoji"): hero pos_z is the AIR
     * tile the hero stands IN (pc_generate_chunk.c's own real spawn
     * comment: "stands ON it, one level higher"), not the ground - a
     * fresh 2D session defaulting straight onto that tile shows a real,
     * correctly-rendered, entirely EMPTY Z-slice (every glyph really is
     * air there), which reads exactly like a broken/blank view even
     * though nothing crashed. Real fix: default one level BELOW the
     * hero, onto the real ground surface itself. */
    return hero_pos_z - 1;
}

/* Same real project-conditional default as bv_compose_frame.c's own
 * default_render_mode logic (2026-08-04, direct instruction "always
 * start in 3d mode") - a host with real Z-layer data defaults straight
 * into 3D, a 2D-only host (civ-txt/tactics-txt) is unaffected. Needs
 * its own has_z_manifest() check (already defined above in this same
 * file, real per-file duplicate per this house's own convention).
 * 2026-08-07, direct instruction ("start in 3d 3rd person as a
 * default, read from a config file so it's flexible"): the host's own
 * pieces/system/arrow_config.txt can now override both defaults.
 * default_render_mode=<0|1> overrides the render_mode default (missing
 * or invalid key falls back to the real has_z_manifest() conditional
 * below, so 2D-only hosts like civ-txt/tactics-txt stay 2D unless they
 * explicitly opt in). default_camera_mode=<1-4> overrides the
 * camera_mode default (missing/invalid = 2, third-person - the real
 * "start in 3d 3rd person" directive default, NOT the old bird's-eye 4). */
static int default_render_mode(const char *root) {
    if (!root || !root[0]) return 0;
    char cfg[PATH_BUF];
    snprintf(cfg, sizeof(cfg), "%s/pieces/system/arrow_config.txt", root);
    int cfg_default = read_kv_int(cfg, "default_render_mode", -1);
    if (cfg_default == 0 || cfg_default == 1) return cfg_default;
    char z_base[PATH_BUF]; int z_count = 0;
    return has_z_manifest(root, z_base, sizeof(z_base), &z_count) ? 1 : 0;
}

static int default_camera_mode(const char *root) {
    if (!root || !root[0]) return 2;
    char cfg[PATH_BUF];
    snprintf(cfg, sizeof(cfg), "%s/pieces/system/arrow_config.txt", root);
    int cfg_default = read_kv_int(cfg, "default_camera_mode", 2);
    if (cfg_default >= 1 && cfg_default <= 4) return cfg_default;
    return 2;
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Optional multi-Z chunk support - same real convention bv_compose_
 * frame.c's own resolve_board_path() and bv_render_3d.c's own
 * resolve_board_path_3d() already use (see either file's own header
 * comment for the full writeup). REAL BUG, caught live 2026-08-03:
 * this file's own arrow-key/selector-movement handling below still
 * read the flat pieces/system/board.txt DIRECTLY to compute board_w/
 * board_h for clamping - a host publishing board_manifest.txt
 * (piececraft-xyz) has no such file anymore, so fopen() failed,
 * board_w/board_h stayed 0, and the whole "if (board_w > 0 && board_h
 * > 0)" block silently never ran - the selector (and by extension
 * anything anchored to it) never moved, with no error, no message,
 * just silently doing nothing. This is the THIRD file this same
 * missing-manifest-awareness bug has been found in - 2D compose, 3D
 * render, and now selector movement - board.txt itself is fully
 * retired for any host that publishes a manifest. */
static int has_z_manifest(const char *root, char *z_base_out, size_t z_base_sz, int *z_count_out) {
    char manifest_path[PATH_BUF];
    snprintf(manifest_path, sizeof(manifest_path), "%s/pieces/system/board_manifest.txt", root);
    z_base_out[0] = '\0';
    *z_count_out = 0;
    read_kv_str(manifest_path, "z_base", z_base_out, z_base_sz);
    *z_count_out = read_kv_int(manifest_path, "z_count", 0);
    return (z_base_out[0] && *z_count_out > 0);
}

static void resolve_board_path(const char *root, int current_z, char *out, size_t out_sz) {
    char z_base[PATH_BUF];
    int z_count = 0;
    if (has_z_manifest(root, z_base, sizeof(z_base), &z_count)) {
        int clamped_z = clamp_int(current_z, 0, z_count - 1);
        snprintf(out, out_sz, "%s/%s%d.txt", root, z_base, clamped_z);
    } else {
        snprintf(out, out_sz, "%s/pieces/system/board.txt", root);
    }
}

static void bump_screen_changed(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/bv_screen_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

/* send_action_to_host - real widget->host command delivery, factored
 * out 2026-08-03 (was inline, keybind-lookup-only, in the "real widget-
 * >host verb send" block below) so movement can reuse the exact same
 * real inbox-append mechanism (civ-vs-piece.md §6a/§6b, direct
 * instruction: "movement will end turn and so can button" - both need
 * to reach the SAME real dispatch path, not two different ones). Reads
 * the focused host's own board_widget_bridge.txt for its real inbox
 * path, appends the literal action string. A host with no bridge file
 * (civ-txt/tactics-txt, before their own Phase 2+ work) silently no-ops -
 * real, matches every other cross-project write in this file. */
static void send_action_to_host(const char *focused_project_root, const char *action) {
    char bridge_path[PATH_BUF];
    snprintf(bridge_path, sizeof(bridge_path), "%s/pieces/system/board_widget_bridge.txt", focused_project_root);
    char inbox_rel[PATH_BUF] = "";
    read_kv_str(bridge_path, "inbox_path", inbox_rel, sizeof(inbox_rel));
    if (!inbox_rel[0]) return;
    char inbox_full[PATH_BUF];
    snprintf(inbox_full, sizeof(inbox_full), "%s/%s", focused_project_root, inbox_rel);
    FILE *ibf = fopen(inbox_full, "a");
    if (ibf) { fprintf(ibf, "%s\n", action); fclose(ibf); }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    int key = atoi(argv[1]);
    if (key == 0) return 0;

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/bv_state.txt", project_root);

    /* No nav_mode flag, no ESC handling, no digit dispatch here anymore
     * - all of that is now the engine's own job (chtpm_parser_pal.c's
     * real INTERACT machinery, triggered by the static <button onClick
     * ="INTERACT"> in board_viewer.chtpm). This op is only ever invoked
     * with an arrow-key code (1000-1003) while genuinely inside real
     * INTERACT mode - the engine guarantees that by construction (raw
     * keys are only relayed into interact_relay.txt while the INTERACT
     * element is the active one). See &.widgits/interact-fix-widget.txt. */
    char focused_project_root[PATH_BUF] = "";
    read_kv_str(state_path, "focused_project_root", focused_project_root, sizeof(focused_project_root));

    /* REAL FIX 2026-08-04, direct user request ("put them in config
     * file instead of hardcoding them to prevent this from happening")
     * - after several live rebuild-and-relaunch cycles arguing about
     * which literal sign is "right" (real evidence kept contradicting
     * itself, most likely due to this session's own repeated stale-
     * binary/relaunch confusion, see human-dev.md's own gotchas list),
     * the actual fix is to stop hardcoding a guess in C at all. The
     * FOCUSED HOST project now owns this mapping as real, editable
     * data (pieces/system/arrow_config.txt) - a host with none gets
     * the same real default this file always shipped with (LEFT=-1,
     * RIGHT=+1, UP=-1, DOWN=+1), so any host that never asks for this
     * (civ-txt/tactics-txt) is unaffected. A human can now fix a
     * flipped arrow by editing ONE text file and relaunching - no
     * rebuild, no C, no guessing which of us was holding the
     * coordinate system backwards this time. */
    int left_dx = -1, right_dx = 1, up_dy = -1, down_dy = 1;
    /* REAL, NEW 2026-08-04, direct instruction ("config file for all
     * the things the keys 'do'... allow custom keys in future") - same
     * real arrow_config.txt every other camera tunable already lives
     * in, extended with EVERY remaining hardcoded camera/movement key
     * literal in this file (0/9/8/z/x/1-4/f/q/e/r/t/wasd/c/v). Read
     * ONCE here (moved out of the arrow-only `if` block so it's usable
     * for the whole function), real int defaults matching this file's
     * own previous hardcoded chars exactly - a fresh install with no
     * customized keys behaves byte-identically to before this change.
     * piececraft-xyz's own real game-verb keys (JUMP/MINE/BUILD/END_
     * TURN/etc) were ALREADY this same real pattern via keybinds.txt -
     * this closes the same gap for board-viewer's own camera keys. */
    char arrow_cfg_path[PATH_BUF] = "";
    int key_toggle_render_mode = '0';
    int key_possess = '9';
    int key_reset_xelector = '8';
    int key_z_down = 'z', key_z_up = 'x';
    int key_reset_view = 'f';
    int key_yaw_left = 'q', key_yaw_right = 'e';
    int key_pitch_down = 'r', key_pitch_up = 't';
    int key_pan_forward = 'w', key_pan_back = 's', key_pan_left = 'a', key_pan_right = 'd';
    int key_cam_down = 'c', key_cam_up = 'v';
    if (focused_project_root[0]) {
        snprintf(arrow_cfg_path, sizeof(arrow_cfg_path), "%s/pieces/system/arrow_config.txt", focused_project_root);
        left_dx = read_kv_int(arrow_cfg_path, "left_dx", left_dx);
        right_dx = read_kv_int(arrow_cfg_path, "right_dx", right_dx);
        up_dy = read_kv_int(arrow_cfg_path, "up_dy", up_dy);
        down_dy = read_kv_int(arrow_cfg_path, "down_dy", down_dy);
        key_toggle_render_mode = read_kv_int(arrow_cfg_path, "key_toggle_render_mode", key_toggle_render_mode);
        key_possess = read_kv_int(arrow_cfg_path, "key_possess", key_possess);
        key_reset_xelector = read_kv_int(arrow_cfg_path, "key_reset_xelector", key_reset_xelector);
        key_z_down = read_kv_int(arrow_cfg_path, "key_z_down", key_z_down);
        key_z_up = read_kv_int(arrow_cfg_path, "key_z_up", key_z_up);
        key_reset_view = read_kv_int(arrow_cfg_path, "key_reset_view", key_reset_view);
        key_yaw_left = read_kv_int(arrow_cfg_path, "key_yaw_left", key_yaw_left);
        key_yaw_right = read_kv_int(arrow_cfg_path, "key_yaw_right", key_yaw_right);
        key_pitch_down = read_kv_int(arrow_cfg_path, "key_pitch_down", key_pitch_down);
        key_pitch_up = read_kv_int(arrow_cfg_path, "key_pitch_up", key_pitch_up);
        key_pan_forward = read_kv_int(arrow_cfg_path, "key_pan_forward", key_pan_forward);
        key_pan_back = read_kv_int(arrow_cfg_path, "key_pan_back", key_pan_back);
        key_pan_left = read_kv_int(arrow_cfg_path, "key_pan_left", key_pan_left);
        key_pan_right = read_kv_int(arrow_cfg_path, "key_pan_right", key_pan_right);
        key_cam_down = read_kv_int(arrow_cfg_path, "key_cam_down", key_cam_down);
        key_cam_up = read_kv_int(arrow_cfg_path, "key_cam_up", key_cam_up);
    }
    int dx = 0, dy = 0;
    if (key == ARROW_LEFT) dx = left_dx;
    else if (key == ARROW_RIGHT) dx = right_dx;
    else if (key == ARROW_UP) dy = up_dy;
    else if (key == ARROW_DOWN) dy = down_dy;

    /* REAL FIX 2026-08-04, direct instruction ("1st/3rd person arrow
     * move direction not the same as mode 3&4 - make it move the
     * camera in the same direction as 3&4 by default"): arrow-driven
     * movement used to be pure grid-absolute in EVERY mode - correct
     * and consistent for 3/4 (fixed/detached viewing angle, "+x" always
     * projects the same way on screen), but modes 1/2 render from
     * INSIDE the world facing the current yaw, so the same raw grid
     * step can visually go left/right/forward/back depending which way
     * you're looking - a real, inherent first-person effect, not a
     * sign-flip bug. Real fix: rotate the raw (dx,dy) vector by
     * (yaw - 180) before applying it, ONLY for modes 1/2 - 180 is this
     * file's own real default yaw (5-pov-widgit.md §2e / the '1'/'2'
     * camera-mode-switch reset above), so at that default the rotation
     * is exactly zero and modes 1/2 match 3/4's own real grid-absolute
     * behavior identically, satisfying "move camera in same direction
     * as 3&4 by default" - as you turn (q/e), movement rotates WITH
     * your facing, true camera-relative controls, matching what a
     * first-person view actually needs. Snapped to the single nearest
     * cardinal grid step (not a fractional/diagonal move) so this stays
     * a real, one-tile-per-press step like every other mode. */
    if ((dx || dy) && focused_project_root[0]) {
        int cam_mode_for_move = read_kv_int(state_path, "camera_mode", default_camera_mode(focused_project_root));
        if (cam_mode_for_move == 1 || cam_mode_for_move == 2) {
            int cam_yaw_for_move = read_kv_int(state_path, "cam_yaw", 180);
            /* NEGATED 2026-08-04, direct user correction ("right and
             * left in 1/2 are still flipped... just swap them wherever
             * that is") - real turn direction was rotating movement
             * the opposite way from the camera's own actual facing. */
            double theta = -(double)(cam_yaw_for_move - 180) * M_PI / 180.0;
            double rdx = dx * cos(theta) - dy * sin(theta);
            double rdy = dx * sin(theta) + dy * cos(theta);
            if (fabs(rdx) >= fabs(rdy)) { dx = (rdx > 0) ? 1 : (rdx < 0 ? -1 : 0); dy = 0; }
            else { dy = (rdy > 0) ? 1 : (rdy < 0 ? -1 : 0); dx = 0; }

            /* REAL FIX 2026-08-04, direct user report ("left arrow is
             * strafing right and right arrow is strafing left in 1/2
             * modes"): build_camera()'s own real right-vector (right =
             * cross(world_up, forward)) works out to (-1,0,0) at the
             * default yaw=180 - the camera's own actual rightward
             * direction points toward DECREASING world x, opposite of
             * raw dx=+1. This is a coordinate-handedness property of
             * modes 1/2 specifically (they render from INSIDE the
             * world using that right vector; modes 3/4 never use it at
             * all), independent of the turn-relative rotation just
             * above - negating dx here corrects the baseline and still
             * composes correctly as you turn (negating a rotated
             * vector = rotating the negated vector). */
            dx = -dx;
        }
    }

    if ((dx || dy) && focused_project_root[0]) {
        int current_z = read_kv_int(state_path, "current_z", default_current_z(focused_project_root));
        char board_path[PATH_BUF];
        resolve_board_path(focused_project_root, current_z, board_path, sizeof(board_path));
        FILE *bf = fopen(board_path, "r");
        int board_w = 0, board_h = 0;
        if (bf) {
            char boardline[MAX_LINE];
            while (board_h < MAX_BOARD_DIM && fgets(boardline, sizeof(boardline), bf)) {
                boardline[strcspn(boardline, "\r\n")] = '\0';
                int len = (int)strlen(boardline);
                if (len == 0) continue;
                if (len > board_w) board_w = len;
                board_h++;
            }
            fclose(bf);
        }

        if (board_w > 0 && board_h > 0) {
            int selector_x = read_kv_int(state_path, "selector_x", board_w / 2);
            int selector_y = read_kv_int(state_path, "selector_y", board_h / 2);
            selector_x = clamp_int(selector_x + dx, 0, board_w - 1);
            selector_y = clamp_int(selector_y + dy, 0, board_h - 1);
            write_kv_int(state_path, "selector_x", selector_x);
            write_kv_int(state_path, "selector_y", selector_y);

            /* REAL FIX 2026-08-03, direct user correction ("arrow key
             * is moving the camera, not xelector like it should and
             * does in civ/mutaclysm"): arrows used to ONLY update this
             * file's own local selector_x/y (a board-viewer-only
             * display/camera-anchor value) - since camera_mode 1/2's
             * own anchor is derived FROM selector_x/y, moving it moves
             * the camera, but the REAL xelector entity (pieces/
             * xelector_01/state.txt, civ-vs-piece.md §6a/§6b) never
             * actually moved. Same real cross-project-file-write
             * pattern z/x already uses: write the FOCUSED host's own
             * real xelector_01/state.txt pos_x/pos_y directly,
             * selector_x/y stays as the local mirror the camera/2D-
             * highlight code already reads. A host with no xelector_01
             * piece (civ-txt, tactics-txt) silently no-ops this
             * specific write (fopen fails, write_kv returns) but still
             * updates the local selector, so this is harmless there
             * too. The xelector ITSELF always moves freely (a cursor
             * has no gravity/collision, per direct instruction "even
             * into the sky or underground") - possession only gates
             * whether that movement ALSO drags the hero along and
             * costs a real tick, see below. */
            char xelector_state_path[PATH_BUF];
            snprintf(xelector_state_path, sizeof(xelector_state_path), "%s/pieces/xelector_01/state.txt", focused_project_root);
            write_kv_int(xelector_state_path, "pos_x", selector_x);
            write_kv_int(xelector_state_path, "pos_y", selector_y);

            /* REAL FIX 2026-08-03, direct instruction ("the tick should
             * only happen when xelector moves 'hero-avatar'" - real
             * possession gating, civ-vs-piece.md §6a/§6b + this
             * session's own '9' toggle above). Free-roam xelector
             * movement (possessed_id != hero_01) is a real, deliberate
             * no-cost camera pan - the hero never moves, no tick is
             * spent, matching a spectator/editor cursor's own real
             * precedent (mutaclysm's own xlector costs nothing to move
             * when not possessing). Only while ACTIVELY possessing
             * hero_01 does xelector movement also drag the hero along
             * (mirrored position write) AND notify the host for a real
             * tick (send_action_to_host(), same real inbox this file's
             * own keybind dispatch below uses - pc_menu_input.c's own
             * MOVE handler advances the real world tick). */
            char possessed_id[64] = "";
            read_kv_str(xelector_state_path, "possessed_id", possessed_id, sizeof(possessed_id));
            if (strcmp(possessed_id, "hero_01") == 0) {
                char hero_state_path[PATH_BUF];
                snprintf(hero_state_path, sizeof(hero_state_path), "%s/pieces/hero_01/state.txt", focused_project_root);
                write_kv_int(hero_state_path, "pos_x", selector_x);
                write_kv_int(hero_state_path, "pos_y", selector_y);
                send_action_to_host(focused_project_root, "MOVE");
            }
        }

        /* REAL FIX 2026-08-04, direct user report ("the pov for 1 and 2
         * camera doesn't move when arrow key is pressed"): this whole
         * block never called bump_screen_changed() - camera_mode 1/2's
         * own eye position is derived DIRECTLY from selector_x/y
         * (build_camera(), bv_render_3d.c), so arrow movement WAS
         * really updating the real state that determines the view, but
         * the 3D pass only ever re-renders when this marker file gets
         * bumped (the entire real reason it exists - gating an
         * expensive re-raymarch behind an actual change) - without it,
         * the already-correct new camera position just never got
         * drawn. Real fix: bump here too, same as every other real
         * state-mutating branch in this file already does. */
        bump_screen_changed(project_root);
    }

    /* Camera controls (5-pov-widgit.md §2e/§2f) - direct port of
     * mutaclysm's real ops/camera_control.c dispatch. Real render_mode/
     * camera_mode toggle is wired here; the actual 3D raymarch pass
     * '0' reveals is separate, later work (§2f, extrusion) - state
     * persists correctly regardless of whether the 3D pass exists yet,
     * so this is independently testable via bv_state.txt/receipts
     * before any 3D pixels are drawn. */
    if (key == key_toggle_render_mode) {
        int render_mode = read_kv_int(state_path, "render_mode", default_render_mode(focused_project_root));
        write_kv_int(state_path, "render_mode", !render_mode);
        bump_screen_changed(project_root);
        return 0;
    }

    /* '9' - REAL POSSESSION TOGGLE, added 2026-08-03, real precedent
     * checked directly (mutaclysm's own dox/ctrl-legend.md: "9 |
     * choice.c | Release possession / reverse-jump"). Real precedent is
     * MORE specific than a plain toggle - mutaclysm enters possession
     * via "panel commit" (walk the xlector onto a possessable piece,
     * confirm), and 9 only ever RELEASES it. piececraft-xyz has exactly
     * ONE possessable target right now (hero_01, no scanning/panel
     * system built) - direct instruction confirmed a real, deliberate
     * simplification of that precedent is fine here: 9 toggles both
     * ways given there's no ambiguity about WHICH entity to possess.
     * Entering possession jumps the xelector to the hero's own real
     * current position (matching precedent's own "reverse-jump" naming
     * - the release direction jumps back; entering jumps TO the
     * target, the natural inverse). A host with no xelector_01/hero_01
     * pieces (civ-txt/tactics-txt) silently no-ops (fopen fails,
     * write_kv/read_kv_str return gracefully). */
    if (key == key_possess && focused_project_root[0]) {
        char xelector_state_path[PATH_BUF];
        snprintf(xelector_state_path, sizeof(xelector_state_path), "%s/pieces/xelector_01/state.txt", focused_project_root);
        char possessed_id[64] = "";
        read_kv_str(xelector_state_path, "possessed_id", possessed_id, sizeof(possessed_id));

        if (strcmp(possessed_id, "hero_01") == 0) {
            /* REAL FIX 2026-08-04, direct user report ("1st/3rd person
             * won't unposses for some reason??"): this branch used to
             * ONLY clear possessed_id - the header comment above always
             * PROMISED a real "reverse-jump" (release direction jumps
             * BACK) but never actually implemented one, so the
             * xelector's own position (and this file's own local
             * selector_x/y camera mirror, which modes 1/2's own anchor
             * is built from) never changed - release genuinely worked
             * (possession state DID clear, movement stopped dragging
             * the hero/costing a tick), it just LOOKED like nothing
             * happened, since the camera never moved. Real fix: restore
             * the xelector to wherever it real was BEFORE this same
             * possess (pre_possess_x/y/z, saved by the possess branch
             * below), matching the promised real reverse-jump. */
            int cur_sel_x = read_kv_int(state_path, "selector_x", 0);
            int cur_sel_y = read_kv_int(state_path, "selector_y", 0);
            int px = read_kv_int(xelector_state_path, "pre_possess_x", cur_sel_x);
            int py = read_kv_int(xelector_state_path, "pre_possess_y", cur_sel_y);
            int pz = read_kv_int(xelector_state_path, "pre_possess_z", 0);
            write_kv(xelector_state_path, "possessed_id", "none");
            write_kv_int(xelector_state_path, "pos_x", px);
            write_kv_int(xelector_state_path, "pos_y", py);
            write_kv_int(xelector_state_path, "pos_z", pz);
            write_kv_int(state_path, "selector_x", px);
            write_kv_int(state_path, "selector_y", py);
            write_kv_int(state_path, "current_z", pz);
        } else {
            /* Possess - jump xelector to hero's own real position,
             * matching precedent's own "reverse-jump" naming (the
             * inverse direction: jump TO the target on entry). Real,
             * NEW 2026-08-04: saves the xelector's own CURRENT position
             * first (pre_possess_x/y/z) so the release branch above has
             * somewhere real to jump back to. */
            int prev_x = read_kv_int(xelector_state_path, "pos_x", read_kv_int(state_path, "selector_x", 0));
            int prev_y = read_kv_int(xelector_state_path, "pos_y", read_kv_int(state_path, "selector_y", 0));
            int prev_z = read_kv_int(xelector_state_path, "pos_z", 0);
            write_kv_int(xelector_state_path, "pre_possess_x", prev_x);
            write_kv_int(xelector_state_path, "pre_possess_y", prev_y);
            write_kv_int(xelector_state_path, "pre_possess_z", prev_z);

            char hero_state_path[PATH_BUF];
            snprintf(hero_state_path, sizeof(hero_state_path), "%s/pieces/hero_01/state.txt", focused_project_root);
            int hx = read_kv_int(hero_state_path, "pos_x", 0);
            int hy = read_kv_int(hero_state_path, "pos_y", 0);
            int hz = read_kv_int(hero_state_path, "pos_z", 0);
            write_kv_int(xelector_state_path, "pos_x", hx);
            write_kv_int(xelector_state_path, "pos_y", hy);
            write_kv_int(xelector_state_path, "pos_z", hz);
            write_kv(xelector_state_path, "possessed_id", "hero_01");
            /* Mirror into board-viewer's own local display state so
             * the camera/selector snap to the hero immediately, same
             * real pattern z/x's own current_z mirror already uses. */
            write_kv_int(state_path, "selector_x", hx);
            write_kv_int(state_path, "selector_y", hy);
            write_kv_int(state_path, "current_z", hz);
        }
        bump_screen_changed(project_root);
        return 0;
    }

    /* '8' - REAL "reset xelector to a viewable position" key, direct
     * instruction 2026-08-04 ("use '8' key to reset xelector to the
     * 'viewable' position" - same session as the "cam always reset
     * showing underground" report). Real, simple definition of
     * "viewable": wherever the hero actually is right now (the same
     * real position '9's own possess-jump already uses above) - jumps
     * the xelector there UNCONDITIONALLY, regardless of current
     * possession state (unlike '9', which only jumps as a side effect
     * of possessing), and syncs board-viewer's own local mirror +
     * camera height the same real way '9's possess branch does, so a
     * lost/stray xelector (or a camera stuck looking at empty sky/
     * underground) always has one guaranteed way back to something
     * real on screen. */
    if (key == key_reset_xelector && focused_project_root[0]) {
        char xelector_state_path[PATH_BUF];
        snprintf(xelector_state_path, sizeof(xelector_state_path), "%s/pieces/xelector_01/state.txt", focused_project_root);
        char hero_state_path[PATH_BUF];
        snprintf(hero_state_path, sizeof(hero_state_path), "%s/pieces/hero_01/state.txt", focused_project_root);
        int hx = read_kv_int(hero_state_path, "pos_x", 0);
        int hy = read_kv_int(hero_state_path, "pos_y", 0);
        int hz = read_kv_int(hero_state_path, "pos_z", 0);
        write_kv_int(xelector_state_path, "pos_x", hx);
        write_kv_int(xelector_state_path, "pos_y", hy);
        write_kv_int(xelector_state_path, "pos_z", hz);
        write_kv_int(state_path, "selector_x", hx);
        write_kv_int(state_path, "selector_y", hy);
        write_kv_int(state_path, "current_z", hz);
        int camera_mode = read_kv_int(state_path, "camera_mode", default_camera_mode(focused_project_root));
        if (camera_mode == 3) write_kv_int(state_path, "cam_pan_y", hz);
        bump_screen_changed(project_root);
        return 0;
    }

    /* z/x - REAL XELECTOR Z MOVEMENT, corrected 2026-08-03 (civ-vs-
     * piece.md §6a/§6b - direct user correction: the first pass here
     * only ever changed board-viewer's own LOCAL display toggle
     * (bv_state.txt's own current_z), never any real entity position -
     * "why doesn't the xelector move" was the right question, the
     * xelector didn't exist as its own real piece yet). Real precedent:
     * mutaclysm's own dox/ctrl-legend.md ("z = Hero Z level -1", "x =
     * Hero Z level +1") and fuzz-op's own real pieces/xlector/state.txt
     * fixture (a genuinely SEPARATE piece with its own real pos_x/y/z,
     * not fields on the possessed entity's own state). This handler now
     * writes the FOCUSED host's own real pieces/xelector_01/state.txt
     * pos_z directly - free movement, "even into the sky or
     * underground" per direct instruction, unconstrained by any entity
     * rule (gravity/collision only ever apply to whatever the xelector
     * currently POSSESSES, a later Phase 2+ mechanic - possessed_id
     * exists as a real field now, nothing enforces it yet) - clamped
     * only to the manifest's own real z_count range (the build-height
     * limit, not a gameplay constraint). board-viewer's own current_z
     * is then set to MIRROR that same value, so the displayed slice
     * always follows the xelector's own real position, never drifts
     * from it. ONE DELIBERATE DIVERGENCE from mutaclysm's own
     * precedent, flagged directly: mutaclysm gates z/x to "3D only
     * (render_mode=1)" - this project's own 3D per-Z voxel rendering
     * doesn't exist yet, so gating identically would make this whole
     * feature untestable today - kept working in 2D for now, a real,
     * testable, documented divergence. A host with no xelector_01 piece
     * (civ-txt/tactics-txt) silently no-ops the position write (fopen
     * fails, write_kv returns) but still updates board-viewer's own
     * local current_z, so this is harmless there too. */
    if (focused_project_root[0] && (key == key_z_down || key == key_z_up)) {
        char xelector_state_path[PATH_BUF];
        snprintf(xelector_state_path, sizeof(xelector_state_path), "%s/pieces/xelector_01/state.txt", focused_project_root);
        int xz = read_kv_int(xelector_state_path, "pos_z", 0);
        xz += (key == key_z_up) ? 1 : -1;
        if (xz < 0) xz = 0;

        char z_base[PATH_BUF];
        int z_count = 0;
        if (has_z_manifest(focused_project_root, z_base, sizeof(z_base), &z_count) && z_count > 0) {
            xz = clamp_int(xz, 0, z_count - 1);
        }

        write_kv_int(xelector_state_path, "pos_z", xz);
        write_kv_int(state_path, "current_z", xz);

        /* REAL FIX 2026-08-03 - same real possession gating as arrow-
         * key movement above (civ-vs-piece.md §6a/§6b, direct
         * instruction "the tick should only happen when xelector moves
         * hero-avatar"): vertical xelector movement is free/no-cost
         * while NOT possessing hero_01 - only drags the hero along and
         * spends a real tick while actively possessing. */
        char possessed_id_z[64] = "";
        read_kv_str(xelector_state_path, "possessed_id", possessed_id_z, sizeof(possessed_id_z));
        if (strcmp(possessed_id_z, "hero_01") == 0) {
            char hero_state_path_z[PATH_BUF];
            snprintf(hero_state_path_z, sizeof(hero_state_path_z), "%s/pieces/hero_01/state.txt", focused_project_root);
            write_kv_int(hero_state_path_z, "pos_z", xz);
            send_action_to_host(focused_project_root, "MOVE");
        }

        bump_screen_changed(project_root);
        return 0;
    }

    int render_mode = read_kv_int(state_path, "render_mode", default_render_mode(focused_project_root));
    if (!render_mode) {
        /* Camera controls are a no-op unless render_mode==1 - matches
         * mutaclysm's own ops/camera_control.c exactly (whole dispatch
         * gated on render_mode==1 there too). Selector movement above
         * already handled arrows unconditionally (2D mode still needs
         * cursor movement regardless of render_mode). */
        bump_screen_changed(project_root);
        return 0;
    }

    /* is_pov_key equivalent: '1'-'4' only reinterpreted as camera_mode
     * switches while render_mode==1 (ops/choice.c:915's real gate). */
    if (key >= '1' && key <= '4') {
        int camera_mode = key - '0';
        write_kv_int(state_path, "camera_mode", camera_mode);
        if (camera_mode == 1 || camera_mode == 2) {
            write_kv_int(state_path, "cam_yaw", 180);
            write_kv_int(state_path, "cam_pitch", 6);
        } else if (camera_mode == 4) {
            /* Center pan on the selector (same as 'f' reset below, and
             * for the same reason: mode 4's pan is ABSOLUTE map coords,
             * per mutaclysm's own real convention - defaulting to (0,0)
             * centers the view on the board's own CORNER instead of
             * wherever the 2D view was actually looking, a real,
             * user-caught bug - "the location of map should start the
             * same place that 2D emoji does". */
            int sel_x = read_kv_int(state_path, "selector_x", 0);
            int sel_y = read_kv_int(state_path, "selector_y", 0);
            write_kv_int(state_path, "cam_pan_x", sel_x);
            write_kv_int(state_path, "cam_pan_y", sel_y);
            write_kv_int(state_path, "cam_pan_z", 0);
            write_kv_int(state_path, "cam_yaw", 180);
            write_kv_int(state_path, "cam_pitch", -90);
        } else {
            /* REAL FIX 2026-08-04 (direct user report: "2d emoji mode
             * starts in underground, should be above ground" - same
             * real root cause hits mode 3 free-roam too): cam_pan_y IS
             * the real world HEIGHT in mode 3 (unlike mode 4's own
             * cam_pan_y, which is a board ROW, not a height - two
             * genuinely different meanings sharing one field name, see
             * the mode-4 branch just above) - hardcoding 0 puts the
             * free-roam camera at world height 0, which is real
             * underground for any project whose ground surface sits
             * higher than that (piececraft-xyz's own debug map: ~16).
             * Real fix: reuse current_z (already tracked per-project,
             * already the "which Z-layer am I viewing" value every
             * other real reset in this file uses) as the height
             * instead - a real, already-meaningful default, not a
             * magic constant. */
            int cz = read_kv_int(state_path, "current_z", default_current_z(focused_project_root));
            write_kv_int(state_path, "cam_pan_x", 0);
            write_kv_int(state_path, "cam_pan_y", cz);
            write_kv_int(state_path, "cam_pan_z", 0);
            write_kv_int(state_path, "cam_yaw", 180);
            write_kv_int(state_path, "cam_pitch", -90);
        }
        bump_screen_changed(project_root);
        return 0;
    }

    int camera_mode = read_kv_int(state_path, "camera_mode", default_camera_mode(focused_project_root));

    if (key == key_reset_view) {
        /* Reset, mode-dependent default (camera_control.c's own 'f'
         * handling per mode - see 5-pov-widgit.md §2e table). */
        if (camera_mode == 1 || camera_mode == 2) {
            write_kv_int(state_path, "cam_yaw", 180);
            write_kv_int(state_path, "cam_pitch", 6);
        } else if (camera_mode == 3) {
            /* Same real "cam_pan_y is a height, not a row" fix as the
             * '1'-'4' switch block above. */
            int cz = read_kv_int(state_path, "current_z", default_current_z(focused_project_root));
            write_kv_int(state_path, "cam_pan_x", 0);
            write_kv_int(state_path, "cam_pan_y", cz);
            write_kv_int(state_path, "cam_pan_z", 0);
            write_kv_int(state_path, "cam_yaw", 180);
            write_kv_int(state_path, "cam_pitch", -90);
        } else if (camera_mode == 4 && focused_project_root[0]) {
            /* Center pan on the selector (this widget's own hero-
             * equivalent anchor - no separate hero to center on). */
            int sel_x = read_kv_int(state_path, "selector_x", 0);
            int sel_y = read_kv_int(state_path, "selector_y", 0);
            write_kv_int(state_path, "cam_pan_x", sel_x);
            write_kv_int(state_path, "cam_pan_y", sel_y);
        }
        bump_screen_changed(project_root);
        return 0;
    }

    /* q/e yaw, r/t pitch - live in modes 1/2/3, no-op in mode 4 (exact
     * per-mode table, 5-pov-widgit.md §2e). */
    if (camera_mode != 4 && (key == key_yaw_left || key == key_yaw_right)) {
        int yaw = read_kv_int(state_path, "cam_yaw", 180);
        yaw += (key == key_yaw_right) ? YAW_STEP : -YAW_STEP;
        write_kv_int(state_path, "cam_yaw", yaw);
        bump_screen_changed(project_root);
        return 0;
    }
    if (camera_mode != 4 && (key == key_pitch_down || key == key_pitch_up)) {
        int pitch = read_kv_int(state_path, "cam_pitch", 6);
        pitch += (key == key_pitch_up) ? PITCH_STEP : -PITCH_STEP;
        pitch = clamp_int(pitch, -89, 89);
        write_kv_int(state_path, "cam_pitch", pitch);
        bump_screen_changed(project_root);
        return 0;
    }

    /* w/a/s/d pan - only modes 3/4, different axis mapping per mode
     * (free-roam pans on z/x; bird's-eye pans on y/x - matches
     * camera_control.c's own real, distinct mode-3-vs-4 mapping). */
    if ((camera_mode == 3 || camera_mode == 4) &&
        (key == key_pan_forward || key == key_pan_left || key == key_pan_back || key == key_pan_right)) {
        if (camera_mode == 3) {
            int pan_z = read_kv_int(state_path, "cam_pan_z", 0);
            int pan_x = read_kv_int(state_path, "cam_pan_x", 0);
            if (key == 'w') pan_z += PAN_STEP;
            else if (key == 's') pan_z -= PAN_STEP;
            else if (key == 'a') pan_x -= PAN_STEP;
            else if (key == 'd') pan_x += PAN_STEP;
            write_kv_int(state_path, "cam_pan_z", pan_z);
            write_kv_int(state_path, "cam_pan_x", pan_x);
        } else {
            int pan_y = read_kv_int(state_path, "cam_pan_y", 0);
            int pan_x = read_kv_int(state_path, "cam_pan_x", 0);
            if (key == 'w') pan_y -= PAN_STEP;
            else if (key == 's') pan_y += PAN_STEP;
            else if (key == 'a') pan_x -= PAN_STEP;
            else if (key == 'd') pan_x += PAN_STEP;
            write_kv_int(state_path, "cam_pan_y", pan_y);
            write_kv_int(state_path, "cam_pan_x", pan_x);
        }
        bump_screen_changed(project_root);
        return 0;
    }

    /* c/v z-level - modes 3/4 only. */
    if ((camera_mode == 3 || camera_mode == 4) && (key == key_cam_down || key == key_cam_up)) {
        int z_level = read_kv_int(state_path, "cam_z_level", 0);
        z_level += (key == key_cam_down) ? 1 : -1;
        write_kv_int(state_path, "cam_z_level", z_level);
        bump_screen_changed(project_root);
        return 0;
    }

    /* Real widget->host verb send - see this file's own header comment
     * (2026-08-03 addition). Every key reaching this point is already
     * confirmed inside real INTERACT/nav mode (render_mode==1, checked
     * above) and didn't match any built-in camera control.
     *
     * REAL POSSESSION GATE, added 2026-08-03, direct instruction:
     * "when possessed also, user receives heros move methods, which
     * will include jump/mine etc" - keybinds.txt today only ever
     * declares real hero abilities (JUMP/MINE/BUILD, civ-vs-piece.md
     * §2/§6a), so the whole dispatch below is gated on actively
     * possessing hero_01 - an unpossessed free-roaming xelector has no
     * business jumping/mining, it's just a camera cursor. A finer-
     * grained per-action gate (if a future keybind ever needs to work
     * regardless of possession) is real future work, not needed while
     * every declared action is hero-only. */
    char possessed_id_verb[64] = "";
    if (focused_project_root[0]) {
        char xelector_state_path_verb[PATH_BUF];
        snprintf(xelector_state_path_verb, sizeof(xelector_state_path_verb), "%s/pieces/xelector_01/state.txt", focused_project_root);
        read_kv_str(xelector_state_path_verb, "possessed_id", possessed_id_verb, sizeof(possessed_id_verb));
    }
    if (focused_project_root[0] && strcmp(possessed_id_verb, "hero_01") == 0) {
        char keybinds_path[PATH_BUF];
        snprintf(keybinds_path, sizeof(keybinds_path), "%s/pieces/system/keybinds.txt", focused_project_root);
        FILE *kf = fopen(keybinds_path, "r");
        if (kf) {
            char kline[MAX_LINE];
            char action[64] = "";
            while (fgets(kline, sizeof(kline), kf)) {
                kline[strcspn(kline, "\r\n")] = '\0';
                if (!kline[0] || kline[0] == '#') continue;
                char *eq = strchr(kline, '=');
                if (!eq) continue;
                *eq = '\0';
                if (atoi(kline) == key) {
                    snprintf(action, sizeof(action), "%s", eq + 1);
                    break;
                }
            }
            fclose(kf);

            if (action[0]) {
                send_action_to_host(focused_project_root, action);
            }
        }
    }

    bump_screen_changed(project_root);
    return 0;
}
