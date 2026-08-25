#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 1024
#define MAX_ENTRIES 512
#define VISIBLE_ENTRIES 6

typedef struct {
    int is_map_control;
    char project_id[128];
    char title[64];
    char mode[64];
    char status[128];
    char current_dir[PATH_MAX];
    char active_path[PATH_MAX];
    int selected_index;
    int scroll_offset;
    char last_action[256];
} FsState;

typedef struct {
    char name[PATH_MAX];
    char rel_path[PATH_MAX];
    int is_dir;
    int is_wraith_project;
    char launch_command[256];
    off_t size;
} FsEntry;

static int detect_wraith_project(const char *repo_root, FsEntry *entry);

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void copy_value(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_sz, "%.*s", (int)dst_sz - 1, src);
}

static void sanitize_token(const char *src, char *dst, size_t dst_sz) {
    size_t di = 0;
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    while (*src && di + 1 < dst_sz) {
        unsigned char ch = (unsigned char)*src++;
        if (isalnum(ch)) dst[di++] = (char)tolower(ch);
        else if (ch == '-' || ch == '_' || ch == ' ' || ch == '/') dst[di++] = '_';
    }
    if (di == 0 && dst_sz > 1) dst[di++] = 'x';
    dst[di] = '\0';
}

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    if (!a || !a[0]) {
        snprintf(out, out_sz, "%s", b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        snprintf(out, out_sz, "%s", a);
        return;
    }
    if (a[strlen(a) - 1] == '/') snprintf(out, out_sz, "%s%s", a, b);
    else snprintf(out, out_sz, "%s/%s", a, b);
}

static void parent_dir(char *path) {
    char *slash;
    if (!path || !path[0]) return;
    slash = strrchr(path, '/');
    if (!slash) {
        path[0] = '\0';
        return;
    }
    if (slash == path) {
        slash[1] = '\0';
        return;
    }
    *slash = '\0';
}

static void derive_repo_root(const char *project_root, char *repo_root, size_t repo_root_sz) {
    const char *needle = "/projects/wraith-alpha/wraith-projects/";
    const char *p = strstr(project_root, needle);
    if (p) {
        size_t len = (size_t)(p - project_root);
        if (len >= repo_root_sz) len = repo_root_sz - 1;
        memcpy(repo_root, project_root, len);
        repo_root[len] = '\0';
        return;
    }
    copy_value(repo_root, repo_root_sz, project_root);
}

static int rel_dir_to_abs(const char *repo_root, const char *rel, char *abs_out, size_t abs_out_sz) {
    struct stat st;
    if (!rel || !rel[0]) snprintf(abs_out, abs_out_sz, "%s", repo_root);
    else path_join(abs_out, abs_out_sz, repo_root, rel);
    if (stat(abs_out, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    return 1;
}

static void abs_to_rel(const char *repo_root, const char *abs_path, char *rel_out, size_t rel_out_sz) {
    size_t root_len = strlen(repo_root);
    if (strncmp(abs_path, repo_root, root_len) == 0) {
        const char *suffix = abs_path + root_len;
        if (*suffix == '/') suffix++;
        copy_value(rel_out, rel_out_sz, suffix);
        return;
    }
    copy_value(rel_out, rel_out_sz, abs_path);
}

static void set_defaults(FsState *st) {
    memset(st, 0, sizeof(*st));
    st->is_map_control = 0;
    snprintf(st->project_id, sizeof(st->project_id), "wraith-alpha/wraith-projects/fs");
    snprintf(st->title, sizeof(st->title), "WRAITH FS");
    snprintf(st->mode, sizeof(st->mode), "filesystem");
    snprintf(st->status, sizeof(st->status), "ready");
    snprintf(st->current_dir, sizeof(st->current_dir), "projects");
    st->selected_index = 0;
    st->scroll_offset = 0;
    snprintf(st->last_action, sizeof(st->last_action), "Initialized");
}

static void normalize_legacy_state(FsState *st) {
    if (!st) return;
    if (strncmp(st->last_action, "Unhandled command FS_SCROLL_", 28) == 0) {
        snprintf(st->status, sizeof(st->status), "ready");
        snprintf(st->last_action, sizeof(st->last_action), "Ready at current directory");
    }
}

static void load_state(const char *root, FsState *st) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;

    set_defaults(st);
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "project_id=", 11) == 0) copy_value(st->project_id, sizeof(st->project_id), line + 11);
        else if (strncmp(line, "title=", 6) == 0) copy_value(st->title, sizeof(st->title), line + 6);
        else if (strncmp(line, "mode=", 5) == 0) copy_value(st->mode, sizeof(st->mode), line + 5);
        else if (strncmp(line, "status=", 7) == 0) copy_value(st->status, sizeof(st->status), line + 7);
        else if (strncmp(line, "is_map_control=", 15) == 0) st->is_map_control = atoi(line + 15);
        else if (strncmp(line, "current_dir=", 12) == 0) copy_value(st->current_dir, sizeof(st->current_dir), line + 12);
        else if (strncmp(line, "active_path=", 12) == 0) copy_value(st->active_path, sizeof(st->active_path), line + 12);
        else if (strncmp(line, "selected_index=", 15) == 0) st->selected_index = atoi(line + 15);
        else if (strncmp(line, "scroll_offset=", 14) == 0) st->scroll_offset = atoi(line + 14);
        else if (strncmp(line, "last_action=", 12) == 0) copy_value(st->last_action, sizeof(st->last_action), line + 12);
    }
    fclose(f);
    normalize_legacy_state(st);
}

