/* forum_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever pal-forum screen is currently showing, modeled directly on
 * pal-chain's own chain_menu_input.c (PAL-CHAIN-STANDARD.txt's own
 * proven, live-tested href + ${piece_methods} + real <cli_io> pattern
 * - see that file's own header comment; reused here rather than
 * reinvented, per PAL-REFACTOR-STANDARD.txt sec. 3's own preference
 * order).
 *
 * Screen switching is a real chtpm <button href="...">, never this
 * op's job; "which screen is current" is derived fresh every call from
 * pieces/display/current_layout.txt.
 *
 * Real <cli_io target_id="..."> fields (login/post_compose/follow/
 * dms.chtpm) persist typed values to projects/pal-forum/manager/
 * gui_state.txt (chtpm_parser_pal.c's own save_cli_io_gui_state()) -
 * read back here via read_gui_state_str(), exactly like pal-chain's
 * own LOGIN/SIGNUP/SEND commands.
 *
 * Self-contained, no shared headers.
 * Usage: forum_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[512];
} MenuItem;

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

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[32][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static int load_menu_items(const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/pal-forum/pieces/%s/piece.pdl", project_root, piece_id);
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

static void write_chtpm_bridge(const char *piece_id) {
    /* logged_in - real chtpm var, substituted into login.chtpm's own
     * `visibility="${logged_in}"` on its "Continue to Home" button -
     * direct user instruction: "i want the login screen to be the
     * only thing on the screen" until an account is actually logged
     * in, matching the shape a future external user-pal login screen
     * would need to slot into cleanly (see USER-PAL-STANDARD.txt).
     * Read net/session.txt directly here (not via session_user_id() -
     * that helper is defined later in this file; inlined rather than
     * reordered/forward-declared for a one-line read). */
    char session_path[PATH_BUF];
    snprintf(session_path, sizeof(session_path), "%s/net/session.txt", project_root);
    char logged_in_user[128];
    read_kv_str_local(session_path, "current_user_id", logged_in_user, sizeof(logged_in_user));

    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=pal-forum\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fprintf(cf, "logged_in=%s\n", logged_in_user[0] ? "true" : "false");
        fclose(cf);
    }
}

static void get_current_piece_id(char *out, size_t out_sz) {
    snprintf(out, out_sz, "login");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
#pragma GCC diagnostic pop
    }
    fclose(f);
}

static void session_user_id(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/session.txt", project_root);
    read_kv_str_local(path, "current_user_id", out, out_sz);
}

static void read_gui_state_str(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/pal-forum/manager/gui_state.txt", project_root);
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
        }
    }
    fclose(f);
}

static void clear_gui_state_str(const char *key) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/pal-forum/manager/gui_state.txt", project_root);
    write_kv(path, key, "");
}

/* Shells a command, captures its first stdout line as the message -
 * matches chain_menu_input.c's own RUN: capture shape exactly. */
