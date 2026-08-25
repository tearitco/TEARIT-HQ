/* setup_menu_input - the Match Setup WIDGIT's own METHOD-table-driven
 * action dispatch. Modeled directly on @.apps/civ-txt/ops/civ_menu_input.c
 * (the house's proven, live-verified numeric/arrow-key index-navigation
 * standard for ${piece_methods} menus):
 *
 *   - The chtpm engine renders the piece.pdl METHOD rows as a focusable
 *     list with [ ]/[>] markers and owns ALL focus navigation (arrow
 *     keys + digit-jump + Enter-to-execute). Rows become
 *     onClick="KEY:<idx>"; Enter dispatches via the engine's
 *     send_command -> inject_raw_key path into
 *     pieces/apps/player_app/interact_relay.txt, where this op's own
 *     PAL loop (pal/main_loop_chtpm.pal) reads it with read_history.
 *   - This op maps the relayed key back to a METHOD row using the SAME
 *     resolved_item formula civ_menu_input uses, and executes the row's
 *     command. It never renders the list or tracks focus.
 *
 * Widget <-> host: executed actions mutate pieces/system/setup_state.txt
 * locally AND enqueue the matching command into the host's cmd-bus inbox
 * (ops/setup_enqueue_cmd). Host-side drainer: ops/tsc_setup.
 *
 * Self-contained, no shared headers.
 * Usage: setup_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[256];
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
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str_local(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
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

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static int load_menu_items(const char *root, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/setup/pieces/setup/piece.pdl", root);
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

static void bump_screen_changed(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/setup_screen_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void cycle_mode(char *mode, size_t sz) {
    if (strcmp(mode, "HvH") == 0) snprintf(mode, sz, "HvC");
    else if (strcmp(mode, "HvC") == 0) snprintf(mode, sz, "CvC");
    else snprintf(mode, sz, "HvH");
}

static void cycle_name(char *name, size_t sz) {
    if (strcmp(name, "Player1") == 0) snprintf(name, sz, "Player2");
    else if (strcmp(name, "Player2") == 0) snprintf(name, sz, "Challenger");
    else snprintf(name, sz, "Player1");
}

static void enqueue(const char *root, const char *cmd, const char *arg) {
    char wdir[PATH_BUF];
    snprintf(wdir, sizeof(wdir), "%s/pieces/system", root);
    char call[PATH_BUF * 2];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (arg[0]) {
        snprintf(call, sizeof(call), "PRISC_PROJECT_ROOT='%s' %s/ops/+x/setup_enqueue_cmd.+x '%s' %s '%s'",
                 root, root, wdir, cmd, arg);
    } else {
        snprintf(call, sizeof(call), "PRISC_PROJECT_ROOT='%s' %s/ops/+x/setup_enqueue_cmd.+x '%s' %s",
                 root, root, wdir, cmd);
    }
#pragma GCC diagnostic pop
    FILE *p = popen(call, "r");
    if (p) pclose(p);
}

/* The widget's own focus.txt points at the HOST session root. In PvP
 * playing mode the host config (symlinked into the host session) is the
 * shared duel state: mode=PvP and game_state=playing means the setup
 * widget's key map flips from menu-edit to real move keys. */
