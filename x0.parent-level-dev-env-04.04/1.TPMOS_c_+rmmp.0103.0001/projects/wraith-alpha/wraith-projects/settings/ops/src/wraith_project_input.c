#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 2048

/*
 * Real hot-path logic for the "settings" Wraith project. Invoked fresh on
 * every KEY: press by wraith-alpha_manager.c's run_active_project_input_op()
 * (see terminal/ops/src/wraith_project_input.c for the reference pattern
 * this follows).
 *
 * Only does anything while settings' embedded page (session/state_changed.txt's
 * last line, written by SETTINGS_PAGE:...) is "window-geom:<project_id>" --
 * i.e. the geometry editor for a specific picked project is on screen.
 * Handles KEY:5-12 (+/- nudges for x/y/width/height) and KEY:13 (Apply:
 * persist the working values into that project's own project.pdl WINDOW
 * section). Everything else (KEY: presses on the settings menu or the
 * project picker) is a no-op here -- those pages don't need a hot-path op.
 */

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    if (a[0] && a[strlen(a) - 1] == '/') snprintf(out, out_sz, "%s%s", a, b);
    else snprintf(out, out_sz, "%s/%s", a, b);
}

static char *trim_str(char *s) {
    char *end;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static int read_last_key_pressed(const char *root) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;
    int last_key = -1;

    path_join(path, sizeof(path), root, "session/history.txt");
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "KEY_PRESSED:");
        if (p) {
            p += strlen("KEY_PRESSED:");
            while (*p && isspace((unsigned char)*p)) p++;
            last_key = atoi(p);
        }
    }
    fclose(f);
    return last_key;
}

static void read_active_page(const char *root, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN];
    char line[256];
    FILE *f;

    snprintf(out, out_sz, "settings");
    path_join(path, sizeof(path), root, "session/state_changed.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0]) snprintf(out, out_sz, "%s", line);
    }
    fclose(f);
}


/* Same substring-key/pipe-split convention as settings_manager.c's /
 * wraith-alpha_manager.c's read_pdl_value(). */
static void read_pdl_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE_LEN];

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

/* Same atomic-write, pipe-delimited (SECTION | KEY | VALUE) update as
 * window-geom_manager.c's write_pdl_value() -- reimplemented locally
 * since this is a separate binary. */
static void write_pdl_value(const char *path, const char *section, const char *key, const char *value) {
    FILE *src, *tmp;
    char line[MAX_LINE_LEN];
    char tmp_path[MAX_PATH_LEN];
    int found = 0;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    src = fopen(path, "r");
    tmp = fopen(tmp_path, "w");
    if (!tmp) {
        if (src) fclose(src);
        return;
    }

    if (src) {
        while (fgets(line, sizeof(line), src)) {
            char *pipe1 = strchr(line, '|');
            char line_section[64] = "", line_key[64] = "";

            if (pipe1) {
                char *pipe2 = strchr(pipe1 + 1, '|');
                if (pipe2) {
                    size_t sec_len = (size_t)(pipe1 - line);
                    size_t key_len = (size_t)(pipe2 - pipe1 - 1);
                    if (sec_len > 0 && sec_len < sizeof(line_section)) {
                        memcpy(line_section, line, sec_len);
                        line_section[sec_len] = '\0';
                        line_section[strcspn(line_section, " \t")] = '\0';
                    }
                    if (key_len > 0 && key_len < sizeof(line_key)) {
                        char *ks, *ke;
                        memcpy(line_key, pipe1 + 1, key_len);
                        line_key[key_len] = '\0';
                        ks = line_key;
                        while (*ks && isspace((unsigned char)*ks)) ks++;
                        ke = ks + strlen(ks) - 1;
                        while (ke > ks && isspace((unsigned char)*ke)) ke--;
                        ke[1] = '\0';
                        memmove(line_key, ks, strlen(ks) + 1);
                    }
                    if (strcmp(line_section, section) == 0 && strcmp(line_key, key) == 0) {
                        fprintf(tmp, "%s | %s | %s\n", section, key, value);
                        found = 1;
                        continue;
                    }
                }
            }
            fputs(line, tmp);
        }
        fclose(src);
    }

    if (!found) {
        fprintf(tmp, "%s | %s | %s\n", section, key, value);
    }

    fclose(tmp);
    rename(tmp_path, path);
}

