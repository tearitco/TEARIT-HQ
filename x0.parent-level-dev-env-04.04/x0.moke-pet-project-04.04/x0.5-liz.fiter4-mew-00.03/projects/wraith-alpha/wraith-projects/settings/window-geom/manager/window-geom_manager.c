#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_LINE 2048

typedef struct {
    int x, y, width, height;
    char edit_x[32], edit_y[32], edit_width[32], edit_height[32];
    char last_action[256];
} EditState;

/*
 * window-geom_manager.c -- init-only manager for the "window-geom"
 * settings entry. See x0.short-term-vision/settings-hub-window-geom-design-j5.md.
 *
 * FIRST SLICE SCOPE (deliberately, not an oversight): this reads
 * whichever window is currently ACTIVE in the wraith-alpha desktop
 * (via projects/wraith-alpha/session/alpha_state.txt's
 * desktop_focused_window_project_id -- the one additive line just
 * added to write_projection() for exactly this) and displays its
 * project.pdl WINDOW section. NOT read-only (that claim went stale
 * once Apply landed, 2026-07-1x): KEY:5-13 nudge x/y/width/height in
 * EditState and KEY:13 (Apply) writes them out via
 * ops/src/wraith_project_input.c's write_pdl_value() calls -- see
 * that file for the actual write path. A full "pick any open window"
 * picker still needs a further additive change (exposing every open
 * window, not just the active one) -- explicitly deferred, not
 * bundled into this slice. The chrome-bar targeted-invocation path
 * (pre-setting a specific target, skipping this active-window read
 * entirely) is also explicitly deferred.
 *
 * Writes to its own manager/state.txt (NOT session/state.txt) for the
 * same reason settings_manager.c does -- that's the generic path
 * wraith_parser_alpha.c's load_vars() already reads for any project,
 * derived from its own layout's path. Zero shared-file changes needed
 * beyond the one additive line already made.
 */

char project_root[2048] = ".";
char project_dir[2048] = ".";
char state_path[4096] = "";
char debug_log_path[4096] = "";

char *trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int root_has_anchors(const char *root) {
    char pieces_path[4096], projects_path[4096];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_paths(void) {
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

    snprintf(project_dir, sizeof(project_dir),
             "%s/projects/wraith-alpha/wraith-projects/settings/window-geom", project_root);
    snprintf(state_path, sizeof(state_path), "%s/manager/state.txt", project_dir);
    snprintf(debug_log_path, sizeof(debug_log_path), "%s/manager/debug_log.txt", project_dir);
}

void log_debug(const char *fmt, ...) {
    FILE *f = fopen(debug_log_path, "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        fprintf(f, "[%ld] ", (long)time(NULL));
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

/* Same key=value scan convention already used across this codebase
 * (e.g. terminal_manager.c's resolve_paths()). */
void read_kvp_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE];
    size_t key_len = strlen(key);

    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *val = trim_str(line + key_len + 1);
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            strncpy(dst, val, dst_sz - 1);
            dst[dst_sz - 1] = '\0';
            break;
        }
    }
    fclose(f);
}

/* Same substring-key/pipe-split convention as
 * wraith-alpha_manager.c's read_pdl_value(). */
void read_pdl_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE];

    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *pipe1, *pipe2, *val, *nl;
        if (!strstr(line, key)) continue;
        pipe1 = strchr(line, '|');
        if (!pipe1) continue;
        pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) continue;
        val = trim_str(pipe2 + 1);
        nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        strncpy(dst, val, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
        break;
    }
    fclose(f);
}

/* Writes/updates one key=value line in this project's OWN
 * manager/gui_state.txt -- the SAME file chtpm_parser.c's
 * save_cli_io_gui_state()/read_cli_io_gui_state_value() and
 * wraith-alpha_manager.c's read_gui_state_value() already read for
 * this window's cli_io fields (2fix-july6.txt section 6/7). Used to
 * seed the edit_x/edit_y/edit_width/edit_height cli_io fields with the
 * target project's REAL current geometry the first time a target is
 * opened, instead of leaving them empty (which only ever renders the
 * cli_io tag's own label text as a placeholder -- see 2fix-july6.txt
 * section 8). */
