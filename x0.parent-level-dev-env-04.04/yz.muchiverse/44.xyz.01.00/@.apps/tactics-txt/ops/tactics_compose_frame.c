/* tactics_compose_frame - renders whichever tactics-txt screen is
 * current into pieces/apps/player_app/view.txt. Modeled directly on
 * @.apps/my-chara-txt's own ops/mychara_compose_frame.c: writes ONLY
 * view.txt, never current_frame.txt directly (ONE WRITER RULE), then
 * bumps pieces/display/frame_changed.txt.
 *
 * P1 scope: setup + main screens only (no board.chtpm/roster.chtpm
 * yet - those are later phases per TACTICS_TXT_DESIGN.md's own build
 * order).
 *
 * Self-contained, no shared headers.
 * Usage: tactics_compose_frame.+x (no args) */
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
    snprintf(out, out_sz, "setup");
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

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF], config_path[PATH_BUF], units_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/tactics-txt/pieces/tactics_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(units_path, sizeof(units_path), "%s/pieces/system/units.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "  T A C T I C S - T X T   [%s]", active_piece);
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "setup") == 0) {
        char mode[32] = "";
        read_kv_str(config_path, "mode", mode, sizeof(mode));
        if (!mode[0]) snprintf(mode, sizeof(mode), "(not set)");

        snprintf(rowbuf, sizeof(rowbuf), "  Mode: %s", mode);
        line(rowbuf);
        blank();
        line("Fixed 3-unit staff armies (Classic mode, P1 skeleton):");
        line("  Side 1: warrior, chef, farmer");
        line("  Side 2: warrior, clown, lawyer");

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/tactics-txt/pieces/setup/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | setup\n\n");
            fprintf(pdl_out, "METHOD       | Mode: Classic (fixed staff armies)  | SET_MODE:classic\n");
            fprintf(pdl_out, "METHOD       | Confirm & Start                     | CONFIRM_START\n");
            fclose(pdl_out);
        }
    } else if (strcmp(active_piece, "main") == 0) {
        int turn = read_kv_int(config_path, "turn", 1);
        int active_side = read_kv_int(config_path, "active_side", 1);
        int actions_remaining = read_kv_int(config_path, "actions_remaining_this_turn", 5);

        snprintf(rowbuf, sizeof(rowbuf), "  Turn: %d", turn);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Active side: %d", active_side);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Actions remaining: %d/5", actions_remaining);
        line(rowbuf);
        blank();

        for (int side = 1; side <= 2; side++) {
            snprintf(rowbuf, sizeof(rowbuf), "  Side %d roster:", side);
            line(rowbuf);
            for (int i = 0; i < 3; i++) {
                char key_prof[64], key_hp[64];
                snprintf(key_prof, sizeof(key_prof), "side_%d_unit_%d_profession", side, i);
                snprintf(key_hp, sizeof(key_hp), "side_%d_unit_%d_hp", side, i);
                char prof[32] = "";
                read_kv_str(units_path, key_prof, prof, sizeof(prof));
                int hp = read_kv_int(units_path, key_hp, 0);
                snprintf(rowbuf, sizeof(rowbuf), "    %s (hp: %d)", prof, hp);
                line(rowbuf);
            }
        }

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/tactics-txt/pieces/main/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | main\n\n");
            fprintf(pdl_out, "METHOD       | End Turn                            | END_TURN\n");
            /* Board widget trigger - same real, separate-GL-window
             * mechanism as civ-txt's own (see ops/tactics_menu_input.c's
             * OPEN_BOARD_WIDGET handler + @.apps/BOARD_WIDGET_
             * ARCHITECTURE.md §3/§7). A real numbered METHOD row, never
             * a <button href> - this project's own screen never changes
             * when the widget opens. */
            fprintf(pdl_out, "METHOD       | View Board (opens separate GL window) | OPEN_BOARD_WIDGET\n");
            fclose(pdl_out);
        }
    } else if (strcmp(active_piece, "roster") == 0) {
        /* Real per-entity selection screen - see @.apps/
         * PORTABLE_ENTITY_ARCHITECTURE.md §4 and this project's own
         * ENTITY_MOVEMENT_PROGRESS.txt "REAL ARCHITECTURAL FINDING"
         * for why this merges the selected unit's own real piece.pdl
         * methods into ROSTER's own dynamically-regenerated piece.pdl,
         * rather than a true active_target_id retarget (that would get
         * stomped by this project's own layout-coupled idle-tick
         * resync - a real, load-bearing mechanism not worth fighting).
         * This is still the real thing: each unit's OWN methods (read
         * from ITS OWN separate piece.pdl file, not hardcoded here)
         * are what actually get merged in and dispatched. */
        int active_side = read_kv_int(config_path, "active_side", 1);
        char selected_unit_id[64] = "";
        read_kv_str(state_path, "selected_unit_id", selected_unit_id, sizeof(selected_unit_id));

        snprintf(rowbuf, sizeof(rowbuf), "  Active side: %d", active_side);
        line(rowbuf);
        blank();

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/tactics-txt/pieces/roster/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");

        if (selected_unit_id[0]) {
            /* A unit is selected - show its own real state + merge its
             * own real piece.pdl methods (Move/Attack, or whatever it
             * actually has - never hardcoded here). */
            char unit_dir[PATH_BUF], unit_state_path[PATH_BUF], unit_pdl_path[PATH_BUF];
            snprintf(unit_dir, sizeof(unit_dir), "%s/pieces/battle_01/units/%s", project_root, selected_unit_id);
            snprintf(unit_state_path, sizeof(unit_state_path), "%s/state.txt", unit_dir);
            snprintf(unit_pdl_path, sizeof(unit_pdl_path), "%s/piece.pdl", unit_dir);

            char profession[32] = "";
            read_kv_str(unit_state_path, "profession_id", profession, sizeof(profession));
            int hp = read_kv_int(unit_state_path, "hp", 0);
            int pos_x = read_kv_int(unit_state_path, "pos_x", -1);
            int pos_y = read_kv_int(unit_state_path, "pos_y", -1);

            snprintf(rowbuf, sizeof(rowbuf), "  Selected: %s (%s)", selected_unit_id, profession);
            line(rowbuf);
            snprintf(rowbuf, sizeof(rowbuf), "  HP: %d   Position: (%d,%d)", hp, pos_x, pos_y);
            line(rowbuf);
            blank();

            if (pdl_out) {
                fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
                fprintf(pdl_out, "----------------------------------------\n");
                fprintf(pdl_out, "META         | piece_id           | roster\n\n");
                fprintf(pdl_out, "METHOD       | Deselect                            | DESELECT_UNIT\n");
                /* Merge the unit's own real METHOD rows in verbatim -
                 * read its own piece.pdl, copy every METHOD line
                 * through unchanged. This is the actual "entity's own
                 * methods become available" mechanic, just merged into
                 * roster's own screen instead of a layout switch. */
                FILE *uf = fopen(unit_pdl_path, "r");
                if (uf) {
                    char uline[MAX_LINE];
                    while (fgets(uline, sizeof(uline), uf)) {
                        if (strncmp(uline, "METHOD", 6) == 0) fputs(uline, pdl_out);
                    }
                    fclose(uf);
                }
                fclose(pdl_out);
            }
        } else {
            /* No selection yet - numbered list of the ACTIVE side's own
             * units only (matches TACTICS_TXT_DESIGN.md §5's own stated
             * design: pick from a real list, not a free-roam cursor
             * requirement). */
            line("  Select a unit (active side only):");
            blank();

            if (pdl_out) {
                fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
                fprintf(pdl_out, "----------------------------------------\n");
                fprintf(pdl_out, "META         | piece_id           | roster\n\n");
            }

            for (int side = 1; side <= 2; side++) {
                if (side != active_side) continue;
                for (int i = 0; i < 3; i++) {
                    char unit_id[32];
                    snprintf(unit_id, sizeof(unit_id), "side%d_unit%d", side, i);
                    char unit_state_path[PATH_BUF];
                    snprintf(unit_state_path, sizeof(unit_state_path), "%s/pieces/battle_01/units/%s/state.txt", project_root, unit_id);
                    char profession[32] = "";
                    read_kv_str(unit_state_path, "profession_id", profession, sizeof(profession));
                    int hp = read_kv_int(unit_state_path, "hp", 0);
                    if (!profession[0]) continue; /* unit dir not materialized yet - CONFIRM_START not run */
                    snprintf(rowbuf, sizeof(rowbuf), "  %s (%s, hp:%d)", unit_id, profession, hp);
                    line(rowbuf);
                    if (pdl_out) {
                        fprintf(pdl_out, "METHOD       | %-35s | SELECT_UNIT:%s\n", unit_id, unit_id);
                    }
                }
            }
            if (pdl_out) fclose(pdl_out);
        }
    }

    blank();
    if (last_message[0]) {
        snprintf(rowbuf, sizeof(rowbuf), "  %s", last_message);
        line(rowbuf);
    }

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);
    return 0;
}
