/* mybiotech_compose_frame - renders whichever my-biotech screen is
 * current into pieces/apps/player_app/view.txt. Modeled directly on
 * my-chara-txt's own ops/mychara_compose_frame.c (itself modeled on
 * 041.pal-chain's real, proven chain_compose_frame.c) - writes ONLY
 * view.txt, never pieces/display/current_frame.txt directly (ONE
 * WRITER RULE), then bumps frame_changed.txt.
 *
 * Self-contained, no shared headers.
 * Usage: mybiotech_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static int count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char c;
    while ((c = fgetc(f)) != EOF) if (c == '\n') n++;
    fclose(f);
    return n;
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

    char state_path[PATH_BUF], view_path[PATH_BUF], config_path[PATH_BUF], corpus_path[PATH_BUF], status_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-biotech/pieces/mybiotech_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(corpus_path, sizeof(corpus_path), "%s/data/corpus/player.txt", project_root);
    snprintf(status_path, sizeof(status_path), "%s/data/research_status.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    int day = read_kv_int(config_path, "day", 1);
    int max_days = read_kv_int(config_path, "max_days", 10);
    int money = read_kv_int(config_path, "money", 500);
    int corpus_lines = count_lines(corpus_path);

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "  Day %d / %d   Money: %d   Corpus: %d facts", day, max_days, money, corpus_lines);
    line(rowbuf);
    border();
    blank();

    /* Live progress indicator for the background research worker (the
     * async fix - see mybiotech_menu_input.c's own header comment and
     * ops/mybiotech_research_worker.c). running=1 means a real gemma-lan
     * call is in flight right now; show elapsed time so the player
     * knows it's not frozen, not just a static "please wait." */
    int research_running = read_kv_int(status_path, "running", 0);
    if (research_running) {
        char element[128], step[64];
        read_kv_str(status_path, "element", element, sizeof(element));
        read_kv_str(status_path, "step", step, sizeof(step));
        long started_at = (long)read_kv_int(status_path, "updated_at", 0);
        long elapsed = started_at > 0 ? (long)time(NULL) - started_at : 0;
        snprintf(rowbuf, sizeof(rowbuf), "  ⏳ Researching %s... [%s] (%lds elapsed)", element, step[0] ? step : "?", elapsed);
        line(rowbuf);
        blank();
    }

    if (last_message[0]) {
        /* Wrap long gemma responses across multiple lines rather than
         * truncating - a real research result is often longer than
         * BOX_W (60 chars). */
        const char *p = last_message;
        size_t remaining = strlen(p);
        while (remaining > 0) {
            size_t chunk = remaining < (size_t)BOX_W ? remaining : (size_t)BOX_W;
            char buf[BOX_W + 1];
            memcpy(buf, p, chunk);
            buf[chunk] = '\0';
            line(buf);
            p += chunk;
            remaining -= chunk;
        }
    } else if (!research_running) {
        line("  (no research attempted yet)");
    }

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);
    return 0;
}