void write_gui_state_kv(const char *key, const char *value) {
    char path[4096];
    char tmp_path[4096];
    char names[64][64];
    char values[64][256];
    int count = 0;
    int found = 0;
    FILE *f;

    snprintf(path, sizeof(path), "%s/manager/gui_state.txt", project_dir);
    f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && count < 64) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            strncpy(names[count], trim_str(line), sizeof(names[0]) - 1);
            names[count][sizeof(names[0]) - 1] = '\0';
            strncpy(values[count], trim_str(eq + 1), sizeof(values[0]) - 1);
            values[count][sizeof(values[0]) - 1] = '\0';
            count++;
        }
        fclose(f);
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], key) == 0) {
            strncpy(values[i], value, sizeof(values[0]) - 1);
            values[i][sizeof(values[0]) - 1] = '\0';
            found = 1;
            break;
        }
    }
    if (!found && count < 64) {
        strncpy(names[count], key, sizeof(names[0]) - 1);
        names[count][sizeof(names[0]) - 1] = '\0';
        strncpy(values[count], value, sizeof(values[0]) - 1);
        values[count][sizeof(values[0]) - 1] = '\0';
        count++;
    }

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s=%s\n", names[i], values[i]);
    }
    fclose(f);
    rename(tmp_path, path);
}

/* Write (or update) a single key=value pair in project.pdl.
 * Reads entire file into memory, finds or creates the line for the key,
 * updates its value, then atomic-writes back (temp+rename).
 * Follows the same pipe-delimited format: SECTION | KEY | VALUE */
void write_pdl_value(const char *path, const char *section, const char *key, const char *value) {
    FILE *src, *tmp;
    char line[MAX_LINE];
    char tmp_path[4096];
    int found = 0;

    if (!path || !section || !key || !value) return;

    /* Temp file path for atomic write. */
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    /* Read existing file and rewrite with updated value. */
    src = fopen(path, "r");
    tmp = fopen(tmp_path, "w");
    if (!tmp) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        if (src) fclose(src);
        return;
    }

    if (src) {
        while (fgets(line, sizeof(line), src)) {
            char *pipe1, *pipe2;
            char line_section[64] = "";
            char line_key[64] = "";

            /* Parse existing line to check if it matches. */
            pipe1 = strchr(line, '|');
            if (pipe1) {
                char *pipe2_candidate = strchr(pipe1 + 1, '|');
                if (pipe2_candidate) {
                    /* Extract section and key for comparison. */
                    size_t sec_len = (size_t)(pipe1 - line);
                    size_t key_len = (size_t)(pipe2_candidate - pipe1 - 1);
                    if (sec_len > 0 && sec_len < sizeof(line_section)) {
                        memcpy(line_section, line, sec_len);
                        line_section[sec_len] = '\0';
                        line_section[strcspn(line_section, " \t")] = '\0';  /* trim right */
                    }
                    if (key_len > 0 && key_len < sizeof(line_key)) {
                        memcpy(line_key, pipe1 + 1, key_len);
                        line_key[key_len] = '\0';
                        char *key_start = line_key;
                        while (*key_start && isspace((unsigned char)*key_start)) key_start++;
                        char *key_end = key_start + strlen(key_start) - 1;
                        while (key_end > key_start && isspace((unsigned char)*key_end)) key_end--;
                        key_end[1] = '\0';
                        strcpy(line_key, key_start);
                    }

                    /* If this line matches our section and key, replace it. */
                    if (strcmp(line_section, section) == 0 && strcmp(line_key, key) == 0) {
                        fprintf(tmp, "%s | %s | %s\n", section, key, value);
                        found = 1;
                    } else {
                        fputs(line, tmp);
                    }
                } else {
                    fputs(line, tmp);
                }
            } else {
                fputs(line, tmp);
            }
        }
        fclose(src);
    }

    /* If not found, append new line at end. */
    if (!found) {
        fprintf(tmp, "%s | %s | %s\n", section, key, value);
    }

    fclose(tmp);
    rename(tmp_path, path);
    log_debug("Updated %s: %s | %s | %s", path, section, key, value);
}

