/* yahoo_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever yahoo-app screen is currently showing. Modeled directly on
 * @.apps/my-chara-txt's own ops/mychara_menu_input.c.
 *
 * Commands:
 *   CHECK_BALANCE - display balance in message
 *   ADD_CREDIT:<amount> - add credit to bank balance
 *   REFRESH_WATCHLIST - refresh watchlist display
 *   LOOKUP_STOCK:<symbol> - lookup stock price
 *   OPEN_BROKER_WIDGET - spawn broker widget
 *   SELECT_BROKER:<id> - select a broker (from broker_select screen)
 *
 * Self-contained, no shared headers.
 * Usage: yahoo_menu_input.+x <keycode> */
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
static char project_id[64] = "yahoo-app";

static char *trim(char *s);

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *pid = getenv("PRISC_PROJECT_ID");
    if (pid && pid[0]) snprintf(project_id, sizeof(project_id), "%s", pid);
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

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/pieces/%s/piece.pdl", root, project_id, piece_id);
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
    snprintf(state_path, sizeof(state_path), "%s/projects/%s/manager/gui_state.txt", project_root, project_id);
    write_kv(state_path, "last_message", msg);
}

static void mark_screen_changed(const char *name) {
    (void)name;
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/yahoo_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "bank");
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
    char layout_name[MAX_LINE] = "bank";
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
        fprintf(cf, "project_id=%s\n", project_id);
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

static void run_external(const char *cmd) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/ops/+x/%s.+x", project_root, cmd);
    if (access(path, X_OK) == 0) {
        char user_hash[64] = "";
        char config_path[PATH_BUF];
        snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
        read_kv_str_local(config_path, "user_hash", user_hash, sizeof(user_hash));
        char full_cmd[PATH_BUF * 2];
        if (user_hash[0]) {
            snprintf(full_cmd, sizeof(full_cmd), "\"%s\" \"%s\"", path, user_hash);
        } else {
            snprintf(full_cmd, sizeof(full_cmd), "\"%s\"", path);
        }
        int rc = system(full_cmd);
        (void)rc;
    }
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
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/yahoo_screen_changed.txt", project_root);
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

    int resolved_item = 0;
    if (keycode >= '0' && keycode <= '9') resolved_item = (keycode - '0') - 1;
    else if (keycode > 9 && keycode < 1000) resolved_item = keycode - 1;
    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;
        if (strcmp(cmd, "CHECK_BALANCE") == 0) {
            char config_path[PATH_BUF];
            snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
            char balance[64] = "0.00";
            read_kv_str_local(config_path, "bank_balance", balance, sizeof(balance));
            snprintf(message, sizeof(message), "Bank Balance: $%s", balance);
        } else if (strncmp(cmd, "ADD_CREDIT:", 11) == 0) {
            const char *amount = cmd + 11;
            char config_path[PATH_BUF];
            snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
            char balance[64] = "0.00";
            read_kv_str_local(config_path, "bank_balance", balance, sizeof(balance));
            float new_bal = atof(balance) + atof(amount);
            write_kv(config_path, "bank_balance", "");
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f", new_bal);
            write_kv(config_path, "bank_balance", buf);
            snprintf(message, sizeof(message), "Added $%s. New balance: $%s", amount, buf);
        } else if (strcmp(cmd, "REFRESH_WATCHLIST") == 0) {
            message[0] = '\0';
        } else if (strncmp(cmd, "LOOKUP_STOCK:", 12) == 0) {
            const char *symbol = cmd + 12;
            run_external("lookup_stock");
            snprintf(message, sizeof(message), "Looked up %s", symbol);
        } else if (strcmp(cmd, "OPEN_BROKER_WIDGET") == 0) {
            char house_root[PATH_BUF] = "";
            char house_root_path[PATH_BUF];
            snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
            FILE *hf = fopen(house_root_path, "r");
            if (hf) {
                if (fgets(house_root, sizeof(house_root), hf)) {
                    house_root[strcspn(house_root, "\r\n")] = '\0';
                }
                fclose(hf);
            }
            if (house_root[0]) {
                char widget_button[PATH_BUF];
                snprintf(widget_button, sizeof(widget_button), "%s/&.widgits/yahoo-broker/button.sh", house_root);
                if (access(widget_button, X_OK) == 0) {
                    char cmd_buf[PATH_BUF * 2];
                    snprintf(cmd_buf, sizeof(cmd_buf),
                        "setsid env RUN_PROFILE=widget bash '%s' run-widget '%s' >/dev/null 2>&1 < /dev/null &",
                        widget_button, project_root);
                    { int _rc = system(cmd_buf); (void)_rc; }
                    snprintf(message, sizeof(message), "Broker widget launching (separate GL window)...");
                } else {
                    snprintf(message, sizeof(message), "Broker widget not built yet.");
                }
            }
        } else if (strncmp(cmd, "SELECT_BROKER:", 14) == 0) {
            const char *broker_id = cmd + 14;
            char broker_state_path[PATH_BUF];
            snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
            write_kv(broker_state_path, "selected_broker", broker_id);
            write_kv(broker_state_path, "focused_project_root", project_root);
            write_kv(broker_state_path, "focused_project_id", project_id);
            char house_root[PATH_BUF] = "";
            char house_root_path[PATH_BUF];
            snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
            FILE *hf = fopen(house_root_path, "r");
            if (hf) {
                if (fgets(house_root, sizeof(house_root), hf)) {
                    house_root[strcspn(house_root, "\r\n")] = '\0';
                }
                fclose(hf);
            }
            if (house_root[0]) {
                char widget_button[PATH_BUF];
                snprintf(widget_button, sizeof(widget_button), "%s/&.widgits/yahoo-broker/button.sh", house_root);
                if (access(widget_button, X_OK) == 0) {
                    char cmd_buf[PATH_BUF * 2];
                    snprintf(cmd_buf, sizeof(cmd_buf),
                        "setsid env RUN_PROFILE=widget bash '%s' run-widget '%s' >/dev/null 2>&1 < /dev/null &",
                        widget_button, project_root);
                    { int _rc = system(cmd_buf); (void)_rc; }
                    snprintf(message, sizeof(message), "Broker widget launching for %s...", broker_id);
                } else {
                    snprintf(message, sizeof(message), "Broker widget not built yet.");
                }
            }
        } else if (strcmp(cmd, "BACK_TO_BANK") == 0) {
            /* handled by href navigation in chtpm, not needed here */
            message[0] = '\0';
        } else {
            snprintf(message, sizeof(message), "Unknown command: %s", cmd);
        }
    }

    if (message[0]) {
        append_message(message);
        mark_screen_changed("yahoo");
    }

    char piece_id[64] = "bank";
    get_current_piece_id(project_root, piece_id, sizeof(piece_id));
    write_chtpm_bridge(piece_id);
    ping_chtpm_render_marker(project_root);

    return 0;
}
