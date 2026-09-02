/* setup_compose_frame - the Match Setup WIDGIT's sole writer of
 * pieces/apps/player_app/view.txt (ONE WRITER RULE - current_frame.txt
 * belongs exclusively to chtpm_parser_pal.c; this op only feeds it the
 * ${game_map} placeholder source, exactly like civ_compose_frame.c).
 *
 * Reads the widget's own setup_state.txt (mode/rating/name/last_message)
 * and the HOST's cmd-bus status.txt (last ack from ops/tsc_setup) via
 * focus.txt, then renders the setup summary box. The engine renders the
 * ${piece_methods} menu list (from piece.pdl) around it.
 *
 * Self-contained, no shared headers.
 * Usage: setup_compose_frame.+x (no args) */
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

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static void read_last_line(const char *path, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    while (fgets(l, sizeof(l), f)) {
        l[strcspn(l, "\r\n")] = '\0';
        if (l[0]) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", l);
#pragma GCC diagnostic pop
        }
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

    char state_path[PATH_BUF], view_path[PATH_BUF], focus_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/setup_state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(focus_path, sizeof(focus_path), "%s/pieces/system/focus.txt", project_root);

    char mode[32] = "HvH";
    char name[128] = "Player1";
    read_kv_str(state_path, "mode", mode, sizeof(mode));
    read_kv_str(state_path, "name", name, sizeof(name));
    int rating = read_kv_int(state_path, "rating", 1000);
    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    /* Host ack via focus.txt's status_path. */
    char host_status[MAX_LINE] = "";
    {
        char st[PATH_BUF] = "";
        read_kv_str(focus_path, "status_path", st, sizeof(st));
        if (st[0]) read_last_line(st, host_status, sizeof(host_status));
    }

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "            M A T C H   S E T U P");
    line(rowbuf);
    border();
    blank();
    snprintf(rowbuf, sizeof(rowbuf), "  Mode:            %s", mode);
    line(rowbuf);
    snprintf(rowbuf, sizeof(rowbuf), "  Opponent ELO:    %d   (difficulty)", rating);
    line(rowbuf);
    snprintf(rowbuf, sizeof(rowbuf), "  Your name:       %s", name);
    line(rowbuf);
    blank();
    snprintf(rowbuf, sizeof(rowbuf), "  Host: %s", "TSC_ELO (True Swords Clash)");
    line(rowbuf);
    if (host_status[0]) {
        snprintf(rowbuf, sizeof(rowbuf), "  Host says: %s", host_status);
        line(rowbuf);
    }
    blank();
    line("  Pick each option below, then START MATCH.");
    line("  Up/Down move, Enter executes. Values apply on START.");
    blank();
    if (last_message[0]) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rowbuf, sizeof(rowbuf), "  %s", last_message);
#pragma GCC diagnostic pop
        line(rowbuf);
        blank();
    }
    border();

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);
    return 0;
}
