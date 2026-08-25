/* tp_arm_placer - the "^" activation mode.
 * Usage: tp_arm_placer.+x <project_root> <widget_state_dir> <desktop_root> <glyph>
 *
 * Direct instruction 2026-08-04 (supersedes an earlier, shelved cli_io-
 * field idea): "I want each [emoji option] to have an activation mode...
 * when you press Enter, instead of placing the emoji, it enters '^' mode
 * till the user presses Escape, where wherever they click (on desk or
 * on a view) the phymoji will appear."
 *
 * Spawned detached (setsid) by tp_menu_input.c on KEY:n instead of
 * placing immediately. Globally grabs the pointer + keyboard (same
 * technique egg_window.c's own right-click popup uses for ITS grab,
 * just root-scoped here instead of window-scoped, since the destination
 * click can land on ANY window on screen) and waits for exactly one of:
 *
 *   - Escape: cancels, no placement, clears armed state.
 *   - A real click (ButtonPress) anywhere: resolves the destination and
 *     places there. Two destinations are recognized:
 *       1. Inside a live, PID-tagged board-viewer window (discovered via
 *          `ledger_peers widget`, matched to its real on-screen rect via
 *          tp_find_window_by_pid.+x - see TILE_PICKER_DESIGN.md §4) that
 *          is currently focused on a project with a real mutaclysm-style
 *          hero (pieces/world_01/<any-map>/hero/state.txt with on_map=1,
 *          map_id, pos_x, pos_y) - enqueues PLACE_TILE there, same
 *          inbox convention tp_place.c/tp_import_from_desktop.c already
 *          use. Any OTHER kind of focused project (e.g. aomorai-editor's
 *          own 3D voxel world, a genuinely different data model with no
 *          PLACE_TILE-equivalent consumer yet) reports clearly instead
 *          of silently doing nothing.
 *       2. Anywhere else (bare desktop, or any non-board-viewer window):
 *          falls through to the same tp_place_desktop.+x path already
 *          proven working, at the real click coordinates (via
 *          TP_INITIAL_X/TP_INITIAL_Y - see tp_place_desktop.c's own
 *          matching fix).
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352
#define MAX_LINE 2048

static void resolve_ops_dir(char *out, size_t out_sz) {
    char self_path[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) { out[0] = '\0'; return; }
    self_path[len] = '\0';
    char *slash = strrchr(self_path, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s", self_path);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *in = fopen(path, "r");
    FILE *out = fopen(tmp, "w");
    if (!out) { if (in) fclose(in); return; }
    int found = 0;
    if (in) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                fprintf(out, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }
    if (!found) fprintf(out, "%s=%s\n", key, val);
    fclose(out);
    rename(tmp, path);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void bump_screen(const char *project_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/tp_screen_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
}

static void set_status(const char *project_root, int armed, const char *glyph, const char *msg) {
    char tp_state[PATH_BUF];
    snprintf(tp_state, sizeof(tp_state), "%s/pieces/system/tp_state.txt", project_root);
    write_kv(tp_state, "armed", armed ? "1" : "0");
    write_kv(tp_state, "armed_glyph", armed ? glyph : "");
    write_kv(tp_state, "status_line", msg);
    bump_screen(project_root);
}

/* Best-effort: does <focused_project_root> have a real mutaclysm-style
 * hero (the only data model this session proved a PLACE_TILE consumer
 * for)? Scans pieces/world_01/<any map dir>/hero/state.txt for on_map=1,
 * returns its map_id/pos_x/pos_y if so. */
