#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE 1024

typedef struct {
    int xel_x, xel_y, xel_z;
    int display_mode;
    int debug_mode_on;
    char active_map_id[256];
    int pet_x, pet_y;
    /* Camera: WASD pans (dedicated to camera only — arrows own entity
       movement), 1/2/3 switch fixed POV presets. Matches piececraft-3d's
       CAMERA_MOVE/CAMERA_MODE verb naming conceptually, but implemented
       as plain KEY_PRESSED handling here since piececraft-wraith already
       routes all raw keys through this op while in map-control mode —
       no need to add a COMMAND: verb to wraith-alpha's dispatcher for this. */
    double cam_pan_x, cam_pan_y, cam_pan_z;
    int camera_mode;
    /* Mouse-driven camera orbit (only fed while in map-control mode —
       wraith-alpha forwards MOUSE_DRAG events to this op just like it
       forwards KEY_PRESSED). cam_yaw spins the map turntable-style; cam_pitch
       is a delta added on top of the active POV preset's tilt. Matches
       plugy3d's free-camera mouse orbit — see py3d-inspo.md. */
    double cam_yaw, cam_pitch;
} GameState;

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    snprintf(out, out_sz, "%s/%s", a, b);
}

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

/* 2026-07-11: reads CHROME_CONTENT_START's live value, published by
 * wraith-alpha_manager.c (see that file's publish_chrome_reserved_nav_count(),
 * added same day) to pieces/display/chrome_reserved_nav_count.txt, instead
 * of hardcoding this project's own guess about where its nav range may
 * safely start. This ops binary is fork+exec'd by
 * wraith-alpha_manager.c's run_active_project_input_op() WITHOUT a chdir(),
 * so it inherits the manager's own cwd (confirmed: the manager itself opens
 * plenty of its own paths as plain "pieces/..." relative strings, e.g.
 * truncate_file("pieces/display/frame_changed.txt") in bootstrap_fresh_session()) --
 * the relative path below is correct without combining it with `root`
 * (which is this PROJECT's own dir, not the repo root). Falls back to the
 * literal this file always used before if the manager hasn't published yet
 * (e.g. this op running standalone/before the manager's first frame) --
 * matching WRAITH_RGB_ARCHITECTURE.md's flagged-but-undone follow-up:
 * "if it's not in a file, it's a lie" instead of nine files guessing. */
static int read_chrome_content_start(int fallback) {
    FILE *f = fopen("pieces/display/chrome_reserved_nav_count.txt", "r");
    int value;
    if (!f) return fallback;
    if (fscanf(f, "%d", &value) != 1 || value <= 0) {
        fclose(f);
        return fallback;
    }
    fclose(f);
    return value;
}

static void set_defaults(GameState *st) {
    memset(st, 0, sizeof(*st));
    st->xel_x = 4;
    st->xel_y = 2;
    st->xel_z = 0;
    st->display_mode = 0;
    st->debug_mode_on = 0;
    snprintf(st->active_map_id, sizeof(st->active_map_id), "map_01");
    /* Fixed test position for now — no pet AI/movement yet, this just
       proves the entity-rendering pipeline (separate from xelector,
       separate from tile extrusion) works end to end. */
    st->pet_x = 14;
    st->pet_y = 6;
    st->cam_pan_x = 0.0;
    st->cam_pan_y = 0.0;
    st->cam_pan_z = 0.0;
    st->camera_mode = 1;
    st->cam_yaw = 0.0;
    st->cam_pitch = 0.0;
}

