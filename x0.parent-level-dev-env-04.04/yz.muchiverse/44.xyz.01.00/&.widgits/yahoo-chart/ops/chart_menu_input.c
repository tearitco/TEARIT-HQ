/* chart_menu_input - METHOD-table dispatcher for the yahoo-chart widget.
 * Modeled directly on broker_menu_input.c's dispatch shape.
 *
 * Handles the chart_widget piece.pdl METHODs:
 *   CHART_RANGE:10y | CHART_RANGE:5y | CHART_RANGE:2.5y | CHART_RANGE:1y
 *   CHART_RANGE:1mo | CHART_RANGE:1wk | REFRESH | BACK_TO_RESEARCH
 * Range buttons just write chart_state.txt's chart_range; the PAL loop
 * re-renders because the screen_changed marker moves on every dispatch.
 *
 * Self-contained. Usage: chart_menu_input.+x [interact_code] */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "w");
    if (!out) return;
    FILE *in = fopen(path, "r");
    int found = 0;
    if (in) {
        char line[MAX_LINE];
        size_t key_len = strlen(key);
        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
                fprintf(out, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }
    if (!found) fprintf(out, "%s=%s\n", key, val);
    fclose(out);
    rename(tmp, path);
}

static void touch_marker(const char *name) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/%s", project_root, name);
    FILE *f = fopen(path, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void read_cmd(int interact, char *cmd, size_t cmd_sz) {
    cmd[0] = '\0';
    if (interact <= 0) {
        /* Poll gui_state for the last command (widgets write via interact
         * relay; read pieces/system/chart_state.txt as the source of truth
         * is not needed here - dispatch is by relay entry). */
        return;
    }
    /* interact code: '2'..'9' -> METHOD 1..8 (piece_methods KEY mapping). */
    snprintf(cmd, cmd_sz, "%s", "");
}

int main(int argc, char *argv[]) {
    resolve_root();

    int interact = 0;
    if (argc > 1) interact = atoi(argv[1]);

    char cmd[MAX_LINE] = "";
    read_cmd(interact, cmd, sizeof(cmd));

    /* The renderer pulls chart_state.txt; commands arrive as METHOD button
     * dispatches via the parser's KEY:n -> printable. Dispatch by reading the
     * piece's click by key. */
    char piece_path[PATH_BUF];
    snprintf(piece_path, sizeof(piece_path), "%s/projects/yahoo-chart/pieces/chart_widget/piece.pdl", project_root);

    char chart_state_path[PATH_BUF];
    snprintf(chart_state_path, sizeof(chart_state_path), "%s/pieces/system/chart_state.txt", project_root);

    /* Range buttons relay '2'..'7' for the 6 ranges, '8' = Refresh,
     * '9' = Back to Research (8 METHODs -> KEY:2..KEY:9). */
    const char *ranges[] = {"10y", "5y", "2.5y", "1y", "1mo", "1wk"};
    if (interact >= 2 && interact <= 7) {
        write_kv(chart_state_path, "chart_range", ranges[interact - 2]);
    } else if (interact == 8) {
        /* REFRESH: bump chart_screen_changed so the loop re-renders. */
        touch_marker("chart_screen_changed.txt");
        return 0;
    } else if (interact == 9) {
        /* BACK_TO_RESEARCH is handled by chtpm href navigation. */
        touch_marker("chart_screen_changed.txt");
        return 0;
    } else {
        return 0;
    }

    touch_marker("chart_screen_changed.txt");
    return 0;
}
