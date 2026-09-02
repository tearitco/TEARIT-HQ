/* mychara_menu_input - piece.pdl METHOD-table-driven ACTION dispatch
 * for whichever my-chara-txt screen is currently showing. Modeled
 * directly on 041.pal-chain's own ops/chain_menu_input.c (real, proven,
 * live-verified precedent - itself modeled on wsr-pal's own
 * wsr_menu_input.c): screen SWITCHING is a real chtpm <button
 * href="..."> handled entirely by chtpm_parser_pal.c, never this op's
 * job (xyzos-standards sec. 18); "which screen is current" is derived
 * fresh every call from pieces/display/current_layout.txt, never
 * separately tracked mutable state.
 *
 * P2 scope: ONE screen ("main"), ONE real command (END_TURN) - proves
 * the full real loop (render -> real key injection -> dispatch ->
 * state mutation -> ledger append -> re-render) end to end before
 * farm/mine/store/inventory screens are added.
 *
 * my-chara-txt-specific commands (piece.pdl METHOD rows dispatch here):
 *   END_TURN - advances day by 1, applies a flat health decay (-5/day,
 *              floored at 0), appends a ledger line, sets last_message
 *
 * Self-contained, no shared headers (matches chain_menu_input.c's own
 * convention).
 * Usage: mychara_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[256];
} MenuItem;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str_local(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str_local(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[32][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
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

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-chara-txt/pieces/%s/piece.pdl", root, piece_id);
#pragma GCC diagnostic pop
    FILE *f = fopen(pdl_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max_items && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *label = trim(p1 + 1);
        char *command = trim(p2 + 1);
        snprintf(items[n].label, sizeof(items[n].label), "%s", label);
        snprintf(items[n].command, sizeof(items[n].command), "%s", command);
        n++;
    }
    fclose(f);
    return n;
}

static void write_chtpm_bridge(const char *piece_id) {
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=my-chara-txt\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "main");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
    FILE *f = fopen(layout_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        const char *slash = strrchr(line, '/');
        const char *base = slash ? slash + 1 : line;
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

static void ledger_append(const char *root, int day, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%s|%s\n", ts, day, action_type, details);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-chara-txt/pieces/mychara_menu/state.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    int key = atoi(argv[1]);

    if (key == 0) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mychara_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        char plots_path[PATH_BUF];
        snprintf(plots_path, sizeof(plots_path), "%s/pieces/system/plots.txt", project_root);
        int day = read_kv_int(config_path, "day", 1);

        if (strcmp(cmd, "END_TURN") == 0) {
            int max_days = read_kv_int(config_path, "max_days", 10);
            int health = read_kv_int(config_path, "health", 100);

            health -= 5;
            if (health < 0) health = 0;
            day += 1;

            write_kv_int(config_path, "health", health);
            write_kv_int(config_path, "day", day);

            for (int i = 0; i < 3; i++) {
                char key_state[64], key_harvest[64];
                snprintf(key_state, sizeof(key_state), "plot_%d_state", i);
                snprintf(key_harvest, sizeof(key_harvest), "plot_%d_harvest_day", i);

                char state[32] = "";
                read_kv_str_local(plots_path, key_state, state, sizeof(state));
                if (!state[0]) snprintf(state, sizeof(state), "empty");

                if (strcmp(state, "growing") == 0) {
                    char harvest_str[32] = "0";
                    read_kv_str_local(plots_path, key_harvest, harvest_str, sizeof(harvest_str));
                    int harvest_day = atoi(harvest_str);
                    if (day >= harvest_day) {
                        write_kv(plots_path, key_state, "ripe");
                    }
                }
            }

            char details[128];
            snprintf(details, sizeof(details), "health:%d", health);
            ledger_append(project_root, day - 1, "day_end", details);

            if (day > max_days) {
                write_kv(config_path, "game_state", "game_over");
                snprintf(message, sizeof(message), "Day %d - GAME OVER (reached max_days).", day - 1);
            } else if (health <= 0) {
                write_kv(config_path, "game_state", "game_over");
                snprintf(message, sizeof(message), "Day %d - GAME OVER (health reached 0).", day - 1);
            } else {
                snprintf(message, sizeof(message), "Day %d began. Health %d.", day, health);
            }
        } else if (strncmp(cmd, "PLANT_CHOOSE:", 13) == 0) {
            int plot_id = atoi(cmd + 13);
            char key_state[64], key_crop[64], key_harvest[64];
            snprintf(key_state, sizeof(key_state), "plot_%d_state", plot_id);
            snprintf(key_crop, sizeof(key_crop), "plot_%d_crop", plot_id);
            snprintf(key_harvest, sizeof(key_harvest), "plot_%d_harvest_day", plot_id);

            char state[32] = "";
            read_kv_str_local(plots_path, key_state, state, sizeof(state));
            if (!state[0]) snprintf(state, sizeof(state), "empty");

            if (strcmp(state, "empty") == 0) {
                int grain_now = read_kv_int(config_path, "grain_in_inventory", 10);
                if (grain_now >= 10) {
                    grain_now -= 10;
                    write_kv_int(config_path, "grain_in_inventory", grain_now);
                    write_kv(plots_path, key_state, "growing");
                    write_kv(plots_path, key_crop, "wheat");
                    char harvest_day_str[32];
                    snprintf(harvest_day_str, sizeof(harvest_day_str), "%d", day + 3);
                    write_kv(plots_path, key_harvest, harvest_day_str);

                    char details[128];
                    snprintf(details, sizeof(details), "wheat:plot_%d", plot_id);
                    ledger_append(project_root, day, "plant", details);

                    snprintf(message, sizeof(message), "Planted wheat on plot %d! Now have %d grain.", plot_id, grain_now);
                } else {
                    snprintf(message, sizeof(message), "Need 10 grain to plant. You have %d.", grain_now);
                }
            } else {
                snprintf(message, sizeof(message), "Plot %d is not empty.", plot_id);
            }
        } else if (strncmp(cmd, "HARVEST:", 8) == 0) {
            int plot_id = atoi(cmd + 8);
            char key_state[64], key_crop[64], key_harvest[64];
            snprintf(key_state, sizeof(key_state), "plot_%d_state", plot_id);
            snprintf(key_crop, sizeof(key_crop), "plot_%d_crop", plot_id);
            snprintf(key_harvest, sizeof(key_harvest), "plot_%d_harvest_day", plot_id);

            char state[32] = "empty", crop[32] = "";
            read_kv_str_local(plots_path, key_state, state, sizeof(state));
            read_kv_str_local(plots_path, key_crop, crop, sizeof(crop));

            if (strcmp(state, "ripe") == 0) {
                int grain_now = read_kv_int(config_path, "grain_in_inventory", 10);
                int harvest_amount = strcmp(crop, "wheat") == 0 ? 50 : 60;
                grain_now += harvest_amount;

                write_kv_int(config_path, "grain_in_inventory", grain_now);
                write_kv(plots_path, key_state, "empty");
                write_kv(plots_path, key_crop, "");

                char details[128];
                snprintf(details, sizeof(details), "%s:%d:plot_%d", crop, harvest_amount, plot_id);
                ledger_append(project_root, day, "harvest", details);

                snprintf(message, sizeof(message), "Harvested %d %s! Now have %d grain.", harvest_amount, crop, grain_now);
            } else {
                snprintf(message, sizeof(message), "Plot %d is not ripe yet.", plot_id);
            }
        } else if (strcmp(cmd, "MINE") == 0) {
            srand(time(NULL) + getpid());
            int roll = rand() % 100;
            int silver_now = read_kv_int(config_path, "silver_in_inventory", 0);
            int gold_now = read_kv_int(config_path, "gold_in_inventory", 0);

            if (roll < 70) {
                silver_now++;
                write_kv_int(config_path, "silver_in_inventory", silver_now);
                char details[128];
                snprintf(details, sizeof(details), "silver:1");
                ledger_append(project_root, day, "mine", details);
                snprintf(message, sizeof(message), "Mined silver! Now have %d.", silver_now);
            } else {
                gold_now++;
                write_kv_int(config_path, "gold_in_inventory", gold_now);
                char details[128];
                snprintf(details, sizeof(details), "gold:1");
                ledger_append(project_root, day, "mine", details);
                snprintf(message, sizeof(message), "Mined gold! Now have %d.", gold_now);
            }
        } else if (strncmp(cmd, "SET_SUPERVISION:", 16) == 0) {
            const char *mode = cmd + 16;
            write_kv(config_path, "supervision_mode", mode);
            if (strcmp(mode, "manual") == 0) {
                write_kv_int(config_path, "paused_for_confirmation", 0);
            }
            snprintf(message, sizeof(message), "Supervision set to: %s", mode);
        } else if (strncmp(cmd, "SET_DECISION_MODE:", 18) == 0) {
            int mode_val = atoi(cmd + 18);
            write_kv_int(config_path, "decision_mode", mode_val);
            snprintf(message, sizeof(message), "Decision mode set to: %d", mode_val);
        } else if (strncmp(cmd, "SET_RISK:", 9) == 0) {
            int risk_val = atoi(cmd + 9);
            if (risk_val < 1) risk_val = 1;
            if (risk_val > 10) risk_val = 10;
            write_kv_int(config_path, "risk_level", risk_val);
            snprintf(message, sizeof(message), "Risk level set to: %d", risk_val);
        } else if (strcmp(cmd, "CONTINUE_AUTO") == 0) {
            write_kv_int(config_path, "paused_for_confirmation", 0);
            snprintf(message, sizeof(message), "Resumed automation for one more action.");
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mychara_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
