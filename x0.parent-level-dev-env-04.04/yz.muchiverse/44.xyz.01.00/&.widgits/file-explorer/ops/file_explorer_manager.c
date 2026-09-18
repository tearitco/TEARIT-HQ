/*
 * file_explorer_manager - Directory browsing backend process
 * Communicates with GUI frontend via text files in package_dir
 * Usage: file_explorer_manager <house_root> <package_dir> <mode>
 *
 * REAL, NEW 2026-09-05 - argv order matches khtpm_core_render.c's own
 * launch_module() convention exactly (house_root, then package_dir,
 * then a <module>'s own id= as a single extra_arg) - this file was
 * originally speced/written with a different, hypothetical argv
 * shape (package_dir, start_dir, mode); adjusted here, once it's
 * actually being wired into the real launcher, rather than inventing
 * a start_dir argv slot launch_module() has no way to fill. Browsing
 * always starts at house_root itself - a real, honest v1 default, not
 * a placeholder; a configurable start_dir is real, separate future
 * work if a consumer ever needs one.
 */
#define _DEFAULT_SOURCE /* usleep() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_ENTRIES 512
#define MAX_NAME 300
#define MAX_CMD_BUFFER 4096

/* Right-click Cut/Copy/Paste/Delete/Place (2026-09-18). Same verbs as
 * HQ CTXMENU / piececraft CTX_*. Target id from fe_ctx_target.txt. */
static int fe_ctx_entry_idx(const char *package_dir) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s/fe_ctx_target.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    char line[MAX_PATH];
    int idx = -1;
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "id=", 3)) {
            const char *id = line + 3;
            if (!strncmp(id, "gentry", 6)) idx = atoi(id + 6);
            else if (!strncmp(id, "entry", 5)) idx = atoi(id + 5);
        }
    }
    fclose(f);
    return idx;
}

static void fe_clip_write(const char *package_dir, const char *mode, const char *path) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s/fe_clipboard.txt", package_dir);
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "mode=%s\npath=%s\n", mode, path);
    fclose(f);
}

static int fe_clip_read(const char *package_dir, char *mode, size_t msz, char *path, size_t psz) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s/fe_clipboard.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    char line[MAX_PATH];
    mode[0] = 0; path[0] = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = 0;
        if (!strncmp(line, "mode=", 5)) snprintf(mode, msz, "%s", line + 5);
        else if (!strncmp(line, "path=", 5)) snprintf(path, psz, "%s", line + 5);
    }
    fclose(f);
    return path[0] != 0;
}

typedef struct {
    char name[MAX_NAME];
    char type[4];
    char size[16];
    char icon[8];
} Entry;

#define MAX_CRUMBS 32
typedef struct {
    char label[MAX_NAME];
    char path[MAX_PATH];
} Crumb;

typedef struct {
    Entry entries[MAX_ENTRIES];
    int count;
    char current_dir[MAX_PATH];
    char mode[10];
    char pending_filename[MAX_NAME];
    int last_seq;
    Crumb crumbs[MAX_CRUMBS];
    int n_crumbs;
    int grid_view; /* 0=list (default), 1=grid - REAL, NEW 2026-09-15, direct live report ("we wanted list/grid toggle") */
    int has_back; /* REAL, NEW 2026-09-15 - Back is its own toolbar button now, not a list entry; see list_directory()'s own comment. */
    time_t dir_mtime;
    nlink_t dir_nlink;
} State;

static void fe_note_dir(State *state) {
    struct stat st;
    if (!state || stat(state->current_dir, &st) != 0) return;
    state->dir_mtime = st.st_mtime;
    state->dir_nlink = st.st_nlink;
}

/* REAL, NEW 2026-09-15, direct live report ("show current file path,
 * as button of each path that allows clicking and will jump to that
 * dir") - splits current_dir into real, individually-clickable
 * ancestor buttons ("Home" for the very root shown, then one per real
 * path segment down to the current dir itself), each one's own real
 * full absolute path stored so a click can jump straight there - no
 * repeated parent-walking needed. */