static void load_state(const char *root, GameState *st) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE];
    FILE *f;
    set_defaults(st);
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "xel_x=", 6) == 0) st->xel_x = atoi(line + 6);
        else if (strncmp(line, "xel_y=", 6) == 0) st->xel_y = atoi(line + 6);
        else if (strncmp(line, "xel_z=", 6) == 0) st->xel_z = atoi(line + 6);
        else if (strncmp(line, "display_mode=", 13) == 0) st->display_mode = (strcmp(line + 13, "3d_voxel") == 0) ? 1 : 0;
        else if (strncmp(line, "debug_mode_on=", 14) == 0) st->debug_mode_on = atoi(line + 14);
        else if (strncmp(line, "pet_x=", 6) == 0) st->pet_x = atoi(line + 6);
        else if (strncmp(line, "pet_y=", 6) == 0) st->pet_y = atoi(line + 6);
        else if (strncmp(line, "cam_pan_x=", 10) == 0) st->cam_pan_x = atof(line + 10);
        else if (strncmp(line, "cam_pan_y=", 10) == 0) st->cam_pan_y = atof(line + 10);
        else if (strncmp(line, "cam_pan_z=", 10) == 0) st->cam_pan_z = atof(line + 10);
        else if (strncmp(line, "camera_mode=", 12) == 0) st->camera_mode = atoi(line + 12);
        else if (strncmp(line, "cam_yaw=", 8) == 0) st->cam_yaw = atof(line + 8);
        else if (strncmp(line, "cam_pitch=", 10) == 0) st->cam_pitch = atof(line + 10);
    }
    fclose(f);
}

static void apply_key(GameState *st, int key) {
    const double pan_step = 1.0;
    switch (key) {
        case 1000: if (st->xel_x > 1) st->xel_x--; break;
        case 1001: if (st->xel_x < 18) st->xel_x++; break;
        case 1002: if (st->xel_y > 1) st->xel_y--; break;
        case 1003: if (st->xel_y < 8) st->xel_y++; break;
        /* Xelector z-level (floor switching) — moved off Z/X (was there
           originally) to free those keys for camera vertical pan, per
           user direction: Z/X are camera-only now, same as WASD. Q/E
           picked as an unused, conventional up/down pair. */
        case 'e': case 'E': if (st->xel_z < 2) st->xel_z++; break;
        case 'q': case 'Q': if (st->xel_z > 0) st->xel_z--; break;
        case '8': st->display_mode = (st->display_mode == 0) ? 1 : 0; break;
        case '9': st->debug_mode_on = (st->debug_mode_on == 0) ? 1 : 0; break;
        /* Camera only — WASD pans horizontally/depth, Z/X pans vertically.
           None of these touch entity state; arrows own all entity
           movement exclusively. */
        case 'w': case 'W': st->cam_pan_z -= pan_step; break;
        case 's': case 'S': st->cam_pan_z += pan_step; break;
        case 'a': case 'A': st->cam_pan_x -= pan_step; break;
        case 'd': case 'D': st->cam_pan_x += pan_step; break;
        case 'x': case 'X': st->cam_pan_y += pan_step; break;
        case 'z': case 'Z': st->cam_pan_y -= pan_step; break;
        case '1': st->camera_mode = 1; break;
        case '2': st->camera_mode = 2; break;
        case '3': st->camera_mode = 3; break;
        default: break;
    }
}

/* Mouse drag → camera orbit. dx spins yaw (turntable), dy tilts pitch.
   Sensitivities are degrees-per-pixel; signs chosen so dragging right
   spins the map right and dragging up tilts the view up (flip either if
   it feels inverted). Pitch delta is clamped so the ground can't invert.
   Only ever called for drags wraith-alpha forwarded while in map-control
   mode. */
static void apply_mouse_drag(GameState *st, int dx, int dy) {
    const double yaw_sens = 0.4;
    const double pitch_sens = 0.3;
    st->cam_yaw += dx * yaw_sens;
    st->cam_pitch -= dy * pitch_sens;
    if (st->cam_pitch > 40.0) st->cam_pitch = 40.0;
    if (st->cam_pitch < -40.0) st->cam_pitch = -40.0;
    /* Keep yaw in a readable range; wrap at +/-360. */
    if (st->cam_yaw > 360.0) st->cam_yaw -= 360.0;
    if (st->cam_yaw < -360.0) st->cam_yaw += 360.0;
}

/* Parses "MOUSE_DRAG: <dx> <dy>" out of a history line. Returns 1 and
   fills dx/dy on match, 0 otherwise. Mirrors key_from_history_line(). */