static int pvp_playing(const char *root, char *host_session, size_t host_sz) {
    host_session[0] = '\0';
    char focus[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(focus, sizeof(focus), "%s/pieces/system/focus.txt", root);
#pragma GCC diagnostic pop
    read_kv_str_local(focus, "session_root", host_session, host_sz);
    if (!host_session[0]) return 0;

    char config[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(config, sizeof(config), "%s/pieces/system/config.txt", host_session);
#pragma GCC diagnostic pop
    char mode[32] = "";
    char state[32] = "";
    read_kv_str_local(config, "mode", mode, sizeof(mode));
    read_kv_str_local(config, "game_state", state, sizeof(state));
    return strcmp(mode, "PvP") == 0 && strcmp(state, "playing") == 0;
}

static const char *move_for_key(int key) {
    switch (key) {
        case '1': return "strike";
        case '2': return "heavy";
        case '3': return "heal";
        case '4': return "block";
        case '5': return "resign";
        default:  return NULL;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/setup_state.txt", project_root);

    int key = atoi(argv[1]);

    char mode[32] = "HvH";
    char name[128] = "Player1";
    read_kv_str_local(state_path, "mode", mode, sizeof(mode));
    read_kv_str_local(state_path, "name", name, sizeof(name));
    int rating = read_kv_int(state_path, "rating", 1000);

    if (key == 0) {
        /* Idle pre-sync: make sure state exists with defaults so the
         * first compose renders sane values even before any keypress.
         * Hardcoded here (NOT the read-back values above): read_kv_str_
         * local clobbers its out-buffer to "" when the file is absent,
         * so re-writing the read-back defaults would seed empty values
         * the first time. Same default-seed button.sh already writes. */
        if (access(state_path, F_OK) != 0) {
            write_kv(state_path, "mode", "HvH");
            write_kv_int(state_path, "rating", 1000);
            write_kv(state_path, "name", "Player1");
            write_kv(state_path, "last_message", "Set up the match, then START.");
        }
        return 0;
    }

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, items, MAX_MENU_ITEMS);

    char host_session[PATH_BUF];
    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    /* PvP PLAYING MODE: the widget's key map becomes the duel controls.
     * '1'-'4' enqueue MOVE:<strike|heavy|heal|block> into the HOST cmd
     * bus (tsc_setup applies + broadcasts on the wire); '5' resigns.
     * NOTE (PITFALL, live-caught by the PvP harness): plain digit keys
     * can never reach this op through the widget's REAL input chain -
     * chtpm_parser_pal consumes digits for nav-jump and only relays
     * Enter-on-METHOD (send_command KEY:n) to interact_relay.txt. The
     * Play: STRIKE/HEAVY/HEAL/BLOCK METHOD rows below are the REAL
     * input path for moves; quick-keys stay for GL users. So a key that
     * is not a quick-move must FALL THROUGH to the menu dispatch. */
    if (pvp_playing(project_root, host_session, sizeof(host_session))) {
        const char *mv = move_for_key(key);
        if (mv) {
            enqueue(project_root, "MOVE", mv);
            if (strcmp(mv, "resign") == 0) {
                enqueue(project_root, "RESIGN", "");
                snprintf(message, sizeof(message), "RESIGN sent to host.");
            } else {
                snprintf(message, sizeof(message), "MOVE %s sent to host.", mv);
            }
            write_kv(state_path, "last_message", message);
            bump_screen_changed(project_root);
            return 0;
        }
    }

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "SET_MODE") == 0) {
            cycle_mode(mode, sizeof(mode));
            write_kv(state_path, "mode", mode);
            enqueue(project_root, "MATCH", mode);
            snprintf(message, sizeof(message), "Mode set to %s (sent to host).", mode);
        } else if (strcmp(cmd, "SET_ELO") == 0) {
            rating += 100;
            if (rating > 3000) rating = 400;
            write_kv_int(state_path, "rating", rating);
            char rbuf[16];
            snprintf(rbuf, sizeof(rbuf), "%d", rating);
            enqueue(project_root, "RATING", rbuf);
            snprintf(message, sizeof(message), "Opponent ELO set to %d (sent to host).", rating);
        } else if (strcmp(cmd, "SET_NAME") == 0) {
            cycle_name(name, sizeof(name));
            write_kv(state_path, "name", name);
            enqueue(project_root, "PLAYER", name);
            snprintf(message, sizeof(message), "Name set to %s (sent to host).", name);
        } else if (strcmp(cmd, "START_MATCH") == 0) {
            enqueue(project_root, "START", "");
            snprintf(message, sizeof(message), "START sent to host - waiting for match to begin...");
        } else if (strcmp(cmd, "PVP_CHALLENGE") == 0) {
            enqueue(project_root, "CHALLENGE", "");
            snprintf(message, sizeof(message), "PVP CHALLENGE sent to host.");
        } else if (strcmp(cmd, "PVP_ACCEPT") == 0) {
            enqueue(project_root, "ACCEPT", "");
            snprintf(message, sizeof(message), "PVP ACCEPT sent to host.");
        } else if (strcmp(cmd, "MOVE_STRIKE") == 0) {
            enqueue(project_root, "MOVE", "strike");
            snprintf(message, sizeof(message), "MOVE strike sent to host.");
        } else if (strcmp(cmd, "MOVE_HEAVY") == 0) {
            enqueue(project_root, "MOVE", "heavy");
            snprintf(message, sizeof(message), "MOVE heavy sent to host.");
        } else if (strcmp(cmd, "MOVE_HEAL") == 0) {
            enqueue(project_root, "MOVE", "heal");
            snprintf(message, sizeof(message), "MOVE heal sent to host.");
        } else if (strcmp(cmd, "MOVE_BLOCK") == 0) {
            enqueue(project_root, "MOVE", "block");
            snprintf(message, sizeof(message), "MOVE block sent to host.");
        }
    }

    write_kv(state_path, "last_message", message);
    bump_screen_changed(project_root);
    return 0;
}
