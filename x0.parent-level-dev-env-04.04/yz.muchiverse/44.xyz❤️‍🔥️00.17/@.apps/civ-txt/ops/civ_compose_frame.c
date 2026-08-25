/* civ_compose_frame - renders whichever civ-txt screen is current into
 * pieces/apps/player_app/view.txt (the file chtpm's own ${game_map}
 * placeholder substitutes verbatim). Modeled directly on
 * @.apps/my-chara-txt's own ops/mychara_compose_frame.c: writes ONLY
 * view.txt, never pieces/display/current_frame.txt directly (ONE
 * WRITER RULE - that's chtpm_parser_pal.c's own exclusive job), then
 * bumps pieces/display/frame_changed.txt.
 *
 * P1 scope: new_game (setup) + main (turn counter) screens only.
 *
 * Self-contained, no shared headers.
 * Usage: civ_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            val = atoi(line + key_len + 1);
        }
    }
    fclose(f);
    return val;
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

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "new_game");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
    FILE *f = fopen(layout_path, "r");
    if (!f) return;
    char line1[MAX_LINE];
    if (fgets(line1, sizeof(line1), f)) {
        line1[strcspn(line1, "\r\n")] = '\0';
        const char *slash = strrchr(line1, '/');
        const char *base = slash ? slash + 1 : line1;
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", base);
        char *dot = strstr(tmp, ".chtpm");
        if (dot) *dot = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
#pragma GCC diagnostic pop
    }
    fclose(f);
}

static FILE *g_view_out = NULL;
static void border(void) {
    if (g_view_out) { fputc('+', g_view_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out); fputc('+', g_view_out); fputc('\n', g_view_out); }
}
static void line(const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    if (g_view_out) {
        fprintf(g_view_out, "|%.*s", len, content);
        for (int i = len; i < BOX_W; i++) fputc(' ', g_view_out);
        fputc('|', g_view_out);
        fputc('\n', g_view_out);
    }
}
static void blank(void) { line(""); }

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

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/civ-txt/pieces/civ_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "  C I V - T X T   [%s]", active_piece);
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "new_game") == 0) {
        char victory[32] = "", map_scale[32] = "", combat[32] = "";
        read_kv_str(config_path, "victory_condition", victory, sizeof(victory));
        read_kv_str(config_path, "map_scale", map_scale, sizeof(map_scale));
        read_kv_str(config_path, "combat_resolution", combat, sizeof(combat));
        if (!victory[0]) snprintf(victory, sizeof(victory), "(not set)");
        if (!map_scale[0]) snprintf(map_scale, sizeof(map_scale), "(not set)");
        if (!combat[0]) snprintf(combat, sizeof(combat), "(not set)");

        snprintf(rowbuf, sizeof(rowbuf), "  Victory condition: %s", victory);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Map scale: %s", map_scale);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Combat resolution: %s", combat);
        line(rowbuf);
        blank();
        line("Pick each option below, then Confirm & Start.");

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/civ-txt/pieces/new_game/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | new_game\n\n");
            fprintf(pdl_out, "METHOD       | Victory: Conquest                   | SET_VICTORY:conquest\n");
            fprintf(pdl_out, "METHOD       | Victory: Score + Turn Limit         | SET_VICTORY:score_turnlimit\n");
            fprintf(pdl_out, "METHOD       | Victory: Tech + Score               | SET_VICTORY:tech_score\n");
            fprintf(pdl_out, "METHOD       | Map: Small (2 civs)                 | SET_MAP_SIZE:small\n");
            fprintf(pdl_out, "METHOD       | Map: Medium (3-4 civs)              | SET_MAP_SIZE:medium\n");
            fprintf(pdl_out, "METHOD       | Combat: Abstract (army-strength)    | SET_COMBAT:abstract\n");
            fprintf(pdl_out, "METHOD       | Combat: Per-unit (grid skirmish)    | SET_COMBAT:per_unit\n");
            fprintf(pdl_out, "METHOD       | Confirm & Start                     | CONFIRM_START\n");
            fclose(pdl_out);
        }
    } else if (strcmp(active_piece, "main") == 0) {
        int turn = read_kv_int(config_path, "turn", 1);
        int treasury = read_kv_int(config_path, "treasury", 50);
        int city_count = read_kv_int(config_path, "city_count", 1);
        char victory[32] = "";
        read_kv_str(config_path, "victory_condition", victory, sizeof(victory));

        snprintf(rowbuf, sizeof(rowbuf), "  Turn: %d", turn);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Treasury: %d", treasury);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Cities: %d", city_count);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Victory condition: %s", victory);
        line(rowbuf);

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/civ-txt/pieces/main/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | main\n\n");
            fprintf(pdl_out, "METHOD       | End Turn                            | END_TURN\n");
            /* Board widget trigger - a real numbered METHOD row, NOT a
             * <button href>. Per direct instruction and
             * @.apps/BOARD_WIDGET_ARCHITECTURE.md §3/§7: this must
             * spawn the SEPARATE &.widgits/board-viewer/ widget program
             * as its own detached process with its own GL window,
             * civ-txt's own CHTPM screen/session never changes. The
             * spawn logic itself lives in civ_menu_input.c's
             * OPEN_BOARD_WIDGET handler (see that file) - this line
             * only makes the button exist. */
            fprintf(pdl_out, "METHOD       | View Board (opens separate GL window) | OPEN_BOARD_WIDGET\n");
            fclose(pdl_out);
        }
    }
    /* The old "map" screen branch (a same-session CHTPM screen showing
     * a placeholder grid, reached via <button href>) was REMOVED here -
     * that was an architectural mistake, corrected per direct
     * instruction. See @.apps/BOARD_WIDGET_ARCHITECTURE.md §0/§7 for
     * the full writeup of why. The placeholder-grid rendering logic
     * that used to live in this branch was not discarded - it's the
     * starting content for &.widgits/board-viewer/'s own compose op
     * (P4 of &.widgits/BOARD_WIDGET_PROGRESS.txt), just relocated to
     * the correct, separate widget project instead of living here. */

    blank();
    if (last_message[0]) {
        snprintf(rowbuf, sizeof(rowbuf), "  %s", last_message);
        line(rowbuf);
    }

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);

    /* Suppress unused-function warning for write_kv (kept for parity
     * with mychara_compose_frame.c's own shape; a future dynamic
     * screen here will use it, same as farm.chtpm's own regen path). */
    (void)write_kv;
    return 0;
}
