/* chat_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever pal-chat-irc screen is currently showing, modeled directly
 * on pal-forum's own forum_menu_input.c (PAL-FORUM-STANDARD.txt's own
 * proven ${piece_methods} + real <cli_io> + href pattern - reused, not
 * reinvented, per PAL-REFACTOR-STANDARD.txt's own preference order).
 *
 * Screen switching between login/room_list/room is real chtpm
 * <button href="...">, never this op's job. JOIN_ROOM is the one
 * exception worth a comment: it can't itself be a href (it needs to
 * read the typed room_name_input first and mkdir -p the room) - see
 * PAL-CHAT-IRC-STANDARD.txt sec. 3. It writes current_room to
 * pieces/system/chat_menu_state.txt and sets room_selected=true so
 * room_list.chtpm's own real href "Enter Room" button (visibility=
 * "${room_selected}") becomes available - same visibility-gate
 * mechanism as pal-forum's own login.chtpm "Continue to Home" button.
 *
 * Self-contained, no shared headers.
 * Usage: chat_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

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

static int valid_room_name(const char *id) {
    if (!id[0]) return 0;
    for (const char *p = id; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    }
    return 1;
}

static int load_menu_items(const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/pal-chat-irc/pieces/%s/piece.pdl", project_root, piece_id);
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

static void session_user_id(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/session.txt", project_root);
    read_kv_str_local(path, "current_user_id", out, out_sz);
}

/* current_room lives in the SAME file as last_message
 * (pieces/system/chat_menu_state.txt) - not a separate chat_state.txt
 * - so JOIN_ROOM's own write_kv(state_path, "current_room", room)
 * below and this read always agree on one file. */
static void current_room(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/chat_menu_state.txt", project_root);
    read_kv_str_local(path, "current_room", out, out_sz);
}

#define MAX_ROOMS 64
/* REAL BUG, LIVE-CAUGHT (2026-07-20, segfault): qsort over a FIXED-
 * ROW-SIZE 2D array (char names[MAX_ROOMS][128], row size 128 passed
 * as qsort's own `size` arg) hands the comparator a pointer to the
 * ROW ITSELF (i.e. already a `const char*`) - NOT a pointer-to-a-
 * pointer-to-char the way an array of `char*` would. The original
 * version here did `strcmp(*(const char**)a, ...)`, dereferencing one
 * level too many - undefined behavior, crashed inside qsort's own
 * msort_with_tmp on the very first real call. */
static int qsort_strcmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/* List every room this node currently knows about (a rooms/<name>/
 * subdirectory - PAL-CHAT-IRC-STANDARD.txt sec. 0's own "a room is
 * created implicitly" rule), alphabetically sorted so the SAME order
 * is reproducible between write_room_choices() (labels) and
 * resolve_room_by_index() (click dispatch) - two separate process
 * invocations, so a raw readdir() order (filesystem-dependent, not
 * guaranteed stable) would risk the two disagreeing if a room got
 * added in between; alphabetical sort is deterministic given the same
 * directory contents. Returns the count, fills names[]. */