static int find_mutaclysm_hero(const char *focused_project_root, char *map_id, size_t map_id_sz,
                                 int *pos_x, int *pos_y) {
    char world_dir[PATH_BUF];
    snprintf(world_dir, sizeof(world_dir), "%s/pieces/world_01", focused_project_root);
    DIR *d = opendir(world_dir);
    if (!d) return 0;
    struct dirent *ent;
    int found = 0;
    while (!found && (ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        char hero_path[PATH_BUF];
        snprintf(hero_path, sizeof(hero_path), "%s/%s/hero/state.txt", world_dir, ent->d_name);
        char on_map[16] = "";
        read_kv(hero_path, "on_map", on_map, sizeof(on_map));
        if (atoi(on_map) == 1) {
            char mid[128] = "", px[16] = "", py[16] = "";
            read_kv(hero_path, "map_id", mid, sizeof(mid));
            read_kv(hero_path, "pos_x", px, sizeof(px));
            read_kv(hero_path, "pos_y", py, sizeof(py));
            if (mid[0]) {
                snprintf(map_id, map_id_sz, "%s", mid);
                *pos_x = atoi(px);
                *pos_y = atoi(py);
                found = 1;
            }
        }
    }
    closedir(d);
    return found;
}

/* Runs ledger_peers.+x widget (scoped to this widget's own session, via
 * PRISC_PROJECT_ROOT), looking for a "Board Viewer" entry whose real
 * window (via tp_find_window_by_pid.+x, PID-based per this project's
 * own §4 fix) contains (click_x, click_y). Returns 1 and fills
 * bv_session_root if found. */
static int find_board_viewer_hit(const char *ops_dir, const char *project_root,
                                   int click_x, int click_y,
                                   char *bv_session_root, size_t bv_sr_sz) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ledger_peers.+x' widget 2>/dev/null",
             project_root, ops_dir);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char line[MAX_LINE];
    int hit = 0;
    while (!hit && fgets(line, sizeof(line), p)) {
        line[strcspn(line, "\r\n")] = '\0';
        char session_root[PATH_BUF] = "", inbox_path[PATH_BUF] = "", display_name[256] = "",
             proj_id[128] = "", pid_str[64] = "";
        char *tok = strtok(line, "|");
        if (tok) snprintf(session_root, sizeof(session_root), "%s", tok);
        tok = strtok(NULL, "|"); if (tok) snprintf(inbox_path, sizeof(inbox_path), "%s", tok);
        tok = strtok(NULL, "|"); if (tok) snprintf(display_name, sizeof(display_name), "%s", tok);
        tok = strtok(NULL, "|"); if (tok) snprintf(proj_id, sizeof(proj_id), "%s", tok);
        tok = strtok(NULL, "|"); if (tok) snprintf(pid_str, sizeof(pid_str), "%s", tok);
        (void)inbox_path; (void)proj_id; (void)pid_str;
        if (strcmp(display_name, "Board Viewer") != 0 || !session_root[0]) continue;

        /* REAL BUG, found 2026-08-04 ("it placed the tile over the
         * viewer, on desktop, not in viewer... doesn't it know the
         * coords of the viewer?"): the ledger's own recorded pid (this
         * loop's now-unused pid_str) is button.sh's own shell "$$",
         * written BEFORE gl_mirror is even spawned - a completely
         * different process than the one bv_set_wm_pid.c actually tags
         * with _NET_WM_PID. Looking up a window by that stale ledger
         * PID always misses, silently falling through to the desktop-
         * placement path every time - exactly the reported symptom.
         * Real fix: resolve the ACTUAL gl_mirror PID ourselves via
         * cwd-scoped pgrep against the ledger's own (reliable)
         * session_root, same technique button.sh's own
         * kill_own_module()/tagging call already use, rather than
         * trusting the ledger's pid field for this purpose. */
        char pgrep_cmd[PATH_BUF];
        snprintf(pgrep_cmd, sizeof(pgrep_cmd), "pgrep -f 'system/gl_mirror' 2>/dev/null");
        FILE *pgp = popen(pgrep_cmd, "r");
        if (!pgp) continue;
        char real_pid[64] = "";
        char cand_line[64];
        while (fgets(cand_line, sizeof(cand_line), pgp)) {
            cand_line[strcspn(cand_line, "\r\n")] = '\0';
            char cwd_link[PATH_BUF], cwd_target[PATH_BUF];
            snprintf(cwd_link, sizeof(cwd_link), "/proc/%s/cwd", cand_line);
            ssize_t clen = readlink(cwd_link, cwd_target, sizeof(cwd_target) - 1);
            if (clen > 0) {
                cwd_target[clen] = '\0';
                if (strcmp(cwd_target, session_root) == 0) {
                    snprintf(real_pid, sizeof(real_pid), "%s", cand_line);
                    break;
                }
            }
        }
        pclose(pgp);
        if (!real_pid[0]) continue;

        char rect_cmd[PATH_BUF];
        snprintf(rect_cmd, sizeof(rect_cmd), "'%s/tp_find_window_by_pid.+x' '%s' 2>/dev/null", ops_dir, real_pid);
        FILE *rp = popen(rect_cmd, "r");
        if (!rp) continue;
        int rx, ry, rw, rh;
        int got = (fscanf(rp, "%d %d %d %d", &rx, &ry, &rw, &rh) == 4);
        pclose(rp);
        if (got && click_x >= rx && click_x < rx + rw && click_y >= ry && click_y < ry + rh) {
            snprintf(bv_session_root, bv_sr_sz, "%s", session_root);
            hit = 1;
        }
    }
    pclose(p);
    return hit;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: tp_arm_placer.+x <project_root> <widget_state_dir> <desktop_root> <glyph>\n");
        return 1;
    }
    const char *project_root = argv[1];
    const char *widget_state_dir = argv[2];
    const char *desktop_root = argv[3];
    const char *glyph = argv[4];

    char ops_dir[PATH_BUF];
    resolve_ops_dir(ops_dir, sizeof(ops_dir));

    char armed_msg[MAX_LINE];
    snprintf(armed_msg, sizeof(armed_msg),
             "^ ARMED: %s -- click anywhere to place, Esc to cancel", glyph);
    set_status(project_root, 1, glyph, armed_msg);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        set_status(project_root, 0, "", "^ mode failed: cannot open display");
        return 1;
    }
    Window root = RootWindow(dpy, DefaultScreen(dpy));

    if (XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                      None, None, CurrentTime) != GrabSuccess) {
        set_status(project_root, 0, "", "^ mode failed: could not grab pointer");
        XCloseDisplay(dpy);
        return 1;
    }
    if (XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess) {
        XUngrabPointer(dpy, CurrentTime);
        set_status(project_root, 0, "", "^ mode failed: could not grab keyboard");
        XCloseDisplay(dpy);
        return 1;
    }

    int click_x = -1, click_y = -1, cancelled = 0;
    while (1) {
        XEvent xev;
        XNextEvent(dpy, &xev);
        if (xev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&xev.xkey, 0);
            if (ks == XK_Escape) { cancelled = 1; break; }
        } else if (xev.type == ButtonPress) {
            click_x = xev.xbutton.x_root;
            click_y = xev.xbutton.y_root;
            break;
        }
    }
    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    XCloseDisplay(dpy);

    if (cancelled) {
        set_status(project_root, 0, "", "^ mode cancelled.");
        return 0;
    }

    char bv_session_root[PATH_BUF] = "";
    if (find_board_viewer_hit(ops_dir, project_root, click_x, click_y, bv_session_root, sizeof(bv_session_root))) {
        char bv_state[PATH_BUF], focused_root[PATH_BUF] = "";
        snprintf(bv_state, sizeof(bv_state), "%s/pieces/system/bv_state.txt", bv_session_root);
        read_kv(bv_state, "focused_project_root", focused_root, sizeof(focused_root));

        char map_id[128] = "";
        int pos_x = 0, pos_y = 0;
        if (focused_root[0] && find_mutaclysm_hero(focused_root, map_id, sizeof(map_id), &pos_x, &pos_y)) {
            char inbox[PATH_BUF];
            snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", focused_root);
            FILE *f = fopen(inbox, "a");
            if (f) {
                fprintf(f, "PLACE_TILE:%s:%d:%d:%s\n", map_id, pos_x, pos_y, glyph);
                fclose(f);
                char msg[MAX_LINE];
                snprintf(msg, sizeof(msg), "Placed %s into board-viewer's %s at %s(%d,%d).",
                         glyph, focused_root, map_id, pos_x, pos_y);
                set_status(project_root, 0, "", msg);
            } else {
                set_status(project_root, 0, "", "Board-viewer target found, but its inbox could not be written.");
            }
        } else {
            char msg[MAX_LINE];
            snprintf(msg, sizeof(msg),
                     "Dropped on a board-viewer showing '%s', but that project has no tile-placement support yet.",
                     focused_root[0] ? focused_root : "(unknown)");
            set_status(project_root, 0, "", msg);
        }
        return 0;
    }

    /* Not on a board-viewer - place on the bare desktop at the real
     * click point (grid-snapped by tp_desktop_window.c itself). */
    char cmd[PATH_BUF * 3], envx[32], envy[32];
    snprintf(envx, sizeof(envx), "%d", click_x);
    snprintf(envy, sizeof(envy), "%d", click_y);
    setenv("TP_INITIAL_X", envx, 1);
    setenv("TP_INITIAL_Y", envy, 1);
    snprintf(cmd, sizeof(cmd), "'%s/tp_place_desktop.+x' '%s' '%s' '%s' >/dev/null 2>&1",
             ops_dir, widget_state_dir, desktop_root, glyph);
    system(cmd);

    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Placed %s on desktop at click point.", glyph);
    set_status(project_root, 0, "", msg);
    return 0;
}