void load_edit_state(EditState *st) {
    char path[4096];
    FILE *f;
    char line[MAX_LINE];

    memset(st, 0, sizeof(*st));
    snprintf(path, sizeof(path), "%s/session/state.txt", project_dir);
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_str(line);
        char *val = trim_str(eq + 1);
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';

        if (strcmp(key, "edit_x") == 0) strncpy(st->edit_x, val, sizeof(st->edit_x) - 1);
        else if (strcmp(key, "edit_y") == 0) strncpy(st->edit_y, val, sizeof(st->edit_y) - 1);
        else if (strcmp(key, "edit_width") == 0) strncpy(st->edit_width, val, sizeof(st->edit_width) - 1);
        else if (strcmp(key, "edit_height") == 0) strncpy(st->edit_height, val, sizeof(st->edit_height) - 1);
        else if (strcmp(key, "last_action") == 0) strncpy(st->last_action, val, sizeof(st->last_action) - 1);
    }
    fclose(f);
}

/* Reads session/wg_target.txt -- a single line, a project_id (e.g.
 * "wraith-alpha/wraith-projects/terminal"). This is the file-backed
 * handoff a future chrome-bar button would write before navigating
 * here (per settings-hub-window-geom-design-j5.md's "targeted"
 * invocation), and it's also how this can be tested by hand right now:
 * write a project_id into this file to simulate "opened from a
 * project"; leave it absent/empty to simulate "opened from settings"
 * (falls back to whatever window is currently active in the desktop). */
void read_target_file(char *out_project_id, size_t out_sz) {
    char target_path[4096];
    FILE *f;
    char line[256];

    out_project_id[0] = '\0';
    snprintf(target_path, sizeof(target_path), "%s/session/wg_target.txt", project_dir);
    f = fopen(target_path, "r");
    if (!f) return;
    if (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        strncpy(out_project_id, trimmed, out_sz - 1);
        out_project_id[out_sz - 1] = '\0';
    }
    fclose(f);
}

void write_state(void) {
    char target_project_id[256] = "";
    char focused_project_id[256] = "";
    char focused_title[128] = "";
    char focused_id[128] = "";
    char tmp_path[4096];
    char state_path[4096];
    FILE *f;
    EditState edit_st;

    read_target_file(target_project_id, sizeof(target_project_id));
    load_edit_state(&edit_st);

    if (target_project_id[0]) {
        strncpy(focused_project_id, target_project_id, sizeof(focused_project_id) - 1);
    } else {
        char alpha_state_path[4096];
        snprintf(alpha_state_path, sizeof(alpha_state_path),
                 "%s/projects/wraith-alpha/session/alpha_state.txt", project_root);
        read_kvp_value(alpha_state_path, "desktop_focused_window_id", focused_id, sizeof(focused_id));
        read_kvp_value(alpha_state_path, "desktop_focused_window_project_id", focused_project_id, sizeof(focused_project_id));
        read_kvp_value(alpha_state_path, "desktop_focused_window_title", focused_title, sizeof(focused_title));
    }

    /* Write state.txt with cli_io and button state */
    snprintf(state_path, sizeof(state_path), "%s/session/state.txt", project_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", state_path);
    f = fopen(tmp_path, "w");
    if (!f) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        return;
    }

    if (focused_project_id[0]) {
        char pdl_path[4096];
        char win_x[32] = "", win_y[32] = "", win_w[32] = "", win_h[32] = "";
        char geom_display[256] = "";

        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root, focused_project_id);
        read_pdl_value(pdl_path, "window_x", win_x, sizeof(win_x));
        read_pdl_value(pdl_path, "window_y", win_y, sizeof(win_y));
        read_pdl_value(pdl_path, "window_width", win_w, sizeof(win_w));
        read_pdl_value(pdl_path, "window_height", win_h, sizeof(win_h));

        snprintf(geom_display, sizeof(geom_display), "x=%s y=%s width=%s height=%s",
                 win_x[0] ? win_x : "0",
                 win_y[0] ? win_y : "0",
                 win_w[0] ? win_w : "0",
                 win_h[0] ? win_h : "0");

        fprintf(f, "current_geometry=%s\n", geom_display);
        fprintf(f, "focused_project_id=%s\n", focused_project_id);
        fprintf(f, "focused_title=%s\n", focused_title[0] ? focused_title : focused_id);
    } else {
        fprintf(f, "current_geometry=No window selected\n");
    }

    fprintf(f, "edit_x=%s\n", edit_st.edit_x);
    fprintf(f, "edit_y=%s\n", edit_st.edit_y);
    fprintf(f, "edit_width=%s\n", edit_st.edit_width);
    fprintf(f, "edit_height=%s\n", edit_st.edit_height);
    fprintf(f, "edit_status=%s\n", edit_st.last_action[0] ? edit_st.last_action : "Ready to edit");

    fclose(f);
    rename(tmp_path, state_path);

    log_debug("Updated state for target=%s", focused_project_id[0] ? focused_project_id : "(none)");
}