static void derive_repo_root(const char *project_dir, char *repo_root, size_t repo_root_sz) {
    const char *needle = "/projects/wraith-alpha/wraith-projects/";
    const char *p = strstr(project_dir, needle);
    if (p) {
        size_t len = (size_t)(p - project_dir);
        if (len >= repo_root_sz) len = repo_root_sz - 1;
        memcpy(repo_root, project_dir, len);
        repo_root[len] = '\0';
        return;
    }
    snprintf(repo_root, repo_root_sz, "%s", project_dir);
}

/* ---- 2026-07-11: real page-navigation state, replacing settings_manager.c's
   removed usleep(50000) polling loop -- see j11.wraith-foundation-fix-fut.txt
   and that file's own header comment. Below: write_gui_state_kv() (merge-
   preserving KVP writer for manager/gui_state.txt) + project discovery +
   write_settings_menu_state()/write_page_state(), computed fresh on every
   invocation of this op instead of on a timer. ---- */

#define MAX_ENTRIES 32
#define MAX_PROJECT_PICKS 128

typedef struct {
    char project_id[256];
    char title[128];
} ProjectPick;

static void write_gui_state_kv(const char *root, const char *key, const char *value) {
    char path[MAX_PATH_LEN];
    char tmp_path[MAX_PATH_LEN];
    char names[64][64];
    char values[64][256];
    int count = 0;
    int found = 0;
    FILE *f;

    path_join(path, sizeof(path), root, "manager/gui_state.txt");
    f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE_LEN];
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

