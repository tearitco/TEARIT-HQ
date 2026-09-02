/* tactics_menu_input - piece.pdl METHOD-table-driven ACTION dispatch
 * for whichever tactics-txt screen is currently showing. Modeled
 * directly on @.apps/my-chara-txt's own ops/mychara_menu_input.c.
 *
 * P1 scope, per TACTICS_TXT_DESIGN.md's own build order: setup.chtpm
 * (Classic mode only, fixed 3-unit staff armies already materialized
 * in pieces/system/units.txt) -> main.chtpm (turn/side/actions-
 * remaining display, one real END_TURN action that switches the
 * active side and resets the shared 5-action pool). No movement/
 * attack/skills yet (that's P2+) - this proves the real CHTPM nav +
 * piece.pdl dispatch + ledger loop works for THIS project first,
 * exactly like my-chara-txt's own P1/P2 did.
 *
 * tactics-txt-specific commands:
 *   SET_MODE:classic - P1 only supports classic, but the command
 *                       exists so setup.chtpm's shape matches what
 *                       Collection mode will slot into later
 *   CONFIRM_START - locks in setup
 *   END_TURN - switches active_side (1<->2), resets
 *              actions_remaining_this_turn to 5, increments turn
 *              counter only when wrapping back to side 1 (a full
 *              round = both sides having acted once)
 *
 * Self-contained, no shared headers.
 * Usage: tactics_menu_input.+x <keycode> */
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
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/tactics-txt/pieces/%s/piece.pdl", root, piece_id);
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
        fprintf(cf, "project_id=tactics-txt\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "setup");
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

static void ledger_append(const char *root, int turn, int side, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%d|%s|%s\n", ts, turn, side, action_type, details);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/tactics-txt/pieces/tactics_menu/state.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    /* REAL widget->host command delivery (2026-08-03, direct user
     * follow-up to the ledger/pairing fix: "all mechanics... paired
     * with the viewer at app runtime... let's do that" re: board-
     * viewer's own real widget_bridge.txt-shaped connection descriptor
     * - see @.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md §4a).
     * pieces/system/widget_cmds/inbox.txt is this project's own real
     * inbox (declared to board-viewer via pieces/system/
     * board_widget_bridge.txt's own inbox_path field, same real shape
     * file-menu's own widget_bridge.txt uses) - board-viewer appends a
     * plain command-string line to it; this op, already called every
     * single idle tick (~30ms, via main_module.pal's own `tactics_
     * menu_input x9` idle poll), drains the FIRST pending line here and
     * feeds it into the EXACT SAME dispatch chain below real numbered
     * piece.pdl METHOD commands already use - no second command
     * interpreter, no duplicated logic. Multiple lines queued in one
     * 30ms window are rare (one append per widget keypress) - only the
     * first is processed per call, a real, documented v1 limitation,
     * not silently pretended otherwise; a true multi-command queue is
     * later work if it's ever actually needed. */
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
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/tactics_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    /* REVERTED 2026-08-02 (same day, caught own mistake) - see
     * civ-txt's own ops/civ_menu_input.c for the full correction
     * writeup. Direct check of chtpm_parser_pal.c's own ${piece_
     * methods} generator (014.wsr-pal💸️📌️+2/system/chtpm_parser_
     * pal.c:1115: `method_idx = (loader) ? 1 : 2`) confirms every
     * non-loader piece's numbered METHOD rows start their internal
     * KEY:N index at 2, not 0/1 - `(key-'0')-1` correctly compensates
     * for that +2 offset, it was never actually a bug in THIS
     * dispatch path (unlike board-viewer's own now-removed digit
     * dispatch, which was tested via direct raw-injection that
     * bypassed this +2 offset entirely - a different, non-comparable
     * situation). This file's own "View Board" not firing has a
     * different real cause - still being investigated, not this. */
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
        int active_side = read_kv_int(config_path, "active_side", 1);

        if (strncmp(cmd, "SET_MODE:", 9) == 0) {
            write_kv(config_path, "mode", cmd + 9);
            snprintf(message, sizeof(message), "Mode set: %s", cmd + 9);
        } else if (strcmp(cmd, "CONFIRM_START") == 0) {
            write_kv(config_path, "game_state", "playing");
            snprintf(message, sizeof(message), "Setup confirmed. Battle started.");

            /* Real board data, same real-not-placeholder principle as
             * civ-txt's own CONFIRM_START (see that project's own
             * civ_menu_input.c) - the shared board-viewer widget (P10
             * of &.widgits/BOARD_WIDGET_PROGRESS.txt, now built) needs
             * real tiles to render. Fixed 10x10 per TACTICS_TXT_DESIGN
             * .md (no map_scale option here, unlike civ-txt - this
             * design is always one fixed board size). Terrain glyphs:
             * '.'=grass, '^'=high-ground, '~'=water/mud, '#'=wall
             * (a real, tall obstacle - distinct from civ-txt's own
             * glyph set, added to board-viewer's own terrain tables
             * alongside this). Selective extrusion per &.widgits/
             * 5-pov-widgit.md §3b - walls get real height in the 3D
             * raymarch view once opened. No unit positions written
             * here yet (P1 has no x/y unit data at all - see this
             * project's own HANDOFF_NEXT_SESSION.md). */
            char board_path[PATH_BUF];
            snprintf(board_path, sizeof(board_path), "%s/pieces/system/board.txt", project_root);
            FILE *bf = fopen(board_path, "w");
            if (bf) {
                srand((unsigned)time(NULL));
                const int grid_n = 10;
                for (int r = 0; r < grid_n; r++) {
                    for (int c = 0; c < grid_n; c++) {
                        int roll = rand() % 100;
                        char glyph;
                        if (roll < 55) glyph = '.';
                        else if (roll < 72) glyph = '^';
                        else if (roll < 85) glyph = '#';
                        else glyph = '~';
                        fputc(glyph, bf);
                    }
                    fputc('\n', bf);
                }
                fclose(bf);
            }

            /* Real per-unit directories, replacing the flat units.txt
             * roster as the source of truth for placed units (units.txt
             * itself is left untouched - still read by tactics_compose_
             * frame.c's own OLD "Side N roster" preview text, harmless
             * duplication for this pass, not worth removing yet). Per
             * @.apps/PORTABLE_ENTITY_ARCHITECTURE.md: entity_type/hp/
             * pos_x/pos_y/owner_id are the universal cross-game fields,
             * profession/skills are this project's own. A minimal real
             * piece.pdl per unit (Move/Attack) is written alongside -
             * this is the actual new precedent this pass sets: no
             * entity in this house has ever had its own piece.pdl
             * before (confirmed via direct research - see that doc's
             * own §1). Fixed starting positions on the 10x10 board,
             * side 1 on the west edge (col 0), side 2 on the east edge
             * (col 9), spread across different rows - simple, real,
             * not yet terrain-aware (a unit could in principle start on
             * a '#' wall tile - acceptable for this pass, terrain-aware
             * placement is later work). */
            {
                const char *profs[2][3] = {
                    {"warrior", "chef", "farmer"},
                    {"warrior", "clown", "lawyer"}
                };
                const int hps[3] = {20, 15, 15};
                const int rows[3] = {3, 5, 7};
                for (int side = 1; side <= 2; side++) {
                    int col = (side == 1) ? 0 : 9;
                    for (int i = 0; i < 3; i++) {
                        char unit_id[32];
                        snprintf(unit_id, sizeof(unit_id), "side%d_unit%d", side, i);
                        char unit_dir[PATH_BUF];
                        snprintf(unit_dir, sizeof(unit_dir), "%s/pieces/battle_01/units/%s", project_root, unit_id);
                        char mkdir_cmd[PATH_BUF * 2];
                        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", unit_dir);
                        { int _rc = system(mkdir_cmd); (void)_rc; }

                        char unit_state_path[PATH_BUF];
                        snprintf(unit_state_path, sizeof(unit_state_path), "%s/state.txt", unit_dir);
                        FILE *uf = fopen(unit_state_path, "w");
                        if (uf) {
                            fprintf(uf, "entity_type=tactics_unit\n");
                            fprintf(uf, "profession_id=%s\n", profs[side - 1][i]);
                            fprintf(uf, "pos_x=%d\n", col);
                            fprintf(uf, "pos_y=%d\n", rows[i]);
                            fprintf(uf, "hp=%d\n", hps[i]);
                            fprintf(uf, "owner_id=%d\n", side);
                            fprintf(uf, "owner_side=%d\n", side);
                            fclose(uf);
                        }

                        char unit_pdl_path[PATH_BUF];
                        snprintf(unit_pdl_path, sizeof(unit_pdl_path), "%s/piece.pdl", unit_dir);
                        FILE *pf = fopen(unit_pdl_path, "w");
                        if (pf) {
                            fprintf(pf, "SECTION      | KEY                | VALUE\n");
                            fprintf(pf, "----------------------------------------\n");
                            fprintf(pf, "META         | piece_id           | %s\n\n", unit_id);
                            fprintf(pf, "METHOD       | Move                                | MOVE_UNIT:%s\n", unit_id);
                            fprintf(pf, "METHOD       | Attack                              | ATTACK_UNIT:%s\n", unit_id);
                            fclose(pf);
                        }
                    }
                }
            }

            /* Real entity rendering on the board widget, part 1 (read-
             * only, no click-to-select yet - that's separate, later
             * command-bus work per @.apps/BOARD_WIDGET_ARCHITECTURE.md
             * §5). Board-viewer stays genuinely project-agnostic (same
             * principle chtpm_rgb_render.c's own on-demand emoji system
             * follows) by never reading this project's own pieces/
             * battle_01/units/ directory shape directly - instead this
             * project generates a simple, generic manifest listing
             * exactly what to draw and where. Format: one line per
             * entity, pipe-delimited:
             *   entity_id|pos_x|pos_y|unicode_hex|r|g|b|owner_id
             * unicode_hex is a real emoji codepoint (profession-based,
             * distinct single-codepoint emoji to avoid VS16/ZWJ
             * sequence complications with the on-demand voxel-
             * generation pipeline) - r/g/b is a side-tint color the
             * widget can use for a solid-color fallback/highlight,
             * civ-txt's own future cities/units would generate the
             * exact same file shape once it has real entities too. */
            {
                const char *emoji_hex[3] = { "1F5E1", "1F373", "1F69C" }; /* warrior/chef/farmer */
                const char *emoji_hex_2[3] = { "1F5E1", "1F921", "1F454" }; /* warrior/clown/lawyer */
                const int rows2[3] = { 3, 5, 7 };
                char entities_path[PATH_BUF];
                snprintf(entities_path, sizeof(entities_path), "%s/pieces/system/entities.txt", project_root);
                FILE *ef = fopen(entities_path, "w");
                if (ef) {
                    for (int side = 1; side <= 2; side++) {
                        int col2 = (side == 1) ? 0 : 9;
                        unsigned char rr = (side == 1) ? 80 : 220;
                        unsigned char gg = (side == 1) ? 140 : 90;
                        unsigned char bb = (side == 1) ? 220 : 80;
                        for (int i = 0; i < 3; i++) {
                            const char *hex = (side == 1) ? emoji_hex[i] : emoji_hex_2[i];
                            fprintf(ef, "side%d_unit%d|%d|%d|%s|%d|%d|%d|%d\n",
                                    side, i, col2, rows2[i], hex, rr, gg, bb, side);
                        }
                    }
                    fclose(ef);
                }
            }
        } else if (strcmp(cmd, "OPEN_BOARD_WIDGET") == 0) {
            /* Exact same mechanism as civ-txt's own OPEN_BOARD_WIDGET
             * handler (ops/civ_menu_input.c) - spawns the SEPARATE,
             * SHARED &.widgits/board-viewer/ widget as its own detached
             * process/GL window. tactics-txt's own CHTPM screen/session
             * never changes. See @.apps/BOARD_WIDGET_ARCHITECTURE.md
             * §3/§7 and civ-txt's own handler for the full reasoning
             * (including the real setsid process-group fix). */
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
             * xyz - if civ/tactics don't do this, they did it wrong")
             * - see civ-txt's own ops/civ_menu_input.c for the full
             * writeup (identical fix, same real ledger-discovery
             * mechanism via board-viewer's own ops/ledger_peers.c).
             * Refocuses an already-running board-viewer session by
             * overwriting its own bv_state.txt directly instead of
             * always spawning a redundant duplicate. */
            /* PER-PROJECT SCOPING (2026-08-03, direct user correction
             * caught while building piececraft-xyz - see that project's
             * own ops/pc_menu_input.c header comment on this same
             * change for the full writeup: this project's own
             * OPEN_BOARD_WIDGET press was clobbering piececraft-xyz's
             * already-open board-viewer window, since ledger_peers only
             * ever tracked ONE global "board-viewer" identity regardless
             * of which host it was focused on). board-viewer's own
             * button.sh now registers as "board-viewer:<host_basename>"
             * - this loop must scan every ledger_peers.+x output line
             * for the one matching THIS host ("board-viewer:tactics-
             * txt"), never blindly take the first line - a different
             * host's own live session is a real possible line here now. */
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
                        if (proj_tok && sess_tok && strcmp(proj_tok, "board-viewer:tactics-txt") == 0) {
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
                write_kv(peer_state_path, "focused_project_id", "tactics-txt");
                write_kv(peer_state_path, "focused_project_root", real_root);
                snprintf(message, sizeof(message), "Board widget already open - refocusing on tactics-txt.");
            } else if (house_root[0] && real_root[0] && access(widget_button, X_OK) == 0) {
                char cmd_buf[PATH_BUF * 2];
                snprintf(cmd_buf, sizeof(cmd_buf),
                         "setsid env RUN_PROFILE=widget bash '%s' run-widget '%s' >/dev/null 2>&1 < /dev/null &",
                         widget_button, real_root);
                { int _rc = system(cmd_buf); (void)_rc; }
                snprintf(message, sizeof(message), "Board widget launching (separate GL window)...");
            } else {
                snprintf(message, sizeof(message), "Board widget not found - check &.widgits/board-viewer exists.");
            }
        } else if (strcmp(cmd, "END_TURN") == 0) {
            char details[128];
            snprintf(details, sizeof(details), "actions_used_unused_this_pass");
            ledger_append(project_root, turn, active_side, "end_turn", details);

            if (active_side == 1) {
                active_side = 2;
            } else {
                active_side = 1;
                turn += 1;
            }
            write_kv_int(config_path, "active_side", active_side);
            write_kv_int(config_path, "turn", turn);
            write_kv_int(config_path, "actions_remaining_this_turn", 5);
            snprintf(message, sizeof(message), "Turn %d, side %d's turn. 5 actions available.", turn, active_side);
        } else if (strncmp(cmd, "SELECT_UNIT:", 12) == 0) {
            /* Real possession-equivalent for this project, per @.apps/
             * PORTABLE_ENTITY_ARCHITECTURE.md's own §4 - roster.chtpm's
             * own compose op reads this back and merges the selected
             * unit's OWN real piece.pdl methods into roster's own
             * dynamically-regenerated piece.pdl (see this file's own
             * ENTITY_MOVEMENT_PROGRESS.txt "REAL ARCHITECTURAL FINDING"
             * for why this is a state field here rather than a true
             * active_target_id retarget). Only selectable if the unit
             * actually belongs to the currently active side - a real,
             * if minimal, rule enforcement, not just UI dressing. */
            const char *unit_id = cmd + 12;
            char check_path[PATH_BUF];
            snprintf(check_path, sizeof(check_path), "%s/pieces/battle_01/units/%s/state.txt", project_root, unit_id);
            int owner = read_kv_int(check_path, "owner_side", 0);
            if (owner == active_side) {
                write_kv(state_path, "selected_unit_id", unit_id);
                snprintf(message, sizeof(message), "%s selected.", unit_id);
            } else {
                snprintf(message, sizeof(message), "That unit isn't on the active side.");
            }
        } else if (strcmp(cmd, "DESELECT_UNIT") == 0) {
            write_kv(state_path, "selected_unit_id", "");
            snprintf(message, sizeof(message), "Deselected.");
        } else if (strncmp(cmd, "MOVE_UNIT:", 10) == 0) {
            /* Real stub, per this pass's own explicit scope (see
             * ENTITY_MOVEMENT_PROGRESS.txt - real path-cost/movement
             * math is separate, later work). Proves the selection ->
             * per-entity-methods -> dispatch pipeline end to end
             * without pretending real movement exists yet. */
            snprintf(message, sizeof(message), "Move: not yet implemented (path-cost math is later work).");
        } else if (strncmp(cmd, "ATTACK_UNIT:", 12) == 0) {
            snprintf(message, sizeof(message), "Attack: not yet implemented (combat resolution is later work).");
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/tactics_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
