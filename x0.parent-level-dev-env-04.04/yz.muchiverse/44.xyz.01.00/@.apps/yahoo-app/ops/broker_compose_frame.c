/* broker_compose_frame - per-screen renderer for the in-app Yahoo Finance
 * broker-sim trading screen (broker.chtpm) and the broker_widget.chtpm
 * widget. Modeled directly on yahoo_compose_frame.c.
 *
 * Renders the active layout's frame into view.txt (the ${game_map} token).
 *
 * Self-contained.
 * Usage: broker_compose_frame.+x */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";
static char project_id[64] = "yahoo-app";

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

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void read_portfolio(const char *focused_root, const char *user_hash, char *out, size_t out_sz) {
    snprintf(out, out_sz, "(run portfolio to see details)");
    if (!focused_root[0] || !user_hash[0]) return;
    char pipe_cmd[PATH_BUF * 2];
    snprintf(pipe_cmd, sizeof(pipe_cmd),
        "\"%s/ops/+x/portfolio_new.+x\" \"%s\" 2>/dev/null",
        focused_root, user_hash);
    FILE *pf = popen(pipe_cmd, "r");
    if (!pf) return;
    char buf[1024];
    size_t total = 0;
    while (fgets(buf + total, sizeof(buf) - total, pf) && total < out_sz - 1) {
        total += strlen(buf + total);
    }
    pclose(pf);
    if (total > 0) snprintf(out, out_sz, "%s", buf);
}

static void render_screen(char *view, size_t view_sz, const char *title,
                          const char *status) {
    char broker_state_path[PATH_BUF];
    snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
    char focused_root[PATH_BUF] = "";
    read_kv_str_local(broker_state_path, "focused_project_root", focused_root, sizeof(focused_root));
    char selected_broker[64] = "none";
    read_kv_str_local(broker_state_path, "selected_broker", selected_broker, sizeof(selected_broker));
    char broker_balance[64] = "0.00";
    read_kv_str_local(broker_state_path, "broker_balance", broker_balance, sizeof(broker_balance));

    char portfolio[1024];
    char user_hash[64] = "";
    if (focused_root[0]) {
        char config_path[PATH_BUF];
        snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", focused_root);
        read_kv_str_local(config_path, "user_hash", user_hash, sizeof(user_hash));
    }
    read_portfolio(focused_root, user_hash, portfolio, sizeof(portfolio));

    char status_line[64];
    snprintf(status_line, sizeof(status_line), "%-57s", status[0] ? status : "");

    snprintf(view, view_sz,
        "+============================================================+\n"
        "|  %-56s|\n"
        "+============================================================+\n"
        "|  Broker: %-50s|\n"
        "|  Broker Balance: $%-43s|\n"
        "+============================================================+\n"
        "|  Status: %-50s|\n"
        "+============================================================+\n"
        "|  Portfolio:                                                |\n"
        "|  %-57s|\n"
        "+============================================================+\n",
        title, selected_broker, broker_balance, status_line, portfolio);
}

int main(int argc, char *argv[]) {
    resolve_root();

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

    const char *layout_name = strrchr(current_layout, '/');
    if (layout_name) layout_name++; else layout_name = current_layout;

    char gui_state[PATH_BUF];
    snprintf(gui_state, sizeof(gui_state), "%s/projects/%s/manager/gui_state.txt", project_root, project_id);
    char status[512] = "";
    read_kv_str_local(gui_state, "last_message", status, sizeof(status));

    if (strcmp(layout_name, "broker.chtpm") == 0 ||
        strcmp(layout_name, "broker_widget.chtpm") == 0) {
        char view[4096];
        const char *title = strcmp(layout_name, "broker.chtpm") == 0
            ? "Y A H O O   F I N A N C E   B R O K E R"
            : "Y A H O O   B R O K E R   W I D G E T";
        render_screen(view, sizeof(view), title, status);
        char view_path[PATH_BUF];
        snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
        write_file(view_path, view);
    }

    ping_chtpm_render_marker(project_root);

    return 0;
}