static void build_crumbs(State *state) {
    state->n_crumbs = 0;
    const char *p = state->current_dir;
    if (*p != '/') return;
    snprintf(state->crumbs[0].path, MAX_PATH, "/");
    snprintf(state->crumbs[0].label, MAX_NAME, "/");
    state->n_crumbs = 1;
    p++;
    char accum[MAX_PATH];
    snprintf(accum, MAX_PATH, "/");
    while (*p && state->n_crumbs < MAX_CRUMBS) {
        const char *seg_end = strchr(p, '/');
        size_t seg_len = seg_end ? (size_t)(seg_end - p) : strlen(p);
        if (seg_len > 0) {
            char seg[MAX_NAME];
            size_t n = seg_len < MAX_NAME - 1 ? seg_len : MAX_NAME - 1;
            memcpy(seg, p, n);
            seg[n] = '\0';
            size_t accum_len = strlen(accum);
            if (accum_len > 1) snprintf(accum + accum_len, MAX_PATH - accum_len, "/");
            accum_len = strlen(accum);
            snprintf(accum + accum_len, MAX_PATH - accum_len, "%s", seg);
            Crumb *c = &state->crumbs[state->n_crumbs];
            snprintf(c->label, MAX_NAME, "%s", seg);
            snprintf(c->path, MAX_PATH, "%s", accum);
            state->n_crumbs++;
        }
        if (!seg_end) break;
        p = seg_end + 1;
    }
}

int is_readable_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* REAL, NEW 2026-09-15, direct live report ("the file-browser widgit
 * is currently ugly and hard to use... a big DIR emoji for dirs, a
 * file emoji for file types and a disk emoji for 'house project'
 * type, that has a .pdl and is actually meant to be run from the
 * picker"). A "house project" dir is one this house's own real launch
 * convention already recognizes as a runnable app/game - `toy.pdl` is
 * that exact real marker (the same file DSR/db-hq-pal/chat-hai/every
 * other real HQ app uses, per khtpm-house-standards' own "toy.pdl
 * convention" - confirmed against this session's own DSR work, not
 * guessed). A plain directory with no toy.pdl is just a folder. */
int has_toy_pdl(const char *dir_path) {
    char toy_path[MAX_PATH];
    snprintf(toy_path, MAX_PATH, "%s/toy.pdl", dir_path);
    struct stat st;
    return stat(toy_path, &st) == 0 && S_ISREG(st.st_mode);
}

void get_parent_dir(const char *path, char *parent) {
    strcpy(parent, path);
    char *last_slash = strrchr(parent, '/');
    if (last_slash == NULL || last_slash == parent) {
        strcpy(parent, "/");
    } else {
        *last_slash = '\0';
    }
}

void format_size(off_t size, char *buf) {
    if (size < 1024) {
        snprintf(buf, 16, "%ldB", (long)size);
    } else if (size < 1024LL * 1024) {
        snprintf(buf, 16, "%ldKB", (long)(size / 1024));
    } else if (size < 1024LL * 1024 * 1024) {
        snprintf(buf, 16, "%ldMB", (long)(size / (1024LL * 1024)));
    } else {
        snprintf(buf, 16, "%ldGB", (long)(size / (1024LL * 1024 * 1024)));
    }
}

/* REAL, NEW 2026-09-15 - PRJ (a "house project" dir, see has_toy_pdl()
 * above) is still folder-shaped for sorting purposes - it sorts with
 * plain DIR entries, before any real file, not alphabetically mixed
 * in with files just because its own type string isn't "DIR". */
static int is_dir_like(const char *type) {
    return strcmp(type, "DIR") == 0 || strcmp(type, "PRJ") == 0;
}

int entry_cmp(const void *a, const void *b) {
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;

    int a_dir = is_dir_like(ea->type), b_dir = is_dir_like(eb->type);
    if (a_dir && !b_dir) return -1;
    if (!a_dir && b_dir) return 1;

    return strcmp(ea->name, eb->name);
}

void list_directory(const char *dir, State *state) {
    DIR *d = opendir(dir);
    if (!d) return;

    state->count = 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && state->count < MAX_ENTRIES) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        strncpy(state->entries[state->count].name, entry->d_name, MAX_NAME - 1);
        state->entries[state->count].name[MAX_NAME - 1] = '\0';

        if (S_ISDIR(st.st_mode)) {
            if (has_toy_pdl(full_path)) {
                strcpy(state->entries[state->count].type, "PRJ");
                strcpy(state->entries[state->count].icon, "\xf0\x9f\x92\xbe"); /* 💾 */
            } else {
                strcpy(state->entries[state->count].type, "DIR");
                strcpy(state->entries[state->count].icon, "\xf0\x9f\x93\x81"); /* 📁 */
            }
            strcpy(state->entries[state->count].size, "");
        } else {
            strcpy(state->entries[state->count].type, "FIL");
            strcpy(state->entries[state->count].icon, "\xf0\x9f\x93\x84"); /* 📄 */
            format_size(st.st_size, state->entries[state->count].size);
        }

        state->count++;
    }

    closedir(d);

    qsort(state->entries, state->count, sizeof(Entry), entry_cmp);

    /* REAL FIX 2026-09-15, direct live report ("can the back button and
     * grid view be on same row, instead of back being tied to other
     * files? get it?") - Back used to be a synthetic ".. Back" row
     * INSIDE state->entries[], scrolling and sorting along with real
     * files/dirs. Real fix: Back is no longer a list entry at all - it
     * is state->has_back (dir != "/"), published as its own flag and
     * rendered as a real, separate toolbar <item> next to the grid/list
     * toggle (file-explorer-pal.xhtpm's own top row), dispatched via
     * FE_BACK -> cmd "BACK" (handled directly, same get_parent_dir()
     * call the old ".." row used to trigger through ENTRY:). */
    state->has_back = strcmp(dir, "/") != 0;

    build_crumbs(state);
    fe_note_dir(state);
}