/* Standalone-mode body, written to session/wraith_body.txt so
 * append_project_probe_body() (wraith-alpha_manager.c) can splice it
 * into this project's own Window the same way settings_manager.c's
 * write_wraith_body() does for the embedded editor page -- same
 * markup shape (cli_io x/y/width/height + KEY:5-13 nudge/apply
 * buttons), just always-on instead of gated behind a page-state
 * machine, since a standalone window-geom Window has no other page to
 * show. target_project_id comes from session/wg_target.txt (written
 * by wraith-alpha_manager.c's DESKTOP_ACTION:open_window_geom: handler
 * right before it opens this Window), not the active-window fallback
 * write_state()/manager/state.txt still use for the older, deferred
 * href-based convention -- that path is left alone, unused by this
 * one, in case anything else still reads it. */
void write_wraith_body(void) {
    char target_project_id[256] = "";
    char body_path[4096];
    char tmp_path[4096];
    FILE *f;

    read_target_file(target_project_id, sizeof(target_project_id));

    snprintf(body_path, sizeof(body_path), "%s/session/wraith_body.txt", project_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", body_path);
    f = fopen(tmp_path, "w");
    if (!f) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        return;
    }

    if (!target_project_id[0]) {
        fprintf(f, "WINDOW GEOMETRY EDITOR\n");
        fprintf(f, "\n");
        fprintf(f, "No target project set (session/wg_target.txt is empty).\n");
        fprintf(f, "Open this from a project window's chrome button.\n");
        fclose(f);
        rename(tmp_path, body_path);
        return;
    }

    {
        char pdl_path[4096];
        char win_x[32] = "", win_y[32] = "", win_w[32] = "", win_h[32] = "";
        char edit_state_path[4096];
        char stored_target[256] = "";
        char edit_x[32] = "", edit_y[32] = "", edit_w[32] = "", edit_h[32] = "";
        char status[256] = "Ready to edit";
        FILE *ef;

        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root, target_project_id);
        read_pdl_value(pdl_path, "window_x", win_x, sizeof(win_x));
        read_pdl_value(pdl_path, "window_y", win_y, sizeof(win_y));
        read_pdl_value(pdl_path, "window_width", win_w, sizeof(win_w));
        read_pdl_value(pdl_path, "window_height", win_h, sizeof(win_h));

        /* Working values keyed to whichever target they were last
         * initialized for -- same reset-on-target-change,
         * preserve-on-same-target logic as settings_manager.c's own
         * window-geom: editor branch, just against this project's own
         * session/edit_state.txt instead of settings'. */
        snprintf(edit_state_path, sizeof(edit_state_path), "%s/session/edit_state.txt", project_dir);
        ef = fopen(edit_state_path, "r");
        if (ef) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), ef)) {
                char *eq = strchr(line, '=');
                char *key, *val, *nl;
                if (!eq) continue;
                *eq = '\0';
                key = trim_str(line);
                val = trim_str(eq + 1);
                nl = strchr(val, '\n');
                if (nl) *nl = '\0';
                if (strcmp(key, "target_project_id") == 0) strncpy(stored_target, val, sizeof(stored_target) - 1);
                else if (strcmp(key, "edit_x") == 0) strncpy(edit_x, val, sizeof(edit_x) - 1);
                else if (strcmp(key, "edit_y") == 0) strncpy(edit_y, val, sizeof(edit_y) - 1);
                else if (strcmp(key, "edit_width") == 0) strncpy(edit_w, val, sizeof(edit_w) - 1);
                else if (strcmp(key, "edit_height") == 0) strncpy(edit_h, val, sizeof(edit_h) - 1);
                else if (strcmp(key, "status") == 0) strncpy(status, val, sizeof(status) - 1);
            }
            fclose(ef);
        }

        if (strcmp(stored_target, target_project_id) != 0) {
            char tmp_es_path[4096];
            FILE *wf;

            strncpy(edit_x, win_x[0] ? win_x : "0", sizeof(edit_x) - 1);
            strncpy(edit_y, win_y[0] ? win_y : "0", sizeof(edit_y) - 1);
            strncpy(edit_w, win_w[0] ? win_w : "0", sizeof(edit_w) - 1);
            strncpy(edit_h, win_h[0] ? win_h : "0", sizeof(edit_h) - 1);
            snprintf(status, sizeof(status), "Editing %s", target_project_id);

            snprintf(tmp_es_path, sizeof(tmp_es_path), "%s.tmp", edit_state_path);
            wf = fopen(tmp_es_path, "w");
            if (wf) {
                fprintf(wf, "target_project_id=%s\n", target_project_id);
                fprintf(wf, "edit_x=%s\n", edit_x);
                fprintf(wf, "edit_y=%s\n", edit_y);
                fprintf(wf, "edit_width=%s\n", edit_w);
                fprintf(wf, "edit_height=%s\n", edit_h);
                fprintf(wf, "status=%s\n", status);
                fclose(wf);
                rename(tmp_es_path, edit_state_path);
            }

            /* Seed the cli_io fields' OWN persisted state (gui_state.txt,
             * keyed by target_id) with the real current geometry, so they
             * open pre-filled with the actual number instead of empty
             * (see 2fix-july6.txt section 8). Only on a genuine target
             * switch, same guard as edit_state.txt above. */
            write_gui_state_kv("edit_x", edit_x);
            write_gui_state_kv("edit_y", edit_y);
            write_gui_state_kv("edit_width", edit_w);
            write_gui_state_kv("edit_height", edit_h);
        }

        fprintf(f, "WINDOW GEOMETRY EDITOR (standalone)\n");
        fprintf(f, "\n");
        fprintf(f, "Project: %s\n", target_project_id);
        fprintf(f, "Current: x=%s y=%s width=%s height=%s\n",
                win_x[0] ? win_x : "0 (not set)", win_y[0] ? win_y : "0 (not set)",
                win_w[0] ? win_w : "0 (not set)", win_h[0] ? win_h : "0 (not set)");
        fprintf(f, "\n");
        fprintf(f, "Edit via CLI Input:\n");
        /* Name is a separate, non-editable <text> caption -- the cli_io's
         * own label is deliberately empty (2fix-july6.txt section 8). */
        fprintf(f, "<text label=\"  X position: \" /><cli_io id=\"edit_x\" label=\"\" target_id=\"edit_x\" input_mode=\"numeric\" /><br/>\n");
        fprintf(f, "<text label=\"  Y position: \" /><cli_io id=\"edit_y\" label=\"\" target_id=\"edit_y\" input_mode=\"numeric\" /><br/>\n");
        fprintf(f, "<text label=\"  Width: \" /><cli_io id=\"edit_width\" label=\"\" target_id=\"edit_width\" input_mode=\"numeric\" /><br/>\n");
        fprintf(f, "<text label=\"  Height: \" /><cli_io id=\"edit_height\" label=\"\" target_id=\"edit_height\" input_mode=\"numeric\" /><br/>\n");
        fprintf(f, "\n");
        fprintf(f, "Or use buttons (working values: x=%s y=%s w=%s h=%s):\n", edit_x, edit_y, edit_w, edit_h);
        fprintf(f, "<button label=\"[-] X\" onClick=\"KEY:5\" /><button label=\"[+] X\" onClick=\"KEY:6\" /><br/>\n");
        fprintf(f, "<button label=\"[-] Y\" onClick=\"KEY:7\" /><button label=\"[+] Y\" onClick=\"KEY:8\" /><br/>\n");
        fprintf(f, "<button label=\"[-] W\" onClick=\"KEY:9\" /><button label=\"[+] W\" onClick=\"KEY:10\" /><br/>\n");
        fprintf(f, "<button label=\"[-] H\" onClick=\"KEY:11\" /><button label=\"[+] H\" onClick=\"KEY:12\" /><br/>\n");
        fprintf(f, "\n");
        fprintf(f, "<button label=\"Apply Changes\" onClick=\"KEY:13\" /><br/>\n");
        fprintf(f, "\n");
        fprintf(f, "Status: %s\n", status);
    }

    fclose(f);
    rename(tmp_path, body_path);
}

