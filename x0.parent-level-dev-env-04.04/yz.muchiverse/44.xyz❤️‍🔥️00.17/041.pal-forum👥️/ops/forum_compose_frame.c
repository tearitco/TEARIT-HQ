/* forum_compose_frame - renders whichever pal-forum screen is current
 * into pieces/apps/player_app/view.txt, modeled directly on
 * pal-chain's own chain_compose_frame.c. ${piece_methods} renders the
 * current screen's own numbered METHOD buttons - this op never draws
 * that menu itself, only the surrounding chrome + live data.
 *
 * Writes ONLY view.txt, never pieces/display/current_frame.txt
 * directly - chtpm_parser_pal.c's own compose_frame() is the sole
 * writer of that file (XYZOS-STANDARDS sec. 20 - a direct second writer
 * here would race it and flicker, confirmed live in pal-chain). Pings
 * pieces/display/frame_changed.txt (the real render-trigger marker,
 * not state_changed.txt - sec. 20's own account of that exact bug).
 *
 * Self-contained, no shared headers.
 * Usage: forum_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 2048
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
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void get_current_piece_id(char *out, size_t out_sz) {
    snprintf(out, out_sz, "login");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
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

static void ping_render_marker(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void session_user_id(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/session.txt", project_root);
    read_kv_str(path, "current_user_id", out, out_sz);
}

/* Shows the last `max_lines` lines of a raw append-only log file (a
 * wall/feed_cache/dm thread), newest first - reads the whole file since
 * these logs are v1-scale, not indexed. */
static void show_recent_lines(const char *path, int max_lines, void (*format_row)(const char *raw)) {
    FILE *f = fopen(path, "r");
    if (!f) { line("  (none yet)"); return; }
    char lines[512][MAX_LINE];
    int n = 0;
    char buf[MAX_LINE];
    while (n < 512 && fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0]) snprintf(lines[n++], MAX_LINE, "%s", buf);
    }
    fclose(f);
    if (n == 0) { line("  (none yet)"); return; }
    int shown = 0;
    for (int i = n - 1; i >= 0 && shown < max_lines; i--, shown++) format_row(lines[i]);
}

static void format_post_row(const char *raw) {
    /* POST|<post_id>|<author>|<timestamp>|<text>|<image_id>[|<like_count>] */
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", raw);
    char *fields[8]; int nf = 0; char *cursor = copy;
    for (; nf < 6; nf++) {
        char *p = strchr(cursor, '|');
        if (!p) break;
        *p = '\0';
        fields[nf] = cursor;
        cursor = p + 1;
    }
    if (nf < 5) return;
    char rowbuf[BOX_W + 1];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  @%s: %s", fields[2], fields[4]);
    line(rowbuf);
    if (nf >= 6) {
        snprintf(rowbuf, sizeof(rowbuf), "    id=%s  likes=%s", fields[1], cursor);
    } else {
        snprintf(rowbuf, sizeof(rowbuf), "    id=%s", fields[1]);
    }
#pragma GCC diagnostic pop
    line(rowbuf);
}

static void format_dm_row(const char *raw) {
    /* DM|<from>|<to>|<timestamp>|<text> */
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", raw);
    char *fields[6]; int nf = 0; char *cursor = copy;
    for (; nf < 4; nf++) {
        char *p = strchr(cursor, '|');
        if (!p) break;
        *p = '\0';
        fields[nf] = cursor;
        cursor = p + 1;
    }
    if (nf < 4) return;
    char rowbuf[BOX_W + 1];
    snprintf(rowbuf, sizeof(rowbuf), "  %s: %s", fields[1], cursor);
    line(rowbuf);
}

static void format_follow_row(const char *raw) {
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", raw);
    char *fields[6]; int nf = 0; char *cursor = copy;
    for (; nf < 3; nf++) {
        char *p = strchr(cursor, '|');
        if (!p) break;
        *p = '\0';
        fields[nf] = cursor;
        cursor = p + 1;
    }
    if (nf < 3) return;
    char rowbuf[BOX_W + 1];
    snprintf(rowbuf, sizeof(rowbuf), "  %s %s", fields[0], fields[2]);
    line(rowbuf);
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/forum_menu_state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_piece[128];
    get_current_piece_id(active_piece, sizeof(active_piece));

    char user_id[128];
    session_user_id(user_id, sizeof(user_id));

    char rowbuf[BOX_W + 1];
    border();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  P A L - F O R U M   [%s]", active_piece);
#pragma GCC diagnostic pop
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "login") == 0) {
        line("  Welcome to pal-forum.");
        line("  Create an account, or log in with an existing user ID.");
    } else if (!user_id[0] && strcmp(active_piece, "login") != 0) {
        line("  Not logged in. Use Back to Login.");
    } else if (strcmp(active_piece, "home") == 0) {
        snprintf(rowbuf, sizeof(rowbuf), "  Logged in as: %s", user_id);
        line(rowbuf);
        blank();
        line("  Your recent posts:");
        char wall_path[PATH_BUF];
        snprintf(wall_path, sizeof(wall_path), "%s/users/%s/wall.txt", project_root, user_id);
        show_recent_lines(wall_path, 5, format_post_row);
    } else if (strcmp(active_piece, "feed") == 0) {
        line("  Global feed (from users you follow):");
        char feed_path[PATH_BUF];
        snprintf(feed_path, sizeof(feed_path), "%s/users/%s/feed_cache.txt", project_root, user_id);
        show_recent_lines(feed_path, 8, format_post_row);
    } else if (strcmp(active_piece, "post_compose") == 0) {
        line("  Type your post text below, then click Post.");
    } else if (strcmp(active_piece, "follow") == 0) {
        line("  Currently following (most recent action shown):");
        char following_path[PATH_BUF];
        snprintf(following_path, sizeof(following_path), "%s/users/%s/following.txt", project_root, user_id);
        show_recent_lines(following_path, 6, format_follow_row);
    } else if (strcmp(active_piece, "dms") == 0) {
        char dm_to[128];
        char gui_state_path[PATH_BUF];
        snprintf(gui_state_path, sizeof(gui_state_path), "%s/projects/pal-forum/manager/gui_state.txt", project_root);
        read_kv_str(gui_state_path, "dm_to_input", dm_to, sizeof(dm_to));
        if (dm_to[0]) {
            snprintf(rowbuf, sizeof(rowbuf), "  Thread with %s:", dm_to);
            line(rowbuf);
            char thread_path[PATH_BUF];
            snprintf(thread_path, sizeof(thread_path), "%s/users/%s/dms/%s.txt", project_root, user_id, dm_to);
            show_recent_lines(thread_path, 6, format_dm_row);
        } else {
            line("  Enter a recipient user ID to view/start a thread.");
        }
    }
    blank();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  > %s", last_message[0] ? last_message : "");
#pragma GCC diagnostic pop
    line(rowbuf);
    line("  (type digit(s), Enter to select, q to quit)");
    border();

    fclose(g_view_out); g_view_out = NULL;
    ping_render_marker();
    return 0;
}