void write_ui_file(const char *package_dir, State *state,
                   const char *result, const char *result_action) {
    char ui_path[MAX_PATH];
    snprintf(ui_path, MAX_PATH, "%s/file_explorer_ui.txt", package_dir);

    FILE *f = fopen(ui_path, "w");
    if (!f) return;

    fprintf(f, "mode=%s\n", state->mode);
    /* REAL, NEW 2026-09-05 - file-explorer-pal.xhtpm's own Save row
     * (the cli_io filename field + Save button) uses show="${show_
     * save_row}" to stay hidden entirely in LOAD mode. */
    fprintf(f, "show_save_row=%d\n", strcmp(state->mode, "SAVE") == 0 ? 1 : 0);
    fprintf(f, "show_load_hint=%d\n", strcmp(state->mode, "SAVE") == 0 ? 0 : 1);
    fprintf(f, "is_list_view=%d\n", state->grid_view ? 0 : 1);
    fprintf(f, "is_grid_view=%d\n", state->grid_view ? 1 : 0);
    fprintf(f, "view_toggle_label=%s\n", state->grid_view ? "List View" : "Grid View");
    fprintf(f, "has_back=%d\n", state->has_back);
    /* REAL, NEW 2026-09-15 - the renderer's own real swatch-grid layout
     * path (khtpm_core_render.c, the SAME one palettes-emojis.xhtpm
     * already uses) triggers for the WHOLE page the instant ANY real
     * <item class="swatch"> exists anywhere in it - not per-region,
     * not show=-gated (that check only cares whether the element is
     * PRESENT in the tree at all). So list mode and grid mode can't be
     * two show=-toggled sibling regions of the same page the way the
     * status/action boxes elsewhere this session were - the grid
     * repeat's own count must be genuinely 0 (producing zero real
     * <item class="swatch"> elements) whenever grid mode is OFF, or
     * every list-mode render would silently flip into the swatch-grid
     * branch instead (which has no real <tabbar>/<scrolllist> handling
     * of its own - breadcrumbs and the file list would both vanish).
     * n_entries itself (the scrolllist's own count) stays the real
     * full count always - only the grid repeat's own count is gated. */
    fprintf(f, "n_grid_entries=%d\n", state->grid_view ? state->count : 0);
    fprintf(f, "dir=%s\n", state->current_dir);
    fprintf(f, "n_crumbs=%d\n", state->n_crumbs);
    for (int i = 0; i < state->n_crumbs; i++) {
        fprintf(f, "crumb_%d_label=%s\n", i, state->crumbs[i].label);
    }
    fprintf(f, "n_entries=%d\n", state->count);

    for (int i = 0; i < state->count; i++) {
        fprintf(f, "entry_%d_name=%s\n", i, state->entries[i].name);
        fprintf(f, "entry_%d_type=%s\n", i, state->entries[i].type);
        fprintf(f, "entry_%d_size=%s\n", i, state->entries[i].size);
        fprintf(f, "entry_%d_icon=%s\n", i, state->entries[i].icon);
    }

    fprintf(f, "filename=%s\n", state->pending_filename);
    fprintf(f, "result=%s\n", result ? result : "");
    fprintf(f, "result_action=%s\n", result_action ? result_action : "");

    fclose(f);
}