void trigger_render(void) {
    char frame_marker[4096];
    char fs_watch_marker[4096];
    FILE *f;

    snprintf(frame_marker, sizeof(frame_marker), "%s/pieces/display/frame_changed.txt", project_root);
    f = fopen(frame_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }

    /* Same reasoning as settings_manager.c's trigger_render(): bumping
       frame_changed.txt alone doesn't make wraith-alpha_manager.c re-run
       update_state() to re-embed fresh content. session/fs_watch.marker is
       already polled at ~60Hz for whichever project is the focused window
       (process_active_project_marker()) -- only matters when window-geom
       IS that focused window (standalone/chrome-button mode), which is
       exactly when this manager's own state changes need to reach the screen. */
    snprintf(fs_watch_marker, sizeof(fs_watch_marker), "%s/session/fs_watch.marker", project_dir);
    f = fopen(fs_watch_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }
}

int main(void) {
    struct stat st;
    off_t last_regenerate_size = 0;
    char regenerate_marker[4096];

    resolve_paths();
    log_debug("Window Geometry Manager Started");
    log_debug("Project root: %s", project_root);
    log_debug("Project dir: %s", project_dir);

    write_state();
    write_wraith_body();
    trigger_render();

    snprintf(regenerate_marker, sizeof(regenerate_marker), "%s/session/regenerate_marker.txt", project_dir);
    if (stat(regenerate_marker, &st) == 0) {
        last_regenerate_size = st.st_size;
    }

    log_debug("Window Geometry Manager init complete, entering hot-path loop");

    while (1) {
        /* write_state() still refreshes manager/state.txt for the older,
           deferred href-based convention (left alone, unused today).
           write_wraith_body() is what actually reaches the screen now
           that this project is opened as a standalone Window via
           launch_window_instance() -- append_project_probe_body() reads
           session/wraith_body.txt, not manager/state.txt, for any
           Window's body. Both are refreshed on the same marker growth:
           this project's own input-op (KEY:5-13, see
           ops/src/wraith_project_input.c) bumps regenerate_marker.txt
           after every edit/Apply. */
        if (stat(regenerate_marker, &st) == 0 && st.st_size > last_regenerate_size) {
            last_regenerate_size = st.st_size;
            log_debug("Regenerate marker grew, updating state for standalone view");
            write_state();
            write_wraith_body();
            trigger_render();
        }

        usleep(50000); /* 50ms poll interval */
    }

    return 0;
}
