#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <windows.h>
#include <process.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#define usleep(us) Sleep((us)/1000)
#ifndef _vscprintf
int asprintf(char** strp, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    if (len < 0) return -1;
    *strp = (char*)malloc(len + 1);
    if (!*strp) return -1;
    int result = vsprintf(*strp, fmt, args);
    va_end(args);
    return result;
}
#endif
#endif

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[2048] = ".";
char project_session_dir[2048] = ".";
char history_path[4096] = "";
char debug_log_path[4096] = "";

int xel_x = 4, xel_y = 2, xel_z = 0;
int display_mode = 0;
int debug_mode_on = 0;
char active_map_id[256] = "map_01";
char last_key_str[64] = "None";
int pet_x = 14, pet_y = 6;

void update_scene_objects(void);

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* 2026-07-11: see ops/src/wraith_project_input.c's identical helper for
 * the full rationale -- reads CHROME_CONTENT_START's live value, published
 * by wraith-alpha_manager.c, instead of hardcoding this file's own guess.
 * This manager binary is fork+exec'd without a chdir() (same pattern as
 * every other init-only manager this session), so it inherits the
 * manager's own cwd and this relative path resolves correctly without
 * combining it with project_root (which is this project's own dir, not
 * the repo root). */
int read_chrome_content_start(int fallback) {
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

char* build_path_malloc(const char* rel) {
    size_t sz = strlen(project_root) + strlen(rel) + 2;
    char* p = (char*)malloc(sz);
    if (p) snprintf(p, sz, "%s/%s", project_root, rel);
    return p;
}

int root_has_anchors(const char* root) {
    char pieces_path[4096], projects_path[4096];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_paths() {
    if (!getcwd(project_root, sizeof(project_root))) {
        strncpy(project_root, ".", sizeof(project_root) - 1);
    }
    project_root[sizeof(project_root) - 1] = '\0';

    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0 && *v) {
                    if (root_has_anchors(v)) {
                        snprintf(project_root, sizeof(project_root), "%s", v);
                    }
                }
            }
        }
        fclose(kvp);
    }

    snprintf(project_session_dir, sizeof(project_session_dir),
             "%s/projects/wraith-alpha/wraith-projects/piececraft-wraith/session", project_root);
    snprintf(history_path, sizeof(history_path), "%s/history.txt", project_session_dir);
    snprintf(debug_log_path, sizeof(debug_log_path), "%s/debug_log.txt", project_session_dir);
}