void read_action_file(const char *package_dir, int *seq, char *cmd) {
    char action_path[MAX_PATH];
    snprintf(action_path, MAX_PATH, "%s/file_explorer_action.txt", package_dir);

    FILE *f = fopen(action_path, "r");
    if (!f) {
        *seq = 0;
        cmd[0] = '\0';
        return;
    }

    char buffer[MAX_CMD_BUFFER];
    memset(buffer, 0, MAX_CMD_BUFFER);
    size_t bytes = fread(buffer, 1, MAX_CMD_BUFFER - 1, f);
    fclose(f);

    if (bytes == 0) {
        *seq = 0;
        cmd[0] = '\0';
        return;
    }

    buffer[bytes] = '\0';

    *seq = 0;
    cmd[0] = '\0';

    char *line1_end = strchr(buffer, '\n');
    if (line1_end) {
        *line1_end = '\0';
    }
    if (strncmp(buffer, "seq=", 4) == 0) {
        *seq = atoi(buffer + 4);
    }

    if (line1_end) {
        char *line2 = line1_end + 1;
        char *line2_end = strchr(line2, '\n');
        if (line2_end) {
            *line2_end = '\0';
        }
        if (strncmp(line2, "cmd=", 4) == 0) {
            strncpy(cmd, line2 + 4, MAX_CMD_BUFFER - 1);
            cmd[MAX_CMD_BUFFER - 1] = '\0';
        }
    }
}

/* REAL, NEW 2026-09-08 - the "make it a real picker" contract. A
 * caller that wants a modal file pick writes <package_dir>/fe_request.txt
 * BEFORE launching this widget:
 *     mode=LOAD|SAVE
 *     start_dir=/abs/path        (where to start browsing)
 *     result_file=/abs/path.txt  (where to write the chosen path)
 * We read it on startup, then UNLINK it (so a stale request can't leak
 * into the next standalone launch). On a pick / saveas / cancel we write
 * the chosen absolute path (empty on cancel) to result_file, atomically
 * (tmp + rename), in addition to the existing file_explorer_ui.txt
 * result= key. No request file  ->  the original argv[3]=mode + start
 * at house_root behavior, byte-for-byte. */
static void fe_read_request(const char *package_dir, char *mode_out, size_t mode_sz,
                            char *start_out, size_t start_sz,
                            char *result_out, size_t result_sz) {
    char rp[MAX_PATH];
    snprintf(rp, MAX_PATH, "%s/fe_request.txt", package_dir);
    FILE *f = fopen(rp, "r");
    if (!f) return;
    char line[MAX_PATH];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        if (!strncmp(line, "mode=", 5))             snprintf(mode_out, mode_sz, "%s", line + 5);
        else if (!strncmp(line, "start_dir=", 10))  snprintf(start_out, start_sz, "%s", line + 10);
        else if (!strncmp(line, "result_file=", 12)) snprintf(result_out, result_sz, "%s", line + 12);
    }
    fclose(f);
    unlink(rp);
}

static void fe_write_result_file(const char *result_file, const char *path) {
    if (!result_file || !result_file[0]) return;
    char tmp[MAX_PATH];
    snprintf(tmp, MAX_PATH, "%s.tmp", result_file);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "%s\n", path ? path : "");
    fclose(f);
    rename(tmp, result_file);
}

