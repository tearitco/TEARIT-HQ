/* chart_compose_frame - per-screen renderer for the yahoo-chart widget.
 * Modeled directly on broker_compose_frame.c.
 *
 * Renders chart.chtpm -> a daily-close ASCII chart into view.txt using the
 * chart_data op (data/research/<SYM>.csv, offline-backfilled when needed).
 *
 * Self-contained.
 * Usage: chart_compose_frame.+x */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_POINTS 4000
#define CHART_W 60
#define CHART_H 8

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

/* Draw an ASCII block chart of `count` closes into `rows` lines of CHART_W
 * columns. Highest close -> top row. */
static void draw_chart(char rows[CHART_H][CHART_W + 1], int count, const double *closes) {
    for (int r = 0; r < CHART_H; r++) {
        memset(rows[r], ' ', CHART_W);
        rows[r][CHART_W] = '\0';
    }
    if (count < 1) return;
    double lo = closes[0], hi = closes[0];
    for (int i = 1; i < count; i++) {
        if (closes[i] < lo) lo = closes[i];
        if (closes[i] > hi) hi = closes[i];
    }
    double span = hi - lo;
    if (span <= 0.0) span = 1.0;
    for (int i = 0; i < count && i < CHART_W; i++) {
        int col = i;
        double v = closes[i];
        int row = CHART_H - 1 - (int)(((v - lo) / span) * (CHART_H - 1) + 0.5);
        if (row < 0) row = 0;
        if (row >= CHART_H) row = CHART_H - 1;
        rows[row][col] = (i == count - 1) ? '*' : '#';
        /* connect one row above/below for a smoother line */
        if (i > 0) {
            int prow = -1;
            double pv = closes[i - 1];
            prow = CHART_H - 1 - (int)(((pv - lo) / span) * (CHART_H - 1) + 0.5);
            if (prow < 0) prow = 0;
            if (prow >= CHART_H) prow = CHART_H - 1;
            while (prow != row) {
                prow += (row > prow) ? 1 : -1;
                if (prow >= 0 && prow < CHART_H && rows[prow][col] == ' ') rows[prow][col] = '|';
            }
        }
    }
}

static void render_chart(char *view, size_t view_sz) {
    char chart_state_path[PATH_BUF];
    snprintf(chart_state_path, sizeof(chart_state_path), "%s/pieces/system/chart_state.txt", project_root);
    char symbol[64] = "NVDA";
    read_kv_str_local(chart_state_path, "chart_symbol", symbol, sizeof(symbol));
    char range[16] = "1y";
    read_kv_str_local(chart_state_path, "chart_range", range, sizeof(range));

    double closes[MAX_POINTS] = {0};
    char dates[MAX_POINTS][16] = {0};
    int count = 0;
    char pipe_cmd[PATH_BUF * 2];
    snprintf(pipe_cmd, sizeof(pipe_cmd), "\"%s/ops/+x/chart_data.+x\" \"%s\" \"%s\" 2>/dev/null",
             project_root, symbol, range);
    FILE *pf = popen(pipe_cmd, "r");
    if (pf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), pf) && count < MAX_POINTS) {
            line[strcspn(line, "\r\n")] = 0;
            double c;
            if (sscanf(line, "%15[^,],%lf", dates[count], &c) == 2) {
                closes[count] = c;
                count++;
            }
        }
        pclose(pf);
    }

    char rows[CHART_H][CHART_W + 1];
    draw_chart(rows, count, closes);

    double lo = closes[0], hi = closes[0];
    for (int i = 1; i < count; i++) {
        if (closes[i] < lo) lo = closes[i];
        if (closes[i] > hi) hi = closes[i];
    }
    double last = count > 0 ? closes[count - 1] : 0.0;
    double first = count > 0 ? closes[0] : 0.0;
    char trend = last >= first ? '+' : '-';

    char body[4096];
    int pos = 0;
    pos += snprintf(body + pos, sizeof(body) - pos,
        "+============================================================+\n"
        "|            Y A H O O   C H A R T   W I D G E T            |\n"
        "+============================================================+\n"
        "|  Symbol: %-50s|\n"
        "|  Range:  %-50s|\n"
        "|  Points: %-6d  Last: $%-38.2f|\n"
        "|  Range:  lo $%-9.2f hi $%-9.2f trend %c                      |\n"
        "+============================================================+\n",
        symbol, range, count, last, lo, hi, trend);
    for (int r = 0; r < CHART_H; r++) {
        pos += snprintf(body + pos, sizeof(body) - pos, "|  %s  |\n", rows[r]);
    }
    pos += snprintf(body + pos, sizeof(body) - pos,
        "+============================================================+\n"
        "|  data: data/research/%s.csv (daily closes, offline-          |\n"
        "|        backfilled when empty)                                |\n"
        "+============================================================+\n",
        symbol);
    if (count > 0) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            "|  %s .. %s                                      |\n", dates[0], dates[count - 1]);
    }
    snprintf(view, view_sz, "%s", body);
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

    if (strcmp(layout_name, "chart.chtpm") == 0) {
        char view[8192];
        render_chart(view, sizeof(view));
        char view_path[PATH_BUF];
        snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
        write_file(view_path, view);
    }

    ping_chtpm_render_marker(project_root);

    return 0;
}