static int mouse_drag_from_history_line(const char *line, int *dx, int *dy) {
    const char *p = strstr(line, "MOUSE_DRAG:");
    if (!p) return 0;
    p += strlen("MOUSE_DRAG:");
    return sscanf(p, "%d %d", dx, dy) == 2;
}

static void render_body(const char *root, const GameState *st) {
    char op_path[MAX_PATH_LEN], map_path[MAX_PATH_LEN], temp_map[MAX_PATH_LEN], body_path[MAX_PATH_LEN], body_tmp[MAX_PATH_LEN], cmd[MAX_PATH_LEN * 2];
    char line[512];
    FILE *map_file, *body_file;

    /* root is already the piececraft project dir (argv[1] from
       wraith-alpha's run_active_project_input_op, = active_project_dir()),
       so these are root-relative — NOT root + the full projects/... prefix
       again. The old double-nested paths never resolved, so the system()
       call silently failed (2>/dev/null) and the ASCII/text map grid was
       always blank. This is the 2D top-down grid the ASCII presenter
       shows; render_body is display-mode-agnostic, so ASCII always renders
       2D regardless of the GL view's 2D/3D toggle. */
    snprintf(op_path, sizeof(op_path),
             "%s/ops/+x/render_map_wraith.+x", root);
    snprintf(map_path, sizeof(map_path),
             "%s/maps/%s_z%d.txt",
             root, st->active_map_id, st->xel_z);
    path_join(temp_map, sizeof(temp_map), root, "session/temp_map.txt");
    path_join(body_path, sizeof(body_path), root, "session/wraith_body.txt");
    snprintf(body_tmp, sizeof(body_tmp), "%s.tmp", body_path);

    snprintf(cmd, sizeof(cmd), "'%s' %d %d %d '%s' > '%s' 2>/dev/null",
             op_path, st->xel_x, st->xel_y, st->xel_z, map_path, temp_map);
    system(cmd);

    /* Atomic write (temp file + rename) — matches the convention used
       everywhere in wraith-alpha_manager.c for state files. The original
       direct fopen(path,"w") pattern (copied from wraith-3d-cube's
       reference op) truncates the file immediately on open, leaving a
       window where a concurrent reader (Wraith's own
       read_project_map_control(), or this op's next load_state() call)
       can see a partial/empty file — this was the actual root cause of
       is_map_control appearing to flicker false. See
       ARCHITECTURE-RGB-RENDERING.md for the full trace that found this. */
    body_file = fopen(body_tmp, "w");
    if (!body_file) return;

    fprintf(body_file, "╔════════════════════════════════════════════╗\n");
    fprintf(body_file, "║   PIECECRAFT WRAITH - MAP VIEW             ║\n");
    fprintf(body_file, "╚════════════════════════════════════════════╝\n\n");

    map_file = fopen(temp_map, "r");
    if (map_file) {
        while (fgets(line, sizeof(line), map_file)) {
            fprintf(body_file, "%s", line);
        }
        fclose(map_file);
    }

    fprintf(body_file, "\n");
    fprintf(body_file, "─────────────────────────────────────────────\n");
    fprintf(body_file, "Position: (%d, %d, %d) | Mode: %s | Debug: %s\n",
            st->xel_x, st->xel_y, st->xel_z,
            (st->display_mode == 0) ? "2D" : "3D",
            st->debug_mode_on ? "ON" : "OFF");
    fprintf(body_file, "─────────────────────────────────────────────\n\n");
    fprintf(body_file, "CONTROLS:\n");
    fprintf(body_file, "  Arrows move | Q/E z-level | 8 mode | 9 debug | WASD+ZX camera | 1/2/3 POV | ESC menu\n");

    fclose(body_file);
    rename(body_tmp, body_path);
    remove(temp_map);
}