/* result_file is filled by fe_read_request(); file scope so the pick
 * handlers in the loop can see it without threading it through. */
static char g_fe_result_file[MAX_PATH] = "";

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> <mode>\n", argv[0]);
        return 1;
    }

    const char *house_root = argv[1];
    const char *package_dir = argv[2];
    char mode_buf[10];
    snprintf(mode_buf, sizeof(mode_buf), "%s", argv[3]);
    char start_buf[MAX_PATH] = "";

    fe_read_request(package_dir, mode_buf, sizeof(mode_buf),
                    start_buf, sizeof(start_buf),
                    g_fe_result_file, sizeof(g_fe_result_file));

    const char *mode = mode_buf;
    const char *start_dir = start_buf[0] ? start_buf : house_root;

    State state;
    memset(&state, 0, sizeof(state));
    strncpy(state.mode, mode, 9);
    state.last_seq = 0;

    if (!is_readable_dir(start_dir)) {
        start_dir = package_dir;
    }

    if (!is_readable_dir(start_dir)) {
        fprintf(stderr, "Error: cannot access start directory\n");
        return 1;
    }

    strncpy(state.current_dir, start_dir, MAX_PATH - 1);
    state.current_dir[MAX_PATH - 1] = '\0';

    char action_path[MAX_PATH];
    snprintf(action_path, MAX_PATH, "%s/file_explorer_action.txt", package_dir);
    FILE *f = fopen(action_path, "w");
    if (f) {
        fprintf(f, "seq=0\ncmd=\n");
        fclose(f);
    }

    list_directory(state.current_dir, &state);
    write_ui_file(package_dir, &state, "", "");

    while (1) {
        usleep(50000);

        {
            struct stat dst;
            if (stat(state.current_dir, &dst) == 0 &&
                (dst.st_mtime != state.dir_mtime || dst.st_nlink != state.dir_nlink)) {
                list_directory(state.current_dir, &state);
                write_ui_file(package_dir, &state, "", "");
            }
        }

        int seq;
        char cmd[MAX_CMD_BUFFER];
        read_action_file(package_dir, &seq, cmd);

        if (seq <= state.last_seq || cmd[0] == '\0') {
            continue;
        }

        state.last_seq = seq;

        if (strncmp(cmd, "ENTRY:", 6) == 0) {
            int idx = atoi(cmd + 6);
            if (idx < 0 || idx >= state.count) {
                continue;
            }

            Entry *e = &state.entries[idx];

            if (strcmp(e->type, "DIR") == 0) {
                char new_dir[MAX_PATH];
                snprintf(new_dir, MAX_PATH, "%s/%s", state.current_dir, e->name);

                if (is_readable_dir(new_dir)) {
                    strncpy(state.current_dir, new_dir, MAX_PATH - 1);
                    state.current_dir[MAX_PATH - 1] = '\0';
                    list_directory(state.current_dir, &state);
                    write_ui_file(package_dir, &state, "", "");
                }
            } else {
                if (strcmp(state.mode, "LOAD") == 0) {
                    char result[MAX_PATH];
                    snprintf(result, MAX_PATH, "%s/%s", state.current_dir, e->name);
                    write_ui_file(package_dir, &state, result, "LOAD");
                    fe_write_result_file(g_fe_result_file, result);
                    return 0;
                } else if (strcmp(state.mode, "SAVE") == 0) {
                    strncpy(state.pending_filename, e->name, MAX_NAME - 1);
                    state.pending_filename[MAX_NAME - 1] = '\0';
                    write_ui_file(package_dir, &state, "", "");
                }
            }
        } else if (strcmp(cmd, "VIEWMODE") == 0) {
            state.grid_view = !state.grid_view;
            write_ui_file(package_dir, &state, "", "");
        } else if (strcmp(cmd, "BACK") == 0) {
            /* REAL, NEW 2026-09-15 - same get_parent_dir() call the old
             * synthetic ".. Back" list entry used to trigger through
             * ENTRY:0, now its own direct toolbar action. */
            char new_dir[MAX_PATH];
            get_parent_dir(state.current_dir, new_dir);
            if (is_readable_dir(new_dir)) {
                strncpy(state.current_dir, new_dir, MAX_PATH - 1);
                state.current_dir[MAX_PATH - 1] = '\0';
                list_directory(state.current_dir, &state);
                write_ui_file(package_dir, &state, "", "");
            }
        } else if (strncmp(cmd, "CRUMB:", 6) == 0) {
            /* REAL, NEW 2026-09-15 - jump straight to a real ancestor
             * path a breadcrumb button carries (build_crumbs()'s own
             * real per-segment path, not re-derived by walking "..").  */
            int idx = atoi(cmd + 6);
            if (idx >= 0 && idx < state.n_crumbs && is_readable_dir(state.crumbs[idx].path)) {
                strncpy(state.current_dir, state.crumbs[idx].path, MAX_PATH - 1);
                state.current_dir[MAX_PATH - 1] = '\0';
                list_directory(state.current_dir, &state);
                write_ui_file(package_dir, &state, "", "");
            }
        } else if (strncmp(cmd, "SAVEAS:", 7) == 0) {
            const char *name = cmd + 7;
            if (name[0] != '\0') {
                char result[MAX_PATH];
                snprintf(result, MAX_PATH, "%s/%s", state.current_dir, name);
                write_ui_file(package_dir, &state, result, "SAVE");
                fe_write_result_file(g_fe_result_file, result);
                return 0;
            }
        } else if (strcmp(cmd, "CANCEL") == 0) {
            write_ui_file(package_dir, &state, "", "CANCEL");
            fe_write_result_file(g_fe_result_file, "");
            return 0;
        } else if (!strncmp(cmd, "CTX_", 4)) {
            const char *verb = cmd + 4;
            int idx = fe_ctx_entry_idx(package_dir);
            char src[MAX_PATH]; src[0] = 0;
            if (idx >= 0 && idx < state.count)
                snprintf(src, sizeof(src), "%s/%s", state.current_dir, state.entries[idx].name);
            if (!strcmp(verb, "CUT") || !strcmp(verb, "COPY")) {
                if (src[0]) fe_clip_write(package_dir, !strcmp(verb, "CUT") ? "cut" : "copy", src);
            } else if (!strcmp(verb, "PASTE")) {
                char mode[16], clip[MAX_PATH];
                if (fe_clip_read(package_dir, mode, sizeof(mode), clip, sizeof(clip))) {
                    const char *base = strrchr(clip, '/');
                    base = base ? base + 1 : clip;
                    char dst[MAX_PATH];
                    snprintf(dst, sizeof(dst), "%s/%s", state.current_dir, base);
                    if (!strcmp(mode, "cut")) {
                        if (rename(clip, dst) == 0) fe_clip_write(package_dir, "copy", dst);
                    } else {
                        char sh[MAX_PATH * 2 + 32];
                        snprintf(sh, sizeof(sh), "cp -a '%s' '%s'", clip, dst);
                        (void)system(sh);
                    }
                    list_directory(state.current_dir, &state);
                    write_ui_file(package_dir, &state, "", "");
                }
            } else if (!strcmp(verb, "DELETE") && src[0]) {
                struct stat st;
                if (stat(src, &st) == 0) {
                    if (S_ISDIR(st.st_mode)) (void)rmdir(src); /* empty dirs only */
                    else (void)unlink(src);
                }
                list_directory(state.current_dir, &state);
                write_ui_file(package_dir, &state, "", "");
            } else if (!strcmp(verb, "PLACE") && src[0]) {
                char pp[MAX_PATH];
                snprintf(pp, sizeof(pp), "%s/fe_place_armed.txt", package_dir);
                FILE *pf = fopen(pp, "w");
                if (pf) { fprintf(pf, "path=%s\n", src); fclose(pf); }
                fe_clip_write(package_dir, "place", src);
                {
                    char sh[MAX_PATH * 4];
                    snprintf(sh, sizeof(sh),
                             "sh '%s/ops/fe_place_on_desk.sh' '%s' '%s' '%s'",
                             package_dir, house_root, package_dir, src);
                    (void)system(sh);
                }
                list_directory(state.current_dir, &state);
                write_ui_file(package_dir, &state, "", "");
            }
        }
    }

    return 0;
}