void log_debug(const char* fmt, ...) {
    FILE *f = fopen(debug_log_path, "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        fprintf(f, "[%ld] ", time(NULL));
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

void save_xelector_state() {
    char xel_state_path[4096];
    snprintf(xel_state_path, sizeof(xel_state_path), "%s/xelector_state.txt", project_session_dir);
    FILE *f = fopen(xel_state_path, "w");
    if (f) {
        fprintf(f, "xel_x=%d\n", xel_x);
        fprintf(f, "xel_y=%d\n", xel_y);
        fprintf(f, "xel_z=%d\n", xel_z);
        fprintf(f, "map_id=%s\n", active_map_id);
        fprintf(f, "piece_type=player_character\n");
        fclose(f);
    }
}

void write_gui_state() {
    char gui_state_path[4096], body_path[4096];
    char line[512];
    FILE *f, *body_f;

    snprintf(gui_state_path, sizeof(gui_state_path), "%s/gui_state.txt", project_session_dir);
    snprintf(body_path, sizeof(body_path), "%s/wraith_body.txt", project_session_dir);

    f = fopen(gui_state_path, "w");
    if (f) {
        fprintf(f, "module_path=projects/wraith-alpha/wraith-projects/piececraft-wraith/manager/+x/piececraft-wraith_manager.+x\n");
        fprintf(f, "project_id=wraith-alpha/wraith-projects/piececraft-wraith\n");
        fprintf(f, "xel_x=%d\n", xel_x);
        fprintf(f, "xel_y=%d\n", xel_y);
        fprintf(f, "xel_z=%d\n", xel_z);
        fprintf(f, "active_tile=.\n");
        fprintf(f, "display_mode=%s\n", display_mode == 0 ? "2d_topdown" : "3d_voxel");
        fprintf(f, "debug_mode_on=%d\n", debug_mode_on);

        fprintf(f, "game_map=");
        body_f = fopen(body_path, "r");
        if (body_f) {
            while (fgets(line, sizeof(line), body_f)) {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                fprintf(f, "%s\\n", line);
            }
            fclose(body_f);
        } else {
            fprintf(f, "[Map render pending]");
        }
        fprintf(f, "\n");

        fclose(f);
    }
}

void save_state_txt() {
    char state_path[4096], state_tmp[4096], body_path[4096], line[512];
    FILE *f, *body_f;

    snprintf(state_path, sizeof(state_path), "%s/state.txt", project_session_dir);
    snprintf(body_path, sizeof(body_path), "%s/wraith_body.txt", project_session_dir);
    snprintf(state_tmp, sizeof(state_tmp), "%s.tmp", state_path);

    /* Atomic write (temp+rename) — matches wraith-alpha_manager.c's own
       convention. See ops/wraith_project_input.c for the full story: a
       non-atomic write to this exact file was the root cause of
       is_map_control intermittently reading as false. */
    f = fopen(state_tmp, "w");
    if (f) {
        fprintf(f, "project_id=wraith-alpha/wraith-projects/piececraft-wraith\n");
        fprintf(f, "reference_project=projects/piececraft-3d\n");
        fprintf(f, "is_map_control=1\n");
        fprintf(f, "active_z=%d\n", xel_z);
        fprintf(f, "xel_x=%d\n", xel_x);
        fprintf(f, "xel_y=%d\n", xel_y);
        fprintf(f, "xel_z=%d\n", xel_z);
        fprintf(f, "active_tile=.\n");
        fprintf(f, "display_mode=%s\n", display_mode == 0 ? "2d_topdown" : "3d_voxel");
        fprintf(f, "debug_mode_on=%d\n", debug_mode_on);
        fprintf(f, "camera_mode=1\n");
        fprintf(f, "cam_pan_x=0.00\n");
        fprintf(f, "cam_pan_y=0.00\n");
        fprintf(f, "cam_pan_z=0.00\n");
        fprintf(f, "pet_x=%d\n", pet_x);
        fprintf(f, "pet_y=%d\n", pet_y);

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

    update_scene_objects();
}

void update_scene_objects(void) {
    char scene_path[4096];
    int nav_base = read_chrome_content_start(6);
    snprintf(scene_path, sizeof(scene_path), "%s/scene.objects.pdl", project_session_dir);

    FILE *f = fopen(scene_path, "w");
    if (!f) return;

    fprintf(f, "# Project-owned Wraith scene records.\n");
    fprintf(f, "# Piececraft world standard: maps/map_01_z*.txt + assets/tiles/registry.txt + tile extrude fields.\n");
    fprintf(f, "OBJECT tag=control id=control_map role=window_toolbar_item x=35 y=5 w=26 h=1 z=26 nav=%d source=semantic:window_toolbar fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=INTERACT target_surface=game_map label=Control_Map\n", nav_base + 0);
    fprintf(f, "OBJECT tag=surface id=game_map role=game_map x=35 y=6 w=48 h=14 z=16 nav=0 source=semantic:game_map_surface fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 action=- label=\n");
    fprintf(f, "OBJECT tag=model id=piececraft_map_01 role=tile_zmap x=35 y=6 w=48 h=14 z=23 "
               "source=projects/wraith-alpha/wraith-projects/piececraft-wraith/maps/%s_z%d.txt "
               "fg=#24C94A bg=#0B1118 border=#FFD166 action=- "
               "label=MAP_SOURCE:source=maps/%s_z%d.txt;registry=assets/tiles/registry.txt;z=%d;selected=%d,%d,%d;pet=%d,%d;mode=%s;camera=1,0.00,0.00,0.00,15.00,0.00,0.00\n",
            active_map_id, xel_z, active_map_id, xel_z, xel_z, xel_x, xel_y, xel_z,
            pet_x, pet_y,
            display_mode == 0 ? "2d" : "3d");

    fprintf(f, "OBJECT tag=control id=btn_up role=game_button x=84 y=6 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1002 label=[Up]\n", nav_base + 1);
    fprintf(f, "OBJECT tag=control id=btn_down role=game_button x=84 y=7 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1003 label=[Down]\n", nav_base + 2);
    fprintf(f, "OBJECT tag=control id=btn_left role=game_button x=84 y=8 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1000 label=[Left]\n", nav_base + 3);
    fprintf(f, "OBJECT tag=control id=btn_right role=game_button x=84 y=9 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:1001 label=[Right]\n", nav_base + 4);
    fprintf(f, "OBJECT tag=control id=btn_ascend role=game_button x=84 y=10 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:101 label=[E] Z-Level Up\n", nav_base + 5);
    fprintf(f, "OBJECT tag=control id=btn_descend role=game_button x=84 y=11 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:113 label=[Q] Z-Level Dn\n", nav_base + 6);
    fprintf(f, "OBJECT tag=control id=btn_mode role=game_button x=84 y=12 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:56 label=[8] 2D/3D\n", nav_base + 7);
    fprintf(f, "OBJECT tag=control id=btn_debug role=game_button x=84 y=13 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:57 label=[9] Debug\n", nav_base + 8);
    fprintf(f, "OBJECT tag=text id=coords_hud role=game_hud x=84 y=15 w=22 h=1 z=25 nav=0 "
               "source=- fg=#FFD166 bg=#0B1118 border=#0B1118 action=- "
               "label=(%d,%d,%d) %s\n",
            xel_x, xel_y, xel_z,
            display_mode == 0 ? "2D" : "3D");
    fprintf(f, "OBJECT tag=text id=debug_hud role=game_hud x=84 y=16 w=22 h=1 z=25 nav=0 "
               "source=- fg=#7EDFF2 bg=#0B1118 border=#0B1118 action=- "
               "label=Debug: %s\n",
            debug_mode_on ? "ON" : "OFF");
    fprintf(f, "OBJECT tag=text id=cam_hud role=game_hud x=84 y=18 w=22 h=1 z=25 nav=0 "
               "source=- fg=#7EDFF2 bg=#0B1118 border=#0B1118 action=- "
               "label=Camera: POV1\n");
    fprintf(f, "OBJECT tag=control id=btn_cam_fwd role=game_button x=84 y=19 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:119 label=[W] Cam Fwd\n", nav_base + 9);
    fprintf(f, "OBJECT tag=control id=btn_cam_back role=game_button x=84 y=20 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:115 label=[S] Cam Back\n", nav_base + 10);
    fprintf(f, "OBJECT tag=control id=btn_cam_left role=game_button x=84 y=21 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:97 label=[A] Cam Left\n", nav_base + 11);
    fprintf(f, "OBJECT tag=control id=btn_cam_right role=game_button x=84 y=22 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:100 label=[D] Cam Right\n", nav_base + 12);
    fprintf(f, "OBJECT tag=control id=btn_cam_up role=game_button x=84 y=23 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:120 label=[X] Cam Up\n", nav_base + 13);
    fprintf(f, "OBJECT tag=control id=btn_cam_down role=game_button x=84 y=24 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:122 label=[Z] Cam Down\n", nav_base + 14);
    fprintf(f, "OBJECT tag=control id=btn_pov1 role=game_button x=84 y=25 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:49 label=[1] POV-1\n", nav_base + 15);
    fprintf(f, "OBJECT tag=control id=btn_pov2 role=game_button x=84 y=26 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:50 label=[2] POV-2\n", nav_base + 16);
    fprintf(f, "OBJECT tag=control id=btn_pov3 role=game_button x=84 y=27 w=14 h=1 z=25 nav=%d "
               "source=- fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=KEY:51 label=[3] POV-3\n", nav_base + 17);

    fclose(f);
}

void trigger_render() {
    char frame_marker[4096];
    snprintf(frame_marker, sizeof(frame_marker), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(frame_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }

    char state_marker[4096];
    snprintf(state_marker, sizeof(state_marker), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    FILE *sm = fopen(state_marker, "a");
    if (sm) {
        fprintf(sm, "X\n");
        fclose(sm);
    }
}

void render_map() {
    char op_path[4096], map_path[4096], body_path[4096], cmd[8192];
    char temp_map[4096], line[512];
    FILE *map_file, *body_file;

    snprintf(op_path, sizeof(op_path),
             "%s/projects/wraith-alpha/wraith-projects/piececraft-wraith/ops/+x/render_map_wraith.+x",
             project_root);

    snprintf(map_path, sizeof(map_path),
             "%s/projects/wraith-alpha/wraith-projects/piececraft-wraith/maps/%s_z%d.txt",
             project_root, active_map_id, xel_z);

    snprintf(temp_map, sizeof(temp_map), "%s/temp_map.txt", project_session_dir);
    snprintf(body_path, sizeof(body_path), "%s/wraith_body.txt", project_session_dir);

    snprintf(cmd, sizeof(cmd), "'%s' %d %d %d '%s' > '%s' 2>/dev/null",
             op_path, xel_x, xel_y, xel_z, map_path, temp_map);
    system(cmd);

    body_file = fopen(body_path, "w");
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
            xel_x, xel_y, xel_z,
            (display_mode == 0) ? "2D" : "3D",
            debug_mode_on ? "ON" : "OFF");
    fprintf(body_file, "─────────────────────────────────────────────\n\n");

    fprintf(body_file, "CONTROLS:\n");
    fprintf(body_file, "  ↑↓←→  Move xelector\n");
    fprintf(body_file, "  X/Z   Ascend/Descend\n");
    fprintf(body_file, "  8     Toggle 2D/3D\n");
    fprintf(body_file, "  D     Debug Mode\n");
    fprintf(body_file, "  ESC   Menu\n");

    fclose(body_file);
    remove(temp_map);
}

/*
 * Forces one synchronous run of ops/+x/wraith_project_input.+x with
 * --force at manager startup, so scene.objects.pdl's camera=... line
 * reflects whatever load_state() reads right now (e.g. a just-applied
 * neutral camera reset from wraith-alpha_manager.c's
 * reset_project_view_from_default()) immediately on open, instead of
 * waiting for the first real keystroke to set `changed` inside that op.
 *
 * Root cause this fixes: this manager's own render_map() is the older
 * 2D ASCII map path and knows nothing about camera_mode/cam_pan_x or
 * scene.objects.pdl -- only the per-keystroke op writes the 3D camera
 * scene, and it only did so when triggered by real input. Mirrors
 * terminal_manager.c's trigger_initial_render() pattern exactly.
 */
void trigger_initial_scene_render() {
    char op_path[4096];
    char project_dir[4096];
    pid_t pid;

    snprintf(op_path, sizeof(op_path),
             "%s/projects/wraith-alpha/wraith-projects/piececraft-wraith/ops/+x/wraith_project_input.+x",
             project_root);
    if (access(op_path, X_OK) != 0) {
        log_debug("Initial scene render skipped: op not found at %s", op_path);
        return;
    }

    snprintf(project_dir, sizeof(project_dir),
             "%s/projects/wraith-alpha/wraith-projects/piececraft-wraith", project_root);

    pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(op_path, op_path, project_dir, "--force", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

/*
 * NOTE: This manager no longer polls history.txt for input. That hot path
 * previously raced against wraith-alpha's synchronous per-keypress
 * trigger_render() call, causing a structural one-keypress-behind display
 * lag (root cause + fix documented in
 * x0.piececrafts/HANDOFF-WRAITH-STATE-MYSTERY.md, "Frame lag" section).
 *
 * Per-keypress state mutation now lives in
 * ops/src/wraith_project_input.c, which wraith-alpha invokes synchronously
 * (fork+execl+waitpid) after every keypress via run_active_project_input_op(),
 * matching the proven pattern in wraith-3d-cube and wraith-ed. This manager's
 * only remaining job is one-time initialization so the window isn't blank
 * before the first keypress arrives.
 */
int main() {
    resolve_paths();

    save_xelector_state();
    save_state_txt();
    write_gui_state();
    render_map();
    trigger_initial_scene_render();
    trigger_render();

    log_debug("Piececraft-Wraith Manager Started (init-only; hot path is ops/wraith_project_input.+x)");
    log_debug("Project root: %s", project_root);
    log_debug("Session dir: %s", project_session_dir);

    return 0;
}