static void save_state(const char *root, const GameState *st) {
    char state_path[MAX_PATH_LEN], state_tmp[MAX_PATH_LEN], body_path[MAX_PATH_LEN], line[512];
    FILE *f, *body_f;
    int is_map_control = 1;

    path_join(state_path, sizeof(state_path), root, "session/state.txt");
    path_join(body_path, sizeof(body_path), root, "session/wraith_body.txt");
    snprintf(state_tmp, sizeof(state_tmp), "%s.tmp", state_path);

    /* 2026-07-11: this used to hardcode "is_map_control=1" unconditionally
       on every keypress. is_map_control is a shared field: the desktop
       (wraith-alpha_manager.c's set_project_map_control_in_dir(), fired
       by the Control_Map button's onClick=INTERACT / by ESC) is the real
       owner of this toggle and writes 0 or 1 into this SAME state.txt.
       Because this op runs synchronously after every keypress
       (run_active_project_input_op()) and always wrote the literal 1
       here, it silently stomped the desktop's own ESC-driven
       is_map_control=0 back to 1 on the very next keystroke -- so ESC
       appeared to work for exactly one tick before the trap re-armed
       itself. Confirmed live: wraith-alpha_manager.c's route_input()
       forwards EVERY key (including arrows) as raw map input and skips
       normal nav-index advancement whenever is_map_control reads true
       (see that function's own early-return branch), so a permanently
       re-asserted 1 made UI nav (arrows/digit-jump) look "stuck" the
       whole time this window was focused. Fix: read the CURRENT value
       out of the existing state.txt first (matching the desktop's own
       read-modify-write pattern in set_project_map_control_in_dir()) and
       preserve it, instead of hardcoding the literal. Defaults to 1 only
       when the file doesn't exist yet (first run), matching prior
       first-open behavior. */
    {
        FILE *existing = fopen(state_path, "r");
        if (existing) {
            char existing_line[256];
            while (fgets(existing_line, sizeof(existing_line), existing)) {
                if (strncmp(existing_line, "is_map_control=", 15) == 0) {
                    is_map_control = atoi(existing_line + 15);
                    break;
                }
            }
            fclose(existing);
        }
    }

    /* Atomic write — see render_body()'s comment for why this matters:
       this is the exact file Wraith's read_project_map_control() re-reads
       on every keystroke, so a non-atomic write here is what caused
       is_map_control to intermittently read as false. */
    f = fopen(state_tmp, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/piececraft-wraith\n");
    fprintf(f, "reference_project=projects/piececraft-3d\n");
    fprintf(f, "is_map_control=%d\n", is_map_control);
    fprintf(f, "active_z=%d\n", st->xel_z);
    fprintf(f, "xel_x=%d\n", st->xel_x);
    fprintf(f, "xel_y=%d\n", st->xel_y);
    fprintf(f, "xel_z=%d\n", st->xel_z);
    fprintf(f, "active_tile=.\n");
    fprintf(f, "display_mode=%s\n", st->display_mode == 0 ? "2d_topdown" : "3d_voxel");
    fprintf(f, "debug_mode_on=%d\n", st->debug_mode_on);
    fprintf(f, "camera_mode=%d\n", st->camera_mode);
    fprintf(f, "cam_pan_x=%.2f\n", st->cam_pan_x);
    fprintf(f, "cam_pan_y=%.2f\n", st->cam_pan_y);
    fprintf(f, "cam_pan_z=%.2f\n", st->cam_pan_z);
    fprintf(f, "cam_yaw=%.2f\n", st->cam_yaw);
    fprintf(f, "cam_pitch=%.2f\n", st->cam_pitch);
    fprintf(f, "pet_x=%d\n", st->pet_x);
    fprintf(f, "pet_y=%d\n", st->pet_y);

    fprintf(f, "game_map=");
    body_f = fopen(body_path, "r");
    if (body_f) {
        int first = 1;
        while (fgets(line, sizeof(line), body_f)) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            if (!first) fprintf(f, "\\n");
            fprintf(f, "%s", line);
            first = 0;
        }
        fclose(body_f);
    } else {
        fprintf(f, "[Map rendering...]");
    }
    fprintf(f, "\n");
    fprintf(f, "last_response=Game running.\n");
    fclose(f);
    rename(state_tmp, state_path);
}

