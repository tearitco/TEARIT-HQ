/* broker_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * the yahoo-broker widget. Modeled directly on yahoo_menu_input.c.
 *
 * Commands:
 *   VIEW_PORTFOLIO - show portfolio summary
 *   VIEW_OPTIONS - show options summary
 *   TRADE_STOCK:<symbol>:<shares> - buy/sell stocks
 *   PREDICT_STOCK:<symbol> - predict price
 *   DEPOSIT:<amount> - deposit from bank to broker
 *   WITHDRAW:<amount> - withdraw from broker to bank
 *   REFRESH - refresh all data
 *
 * Self-contained.
 * Usage: broker_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static char *trim(char *s);

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
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    FILE *orig = fopen(path, "r");
    if (orig) {
        char line[MAX_LINE];
        int found = 0;
        while (fgets(line, sizeof(line), orig)) {
            if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                fprintf(f, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, f);
            }
        }
        if (!found) fprintf(f, "%s=%s\n", key, val);
        fclose(orig);
    } else {
        fprintf(f, "%s=%s\n", key, val);
    }
    fclose(f);
    rename(tmp, path);
}

static void read_piece_pdl(const char *path, MenuItem *items, int *count) {
    *count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 5) == 0 && line[5] == ' ') {
            if (*count >= MAX_MENU_ITEMS) break;
            char *p = line + 6;
            while (*p == ' ' || *p == '|') p++;
            char *label_end = strstr(p, " |");
            if (!label_end) label_end = strchr(p, '\n');
            if (!label_end) label_end = p + strlen(p);
            size_t label_len = label_end - p;
            if (label_len >= sizeof(items[*count].label)) label_len = sizeof(items[*count].label) - 1;
            memcpy(items[*count].label, p, label_len);
            items[*count].label[label_len] = '\0';
            char *cmd_start = strstr(label_end ? label_end : p, " |");
            if (cmd_start) {
                cmd_start += 3;
                while (*cmd_start == ' ') cmd_start++;
                char *cmd_end = strchr(cmd_start, '\n');
                if (cmd_end) *cmd_end = '\0';
                snprintf(items[*count].command, sizeof(items[*count].command), "%s", cmd_start);
            } else {
                items[*count].command[0] = '\0';
            }
            (*count)++;
        }
    }
    fclose(f);
}

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/yahoo-broker/pieces/%s/piece.pdl", root, piece_id);
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

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static void append_message(const char *msg) {
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/yahoo-broker/manager/gui_state.txt", project_root);
    write_kv(state_path, "last_message", msg);
}

static void mark_screen_changed(const char *name) {
    (void)name;
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/broker_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "broker_widget");
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
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
    }
    fclose(f);
}

static void resolve_piece_pdl_path(const char *current_layout, const char *root, char *out, size_t out_sz) {
    char layout_name[MAX_LINE] = "broker_widget";
    const char *slash = strrchr(current_layout, '/');
    const char *base = slash ? slash + 1 : current_layout;
    char tmp[MAX_LINE];
    snprintf(tmp, sizeof(tmp), "%s", base);
    char *dot = strstr(tmp, ".chtpm");
    if (dot) *dot = '\0';
    if (tmp[0]) snprintf(layout_name, sizeof(layout_name), "%s", tmp);

    snprintf(out, out_sz, "%s/pieces/%s/%s.pdl", root, layout_name, layout_name);
    if (access(out, F_OK) != 0) {
        snprintf(out, out_sz, "%s/pieces/apps/player_app/piece.pdl", root);
    }
}

static void write_chtpm_bridge(const char *piece_id) {
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=yahoo-broker\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void run_external(const char *cmd, const char *arg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/ops/+x/%s.+x", project_root, cmd);
    if (access(path, X_OK) == 0) {
        char full_cmd[PATH_BUF * 2];
        if (arg && arg[0]) {
            snprintf(full_cmd, sizeof(full_cmd), "\"%s\" \"%s\"", path, arg);
        } else {
            snprintf(full_cmd, sizeof(full_cmd), "\"%s\"", path);
        }
        int rc = system(full_cmd);
        (void)rc;
    }
}

static void transfer_funds(const char *direction, const char *amount_str) {
    char broker_state_path[PATH_BUF];
    snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
    char focused_root[PATH_BUF] = "";
    read_kv_str_local(broker_state_path, "focused_project_root", focused_root, sizeof(focused_root));
    if (!focused_root[0]) return;

    char bank_config[PATH_BUF];
    snprintf(bank_config, sizeof(bank_config), "%s/pieces/system/config.txt", focused_root);
    char bank_balance_str[64] = "0.00";
    read_kv_str_local(bank_config, "bank_balance", bank_balance_str, sizeof(bank_balance_str));
    float bank_balance = atof(bank_balance_str);

    char broker_balance_str[64] = "0.00";
    read_kv_str_local(broker_state_path, "broker_balance", broker_balance_str, sizeof(broker_balance_str));
    float broker_balance = atof(broker_balance_str);

    float amount = atof(amount_str);
    if (strcmp(direction, "deposit") == 0) {
        if (amount > bank_balance) amount = bank_balance;
        bank_balance -= amount;
        broker_balance += amount;
    } else {
        if (amount > broker_balance) amount = broker_balance;
        bank_balance += amount;
        broker_balance -= amount;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", bank_balance);
    write_kv(bank_config, "bank_balance", buf);
    snprintf(buf, sizeof(buf), "%.2f", broker_balance);
    write_kv(broker_state_path, "broker_balance", buf);
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    int keycode = atoi(argv[1]);
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/state.txt", project_root);

    if (keycode == 0) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char current_target[128];
        read_kv_str_local(state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/broker_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char current_layout[PATH_BUF] = "";
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    {
        FILE *f = fopen(layout_path, "r");
        if (f) {
            if (fgets(current_layout, sizeof(current_layout), f)) {
                current_layout[strcspn(current_layout, "\r\n")] = '\0';
            }
            fclose(f);
        }
    }

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    char message[256] = "";

    if (keycode > 0 && keycode <= item_count) {
        const char *cmd = items[keycode - 1].command;
        if (strncmp(cmd, "VIEW_", 5) == 0) {
            snprintf(message, sizeof(message), "Showing %s...", cmd + 5);
        } else if (strncmp(cmd, "TRADE_STOCK:", 12) == 0) {
            run_external("buy_stock", cmd + 12);
            snprintf(message, sizeof(message), "Trade executed: %s", cmd + 12);
        } else if (strncmp(cmd, "PREDICT_STOCK:", 14) == 0) {
            run_external("predictions", cmd + 14);
            snprintf(message, sizeof(message), "Prediction for %s", cmd + 14);
        } else if (strncmp(cmd, "DEPOSIT:", 8) == 0) {
            transfer_funds("deposit", cmd + 8);
            snprintf(message, sizeof(message), "Deposited $%s to broker", cmd + 8);
        } else if (strncmp(cmd, "WITHDRAW:", 9) == 0) {
            transfer_funds("withdraw", cmd + 9);
            snprintf(message, sizeof(message), "Withdrew $%s from broker", cmd + 9);
        } else if (strcmp(cmd, "REFRESH") == 0) {
            run_external("portfolio_new", NULL);
            snprintf(message, sizeof(message), "Portfolio refreshed");
        } else if (strcmp(cmd, "BACK_TO_BANK") == 0) {
            message[0] = '\0';
        } else if (strcmp(cmd, "DEBUG_LEDGER") == 0) {
            char ledger_path[PATH_BUF];
            snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", project_root);
            const char *display = getenv("DISPLAY");
            if (display && display[0]) {
                char gcmd[PATH_BUF * 2];
                snprintf(gcmd, sizeof(gcmd), "gedit \"%s\" >/dev/null 2>&1 &", ledger_path);
                system(gcmd);
                snprintf(message, sizeof(message), "Opened master ledger in gedit: %s", ledger_path);
            } else {
                snprintf(message, sizeof(message), "Ledger: %s", ledger_path);
            }
        } else {
            snprintf(message, sizeof(message), "Command: %s", cmd);
        }
    }

    if (message[0]) {
        append_message(message);
        mark_screen_changed("broker");
    }

    char piece_id[64] = "broker_widget";
    get_current_piece_id(project_root, piece_id, sizeof(piece_id));
    write_chtpm_bridge(piece_id);
    ping_chtpm_render_marker(project_root);

    return 0;
}
