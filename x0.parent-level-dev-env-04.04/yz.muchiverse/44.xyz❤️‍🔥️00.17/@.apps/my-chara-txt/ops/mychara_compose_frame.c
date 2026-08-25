/* mychara_compose_frame - renders whichever my-chara-txt screen is
 * current into pieces/apps/player_app/view.txt (the file chtpm's own
 * ${game_map} placeholder substitutes verbatim). ${piece_methods} (a
 * separate chtpm-native placeholder) renders the current screen's own
 * numbered METHOD buttons - this op never draws that menu itself, only
 * the surrounding chrome + any screen-specific live data (day/health/
 * money/inventory for now - P2 has one screen, "main").
 *
 * Modeled directly on 041.pal-chain's own ops/chain_compose_frame.c
 * (real, proven, live-verified precedent - see that file's own header
 * comment for the full "ONE WRITER RULE" / "ONE VISIBLE FRAME WRITER
 * RULE" history this shape already resolves): writes ONLY view.txt,
 * never pieces/display/current_frame.txt directly (that's
 * chtpm_parser_pal.c's own exclusive job) - then bumps
 * pieces/display/frame_changed.txt so the parser's own dirty-check
 * picks up the change on its very next loop iteration, matching
 * chain_compose_frame.c's own ping_chtpm_render_marker().
 *
 * Self-contained, no shared headers.
 * Usage: mychara_compose_frame.+x (no args) */
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
    snprintf(out, out_sz, "main");
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

    char state_path[PATH_BUF], view_path[PATH_BUF], config_path[PATH_BUF], plots_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-chara-txt/pieces/mychara_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(plots_path, sizeof(plots_path), "%s/pieces/system/plots.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    int day = read_kv_int(config_path, "day", 1);
    int max_days = read_kv_int(config_path, "max_days", 10);
    int health = read_kv_int(config_path, "health", 100);
    int money = read_kv_int(config_path, "money", 500);
    int grain = read_kv_int(config_path, "grain_in_inventory", 10);

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "  M Y - C H A R A   [%s]", active_piece);
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "main") == 0) {
        snprintf(rowbuf, sizeof(rowbuf), "  Day %d / %d", day, max_days);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Health: %d / 100", health);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Money: %d", money);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Grain: %d", grain);
        line(rowbuf);
    } else if (strcmp(active_piece, "farm") == 0) {
        snprintf(rowbuf, sizeof(rowbuf), "  Grain in inventory: %d", grain);
        line(rowbuf);
        blank();

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-chara-txt/pieces/farm/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | farm\n\n");

            for (int i = 0; i < 3; i++) {
                char key_state[64], key_crop[64], key_harvest[64];
                snprintf(key_state, sizeof(key_state), "plot_%d_state", i);
                snprintf(key_crop, sizeof(key_crop), "plot_%d_crop", i);
                snprintf(key_harvest, sizeof(key_harvest), "plot_%d_harvest_day", i);

                char state[32] = "", crop[32] = "", harvest_str[32] = "";
                read_kv_str(plots_path, key_state, state, sizeof(state));
                read_kv_str(plots_path, key_crop, crop, sizeof(crop));
                read_kv_str(plots_path, key_harvest, harvest_str, sizeof(harvest_str));

                if (!state[0]) snprintf(state, sizeof(state), "empty");
                if (!harvest_str[0]) snprintf(harvest_str, sizeof(harvest_str), "0");
                int harvest_day = atoi(harvest_str);

                if (strcmp(state, "empty") == 0) {
                    fprintf(pdl_out, "METHOD       | Plot %d: Empty (plant wheat/corn)    | PLANT_CHOOSE:%d\n", i, i);
                } else if (strcmp(state, "growing") == 0) {
                    int days_left = harvest_day - day;
                    fprintf(pdl_out, "METHOD       | Plot %d: Growing (%d days left)       | NOOP\n", i, days_left > 0 ? days_left : 0);
                } else if (strcmp(state, "ripe") == 0) {
                    fprintf(pdl_out, "METHOD       | Plot %d: Ripe! (harvest %s)           | HARVEST:%d\n", i, crop, i);
                }
            }
            fclose(pdl_out);
        }

        line("Plots status regenerated.");
    } else if (strcmp(active_piece, "mine") == 0) {
        int silver = read_kv_int(config_path, "silver_in_inventory", 0);
        int gold = read_kv_int(config_path, "gold_in_inventory", 0);
        snprintf(rowbuf, sizeof(rowbuf), "  Silver: %d", silver);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Gold: %d", gold);
        line(rowbuf);
        blank();

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-chara-txt/pieces/mine/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | mine\n\n");
            fprintf(pdl_out, "METHOD       | Mine (RNG: 70%% silver, 30%% gold) | MINE\n");
            fclose(pdl_out);
        }
    } else if (strcmp(active_piece, "automation") == 0) {
        char supervision[32] = "";
        read_kv_str(config_path, "supervision_mode", supervision, sizeof(supervision));
        if (!supervision[0]) snprintf(supervision, sizeof(supervision), "manual");
        int decision_mode = read_kv_int(config_path, "decision_mode", 0);
        int risk_level = read_kv_int(config_path, "risk_level", 5);
        int paused = read_kv_int(config_path, "paused_for_confirmation", 0);

        snprintf(rowbuf, sizeof(rowbuf), "  Supervision: %s", supervision);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Decision mode: %d (0=preset)", decision_mode);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Risk level: %d/10", risk_level);
        line(rowbuf);
        if (strcmp(supervision, "semi") == 0) {
            snprintf(rowbuf, sizeof(rowbuf), "  Paused for confirmation: %s", paused ? "YES (use Continue)" : "no");
            line(rowbuf);
        }
        blank();

        char pdl_path[PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-chara-txt/pieces/automation/piece.pdl", project_root);
        FILE *pdl_out = fopen(pdl_path, "w");
        if (pdl_out) {
            fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
            fprintf(pdl_out, "----------------------------------------\n");
            fprintf(pdl_out, "META         | piece_id           | automation\n\n");
            fprintf(pdl_out, "METHOD       | Set Manual (you control every action) | SET_SUPERVISION:manual\n");
            fprintf(pdl_out, "METHOD       | Set Semi (auto one action, then pause) | SET_SUPERVISION:semi\n");
            fprintf(pdl_out, "METHOD       | Set Full (auto continuously, ~1/sec)  | SET_SUPERVISION:full\n");
            if (strcmp(supervision, "semi") == 0 && paused) {
                fprintf(pdl_out, "METHOD       | Continue (resume auto-play)           | CONTINUE_AUTO\n");
            }
            fclose(pdl_out);
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