static void save_state(const char *root, const FsState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    FsState clean;

    clean = *st;
    normalize_legacy_state(&clean);

    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=%s\n", clean.project_id);
    fprintf(f, "title=%s\n", clean.title);
    fprintf(f, "mode=%s\n", clean.mode);
    fprintf(f, "status=%s\n", clean.status);
    fprintf(f, "is_map_control=%d\n", clean.is_map_control);
    fprintf(f, "current_dir=%s\n", clean.current_dir);
    fprintf(f, "active_path=%s\n", clean.active_path);
    fprintf(f, "selected_index=%d\n", clean.selected_index);
    fprintf(f, "scroll_offset=%d\n", clean.scroll_offset);
    fprintf(f, "last_action=%s\n", clean.last_action);
    fclose(f);
}

static long read_cursor(const char *root) {
    char path[MAX_PATH_LEN];
    FILE *f;
    long cursor = 0;

    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "r");
    if (!f) return 0;
    if (fscanf(f, "%ld", &cursor) != 1) cursor = 0;
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

static const char *command_from_history_line(const char *line) {
    const char *p = strstr(line, "COMMAND:");
    if (!p) return NULL;
    p += strlen("COMMAND:");
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int entry_cmp(const void *a, const void *b) {
    const FsEntry *ea = (const FsEntry *)a;
    const FsEntry *eb = (const FsEntry *)b;
    if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    return strcasecmp(ea->name, eb->name);
}

static int scan_entries(const char *repo_root, const FsState *st, FsEntry *entries, int max_entries) {
    DIR *dir;
    struct dirent *de;
    char abs_dir[PATH_MAX];
    int count = 0;

    if (!rel_dir_to_abs(repo_root, st->current_dir, abs_dir, sizeof(abs_dir))) return 0;
    dir = opendir(abs_dir);
    if (!dir) return 0;

    while ((de = readdir(dir)) && count < max_entries) {
        struct stat entry_st;
        char abs_entry[PATH_MAX];

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        path_join(abs_entry, sizeof(abs_entry), abs_dir, de->d_name);
        if (lstat(abs_entry, &entry_st) != 0) continue;

        memset(&entries[count], 0, sizeof(entries[count]));
        copy_value(entries[count].name, sizeof(entries[count].name), de->d_name);
        if (st->current_dir[0]) path_join(entries[count].rel_path, sizeof(entries[count].rel_path), st->current_dir, de->d_name);
        else copy_value(entries[count].rel_path, sizeof(entries[count].rel_path), de->d_name);
        entries[count].is_dir = S_ISDIR(entry_st.st_mode) ? 1 : 0;
        entries[count].size = entry_st.st_size;
        detect_wraith_project(repo_root, &entries[count]);
        count++;
    }

    closedir(dir);
    qsort(entries, count, sizeof(FsEntry), entry_cmp);
    return count;
}

static void ensure_selection(FsState *st, int entry_count) {
    if (entry_count < 0) entry_count = 0;
    if (st->selected_index < 0) st->selected_index = 0;
    if (entry_count == 0) st->selected_index = 0;
    else if (st->selected_index > entry_count) st->selected_index = entry_count;
    if (st->selected_index < st->scroll_offset) st->scroll_offset = st->selected_index;
    if (st->selected_index >= st->scroll_offset + VISIBLE_ENTRIES) {
        st->scroll_offset = st->selected_index - VISIBLE_ENTRIES + 1;
    }
    if (st->scroll_offset < 0) st->scroll_offset = 0;
}

static void format_size(off_t size, char *out, size_t out_sz) {
    if (size < 1024) snprintf(out, out_sz, "%lldB", (long long)size);
    else if (size < 1024 * 1024) snprintf(out, out_sz, "%lldKB", (long long)((size + 1023) / 1024));
    else snprintf(out, out_sz, "%lldMB", (long long)((size + (1024 * 1024 - 1)) / (1024 * 1024)));
}

static int max_scroll_offset(int entry_count) {
    int max_offset = entry_count - VISIBLE_ENTRIES;
    return max_offset > 0 ? max_offset : 0;
}

static void scroll_up(FsState *st) {
    if (st->scroll_offset > 0) {
        st->scroll_offset--;
        snprintf(st->status, sizeof(st->status), "scrolled");
        snprintf(st->last_action, sizeof(st->last_action), "Scrolled up");
    } else {
        snprintf(st->last_action, sizeof(st->last_action), "Already at top");
    }
}

static void scroll_down(FsState *st, int entry_count) {
    int max_offset = max_scroll_offset(entry_count);
    if (st->scroll_offset < max_offset) {
        st->scroll_offset++;
        snprintf(st->status, sizeof(st->status), "scrolled");
        snprintf(st->last_action, sizeof(st->last_action), "Scrolled down");
    } else {
        snprintf(st->last_action, sizeof(st->last_action), "Already at bottom");
    }
}

static void build_thumb_label(char *out, size_t out_sz, int scroll_offset, int entry_count) {
    char track[9] = "--------";
    int visible_total = entry_count + 1;
    int max_offset = max_scroll_offset(entry_count);
    int thumb_pos = 0;
    int window_start = scroll_offset + 1;
    int window_end = window_start + VISIBLE_ENTRIES - 1;

    if (window_end > visible_total) window_end = visible_total;
    if (max_offset > 0) {
        thumb_pos = (scroll_offset * 7) / max_offset;
    }
    if (thumb_pos < 0) thumb_pos = 0;
    if (thumb_pos > 7) thumb_pos = 7;
    track[thumb_pos] = '#';
    snprintf(out, out_sz, "Thumb:[%s]%d-%d/%d", track, window_start, window_end, visible_total);
}

static void ensure_watch_process(const char *root) {
    char pid_path[MAX_PATH_LEN];
    char op_path[MAX_PATH_LEN];
    FILE *f;
    long pid = 0;

    path_join(pid_path, sizeof(pid_path), root, "session/fs_watch.pid");
    f = fopen(pid_path, "r");
    if (f) {
        if (fscanf(f, "%ld", &pid) == 1 && pid > 0 && kill((pid_t)pid, 0) == 0) {
            fclose(f);
            return;
        }
        fclose(f);
    }

    path_join(op_path, sizeof(op_path), root, "ops/+x/wraith_project_watch.+x");
    if (access(op_path, X_OK) != 0) {
        return;
    }

    pid = fork();
    if (pid == 0) {
        setsid();
        execl(op_path, op_path, root, NULL);
        _exit(127);
    }
}

static int is_wraith_project_rel(const char *rel_path) {
    const char *prefix = "projects/wraith-alpha/wraith-projects/";
    return rel_path && strncmp(rel_path, prefix, strlen(prefix)) == 0;
}

static int detect_wraith_project(const char *repo_root, FsEntry *entry) {
    char project_dir[PATH_MAX];
    char project_pdl[PATH_MAX];
    char launch_id[128];
    struct stat st;

    if (!repo_root || !entry || !entry->is_dir || !is_wraith_project_rel(entry->rel_path)) {
        return 0;
    }

    path_join(project_dir, sizeof(project_dir), repo_root, entry->rel_path);
    path_join(project_pdl, sizeof(project_pdl), project_dir, "project.pdl");
    if (stat(project_pdl, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }

    sanitize_token(entry->name, launch_id, sizeof(launch_id));
    snprintf(entry->launch_command, sizeof(entry->launch_command), "DESKTOP_ACTION:launch_%s", launch_id);
    entry->is_wraith_project = 1;
    return 1;
}

static void enqueue_desktop_action(const char *repo_root, const char *action) {
    char queue_path[MAX_PATH_LEN];
    FILE *f;

    if (!repo_root || !action || !action[0]) return;
    path_join(queue_path, sizeof(queue_path), repo_root, "projects/wraith-alpha/session/desktop_actions.txt");
    f = fopen(queue_path, "a");
    if (!f) return;
    fprintf(f, "%s\n", action);
    fclose(f);
}

static void go_parent(const char *repo_root, FsState *st) {
    char current_abs[PATH_MAX];

    if (!rel_dir_to_abs(repo_root, st->current_dir, current_abs, sizeof(current_abs))) {
        snprintf(st->last_action, sizeof(st->last_action), "Cannot resolve current dir");
        return;
    }
    if (strcmp(current_abs, repo_root) == 0) {
        snprintf(st->last_action, sizeof(st->last_action), "Already at repo root");
        return;
    }
    parent_dir(current_abs);
    if (strncmp(current_abs, repo_root, strlen(repo_root)) != 0) {
        snprintf(current_abs, sizeof(current_abs), "%s", repo_root);
    }
    abs_to_rel(repo_root, current_abs, st->current_dir, sizeof(st->current_dir));
    st->selected_index = 0;
    st->scroll_offset = 0;
    st->active_path[0] = '\0';
    snprintf(st->status, sizeof(st->status), "navigated");
    snprintf(st->last_action, sizeof(st->last_action), "Moved to parent dir");
}

static void activate_slot(const char *repo_root, FsState *st, const FsEntry *entries, int entry_count, int slot) {
    const FsEntry *entry;

    if (slot < 0) slot = 0;
    if (slot > entry_count) slot = entry_count;
    st->selected_index = slot;
    ensure_selection(st, entry_count);

    if (slot == 0) {
        go_parent(repo_root, st);
        return;
    }

    entry = &entries[slot - 1];
    if (entry->is_wraith_project && entry->launch_command[0]) {
        enqueue_desktop_action(repo_root, entry->launch_command);
        copy_value(st->active_path, sizeof(st->active_path), entry->rel_path);
        snprintf(st->status, sizeof(st->status), "launched");
        snprintf(st->last_action, sizeof(st->last_action), "Launched project %.170s", entry->name);
    } else if (entry->is_dir) {
        copy_value(st->current_dir, sizeof(st->current_dir), entry->rel_path);
        st->selected_index = 0;
        st->scroll_offset = 0;
        st->active_path[0] = '\0';
        snprintf(st->status, sizeof(st->status), "navigated");
        snprintf(st->last_action, sizeof(st->last_action), "Opened dir %.180s", entry->name);
    } else {
        copy_value(st->active_path, sizeof(st->active_path), entry->rel_path);
        snprintf(st->status, sizeof(st->status), "file-selected");
        snprintf(st->last_action, sizeof(st->last_action), "Selected file %.176s", entry->name);
    }
}

static void process_project_command(const char *repo_root, FsState *st, FsEntry *entries, int entry_count, const char *command) {
    char trimmed[256];
    if (!command || !command[0]) return;
    copy_value(trimmed, sizeof(trimmed), command);
    trim_newline(trimmed);

    if (strncmp(trimmed, "FS_OPEN:", 8) == 0) {
        activate_slot(repo_root, st, entries, entry_count, atoi(trimmed + 8));
        return;
    }
    if (strcmp(trimmed, "FS_SCROLL_UP") == 0) {
        scroll_up(st);
        ensure_selection(st, entry_count);
        snprintf(st->status, sizeof(st->status), "navigated");
        snprintf(st->last_action, sizeof(st->last_action), "Scrolled up");
        return;
    }
    if (strcmp(trimmed, "FS_SCROLL_DOWN") == 0) {
        scroll_down(st, entry_count);
        ensure_selection(st, entry_count);
        snprintf(st->status, sizeof(st->status), "navigated");
        snprintf(st->last_action, sizeof(st->last_action), "Scrolled down");
        return;
    }
    if (strcmp(trimmed, "FS_PARENT") == 0) {
        go_parent(repo_root, st);
        return;
    }
    snprintf(st->last_action, sizeof(st->last_action), "Unhandled command %.180s", trimmed);
}

static void process_map_key(const char *repo_root, FsState *st, FsEntry *entries, int entry_count, int key) {
    int total_slots = entry_count + 1;

    if (!st->is_map_control || key <= 0) return;

    switch (key) {
        case 'w':
        case 'k':
        case 1002:
            if (st->selected_index > 0) st->selected_index--;
            snprintf(st->last_action, sizeof(st->last_action), "Selection moved up");
            break;
        case 's':
        case 'j':
        case 1003:
            if (st->selected_index < total_slots - 1) st->selected_index++;
            snprintf(st->last_action, sizeof(st->last_action), "Selection moved down");
            break;
        case 'a':
        case 'h':
        case 8:
        case 127:
            go_parent(repo_root, st);
            break;
        case 'd':
        case 'l':
        case 10:
        case 13:
            activate_slot(repo_root, st, entries, entry_count, st->selected_index);
            break;
        case 'u':
        case 'U':
            scroll_up(st);
            snprintf(st->status, sizeof(st->status), "navigated");
            snprintf(st->last_action, sizeof(st->last_action), "Scrolled up");
            break;
        case 'n':
        case 'N':
            scroll_down(st, entry_count);
            snprintf(st->status, sizeof(st->status), "navigated");
            snprintf(st->last_action, sizeof(st->last_action), "Scrolled down");
            break;
        default:
            snprintf(st->last_action, sizeof(st->last_action), "Unhandled key %d", key);
            break;
    }
    ensure_selection(st, entry_count);
}

static void write_body(const char *root, const char *repo_root, FsState *st, int entry_count) {
    char path[MAX_PATH_LEN];
    char dir_display[PATH_MAX];
    FILE *f;
    int window_end;

    (void)repo_root;

    normalize_legacy_state(st);
    if (st->current_dir[0]) snprintf(dir_display, sizeof(dir_display), "%.140s/", st->current_dir);
    else snprintf(dir_display, sizeof(dir_display), "repo-root/");
    window_end = ((st->scroll_offset + VISIBLE_ENTRIES) < (entry_count + 1)) ? (st->scroll_offset + VISIBLE_ENTRIES) : (entry_count + 1);
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "WRAITH FS\n");
    fprintf(f, "Dir: %s\n", dir_display);
    fprintf(f, "Entries: %d | Window: %d-%d | %s\n", entry_count + 1, st->scroll_offset + 1, window_end, st->status);
    fprintf(f, "Last: %.140s\n", st->last_action);
    fclose(f);
}

static void write_scene(const char *root, const FsState *st, FsEntry *entries, int entry_count) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int row;
    int nav;
    int scene_y;
    char thumb_label[64];

    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "# Wraith FS scene rows. Host owns numbering/selectors; labels stay raw.\n");
    fprintf(f, "OBJECT tag=text id=fs_header role=window_toolbar_item x=4 y=8 w=24 h=1 z=30 nav=0 source=semantic:fs_header fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=- label=Directory_Contents src=\n");

    nav = 5;
    scene_y = 9;
    build_thumb_label(thumb_label, sizeof(thumb_label), st->scroll_offset, entry_count);
    fprintf(f, "OBJECT tag=control id=fs_scroll_up role=window_toolbar_item x=4 y=%d w=10 h=1 z=30 nav=%d source=semantic:fs_scroll fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:FS_SCROLL_UP label=^_UP src=\n",
        scene_y, nav++);
    fprintf(f, "OBJECT tag=control id=fs_scroll_down role=window_toolbar_item x=15 y=%d w=12 h=1 z=30 nav=%d source=semantic:fs_scroll fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:FS_SCROLL_DOWN label=v_DOWN src=\n",
        scene_y, nav++);
    fprintf(f, "OBJECT tag=text id=fs_thumb role=window_toolbar_item x=29 y=%d w=30 h=1 z=30 nav=0 source=semantic:fs_thumb fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=- label=%s src=\n",
        scene_y++, thumb_label);
    fprintf(f, "OBJECT tag=control id=fs_back role=window_toolbar_item x=4 y=%d w=34 h=1 z=30 nav=%d source=semantic:fs_row fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:FS_OPEN:0 label=<-_BACK src=\n",
        scene_y++, nav++);

    for (row = st->scroll_offset; row < entry_count && row < st->scroll_offset + VISIBLE_ENTRIES; row++) {
        char label[256];
        char size_buf[32];
        char action_buf[64];
        const char *action;
        if (entries[row].is_wraith_project) {
            snprintf(label, sizeof(label), "PROJ:%.176s/", entries[row].name);
            action = entries[row].launch_command;
        } else if (entries[row].is_dir) {
            snprintf(label, sizeof(label), "DIR:%.180s/", entries[row].name);
            action = NULL;
        } else {
            format_size(entries[row].size, size_buf, sizeof(size_buf));
            snprintf(label, sizeof(label), "FIL:%.150s(%s)", entries[row].name, size_buf);
            action = NULL;
        }
        if (!action) {
            snprintf(action_buf, sizeof(action_buf), "PROJECT_ACTION:FS_OPEN:%d", row + 1);
            action = action_buf;
        }
        fprintf(f, "OBJECT tag=control id=fs_row_%02d role=window_toolbar_item x=4 y=%d w=52 h=1 z=30 nav=%d source=semantic:fs_row fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=%s label=%s src=\n",
            row + 1, scene_y++, nav++, action, label);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root;
    char repo_root[PATH_MAX];
    char history_path[MAX_PATH_LEN];
    char probe_dir[PATH_MAX];
    FILE *history;
    long cursor;
    long end_pos;
    char line[512];
    FsState st;
    FsEntry entries[MAX_ENTRIES];
    int entry_count;

    if (argc < 2) return 2;
    root = argv[1];

    load_state(root, &st);
    ensure_watch_process(root);
    derive_repo_root(root, repo_root, sizeof(repo_root));
    if (!rel_dir_to_abs(repo_root, st.current_dir, probe_dir, sizeof(probe_dir))) {
        snprintf(st.current_dir, sizeof(st.current_dir), "projects");
        snprintf(st.last_action, sizeof(st.last_action), "Reset invalid directory to projects");
    }

    entry_count = scan_entries(repo_root, &st, entries, MAX_ENTRIES);
    ensure_selection(&st, entry_count);

    cursor = read_cursor(root);
    path_join(history_path, sizeof(history_path), root, "session/history.txt");
    history = fopen(history_path, "r");
    if (history) {
        fseek(history, 0, SEEK_END);
        end_pos = ftell(history);
        if (cursor < 0 || cursor > end_pos) cursor = 0;
        fseek(history, cursor, SEEK_SET);
        while (fgets(line, sizeof(line), history)) {
            const char *command = command_from_history_line(line);
            int key;

            if (command) {
                process_project_command(repo_root, &st, entries, entry_count, command);
                entry_count = scan_entries(repo_root, &st, entries, MAX_ENTRIES);
                ensure_selection(&st, entry_count);
                continue;
            }

            key = key_from_history_line(line);
            if (key > 0) {
                process_map_key(repo_root, &st, entries, entry_count, key);
                entry_count = scan_entries(repo_root, &st, entries, MAX_ENTRIES);
                ensure_selection(&st, entry_count);
            }
        }
        cursor = ftell(history);
        fclose(history);
        write_cursor(root, cursor);
    }

    save_state(root, &st);
    write_body(root, repo_root, &st, entry_count);
    write_scene(root, &st, entries, entry_count);
    return 0;
}