static int discover_entries(const char *root, char labels[MAX_ENTRIES][256], char layouts[MAX_ENTRIES][MAX_PATH_LEN]) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(root);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL && count < MAX_ENTRIES) {
        char sub_dir[MAX_PATH_LEN];
        char marker_path[MAX_PATH_LEN];
        struct stat st;

        if (entry->d_name[0] == '.') continue;

        path_join(sub_dir, sizeof(sub_dir), root, entry->d_name);
        if (stat(sub_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        path_join(marker_path, sizeof(marker_path), sub_dir, "settings_entry.pdl");
        if (access(marker_path, F_OK) != 0) continue;

        read_pdl_value(marker_path, "label", labels[count], sizeof(labels[0]));
        read_pdl_value(marker_path, "entry_layout", layouts[count], sizeof(layouts[0]));
        if (labels[count][0] && layouts[count][0]) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

static void discover_wraith_projects_recursive(const char *dir, ProjectPick *out, int *count, int max) {
    DIR *d;
    struct dirent *entry;
    char pdl_path[MAX_PATH_LEN];

    if (*count >= max) return;

    path_join(pdl_path, sizeof(pdl_path), dir, "project.pdl");
    if (access(pdl_path, F_OK) == 0) {
        char project_id[256] = "";
        char title[128] = "";
        read_pdl_value(pdl_path, "project_id", project_id, sizeof(project_id));
        read_pdl_value(pdl_path, "title", title, sizeof(title));
        if (project_id[0] && *count < max) {
            strncpy(out[*count].project_id, project_id, sizeof(out[*count].project_id) - 1);
            out[*count].project_id[sizeof(out[*count].project_id) - 1] = '\0';
            strncpy(out[*count].title, title[0] ? title : project_id, sizeof(out[*count].title) - 1);
            out[*count].title[sizeof(out[*count].title) - 1] = '\0';
            (*count)++;
        }
    }

    d = opendir(dir);
    if (!d) return;
    while ((entry = readdir(d)) != NULL && *count < max) {
        char sub_dir[MAX_PATH_LEN];
        struct stat st;

        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "manager") == 0 || strcmp(entry->d_name, "ops") == 0 ||
            strcmp(entry->d_name, "plugins") == 0 || strcmp(entry->d_name, "layouts") == 0 ||
            strcmp(entry->d_name, "session") == 0 || strcmp(entry->d_name, "+x") == 0) continue;

        path_join(sub_dir, sizeof(sub_dir), dir, entry->d_name);
        if (stat(sub_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        discover_wraith_projects_recursive(sub_dir, out, count, max);
    }
    closedir(d);
}

static int discover_all_wraith_projects(const char *repo_root, ProjectPick *out, int max) {
    char wraith_projects_dir[MAX_PATH_LEN];
    int count = 0;
    path_join(wraith_projects_dir, sizeof(wraith_projects_dir), repo_root, "projects/wraith-alpha/wraith-projects");
    discover_wraith_projects_recursive(wraith_projects_dir, out, &count, max);
    return count;
}

static void write_settings_menu_state(const char *root) {
    char labels[MAX_ENTRIES][256];
    char layouts[MAX_ENTRIES][MAX_PATH_LEN];
    int count;
    int i;
    char menu_markup[4096] = "";

    count = discover_entries(root, labels, layouts);

    if (count == 0) {
        snprintf(menu_markup, sizeof(menu_markup), "(no settings entries found)");
    } else {
        for (i = 0; i < count; i++) {
            char one[1024];
            char entry_id[256];
            char layout_copy[MAX_PATH_LEN];
            char *layouts_pos, *slash;
            strncpy(layout_copy, layouts[i], sizeof(layout_copy) - 1);
            layout_copy[sizeof(layout_copy) - 1] = '\0';
            layouts_pos = strstr(layout_copy, "/layouts/");
            if (layouts_pos) {
                *layouts_pos = '\0';
                slash = strrchr(layout_copy, '/');
                if (slash) {
                    strncpy(entry_id, slash + 1, sizeof(entry_id) - 1);
                    entry_id[sizeof(entry_id) - 1] = '\0';
                } else {
                    strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
                }
            } else {
                strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
            }
            snprintf(one, sizeof(one), "<button label=\"%s\" onClick=\"SETTINGS_PAGE:%s\" /><br/>",
                     labels[i], entry_id);
            strncat(menu_markup, one, sizeof(menu_markup) - strlen(menu_markup) - 1);
        }
    }

    write_gui_state_kv(root, "settings_menu_markup", menu_markup);
    {
        char count_str[16];
        snprintf(count_str, sizeof(count_str), "%d", count);
        write_gui_state_kv(root, "settings_entry_count", count_str);
    }
}

/* Publishes VALUES for whichever page is active to manager/gui_state.txt
   (ASCII, via wraith-alpha_manager.c's render_project_layout_body() +
   active_layout), and dual-writes real per-page markup into
   session/wraith_body.txt (GL, via emit_embedded_line_objects() -- not
   yet migrated onto gui_state.txt, see j11.wraith-foundation-fix-fut.txt
   "ASCII/GL parity"). */
static void write_page_state(const char *root, const char *repo_root, const char *active_page) {
    char body_path[MAX_PATH_LEN];
    char body_content[16384] = "";
    FILE *bf;

    if (strcmp(active_page, "settings") == 0) {
        int label_len, rendered_estimate, pad;
        char labels[MAX_ENTRIES][256];
        char layouts[MAX_ENTRIES][MAX_PATH_LEN];
        int count = discover_entries(root, labels, layouts);
        int i;
        write_gui_state_kv(root, "active_layout", "");
        if (count == 0) {
            strncat(body_content, "<text label=\"| |  (no settings entries found)                                                            |\" /><br/>\n",
                    sizeof(body_content) - strlen(body_content) - 1);
        } else {
            for (i = 0; i < count; i++) {
                char entry_id[256];
                char layout_copy[MAX_PATH_LEN];
                char *layouts_pos, *slash;
                char one[1024];
                strncpy(layout_copy, layouts[i], sizeof(layout_copy) - 1);
                layout_copy[sizeof(layout_copy) - 1] = '\0';
                layouts_pos = strstr(layout_copy, "/layouts/");
                if (layouts_pos) {
                    *layouts_pos = '\0';
                    slash = strrchr(layout_copy, '/');
                    if (slash) {
                        strncpy(entry_id, slash + 1, sizeof(entry_id) - 1);
                        entry_id[sizeof(entry_id) - 1] = '\0';
                    } else {
                        strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
                    }
                } else {
                    strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
                }
                label_len = (int)strlen(labels[i]);
                rendered_estimate = label_len + 8;
                pad = 83 - rendered_estimate;
                if (pad < 1) pad = 1;
                snprintf(one, sizeof(one), "<text label=\"| |  \" /><button label=\"%s\" onClick=\"SETTINGS_PAGE:%s\" /><text label=\"%*s|\" /><br/>\n",
                        labels[i], entry_id, pad, "");
                strncat(body_content, one, sizeof(body_content) - strlen(body_content) - 1);
            }
        }
    } else if (strcmp(active_page, "window-geom") == 0) {
        ProjectPick picks[MAX_PROJECT_PICKS];
        int pick_count = discover_all_wraith_projects(repo_root, picks, MAX_PROJECT_PICKS);
        char picker_markup[8192] = "";
        char picker_body_lines[8192] = "";
        int i;

        if (pick_count == 0) {
            snprintf(picker_markup, sizeof(picker_markup), "(no wraith projects found)<br/>");
            snprintf(picker_body_lines, sizeof(picker_body_lines), "(no wraith projects found)\n");
        } else {
            for (i = 0; i < pick_count; i++) {
                char one[512];
                char one_line[560];
                snprintf(one, sizeof(one), "<button label=\"%s\" onClick=\"SETTINGS_PAGE:window-geom:%s\" /><br/>",
                         picks[i].title, picks[i].project_id);
                strncat(picker_markup, one, sizeof(picker_markup) - strlen(picker_markup) - 1);
                snprintf(one_line, sizeof(one_line), "%s\n", one);
                strncat(picker_body_lines, one_line, sizeof(picker_body_lines) - strlen(picker_body_lines) - 1);
            }
        }
        write_gui_state_kv(root, "project_picker_markup", picker_markup);
        write_gui_state_kv(root, "active_layout",
            "projects/wraith-alpha/wraith-projects/settings/layouts/window-geom-picker.chtpm");

        strncat(body_content, "WINDOW GEOMETRY -- choose a project to edit:\n\n",
                sizeof(body_content) - strlen(body_content) - 1);
        strncat(body_content, picker_body_lines, sizeof(body_content) - strlen(body_content) - 1);
        strncat(body_content, "\n<button label=\"Back\" onClick=\"SETTINGS_PAGE:settings\" /><br/>\n",
                sizeof(body_content) - strlen(body_content) - 1);
    } else if (strncmp(active_page, "window-geom:", 12) == 0) {
        const char *target_project_id = active_page + 12;
        char pdl_path[MAX_PATH_LEN];
        char win_x[32] = "", win_y[32] = "", win_w[32] = "", win_h[32] = "";
        char edit_state_path[MAX_PATH_LEN];
        char edit_x[32] = "0", edit_y[32] = "0", edit_w[32] = "0", edit_h[32] = "0";
        char status[256] = "Ready to edit";
        FILE *ef;

        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", repo_root, target_project_id);
        read_pdl_value(pdl_path, "window_x", win_x, sizeof(win_x));
        read_pdl_value(pdl_path, "window_y", win_y, sizeof(win_y));
        read_pdl_value(pdl_path, "window_width", win_w, sizeof(win_w));
        read_pdl_value(pdl_path, "window_height", win_h, sizeof(win_h));

        /* Re-read edit_state.txt fresh here (rather than threading the
           nudge/Apply block's own values through) -- simpler, and this
           file is the single source of truth for "current working
           values" either way; main()'s nudge/Apply block already wrote
           it before falling through to this function. */
        path_join(edit_state_path, sizeof(edit_state_path), root, "session/edit_state.txt");
        ef = fopen(edit_state_path, "r");
        if (ef) {
            char line[MAX_LINE_LEN];
            while (fgets(line, sizeof(line), ef)) {
                char *eq = strchr(line, '=');
                char *k, *v, *nl;
                if (!eq) continue;
                *eq = '\0';
                k = trim_str(line);
                v = trim_str(eq + 1);
                nl = strchr(v, '\n');
                if (nl) *nl = '\0';
                if (strcmp(k, "edit_x") == 0) strncpy(edit_x, v, sizeof(edit_x) - 1);
                else if (strcmp(k, "edit_y") == 0) strncpy(edit_y, v, sizeof(edit_y) - 1);
                else if (strcmp(k, "edit_width") == 0) strncpy(edit_w, v, sizeof(edit_w) - 1);
                else if (strcmp(k, "edit_height") == 0) strncpy(edit_h, v, sizeof(edit_h) - 1);
                else if (strcmp(k, "status") == 0) strncpy(status, v, sizeof(status) - 1);
            }
            fclose(ef);
        } else {
            /* First time this target is opened this session -- seed
               edit_state.txt AND the cli_io fields' own gui_state.txt
               values from the target's real current geometry, matching
               main()'s nudge/Apply block's "genuine target switch" path
               (2fix-july6.txt section 8: an empty cli_io only ever
               renders its own label text, never a real default). */
            char tmp_path[MAX_PATH_LEN];
            FILE *wf;
            strncpy(edit_x, win_x[0] ? win_x : "0", sizeof(edit_x) - 1);
            strncpy(edit_y, win_y[0] ? win_y : "0", sizeof(edit_y) - 1);
            strncpy(edit_w, win_w[0] ? win_w : "0", sizeof(edit_w) - 1);
            strncpy(edit_h, win_h[0] ? win_h : "0", sizeof(edit_h) - 1);
            snprintf(status, sizeof(status), "Editing %s", target_project_id);

            snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", edit_state_path);
            wf = fopen(tmp_path, "w");
            if (wf) {
                fprintf(wf, "target_project_id=%s\n", target_project_id);
                fprintf(wf, "edit_x=%s\n", edit_x);
                fprintf(wf, "edit_y=%s\n", edit_y);
                fprintf(wf, "edit_width=%s\n", edit_w);
                fprintf(wf, "edit_height=%s\n", edit_h);
                fprintf(wf, "status=%s\n", status);
                fclose(wf);
                rename(tmp_path, edit_state_path);
            }
            write_gui_state_kv(root, "edit_x", edit_x);
            write_gui_state_kv(root, "edit_y", edit_y);
            write_gui_state_kv(root, "edit_width", edit_w);
            write_gui_state_kv(root, "edit_height", edit_h);
        }

        write_gui_state_kv(root, "target_project_id", target_project_id);
        write_gui_state_kv(root, "win_x", win_x[0] ? win_x : "0 (not set)");
        write_gui_state_kv(root, "win_y", win_y[0] ? win_y : "0 (not set)");
        write_gui_state_kv(root, "win_w", win_w[0] ? win_w : "0 (not set)");
        write_gui_state_kv(root, "win_h", win_h[0] ? win_h : "0 (not set)");
        write_gui_state_kv(root, "edit_x", edit_x);
        write_gui_state_kv(root, "edit_y", edit_y);
        write_gui_state_kv(root, "edit_w", edit_w);
        write_gui_state_kv(root, "edit_h", edit_h);
        write_gui_state_kv(root, "status", status);
        write_gui_state_kv(root, "active_layout",
            "projects/wraith-alpha/wraith-projects/settings/layouts/window-geom-editor.chtpm");

        {
            char buf[4096];
            snprintf(buf, sizeof(buf),
                "WINDOW GEOMETRY EDITOR\n\nProject: %s\nCurrent: x=%s y=%s width=%s height=%s\n\n"
                "Edit via CLI Input:\n"
                "<text label=\"  X position: \" /><cli_io id=\"edit_x\" label=\"\" target_id=\"edit_x\" input_mode=\"numeric\" /><br/>\n"
                "<text label=\"  Y position: \" /><cli_io id=\"edit_y\" label=\"\" target_id=\"edit_y\" input_mode=\"numeric\" /><br/>\n"
                "<text label=\"  Width: \" /><cli_io id=\"edit_width\" label=\"\" target_id=\"edit_width\" input_mode=\"numeric\" /><br/>\n"
                "<text label=\"  Height: \" /><cli_io id=\"edit_height\" label=\"\" target_id=\"edit_height\" input_mode=\"numeric\" /><br/>\n"
                "\nOr use buttons (working values: x=%s y=%s w=%s h=%s):\n"
                "<button label=\"[-] X\" onClick=\"KEY:5\" /><button label=\"[+] X\" onClick=\"KEY:6\" /><br/>\n"
                "<button label=\"[-] Y\" onClick=\"KEY:7\" /><button label=\"[+] Y\" onClick=\"KEY:8\" /><br/>\n"
                "<button label=\"[-] W\" onClick=\"KEY:9\" /><button label=\"[+] W\" onClick=\"KEY:10\" /><br/>\n"
                "<button label=\"[-] H\" onClick=\"KEY:11\" /><button label=\"[+] H\" onClick=\"KEY:12\" /><br/>\n"
                "\n<button label=\"Apply Changes\" onClick=\"KEY:13\" /><br/>\n"
                "<button label=\"Back to Project List\" onClick=\"SETTINGS_PAGE:window-geom\" /><br/>\n"
                "\nStatus: %s\n",
                target_project_id,
                win_x[0] ? win_x : "0 (not set)", win_y[0] ? win_y : "0 (not set)",
                win_w[0] ? win_w : "0 (not set)", win_h[0] ? win_h : "0 (not set)",
                edit_x, edit_y, edit_w, edit_h, status);
            strncat(body_content, buf, sizeof(body_content) - strlen(body_content) - 1);
        }
    }

    path_join(body_path, sizeof(body_path), root, "session/wraith_body.txt");
    bf = fopen(body_path, "w");
    if (bf) {
        fputs(body_content, bf);
        fclose(bf);
    }
}

int main(int argc, char **argv) {
    const char *root;
    char repo_root[MAX_PATH_LEN];
    char active_page[256];
    char edit_state_path[MAX_PATH_LEN];
    char target_project_id[256] = "";
    char edit_x[32] = "0", edit_y[32] = "0", edit_w[32] = "0", edit_h[32] = "0";
    char status[256] = "";
    int key;
    int x, y, w, h;
    FILE *ef;

    if (argc < 2) return 2;
    root = argv[1];

    /* wraith-alpha_manager.c's process_active_project_marker() re-invokes
     * this op every time session/fs_watch.marker grows -- including when
     * OUR OWN trigger_render() below is what grew it, not a new keypress.
     * It passes "marker_tick" as argv[2] specifically so this op can tell
     * the difference (a real key event comes through the OTHER call path,
     * run_active_project_input_op(), with no argv[2] at all). Ignoring
     * this was a real, confirmed, currently-shipping bug (2fix-july6.txt,
     * bug 1): read_last_key_pressed() re-reads whatever the last
     * KEY_PRESSED line happens to be, with no concept of "already
     * consumed," so any marker_tick invocation re-processed the same
     * stale key and re-triggered a render, which regrew fs_watch.marker,
     * which triggered another marker_tick, forever -- a real ~60Hz
     * infinite loop, not a one-time redundant render, and the direct
     * cause of the ASCII geometry editor's flicker. Still relevant after
     * the 2026-07-11 rewrite below: trigger_render() here only bumps
     * frame_changed.txt now (not fs_watch.marker), so this guard is now
     * belt-and-suspenders rather than load-bearing, but kept since
     * removing a safety net "because it shouldn't matter anymore" is
     * exactly the kind of unverified assumption this whole session's
     * regressions came from. */
    if (argc > 2 && strcmp(argv[2], "marker_tick") == 0) {
        return 0;
    }

    derive_repo_root(root, repo_root, sizeof(repo_root));

    read_active_page(root, active_page, sizeof(active_page));

    /* Nudge/Apply (KEY:5-13) -- ONLY meaningful on the geometry-editor
       page, unchanged from before the 2026-07-11 rewrite. Mutates
       edit_state.txt/project.pdl BEFORE write_settings_menu_state()/
       write_page_state() below run, so the freshly-nudged values are
       reflected in gui_state.txt/wraith_body.txt in this SAME
       invocation -- no second round-trip needed. */
    key = (strncmp(active_page, "window-geom:", 12) == 0) ? read_last_key_pressed(root) : -1;
    if (key < 5 || key > 13) key = -1;
    if (key < 0) goto publish_page_state;

    path_join(edit_state_path, sizeof(edit_state_path), root, "session/edit_state.txt");
    ef = fopen(edit_state_path, "r");
    if (ef) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), ef)) {
            char *eq = strchr(line, '=');
            char *k, *v, *nl;
            if (!eq) continue;
            *eq = '\0';
            k = trim_str(line);
            v = trim_str(eq + 1);
            nl = strchr(v, '\n');
            if (nl) *nl = '\0';
            if (strcmp(k, "target_project_id") == 0) strncpy(target_project_id, v, sizeof(target_project_id) - 1);
            else if (strcmp(k, "edit_x") == 0) strncpy(edit_x, v, sizeof(edit_x) - 1);
            else if (strcmp(k, "edit_y") == 0) strncpy(edit_y, v, sizeof(edit_y) - 1);
            else if (strcmp(k, "edit_width") == 0) strncpy(edit_w, v, sizeof(edit_w) - 1);
            else if (strcmp(k, "edit_height") == 0) strncpy(edit_h, v, sizeof(edit_h) - 1);
        }
        fclose(ef);
    }
    if (!target_project_id[0]) return 0;

    /* Typed cli_io values (if any) override the working values before
     * applying a nudge or Apply -- lets the user type an exact number
     * instead of only nudging with +/- buttons. Each cli_io field's
     * target_id (edit_x/edit_y/edit_width/edit_height) is its own
     * gui_state.txt variable -- see the wraith_parser_alpha.c fix that
     * made target_id actually distinguish fields instead of every cli_io
     * colliding on one shared "input_text" slot. */
    {
        char gui_state_path[MAX_PATH_LEN];
        FILE *gf;
        path_join(gui_state_path, sizeof(gui_state_path), root, "manager/gui_state.txt");
        gf = fopen(gui_state_path, "r");
        if (gf) {
            char line[MAX_LINE_LEN];
            while (fgets(line, sizeof(line), gf)) {
                char *eq = strchr(line, '=');
                char *k, *v, *nl;
                if (!eq) continue;
                *eq = '\0';
                k = trim_str(line);
                v = trim_str(eq + 1);
                nl = strchr(v, '\n');
                if (nl) *nl = '\0';
                if (v[0] && strcmp(k, "edit_x") == 0) strncpy(edit_x, v, sizeof(edit_x) - 1);
                else if (v[0] && strcmp(k, "edit_y") == 0) strncpy(edit_y, v, sizeof(edit_y) - 1);
                else if (v[0] && strcmp(k, "edit_width") == 0) strncpy(edit_w, v, sizeof(edit_w) - 1);
                else if (v[0] && strcmp(k, "edit_height") == 0) strncpy(edit_h, v, sizeof(edit_h) - 1);
            }
            fclose(gf);
        }
    }

    x = atoi(edit_x);
    y = atoi(edit_y);
    w = atoi(edit_w);
    h = atoi(edit_h);

    switch (key) {
        case 5: x -= 1; break;
        case 6: x += 1; break;
        case 7: y -= 1; break;
        case 8: y += 1; break;
        case 9: w -= 1; break;
        case 10: w += 1; break;
        case 11: h -= 1; break;
        case 12: h += 1; break;
        case 13: {
            char pdl_path[MAX_PATH_LEN];
            char val[32];

            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (w < 0) w = 0;
            if (h < 0) h = 0;

            snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", repo_root, target_project_id);
            snprintf(val, sizeof(val), "%d", x); write_pdl_value(pdl_path, "WINDOW", "window_x", val);
            snprintf(val, sizeof(val), "%d", y); write_pdl_value(pdl_path, "WINDOW", "window_y", val);
            snprintf(val, sizeof(val), "%d", w); write_pdl_value(pdl_path, "WINDOW", "window_width", val);
            snprintf(val, sizeof(val), "%d", h); write_pdl_value(pdl_path, "WINDOW", "window_height", val);

            snprintf(status, sizeof(status), "Saved to %s", target_project_id);
            break;
        }
        default: break;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (w < 0) w = 0;
    if (h < 0) h = 0;

    {
        char tmp_path[MAX_PATH_LEN];
        FILE *wf;
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", edit_state_path);
        wf = fopen(tmp_path, "w");
        if (wf) {
            fprintf(wf, "target_project_id=%s\n", target_project_id);
            fprintf(wf, "edit_x=%d\n", x);
            fprintf(wf, "edit_y=%d\n", y);
            fprintf(wf, "edit_width=%d\n", w);
            fprintf(wf, "edit_height=%d\n", h);
            fprintf(wf, "status=%s\n", status[0] ? status : "Ready to edit");
            fclose(wf);
            rename(tmp_path, edit_state_path);
        }
    }

publish_page_state:
    /* 2026-07-11 rewrite: no more marker-polling loop in settings_manager.c
       to notify (removed -- see that file's own header comment). This op
       now directly computes and publishes the current page's full state
       every time it runs, in the SAME invocation that may have just
       applied a nudge/Apply above -- no round-trip through a second
       process required. */
    write_settings_menu_state(root);
    write_page_state(root, repo_root, active_page);

    /* Do NOT call trigger_render() here -- confirmed live, 2026-07-11:
       every OTHER project's ops (fs, piececraft-wraith, web-cam, terminal,
       wraith-ed -- checked directly, none write frame_changed.txt) leaves
       render-triggering entirely to the caller. wraith-alpha_manager.c's
       route_command() ALREADY calls trigger_render() once, right after
       run_active_project_input_op(), for every dispatch path that reaches
       this op (KEY:/PROJECT_ACTION:/SETTINGS_PAGE:). This op self-triggering
       too (as it did before this rewrite, and as the merged-in original
       file from 2026-07-06 also did) means every settings action bumped
       frame_changed.txt TWICE -- a real, confirmed "second visible-frame
       trigger" (Pitfall #48, PITFALLS_ACTIVE_2026-03-18.txt), and the
       direct cause of the duplicate "FRAME UPDATE" blocks seen live for
       one single action. */

    return 0;
}