static void write_scene(const char *root, const GameState *st) {
    char path[MAX_PATH_LEN], tmp_path[MAX_PATH_LEN];
    FILE *f;
    int nav_base = read_chrome_content_start(6);
    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;

    fprintf(f, "# Project-owned Wraith scene records.\n");
    fprintf(f, "# Piececraft world standard: maps/map_01_z*.txt + assets/tiles/registry.txt + tile extrude fields.\n");
    fprintf(f, "OBJECT tag=control id=control_map role=window_toolbar_item x=35 y=5 w=26 h=1 z=26 nav=%d source=semantic:window_toolbar fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=INTERACT target_surface=game_map label=Control_Map\n", nav_base + 0);
    fprintf(f, "OBJECT tag=surface id=game_map role=game_map x=35 y=6 w=48 h=14 z=16 nav=0 source=semantic:game_map_surface fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 action=- label=\n");
    fprintf(f, "OBJECT tag=model id=piececraft_map_01 role=tile_zmap x=35 y=6 w=48 h=14 z=23 "
               "source=projects/wraith-alpha/wraith-projects/piececraft-wraith/maps/%s_z%d.txt "
               "fg=#24C94A bg=#0B1118 border=#FFD166 action=- "
               "label=MAP_SOURCE:source=maps/%s_z%d.txt;registry=assets/tiles/registry.txt;z=%d;selected=%d,%d,%d;pet=%d,%d;mode=%s;camera=%d,%.2f,%.2f,%.2f,%.2f,%.2f,0.00\n",
            st->active_map_id, st->xel_z, st->active_map_id, st->xel_z, st->xel_z, st->xel_x, st->xel_y, st->xel_z,
            st->pet_x, st->pet_y,
            st->display_mode == 0 ? "2d" : "3d",
            st->camera_mode, st->cam_pan_x, st->cam_pan_y, st->cam_pan_z, st->cam_yaw, st->cam_pitch);
    fprintf(f, "OBJECT tag=control id=btn_up role=game_button x=84 y=6 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1002 label=[Up]\n", nav_base + 1);
    fprintf(f, "OBJECT tag=control id=btn_down role=game_button x=84 y=7 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1003 label=[Down]\n", nav_base + 2);
    fprintf(f, "OBJECT tag=control id=btn_left role=game_button x=84 y=8 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1000 label=[Left]\n", nav_base + 3);
    fprintf(f, "OBJECT tag=control id=btn_right role=game_button x=84 y=9 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1001 label=[Right]\n", nav_base + 4);
    fprintf(f, "OBJECT tag=control id=btn_ascend role=game_button x=84 y=10 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:101 label=[E] Z-Level Up\n", nav_base + 5);
    fprintf(f, "OBJECT tag=control id=btn_descend role=game_button x=84 y=11 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:113 label=[Q] Z-Level Dn\n", nav_base + 6);
    fprintf(f, "OBJECT tag=control id=btn_mode role=game_button x=84 y=12 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:56 label=[8] 2D/3D\n", nav_base + 7);
    fprintf(f, "OBJECT tag=control id=btn_debug role=game_button x=84 y=13 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:57 label=[9] Debug\n", nav_base + 8);
    fprintf(f, "OBJECT tag=text id=coords_hud role=game_hud x=84 y=15 w=22 h=1 z=25 nav=0 "
               "source=- fg=#FFD166 bg=#0B1118 border=#0B1118 action=- "
               "label=(%d,%d,%d) %s\n",
            st->xel_x, st->xel_y, st->xel_z,
            st->display_mode == 0 ? "2D" : "3D");
    fprintf(f, "OBJECT tag=text id=debug_hud role=game_hud x=84 y=16 w=22 h=1 z=25 nav=0 "
               "source=- fg=#7EDFF2 bg=#0B1118 border=#0B1118 action=- "
               "label=Debug: %s\n",
            st->debug_mode_on ? "ON" : "OFF");

    /* Camera controls — WASD pan (dedicated to camera only, never entity
       movement) + 1/2/3 POV presets. Same tag=control/KEY: pattern as the
       movement buttons above, just raw ASCII codes for w/a/s/d/1/2/3. */
    fprintf(f, "OBJECT tag=text id=cam_hud role=game_hud x=84 y=18 w=22 h=1 z=25 nav=0 "
               "source=- fg=#7EDFF2 bg=#0B1118 border=#0B1118 action=- "
               "label=Camera: POV%d\n", st->camera_mode);
    fprintf(f, "OBJECT tag=control id=btn_cam_fwd role=game_button x=84 y=19 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:119 label=[W] Cam Fwd\n", nav_base + 9);
    fprintf(f, "OBJECT tag=control id=btn_cam_back role=game_button x=84 y=20 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:115 label=[S] Cam Back\n", nav_base + 10);
    fprintf(f, "OBJECT tag=control id=btn_cam_left role=game_button x=84 y=21 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:97 label=[A] Cam Left\n", nav_base + 11);
    fprintf(f, "OBJECT tag=control id=btn_cam_right role=game_button x=84 y=22 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:100 label=[D] Cam Right\n", nav_base + 12);
    fprintf(f, "OBJECT tag=control id=btn_cam_up role=game_button x=84 y=23 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:120 label=[X] Cam Up\n", nav_base + 13);
    fprintf(f, "OBJECT tag=control id=btn_cam_down role=game_button x=84 y=24 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:122 label=[Z] Cam Down\n", nav_base + 14);
    fprintf(f, "OBJECT tag=control id=btn_pov1 role=game_button x=84 y=25 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:49 label=[1] POV-1\n", nav_base + 15);
    fprintf(f, "OBJECT tag=control id=btn_pov2 role=game_button x=84 y=26 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:50 label=[2] POV-2\n", nav_base + 16);
    fprintf(f, "OBJECT tag=control id=btn_pov3 role=game_button x=84 y=27 w=14 h=1 z=25 nav=%d source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:51 label=[3] POV-3\n", nav_base + 17);

    fclose(f);
    rename(tmp_path, path);
}

