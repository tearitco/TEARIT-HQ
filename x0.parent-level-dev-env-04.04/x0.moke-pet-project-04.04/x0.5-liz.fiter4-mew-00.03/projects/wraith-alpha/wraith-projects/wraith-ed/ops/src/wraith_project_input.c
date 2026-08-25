#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024
#define MAP_ROWS 10
#define MAP_COLS 20

typedef struct {
    int is_map_control;
    char game_id[64];
    char current_map[64];
    int cursor_x;
    int cursor_y;
    int cursor_z;
    char active_glyph;
    char active_tile[32];
    char mode[64];
    char last_action[160];
} EditorState;

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    snprintf(out, out_sz, "%s/%s", a, b);
}

/* 2026-07-11: reads CHROME_CONTENT_START's live value, published by
 * wraith-alpha_manager.c (see that file's publish_chrome_reserved_nav_count(),
 * added same day) to pieces/display/chrome_reserved_nav_count.txt, instead
 * of hardcoding this project's own guess about where its nav range may
 * safely start. This ops binary is fork+exec'd by
 * wraith-alpha_manager.c's run_active_project_input_op() WITHOUT a chdir(),
 * so it inherits the manager's own cwd -- the relative path below is
 * correct without combining it with `root` (which is this PROJECT's own
 * dir, not the repo root). Falls back to the literal this file always used
 * before if the manager hasn't published yet. */
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

static void set_defaults(EditorState *st) {
    memset(st, 0, sizeof(*st));
    st->is_map_control = 0;
    snprintf(st->game_id, sizeof(st->game_id), "test-game-01");
    snprintf(st->current_map, sizeof(st->current_map), "map_0001_z0");
    st->cursor_x = 4;
    st->cursor_y = 3;
    st->cursor_z = 0;
    st->active_glyph = 'T';
    snprintf(st->active_tile, sizeof(st->active_tile), "tree");
    snprintf(st->mode, sizeof(st->mode), "editor_nav");
    snprintf(st->last_action, sizeof(st->last_action), "Initialized");
}

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void copy_value(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_sz, "%.*s", (int)dst_sz - 1, src);
}

static void load_state(const char *root, EditorState *st) {
    char path[MAX_PATH_LEN];
    char line[256];
    FILE *f;
    set_defaults(st);
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "is_map_control=", 15) == 0) st->is_map_control = atoi(line + 15);
        else if (strncmp(line, "game_id=", 8) == 0) copy_value(st->game_id, sizeof(st->game_id), line + 8);
        else if (strncmp(line, "current_map=", 12) == 0) copy_value(st->current_map, sizeof(st->current_map), line + 12);
        else if (strncmp(line, "cursor_x=", 9) == 0) st->cursor_x = atoi(line + 9);
        else if (strncmp(line, "cursor_y=", 9) == 0) st->cursor_y = atoi(line + 9);
        else if (strncmp(line, "cursor_z=", 9) == 0) st->cursor_z = atoi(line + 9);
        else if (strncmp(line, "active_glyph=", 13) == 0 && line[13]) st->active_glyph = line[13];
        else if (strncmp(line, "active_tile=", 12) == 0) copy_value(st->active_tile, sizeof(st->active_tile), line + 12);
        else if (strncmp(line, "mode=", 5) == 0) copy_value(st->mode, sizeof(st->mode), line + 5);
        else if (strncmp(line, "last_action=", 12) == 0) copy_value(st->last_action, sizeof(st->last_action), line + 12);
    }
    fclose(f);
}

static void save_state(const char *root, const EditorState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/wraith-ed\n");
    fprintf(f, "is_map_control=%d\n", st->is_map_control);
    fprintf(f, "game_id=%s\n", st->game_id);
    fprintf(f, "current_map=%s\n", st->current_map);
    fprintf(f, "cursor_x=%d\n", st->cursor_x);
    fprintf(f, "cursor_y=%d\n", st->cursor_y);
    fprintf(f, "cursor_z=%d\n", st->cursor_z);
    fprintf(f, "active_glyph=%c\n", st->active_glyph);
    fprintf(f, "active_tile=%s\n", st->active_tile);
    fprintf(f, "mode=%s\n", st->mode);
    fprintf(f, "last_action=%s\n", st->last_action);
    fclose(f);
}

