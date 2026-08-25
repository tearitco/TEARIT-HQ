/* broker_compose_frame - per-screen renderer for yahoo-broker widget.
 * Modeled directly on yahoo_compose_frame.c.
 *
 * Renders broker_widget.chtpm -> portfolio summary into view.txt
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

static void render_broker(char *view, size_t view_sz) {
    char broker_state_path[PATH_BUF];
    snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
    char focused_root[PATH_BUF] = "";
    read_kv_str_local(broker_state_path, "focused_project_root", focused_root, sizeof(focused_root));
    char selected_broker[64] = "none";
    read_kv_str_local(broker_state_path, "selected_broker", selected_broker, sizeof(selected_broker));
    char broker_balance[64] = "0.00";
    read_kv_str_local(broker_state_path, "broker_balance", broker_balance, sizeof(broker_balance));

    char portfolio[1024] = "(run portfolio to see details)";
    if (focused_root[0]) {
        char user_hash[64] = "";
        char config_path[PATH_BUF];
        snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", focused_root);
        read_kv_str_local(config_path, "user_hash", user_hash, sizeof(user_hash));
        if (user_hash[0]) {
            char pipe_cmd[PATH_BUF * 2];
            snprintf(pipe_cmd, sizeof(pipe_cmd),
                "\"%s/ops/+x/portfolio_new.+x\" \"%s\" 2>/dev/null",
                focused_root, user_hash);
            FILE *pf = popen(pipe_cmd, "r");
            if (pf) {
                char buf[1024];
                size_t total = 0;
                while (fgets(buf + total, sizeof(buf) - total, pf) && total < sizeof(portfolio) - 1) {
                    total += strlen(buf + total);
                }
                pclose(pf);
                if (total > 0) {
                    snprintf(portfolio, sizeof(portfolio), "%s", buf);
                }
            }
        }
    }

    snprintf(view, view_sz,
        "+============================================================+\n"
        "|              Y A H O O   B R O K E R   W I D G E T        |\n"
        "+============================================================+\n"
        "|  Broker: %-50s|\n"
        "|  Broker Balance: $%-43s|\n"
        "+============================================================+\n"
        "|  Portfolio:                                                |\n"
        "|  %-57s|\n"
        "+============================================================+\n",
        selected_broker, broker_balance, portfolio);
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

    if (strcmp(layout_name, "broker_widget.chtpm") == 0) {
        char view[4096];
        render_broker(view, sizeof(view));
        char view_path[PATH_BUF];
        snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
        write_file(view_path, view);
    }

    ping_chtpm_render_marker(project_root);

    return 0;
}
