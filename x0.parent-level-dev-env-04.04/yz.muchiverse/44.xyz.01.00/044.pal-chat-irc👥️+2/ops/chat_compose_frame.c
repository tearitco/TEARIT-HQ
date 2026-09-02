/* chat_compose_frame - renders whichever pal-chat-irc screen is
 * current into pieces/apps/player_app/view.txt, modeled directly on
 * pal-forum's own forum_compose_frame.c. ${piece_methods} renders the
 * current screen's own numbered METHOD buttons - this op never draws
 * that menu itself, only the surrounding chrome + live data.
 *
 * Writes ONLY view.txt, never pieces/display/current_frame.txt
 * directly (XYZOS-STANDARDS sec. 20 - ONE VISIBLE FRAME WRITER RULE).
 * Pings pieces/display/frame_changed.txt, the real render-trigger
 * marker.
 *
 * Self-contained, no shared headers.
 * Usage: chat_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static void append_frame_history(const char *view_path) {
    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/debug/frame_history.txt", project_root);
    FILE *hf = fopen(history_path, "a");
    if (!hf) return;

    time_t now = time(NULL);
    fprintf(hf, "\n=== FRAME @ %ld ===\n", now);

    FILE *vf = fopen(view_path, "r");
    if (vf) {
        char buf[MAX_LINE];
        while (fgets(buf, sizeof(buf), vf)) {
            fputs(buf, hf);
        }
        fclose(vf);
    }
    fprintf(hf, "---\n");
    fclose(hf);
}

static void session_user_id(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/session.txt", project_root);
    read_kv_str(path, "current_user_id", out, out_sz);
}

static void current_room(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/chat_menu_state.txt", project_root);
    read_kv_str(path, "current_room", out, out_sz);
}

/* Shows the last `max_lines` messages of a room's own log, newest
 * last (real terminal scrollback shape, matches xyzos-standards.txt
 * sec. 25's own default renderer behavior this project inherits
 * unmodified). */
static void show_recent_messages(const char *path, int max_lines) {
    FILE *f = fopen(path, "r");
    if (!f) { line("  (no messages yet)"); return; }
    char lines[512][MAX_LINE];
    int n = 0;
    char buf[MAX_LINE];
    while (n < 512 && fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0]) snprintf(lines[n++], MAX_LINE, "%s", buf);
    }
    fclose(f);
    if (n == 0) { line("  (no messages yet)"); return; }
    int start = n - max_lines;
    if (start < 0) start = 0;
    for (int i = start; i < n; i++) {
        /* MSG|<msg_id>|<room_name>|<user_id>|<timestamp>|<text> */
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", lines[i]);
        char *fields[6]; int nf = 0; char *cursor = copy;
        for (; nf < 5; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 5) continue;
        char rowbuf[BOX_W + 1];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rowbuf, sizeof(rowbuf), "  <%s> %s", fields[3], cursor);
#pragma GCC diagnostic pop
        line(rowbuf);
    }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/chat_menu_state.txt", project_root);
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
    snprintf(rowbuf, sizeof(rowbuf), "  P A L - C H A T - I R C   [%s]", active_piece);
#pragma GCC diagnostic pop
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "login") == 0) {
        line("  Welcome to pal-chat-irc.");
        line("  Create an account, or log in with an existing user ID.");
    } else if (!user_id[0]) {
        line("  Not logged in. Use Back to Login.");
    } else if (strcmp(active_piece, "room_list") == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rowbuf, sizeof(rowbuf), "  Logged in as: %s", user_id);
#pragma GCC diagnostic pop
        line(rowbuf);
        blank();
        line("  Enter a room name to join or create it, then");
        line("  click Enter Room.");
    } else if (strcmp(active_piece, "room") == 0) {
        char room[128];
        current_room(room, sizeof(room));
        if (!room[0]) {
            line("  No room selected. Use Leave Room, then join one.");
        } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(rowbuf, sizeof(rowbuf), "  Room: %s (as %s)", room, user_id);
#pragma GCC diagnostic pop
            line(rowbuf);
            border();
            char rooms_root[PATH_BUF];
            snprintf(rooms_root, sizeof(rooms_root), "%s/rooms/%s/messages.txt", project_root, room);
            show_recent_messages(rooms_root, 12);
        }
    }

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
    append_frame_history(view_path);
    ping_render_marker();
    return 0;
}