static void map_path(char *out, size_t out_sz, const char *root, const EditorState *st) {
    snprintf(out, out_sz, "%s/games/%s/maps/%s.txt", root, st->game_id, st->current_map);
}

static int load_map(const char *root, const EditorState *st, char map[MAP_ROWS][MAP_COLS + 1]) {
    char path[MAX_PATH_LEN];
    char line[256];
    FILE *f;
    int r = 0;
    int c;
    map_path(path, sizeof(path), root, st);
    f = fopen(path, "r");
    if (!f) return 0;
    while (r < MAP_ROWS && fgets(line, sizeof(line), f)) {
        trim_newline(line);
        for (c = 0; c < MAP_COLS; c++) {
            map[r][c] = line[c] ? line[c] : '.';
        }
        map[r][MAP_COLS] = '\0';
        r++;
    }
    fclose(f);
    while (r < MAP_ROWS) {
        for (c = 0; c < MAP_COLS; c++) map[r][c] = (r == 0 || r == MAP_ROWS - 1 || c == 0 || c == MAP_COLS - 1) ? '#' : '.';
        map[r][MAP_COLS] = '\0';
        r++;
    }
    return 1;
}

static void save_map(const char *root, const EditorState *st, char map[MAP_ROWS][MAP_COLS + 1]) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int r;
    map_path(path, sizeof(path), root, st);
    f = fopen(path, "w");
    if (!f) return;
    for (r = 0; r < MAP_ROWS; r++) {
        fprintf(f, "%s\n", map[r]);
    }
    fclose(f);
}

static void active_tile_from_glyph(EditorState *st) {
    switch (st->active_glyph) {
        case '.': snprintf(st->active_tile, sizeof(st->active_tile), "grass"); break;
        case 'T': snprintf(st->active_tile, sizeof(st->active_tile), "tree"); break;
        case 'R': snprintf(st->active_tile, sizeof(st->active_tile), "rock"); break;
        case '#': snprintf(st->active_tile, sizeof(st->active_tile), "wall"); break;
        case '@': snprintf(st->active_tile, sizeof(st->active_tile), "actor"); break;
        case '!': snprintf(st->active_tile, sizeof(st->active_tile), "event"); break;
        default: snprintf(st->active_tile, sizeof(st->active_tile), "custom"); break;
    }
}

static void cycle_glyph(EditorState *st) {
    const char glyphs[] = ".TR#@!";
    const char *p = strchr(glyphs, st->active_glyph);
    if (!p || !p[1]) st->active_glyph = glyphs[0];
    else st->active_glyph = p[1];
    active_tile_from_glyph(st);
}

static void create_save_snapshot(const char *root, const EditorState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    snprintf(path, sizeof(path), "%s/games/%s/saves/save_latest.txt", root, st->game_id);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "game_id=%s\n", st->game_id);
    fprintf(f, "current_map=%s\n", st->current_map);
    fprintf(f, "cursor=%d,%d,%d\n", st->cursor_x, st->cursor_y, st->cursor_z);
    fprintf(f, "active_glyph=%c\n", st->active_glyph);
    fprintf(f, "saved_by=wraith-ed\n");
    fclose(f);
}

static void create_pal_event(const char *root, const EditorState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    snprintf(path, sizeof(path), "%s/pal/events/event_cursor_%02d_%02d.pal", root, st->cursor_x, st->cursor_y);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "; Wraith Ed generated PAL event draft.\n");
    fprintf(f, "TARGET_MAP \"%s\"\n", st->current_map);
    fprintf(f, "TARGET_CELL %d %d %d\n", st->cursor_x, st->cursor_y, st->cursor_z);
    fprintf(f, "SHOW_TEXT \"Event at %d,%d\"\n", st->cursor_x, st->cursor_y);
    fprintf(f, "END\n");
    fclose(f);
}