static int list_rooms(char names[][128], int max_rooms) {
    char rooms_root[PATH_BUF];
    snprintf(rooms_root, sizeof(rooms_root), "%s/rooms", project_root);
    DIR *d = opendir(rooms_root);
    if (!d) return 0;
    int n = 0;
    struct dirent *ent;
    while (n < max_rooms && (ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[PATH_BUF];
        snprintf(full, sizeof(full), "%s/%s", rooms_root, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(names[n], 128, "%s", ent->d_name);
#pragma GCC diagnostic pop
        n++;
    }
    closedir(d);
    qsort(names, n, 128, qsort_strcmp);
    return n;
}

/* DIRECT USER REQUEST, 2026-07-20: "occupied rooms should show as a
 * choice to join" - real chtpm buttons, not a hardcoded/hand-drawn
 * list (xyzos-standards.txt sec. 12/13's own "must stay ${piece_methods}-
 * driven, never hardcode buttons" rule). Since these buttons are NOT
 * piece.pdl METHOD rows (the room count is dynamic, piece.pdl is a
 * static file), this follows the OTHER established real convention
 * for dynamic button lists in this family (xyzos-standards.txt sec. 13,
 * the groq-ollama/mutaclsym precedent): build real
 * `<button onClick="KEY:n">` markup directly and write it into
 * gui_state.txt as a named var (${room_choices}, referenced directly
 * in room_list.chtpm) - chtpm's own substitute_vars() splices it into
 * the token stream before parsing, so these become genuinely
 * real, focusable, arrow/digit-navigable elements, not a text overlay.
 *
 * onClick="KEY:<100+index>" (not small sequential 1/2/3...) so these
 * can NEVER collide with piece.pdl-driven method numbers (JOIN_ROOM is
 * always KEY:1) - see resolve_room_by_index()'s own dispatch, keyed
 * off the raw injected value, completely independent of chtpm's own
 * on-screen "N." display numbering (which is just a rendering-order
 * counter, unrelated to the onClick string - confirmed by direct read
 * of chtpm_parser_pal.c's own render_element()). */
static void write_room_choices(void) {
    char names[MAX_ROOMS][128];
    int n = list_rooms(names, MAX_ROOMS);

    char buf[4096] = "";
    for (int i = 0; i < n; i++) {
        char btn[256];
        snprintf(btn, sizeof(btn), "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>", names[i], 100 + i);
        if (strlen(buf) + strlen(btn) < sizeof(buf) - 1) strcat(buf, btn);
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/pal-chat-irc/manager/gui_state.txt", project_root);
    write_kv(path, "room_choices", buf);
}

/* Companion to write_room_choices() - resolves a KEY:100+index click
 * back to a room name using the SAME list_rooms() call (same
 * alphabetical order). Returns 1 and fills out on success. */
static int resolve_room_by_index(int idx, char *out, size_t out_sz) {
    char names[MAX_ROOMS][128];
    int n = list_rooms(names, MAX_ROOMS);
    if (idx < 0 || idx >= n) return 0;
    snprintf(out, out_sz, "%s", names[idx]);
    return 1;
}

static void write_chtpm_bridge(const char *piece_id) {
    char logged_in_user[128];
    session_user_id(logged_in_user, sizeof(logged_in_user));
    char room[128];
    current_room(room, sizeof(room));

    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=pal-chat-irc\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fprintf(cf, "logged_in=%s\n", logged_in_user[0] ? "true" : "false");
        fprintf(cf, "room_selected=%s\n", room[0] ? "true" : "false");
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

static void read_gui_state_str(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/pal-chat-irc/manager/gui_state.txt", project_root);
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
    snprintf(path, sizeof(path), "%s/projects/pal-chat-irc/manager/gui_state.txt", project_root);
    write_kv(path, key, "");
}

static void run_capture(const char *cmd, char *message, size_t message_sz) {
    char full[PATH_BUF * 2];
    snprintf(full, sizeof(full), "cd '%s' && %s 2>&1", project_root, cmd);
    FILE *p = popen(full, "r");
    if (!p) { snprintf(message, message_sz, "Action failed to start."); return; }
    if (!fgets(message, message_sz, p)) snprintf(message, message_sz, "Ran: %s", cmd);
    else message[strcspn(message, "\n")] = '\0';
    pclose(p);
}

/* ENTER-SENDS-CHAT (direct user instruction, 2026-07-20: "when i hit
 * enter in chat it should send the chat. we dont need that second
 * option to send chat... that feature exists in the original chtpm
 * cli-io... steal it from there"). Confirmed by direct read of the
 * real chtpm_parser.c: on Enter inside an active cli_io, it ALREADY
 * saves the buffer to gui_state, clears it, and calls
 * inject_raw_key(13) with the comment "Trigger the Send action by
 * dispatching KEY:13" - that raw 13 lands right back here as this
 * op's own `key` argument, via pieces/apps/player_app/
 * interact_relay.txt (the same file every numbered METHOD click
 * already arrives through - confirmed by direct trace of
 * inject_raw_key()'s own PRIORITY 1 branch, which uses the
 * <interact src="..."> path every one of our own layouts already
 * declares). The mechanism was already there; wiring it up here (not
 * in chtpm_parser_pal.c) is the correct side of the boundary - the
 * parser already does its job, this project just needs to answer.
 *
 * Called DIRECTLY on key==13 (a keyboard shortcut, same category as
 * this family's own hardcoded 'q'-quits-everywhere convention), NOT
 * through the piece.pdl/${piece_methods} numbered-item table -
 * room/piece.pdl has NO "Post Message" METHOD row at all anymore
 * (direct user correction: "just remove it from pdl" - the numbered,
 * CLICKABLE menu must stay piece.pdl-driven per xyzos-standards.txt
 * sec. 12, but a keyboard shortcut that isn't part of that numbered
 * menu is a different thing, same as 'q'). */
static void handle_post_message(char *message, size_t message_sz) {
    char user_id[128], room[128], text[MAX_LINE];
    session_user_id(user_id, sizeof(user_id));
    current_room(room, sizeof(room));
    read_gui_state_str("message_text_input", text, sizeof(text));
    if (!user_id[0]) {
        snprintf(message, message_sz, "Not logged in.");
    } else if (!room[0]) {
        snprintf(message, message_sz, "No room selected.");
    } else if (!text[0]) {
        snprintf(message, message_sz, "Type a message and press Enter to send.");
    } else {
        char cmdbuf[PATH_BUF * 2];
        snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/chat_post_message.+x '%s' '%s' '%s'", room, user_id, text);
        run_capture(cmdbuf, message, message_sz);
        clear_gui_state_str("message_text_input");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/chat_menu_state.txt", project_root);

    int key = atoi(argv[1]);

    if (key == 0) {
        char derived[128];
        get_current_piece_id(derived, sizeof(derived));
        /* Refresh the room list EVERY catch-up tick, not just on a
         * real screen change (unlike the early-return below) - a room
         * can appear via a network peer while this player idles on
         * this exact screen, same "keep it live" reasoning as
         * PAL-NET-STANDARD.txt sec. 6's own trigger_render() fix. */
        if (strcmp(derived, "room_list") == 0) write_room_choices();

        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/chat_screen_changed.txt", project_root);
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

    /* key==13 on the room screen is the keyboard-shortcut send path -
     * see handle_post_message()'s own header comment. Handled BEFORE
     * (and instead of) the piece.pdl-driven items[] dispatch below,
     * since room/piece.pdl has no METHOD row for this at all. */
    if (key == 13 && strcmp(active_piece, "room") == 0) {
        handle_post_message(message, sizeof(message));
    } else if (key >= 100 && key < 1000 && strcmp(active_piece, "room_list") == 0) {
        /* A ${room_choices} click (write_room_choices()'s own
         * onClick="KEY:100+index" scheme) - resolve which room, same
         * "ready to enter" bookkeeping as JOIN_ROOM below, just
         * skipping the typed-name/mkdir step since the room already
         * exists. */
        char room[128];
        if (resolve_room_by_index(key - 100, room, sizeof(room))) {
            write_kv(state_path, "current_room", room);
            snprintf(message, sizeof(message), "Room '%s' ready - click Enter Room.", room);
        } else {
            snprintf(message, sizeof(message), "That room is no longer available - refreshing list.");
        }
    } else if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "SIGNUP") == 0) {
            char user_id[128], display_name[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            read_gui_state_str("display_name_input", display_name, sizeof(display_name));
            if (!user_id[0] || !display_name[0]) {
                snprintf(message, sizeof(message), "Enter a user ID and display name, then click Create Account.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/chat_create_user.+x '%s' '%s'", user_id, display_name);
                run_capture(cmdbuf, message, sizeof(message));
                if (strstr(message, "created")) {
                    char switchbuf[PATH_BUF];
                    snprintf(switchbuf, sizeof(switchbuf), "./ops/+x/chat_switch_user.+x '%s'", user_id);
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
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/chat_switch_user.+x '%s'", user_id);
                run_capture(cmdbuf, message, sizeof(message));
            }
        } else if (strcmp(cmd, "JOIN_ROOM") == 0) {
            char room[128];
            read_gui_state_str("room_name_input", room, sizeof(room));
            if (!room[0]) {
                snprintf(message, sizeof(message), "Enter a room name, then click Join / Create Room.");
            } else if (!valid_room_name(room)) {
                snprintf(message, sizeof(message), "Room names: letters, digits, _ and - only.");
            } else {
                char rooms_root[PATH_BUF];
                snprintf(rooms_root, sizeof(rooms_root), "%s/rooms", project_root);
                mkdir(rooms_root, 0755);
                char room_dir[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(room_dir, sizeof(room_dir), "%s/rooms/%s", project_root, room);
#pragma GCC diagnostic pop
                mkdir(room_dir, 0755);
                char messages_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(messages_path, sizeof(messages_path), "%s/messages.txt", room_dir);
#pragma GCC diagnostic pop
                FILE *tf = fopen(messages_path, "a"); if (tf) fclose(tf);
                write_kv(state_path, "current_room", room);
                snprintf(message, sizeof(message), "Room '%s' ready - click Enter Room.", room);
            }
        } else {
            snprintf(message, sizeof(message), "Unknown command.");
        }
    }

    write_kv(state_path, "last_message", message);
    write_chtpm_bridge(active_piece);
    return 0;
}
