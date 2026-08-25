/* civ_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever civ-txt screen is currently showing. Modeled directly on
 * @.apps/my-chara-txt's own ops/mychara_menu_input.c (real, proven,
 * live-verified precedent): screen SWITCHING is a real chtpm <button
 * href="..."> handled entirely by chtpm_parser_pal.c, never this op's
 * job; "which screen is current" is derived fresh every call from
 * pieces/display/current_layout.txt, never separately tracked mutable
 * state.
 *
 * P1 scope, per CIV_TXT_DESIGN.md's own build order: new_game.chtpm
 * (setup options writing config.txt) -> main.chtpm (turn counter, one
 * real END_TURN action). No cities/tech/units/AI civs yet - this is
 * the skeleton proving the real CHTPM nav + piece.pdl dispatch + ledger
 * loop works for THIS project, exactly like my-chara-txt's own P1/P2
 * did before farm/mine were added.
 *
 * civ-txt-specific commands:
 *   SET_VICTORY:<conquest|score_turnlimit|tech_score>
 *   SET_MAP_SIZE:<small|medium>
 *   SET_COMBAT:<abstract|per_unit>
 *   CONFIRM_START - locks in setup, navigates player to main.chtpm
 *                   (note: navigation itself still happens via a real
 *                   <button href> on the CONFIRM_START row's own next
 *                   click, not simulated here - this op only writes
 *                   the config fields the setup screen collected)
 *   END_TURN - advances turn counter, appends ledger entry
 *
 * Self-contained, no shared headers.
 * Usage: civ_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/civ-txt/pieces/%s/piece.pdl", root, piece_id);
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
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=civ-txt\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "new_game");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
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

static void ledger_append(const char *root, int turn, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|player|%s|%s\n", ts, turn, action_type, details);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/civ-txt/pieces/civ_menu/state.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    /* REAL widget->host command delivery (2026-08-03) - see tactics-
     * txt's own ops/tactics_menu_input.c for the full writeup (identical
     * fix, same real inbox mechanism per @.apps/piececraft-xyz/
     * PIECECRAFT_XYZ_DESIGN.md §4a). Drains the FIRST pending line from
     * this project's own real widget inbox and feeds it into the SAME
     * dispatch chain real numbered piece.pdl METHOD commands already
     * use. */
    char inbox_path[PATH_BUF];
    snprintf(inbox_path, sizeof(inbox_path), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    char inbox_cmd_buf[MAX_LINE] = "";
    {
        FILE *ibf = fopen(inbox_path, "r");
        if (ibf) {
            if (fgets(inbox_cmd_buf, sizeof(inbox_cmd_buf), ibf)) {
                inbox_cmd_buf[strcspn(inbox_cmd_buf, "\r\n")] = '\0';
            }
            fclose(ibf);
        }
        if (inbox_cmd_buf[0]) {
            FILE *tf = fopen(inbox_path, "w"); /* consumed - truncate */
            if (tf) fclose(tf);
        }
    }

    int key = atoi(argv[1]);

    if (key == 0 && !inbox_cmd_buf[0]) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/civ_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    /* REVERTED 2026-08-02 (same day, caught own mistake before it did
     * real damage): a "fix" was briefly applied here based on an
     * incomplete earlier finding ("argv[1] is the raw ASCII digit
     * code" - true, but incomplete). Direct check of chtpm_parser_
     * pal.c's own ${piece_methods} button generator
     * (014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c:1115) shows
     * `method_idx = (loader) ? 1 : 2` - EVERY non-loader piece's own
     * numbered METHOD rows start their internal KEY:N index at 2, not
     * 0 or 1. send_command()'s own "KEY:" handler does
     * inject_raw_key('0'+k) for k in [0,9] - so the FIRST method row is
     * really sent as raw key '2' (not '1'), the second as '3', etc.
     * `(key-'0')-1` is the CORRECT compensation for that +2 offset, not
     * a bug: key='2' -> resolved_item=(50-48)-1=1 -> items[0], correct.
     * This formula was validated by this project's own extensive,
     * successful live testing all session (setup screen's 7 numbered
     * options). The "fix" earlier today would have broken every
     * numbered action in this file - reverted before ever being built/
     * tested. See &.widgits/!.HOUSE_STDS.md §A.3 - THAT section itself
     * needs a correction/caveat added, this was a real, if caught in
     * time, mistake generalizing board-viewer's own (different, raw-
     * injection-tested, not KEY:N-routed) situation to this file. */
    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    const char *cmd = NULL;
    if (inbox_cmd_buf[0]) {
        cmd = inbox_cmd_buf;
    } else if (resolved_item >= 1 && resolved_item <= item_count) {
        cmd = items[resolved_item - 1].command;
    }

    if (cmd) {
        int turn = read_kv_int(config_path, "turn", 1);

        if (strncmp(cmd, "SET_VICTORY:", 12) == 0) {
            write_kv(config_path, "victory_condition", cmd + 12);
            snprintf(message, sizeof(message), "Victory condition set: %s", cmd + 12);
        } else if (strncmp(cmd, "SET_MAP_SIZE:", 13) == 0) {
            write_kv(config_path, "map_scale", cmd + 13);
            snprintf(message, sizeof(message), "Map scale set: %s", cmd + 13);
        } else if (strncmp(cmd, "SET_COMBAT:", 11) == 0) {
            write_kv(config_path, "combat_resolution", cmd + 11);
            snprintf(message, sizeof(message), "Combat resolution set: %s", cmd + 11);
        } else if (strcmp(cmd, "CONFIRM_START") == 0) {
            write_kv(config_path, "game_state", "playing");
            snprintf(message, sizeof(message), "Setup confirmed. Game started.");

            /* Real, if simple, board data - generated once here rather
             * than left as an unbacked placeholder. NOT full world-gen
             * (CIV_TXT_DESIGN.md's own P4+ is the real thing) - a
             * seeded terrain fill + one capital marker, enough for the
             * board-viewer widget (P4/P5 of
             * &.widgits/BOARD_WIDGET_PROGRESS.txt) to have real tiles
             * to camera-pan/select over, per direct user priority to
             * test camera controls soon. Terrain glyphs: '.'=plains,
             * '^'=hills, '~'=water, 'f'=forest, 'C'=capital (a real
             * city marker, distinct from any terrain glyph). Selective
             * extrusion (per &.widgits/5-pov-widgit.md §3b) will later
             * key off these same glyphs once 3D raymarch is built. */
            char map_scale_now[32] = "";
            read_kv_str_local(config_path, "map_scale", map_scale_now, sizeof(map_scale_now));
            int grid_n = (strcmp(map_scale_now, "medium") == 0) ? 14 : 10;

            char board_path[PATH_BUF];
            snprintf(board_path, sizeof(board_path), "%s/pieces/system/board.txt", project_root);
            FILE *bf = fopen(board_path, "w");
            if (bf) {
                srand((unsigned)time(NULL));
                int capital_r = grid_n / 2, capital_c = grid_n / 2;
                for (int r = 0; r < grid_n; r++) {
                    for (int c = 0; c < grid_n; c++) {
                        if (r == capital_r && c == capital_c) {
                            fputc('C', bf);
                            continue;
                        }
                        int roll = rand() % 100;
                        char glyph;
                        if (roll < 55) glyph = '.';
                        else if (roll < 70) glyph = 'f';
                        else if (roll < 85) glyph = '^';
                        else glyph = '~';
                        fputc(glyph, bf);
                    }
                    fputc('\n', bf);
                }
                fclose(bf);
            }
        } else if (strcmp(cmd, "END_TURN") == 0) {
            turn += 1;
            write_kv_int(config_path, "turn", turn);
            char details[128];
            snprintf(details, sizeof(details), "turn:%d", turn);
            ledger_append(project_root, turn - 1, "end_turn", details);
            snprintf(message, sizeof(message), "Turn %d began.", turn);
        } else if (strcmp(cmd, "OPEN_BOARD_WIDGET") == 0) {
            /* Spawns the SEPARATE &.widgits/board-viewer/ widget
             * program as its own detached process with its own GL
             * window - civ-txt's own CHTPM screen/session never
             * changes (no href, no screen switch here). Per
             * @.apps/BOARD_WIDGET_ARCHITECTURE.md §3/§7 and direct
             * instruction. Honest about not-yet-built state: P2 of
             * &.widgits/BOARD_WIDGET_PROGRESS.txt (scaffolding the
             * widget project) hasn't happened yet as of this handler's
             * own writing - checks for the widget's real button.sh
             * before trying to launch it, rather than silently
             * failing or pretending success. */
            /* house_root.txt/real_project_root.txt have no key=value
             * shape, just one raw path per line - read_kv_str_local
             * expects a "key=" prefix, so read these directly instead. */
            char house_root_path[PATH_BUF], house_root[PATH_BUF] = "";
            snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
            FILE *hf = fopen(house_root_path, "r");
            if (hf) {
                if (fgets(house_root, sizeof(house_root), hf)) {
                    house_root[strcspn(house_root, "\r\n")] = '\0';
                }
                fclose(hf);
            }

            char real_root_path[PATH_BUF], real_root[PATH_BUF] = "";
            snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", project_root);
            FILE *rf = fopen(real_root_path, "r");
            if (rf) {
                if (fgets(real_root, sizeof(real_root), rf)) {
                    real_root[strcspn(real_root, "\r\n")] = '\0';
                }
                fclose(rf);
            }

            char widget_button[PATH_BUF];
            snprintf(widget_button, sizeof(widget_button), "%s/&.widgits/board-viewer/button.sh", house_root);

            /* REAL FIX 2026-08-03, direct user correction ("all
             * mechanics for the game should be paired with the viewer
             * at app runtime, the way fm-widget is with text-editor-
             * xyz - if civ/tactics don't do this, they did it wrong"):
             * this handler used to ALWAYS spawn a brand new board-
             * viewer process on every single press, even if one was
             * already open - mashing the button repeatedly leaked N
             * redundant GL windows. Same real ledger-discovery
             * mechanism editor_menu_input.c's own find_widget_via_
             * ledger() uses to find file-menu, ported into board-
             * viewer's own ops/ledger_peers.c (see that file's own
             * header comment for the "default" xyzfs fallback that
             * makes it work with no active login). If a live session
             * is found, refocus it directly by overwriting its OWN
             * bv_state.txt (every ops/bv_*.c re-reads that file fresh
             * every single frame, so this takes effect on the very
             * next render - no cmd-bus needed just to refocus) instead
             * of spawning a duplicate. */
            /* PER-PROJECT SCOPING (2026-08-03, direct user correction
             * caught while building piececraft-xyz - see that project's
             * own ops/pc_menu_input.c header comment on this same
             * change for the full writeup: tactics-txt's own
             * OPEN_BOARD_WIDGET press was clobbering piececraft-xyz's
             * already-open board-viewer window, since ledger_peers only
             * ever tracked ONE global "board-viewer" identity regardless
             * of which host it was focused on). board-viewer's own
             * button.sh now registers as "board-viewer:<host_basename>"
             * - this loop must scan every ledger_peers.+x output line
             * for the one matching THIS host ("board-viewer:civ-txt"),
             * never blindly take the first line - a different host's
             * own live session is a real possible line here now. */
            char peer_session_root[PATH_BUF] = "";
            if (house_root[0]) {
                char peer_cmd[PATH_BUF * 2];
                snprintf(peer_cmd, sizeof(peer_cmd),
                         "PRISC_PROJECT_ROOT='%s' '%s/&.widgits/board-viewer/ops/+x/ledger_peers.+x' widget 2>/dev/null",
                         project_root, house_root);
                FILE *pf = popen(peer_cmd, "r");
                if (pf) {
                    char peer_line[MAX_LINE];
                    while (fgets(peer_line, sizeof(peer_line), pf)) {
                        peer_line[strcspn(peer_line, "\r\n")] = '\0';
                        /* fields: session_root|inbox_path|display_name|project_id|pid */
                        char *save = NULL;
                        char *sess_tok = strtok_r(peer_line, "|", &save);
                        strtok_r(NULL, "|", &save); /* inbox_path, unused here */
                        strtok_r(NULL, "|", &save); /* display_name, unused here */
                        char *proj_tok = strtok_r(NULL, "|", &save);
                        if (proj_tok && sess_tok && strcmp(proj_tok, "board-viewer:civ-txt") == 0) {
                            snprintf(peer_session_root, sizeof(peer_session_root), "%s", sess_tok);
                            break;
                        }
                    }
                    pclose(pf);
                }
            }

            if (peer_session_root[0] && real_root[0]) {
                char peer_state_path[PATH_BUF];
                snprintf(peer_state_path, sizeof(peer_state_path), "%s/pieces/system/bv_state.txt", peer_session_root);
                write_kv(peer_state_path, "focused_project_id", "civ-txt");
                write_kv(peer_state_path, "focused_project_root", real_root);
                snprintf(message, sizeof(message), "Board widget already open - refocusing on civ-txt.");
            } else if (house_root[0] && real_root[0] && access(widget_button, X_OK) == 0) {
                /* REAL BUG FOUND AND FIXED (2026-08-02): a bare
                 * "cmd &" backgrounds the widget but does NOT put it in
                 * a new process group - it stays in civ-txt's own
                 * session's group, so a `timeout` wrapping THIS
                 * session's own button.sh run cascades SIGTERM to the
                 * widget too when it expires, killing a supposedly
                 * "separate, independent" GL window along with the
                 * host. Confirmed live: the widget vanished the instant
                 * civ-txt's own test timeout fired. Fixed with setsid,
                 * which starts the widget in a genuinely new session/
                 * process group, immune to signals sent to civ-txt's
                 * own group (civ-txt's own explicit Ctrl+C/kill still
                 * only ever targeted its own processes anyway - this
                 * only affects the group-wide signal-cascade case). */
                char cmd_buf[PATH_BUF * 2];
                snprintf(cmd_buf, sizeof(cmd_buf),
                         "setsid env RUN_PROFILE=widget bash '%s' run-widget '%s' >/dev/null 2>&1 < /dev/null &",
                         widget_button, real_root);
                { int _rc = system(cmd_buf); (void)_rc; }
                snprintf(message, sizeof(message), "Board widget launching (separate GL window)...");
            } else {
                snprintf(message, sizeof(message),
                         "Board widget not built yet (see &.widgits/BOARD_WIDGET_PROGRESS.txt P2).");
            }
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/civ_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