static void create_export_manifest(const char *root, const EditorState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    snprintf(path, sizeof(path), "%s/exports/%s.export.pdl", root, st->game_id);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "META | exporter | wraith-ed\n");
    fprintf(f, "META | game_id | %s\n", st->game_id);
    fprintf(f, "FILE | map | games/%s/maps/%s.txt\n", st->game_id, st->current_map);
    fprintf(f, "FILE | project | games/%s/project.pdl\n", st->game_id);
    fprintf(f, "FILE | script | games/%s/scripts/talk.pal\n", st->game_id);
    fprintf(f, "DIR | pal_events | pal/events\n");
    fclose(f);
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

static int process_key(const char *root, EditorState *st, int key) {
    char map[MAP_ROWS][MAP_COLS + 1];
    int changed = 0;
    if (!st->is_map_control || key <= 0) {
        return 0;
    }
    if (!load_map(root, st, map)) {
        snprintf(st->last_action, sizeof(st->last_action), "Map load failed");
        return 1;
    }
    switch (key) {
        case 'w': if (st->cursor_y > 1) st->cursor_y--; snprintf(st->last_action, sizeof(st->last_action), "Move cursor north"); changed = 1; break;
        case 's': if (st->cursor_y < MAP_ROWS - 2) st->cursor_y++; snprintf(st->last_action, sizeof(st->last_action), "Move cursor south"); changed = 1; break;
        case 'a': if (st->cursor_x > 1) st->cursor_x--; snprintf(st->last_action, sizeof(st->last_action), "Move cursor west"); changed = 1; break;
        case 'd': if (st->cursor_x < MAP_COLS - 2) st->cursor_x++; snprintf(st->last_action, sizeof(st->last_action), "Move cursor east"); changed = 1; break;
        case 'S': create_save_snapshot(root, st); snprintf(st->last_action, sizeof(st->last_action), "Saved game snapshot"); changed = 1; break;
        case 'x': case 'X': create_export_manifest(root, st); snprintf(st->last_action, sizeof(st->last_action), "Export manifest written"); changed = 1; break;
        case 'g': case 'G': cycle_glyph(st); snprintf(st->last_action, sizeof(st->last_action), "Cycled glyph to %c", st->active_glyph); changed = 1; break;
        case 'p': case 'P': case ' ': map[st->cursor_y][st->cursor_x] = st->active_glyph; save_map(root, st, map); snprintf(st->last_action, sizeof(st->last_action), "Placed glyph %c at %d,%d", st->active_glyph, st->cursor_x, st->cursor_y); changed = 1; break;
        case 'l': case 'L': snprintf(st->last_action, sizeof(st->last_action), "Loaded map %s", st->current_map); changed = 1; break;
        case 'e': case 'E': create_pal_event(root, st); snprintf(st->last_action, sizeof(st->last_action), "PAL event draft written"); changed = 1; break;
        default: snprintf(st->last_action, sizeof(st->last_action), "Unhandled key %d", key); changed = 1; break;
    }
    if (key == 'w' || key == 's' || key == 'a' || key == 'd') {
        save_map(root, st, map);
    }
    snprintf(st->mode, sizeof(st->mode), "map_edit");
    return changed;
}

static void write_body(const char *root, const EditorState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "WRAITH ED | ${game_map} | game=%s map=%s\n", st->game_id, st->current_map);
    fprintf(f, "Mode: %s | Cursor: (%d,%d,%d) | Active glyph: %c %s\n", st->mode, st->cursor_x, st->cursor_y, st->cursor_z, st->active_glyph, st->active_tile);
    fprintf(f, "Controls: Enter Control_Editor_Map, WASD move, G cycle glyph, P/Space place\n");
    fprintf(f, "Ops: L load map | S save game | E create PAL event | X export manifest\n");
    fprintf(f, "Last: %s\n", st->last_action);
    fprintf(f, "Reference: op-ed sovereign context + piececraft tile z-map + chtmgl panels\n");
    fclose(f);
}