static long read_cursor(const char *root) {
    char path[MAX_PATH_LEN];
    FILE *f;
    long cursor = 0;
    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "r");
    if (!f) return 0;
    if (fscanf(f, "%ld", &cursor) != 1) {
        cursor = 0;
    }
    fclose(f);
    return cursor;
}

static void write_cursor(const char *root, long cursor) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", cursor);
    fclose(f);
}

static int key_from_history_line(const char *line) {
    const char *p = strstr(line, "KEY_PRESSED:");
    if (!p) return -1;
    p += strlen("KEY_PRESSED:");
    while (*p && isspace((unsigned char)*p)) p++;
    return atoi(p);
}

int main(int argc, char **argv) {
    const char *root;
    char history_path[MAX_PATH_LEN];
    FILE *history;
    long cursor;
    long end_pos;
    char line[MAX_LINE];
    GameState st;
    int changed = 0;
    /* --force: used once by the manager's initial-render trigger so a
       freshly-opened window's scene.objects.pdl (camera=...) reflects
       whatever load_state() just read (e.g. a just-applied neutral
       camera reset) immediately, instead of waiting for the first real
       keystroke to set `changed`. Normal per-keystroke invocations never
       pass this. */
    int force = (argc > 2 && strcmp(argv[2], "--force") == 0);

    if (argc < 2) {
        return 2;
    }
    root = argv[1];
    load_state(root, &st);

    cursor = read_cursor(root);
    path_join(history_path, sizeof(history_path), root, "session/history.txt");
    history = fopen(history_path, "r");
    if (history) {
        fseek(history, 0, SEEK_END);
        end_pos = ftell(history);
        if (cursor < 0 || cursor > end_pos) cursor = 0;
        fseek(history, cursor, SEEK_SET);
        while (fgets(line, sizeof(line), history)) {
            int key = key_from_history_line(line);
            int dx, dy;
            if (key > 0) {
                apply_key(&st, key);
                changed = 1;
            } else if (mouse_drag_from_history_line(line, &dx, &dy)) {
                apply_mouse_drag(&st, dx, dy);
                changed = 1;
            }
        }
        cursor = ftell(history);
        fclose(history);
        write_cursor(root, cursor);
    }

    if (changed || force) {
        render_body(root, &st);
        save_state(root, &st);
        write_scene(root, &st);
    }
    return 0;
}