static void run_capture(const char *cmd, char *message, size_t message_sz) {
    char full[PATH_BUF * 2];
    snprintf(full, sizeof(full), "cd '%s' && %s 2>&1", project_root, cmd);
    FILE *p = popen(full, "r");
    if (!p) { snprintf(message, message_sz, "Action failed to start."); return; }
    if (!fgets(message, message_sz, p)) snprintf(message, message_sz, "Ran: %s", cmd);
    else message[strcspn(message, "\n")] = '\0';
    pclose(p);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/forum_menu_state.txt", project_root);

    int key = atoi(argv[1]);

    if (key == 0) {
        char derived[128];
        get_current_piece_id(derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/forum_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_piece[128];
    get_current_piece_id(active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(active_piece, items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "SIGNUP") == 0) {
            char user_id[128], display_name[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            read_gui_state_str("display_name_input", display_name, sizeof(display_name));
            if (!user_id[0] || !display_name[0]) {
                snprintf(message, sizeof(message), "Enter a user ID and display name, then click Create Account.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/forum_create_user.+x '%s' '%s'", user_id, display_name);
                run_capture(cmdbuf, message, sizeof(message));
                if (strstr(message, "created")) {
                    char switchbuf[PATH_BUF];
                    snprintf(switchbuf, sizeof(switchbuf), "./ops/+x/forum_switch_user.+x '%s'", user_id);
                    char discard[MAX_LINE];
                    run_capture(switchbuf, discard, sizeof(discard));
                    snprintf(message, sizeof(message), "Account created - logged in as %s. Click Continue.", user_id);
                }
            }
        } else if (strcmp(cmd, "LOGIN") == 0) {
            char user_id[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Enter your user ID, then click Log In.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/forum_switch_user.+x '%s'", user_id);
                run_capture(cmdbuf, message, sizeof(message));
            }
        } else if (strcmp(cmd, "LOGOUT") == 0) {
            char session_path[PATH_BUF];
            snprintf(session_path, sizeof(session_path), "%s/net/session.txt", project_root);
            FILE *sf = fopen(session_path, "w");
            if (sf) fclose(sf);
            snprintf(message, sizeof(message), "Logged out - click Back to Login.");
        } else if (strcmp(cmd, "POST") == 0) {
            char user_id[128], text[MAX_LINE];
            session_user_id(user_id, sizeof(user_id));
            read_gui_state_str("post_text_input", text, sizeof(text));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else if (!text[0]) {
                snprintf(message, sizeof(message), "Type something to post, then click Post.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/forum_post.+x '%s' '%s'", user_id, text);
                run_capture(cmdbuf, message, sizeof(message));
                clear_gui_state_str("post_text_input");
            }
        } else if (strcmp(cmd, "REFRESH_WALL") == 0) {
            snprintf(message, sizeof(message), "Wall refreshed.");
        } else if (strcmp(cmd, "REFRESH_FEED") == 0) {
            char user_id[128];
            session_user_id(user_id, sizeof(user_id));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/forum_compute_feed.+x '%s'", user_id);
                run_capture(cmdbuf, message, sizeof(message));
            }
        } else if (strcmp(cmd, "LIKE") == 0 || strcmp(cmd, "RETWEET") == 0) {
            char user_id[128], post_id[128];
            session_user_id(user_id, sizeof(user_id));
            read_gui_state_str("post_id_input", post_id, sizeof(post_id));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else if (!post_id[0]) {
                snprintf(message, sizeof(message), "Type a post ID from the feed below, then click Like/Retweet.");
            } else {
                const char *op = (strcmp(cmd, "LIKE") == 0) ? "forum_like" : "forum_retweet";
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/%s.+x '%s' '%s'", op, user_id, post_id);
                run_capture(cmdbuf, message, sizeof(message));
            }
        } else if (strcmp(cmd, "FOLLOW") == 0 || strcmp(cmd, "UNFOLLOW") == 0) {
            char user_id[128], target_id[128];
            session_user_id(user_id, sizeof(user_id));
            read_gui_state_str("target_user_input", target_id, sizeof(target_id));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else if (!target_id[0]) {
                snprintf(message, sizeof(message), "Enter a user ID, then click Follow/Unfollow.");
            } else {
                const char *mode = (strcmp(cmd, "FOLLOW") == 0) ? "follow" : "unfollow";
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/forum_follow.+x '%s' '%s' %s", user_id, target_id, mode);
                run_capture(cmdbuf, message, sizeof(message));
            }
        } else if (strcmp(cmd, "SEND_DM") == 0) {
            char user_id[128], to_id[128], text[MAX_LINE];
            session_user_id(user_id, sizeof(user_id));
            read_gui_state_str("dm_to_input", to_id, sizeof(to_id));
            read_gui_state_str("dm_text_input", text, sizeof(text));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else if (!to_id[0] || !text[0]) {
                snprintf(message, sizeof(message), "Enter a recipient and message, then click Send DM.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/forum_dm.+x '%s' '%s' '%s'", user_id, to_id, text);
                run_capture(cmdbuf, message, sizeof(message));
                clear_gui_state_str("dm_text_input");
            }
        } else if (strcmp(cmd, "STUB") == 0) {
            snprintf(message, sizeof(message), "Not yet available in this build.");
        } else {
            snprintf(message, sizeof(message), "Unknown command.");
        }
    }

    write_kv(state_path, "last_message", message);
    write_chtpm_bridge(active_piece);
    return 0;
}