static void write_scene(const char *root, const EditorState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int nav_base = read_chrome_content_start(6);
    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Project-owned Wraith scene records.\n");
    fprintf(f, "# Wraith Ed merges op-ed editor controls with piececraft-style game_map editing.\n");
    fprintf(f, "OBJECT tag=panel id=editor_panel role=chtmgl_panel x=3 y=5 w=28 h=12 z=24 nav=0 source=reference:op-ed fg=#E8F1F2 bg=#17293A border=#7EDFF2 action=- label=Editor_Tools\n");
    fprintf(f, "OBJECT tag=text id=editor_header role=chtmgl_header x=4 y=5 w=24 h=1 z=28 nav=0 source=semantic:wraith_ed fg=#E8F1F2 bg=#17293A border=#7EDFF2 action=- label=WRAITH_ED\n");
    fprintf(f, "OBJECT tag=control id=control_editor_map role=window_toolbar_item x=4 y=7 w=24 h=1 z=30 nav=%d source=semantic:project_control fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=INTERACT target_surface=game_map label=Control_Editor_Map\n", nav_base + 0);
    fprintf(f, "OBJECT tag=control id=load_map role=chtmgl_button x=4 y=9 w=13 h=1 z=30 nav=%d source=reference:op-ed fg=#E8F1F2 bg=#22384A border=#FFD166 action=EDITOR:load_map label=Load_Map\n", nav_base + 1);
    fprintf(f, "OBJECT tag=control id=place_glyph role=chtmgl_button x=18 y=9 w=12 h=1 z=30 nav=%d source=reference:op-ed fg=#E8F1F2 bg=#22384A border=#FFD166 action=EDITOR:place_glyph label=Place_%c\n", nav_base + 2, st->active_glyph);
    fprintf(f, "OBJECT tag=control id=save_game role=chtmgl_button x=4 y=11 w=13 h=1 z=30 nav=%d source=reference:op-ed fg=#E8F1F2 bg=#22384A border=#FFD166 action=EDITOR:save_game label=Save_Game\n", nav_base + 3);
    fprintf(f, "OBJECT tag=control id=pal_event role=chtmgl_button x=18 y=11 w=12 h=1 z=30 nav=%d source=reference:op-ed fg=#E8F1F2 bg=#22384A border=#FFD166 action=EDITOR:pal_event label=PAL_Event\n", nav_base + 4);
    fprintf(f, "OBJECT tag=control id=export_game role=chtmgl_button x=4 y=13 w=13 h=1 z=30 nav=%d source=reference:op-ed fg=#E8F1F2 bg=#22384A border=#FFD166 action=EDITOR:export label=Export\n", nav_base + 5);
    fprintf(f, "OBJECT tag=panel id=game_panel role=chtmgl_panel x=34 y=5 w=52 h=18 z=24 nav=0 source=reference:piececraft-wraith fg=#E8F1F2 bg=#142638 border=#7EDFF2 action=- label=Game_Map\n");
    fprintf(f, "OBJECT tag=surface id=game_map role=game_map x=36 y=7 w=48 h=14 z=26 nav=0 source=semantic:game_map_surface fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 action=- label=\n");
    fprintf(f, "OBJECT tag=model id=wraith_ed_map role=tile_zmap x=36 y=7 w=48 h=14 z=32 source=projects/wraith-alpha/wraith-projects/wraith-ed/games/%s/maps/%s.txt fg=#24C94A bg=#0B1118 border=#FFD166 action=- label=MAP_SOURCE:source=games/%s/maps/%s.txt;registry=assets/tiles/registry.txt;z=%d;selected=%d,%d,%d;glyph=%c;editor=wraith-ed\n",
        st->game_id, st->current_map, st->game_id, st->current_map, st->cursor_z, st->cursor_x, st->cursor_y, st->cursor_z, st->active_glyph);
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root;
    char history_path[MAX_PATH_LEN];
    FILE *history;
    long cursor;
    long end_pos;
    char line[512];
    EditorState st;
    int changed = 0;

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
            if (key > 0) {
                changed |= process_key(root, &st, key);
            }
        }
        cursor = ftell(history);
        fclose(history);
        write_cursor(root, cursor);
    }

    if (changed) {
        save_state(root, &st);
    }
    write_body(root, &st);
    write_scene(root, &st);
    return 0;
}
