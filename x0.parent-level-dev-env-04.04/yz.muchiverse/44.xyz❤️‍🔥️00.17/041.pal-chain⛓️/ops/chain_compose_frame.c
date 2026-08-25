/* chain_compose_frame - renders whichever pal-chain screen is current
 * into pieces/apps/player_app/view.txt (the file chtpm's own
 * ${game_map} placeholder substitutes verbatim). ${piece_methods} (a
 * separate chtpm-native placeholder) renders the current screen's own
 * numbered METHOD buttons - this op never draws that menu itself, only
 * the surrounding chrome + any screen-specific live data (balance,
 * mining stats).
 *
 * REVISED (real, user-reported flicker, 2026-07-19): the first version
 * of this file also wrote pieces/display/current_frame.txt DIRECTLY
 * (copied from wsr-pal's own wsr_compose_frame.c, whose own header
 * comment argued this was needed to avoid "waiting on chtpm's own
 * separate poll cycle"). That premise is wrong for THIS parser, checked
 * directly against shared-ops/chtpm_parser_pal.c: process_key() already
 * appends to pieces/display/frame_changed.txt on EVERY keystroke
 * unconditionally, and the main loop's own dirty-marker check picks
 * that up and calls the parser's OWN compose_frame() (which re-reads
 * view.txt via ${game_map} and writes current_frame.txt itself) on the
 * very next loop iteration - there is no real lag to work around. With
 * BOTH this op AND the parser's own compose_frame() writing
 * current_frame.txt, a viewer briefly saw whichever one raced there
 * first before the other overwrote it - the exact "dual writer"
 * flicker/corruption class named in
 * 1.TPMOS_c_+rmmp.0102.0028/!.gem-flashlite--yolo/#.docs/^.pmo.ld-faq+8/
 * PITFALLS_ACTIVE_2026-03-18.txt #17 ("ONE WRITER RULE") and #36 ("ONE
 * VISIBLE FRAME WRITER RULE"). Fixed by making this op write ONLY
 * view.txt - chtpm_parser_pal.c's own compose_frame() is now the sole
 * writer of pieces/display/current_frame.txt, exactly as those two
 * rules require. See XYZOS-STANDARDS-PITFALLS.txt sec. 1 for this
 * project family's own copy of the relevant rules, added alongside
 * this fix.
 *
 * Self-contained, no shared headers.
 * Usage: chain_compose_frame.+x (no args) */
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

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
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

static void get_current_piece_id(const char *project_root_, char *out, size_t out_sz) {
    snprintf(out, out_sz, "login");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root_);
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

/* REAL BUG, LIVE-CAUGHT immediately after the sec. 20/one-writer fix
 * above (removing the direct current_frame.txt write correctly killed
 * the flicker, but then nothing ever re-rendered until the NEXT real
 * keystroke): pieces/apps/player_app/state_changed.txt growth only
 * ever makes chtpm_parser_pal.c reload vars/reparse the layout - by
 * that file's own explicit "RENDER TRIGGER" banner comment (confirmed
 * by direct read), ONLY pieces/display/frame_changed.txt growth sets
 * dirty=1 and actually calls compose_frame() - that comment names
 * exactly this case: "Game layouts: render_map.c after deduped view
 * update." compose_frame() itself calls load_vars() as its own first
 * line, so bumping frame_changed.txt alone is sufficient - no need to
 * also bump state_changed.txt for a normal content refresh. */
static void ping_chtpm_render_marker(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/pal-chain/pieces/chain_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_menu_piece[128];
    get_current_piece_id(project_root, active_menu_piece, sizeof(active_menu_piece));

    char session_path[PATH_BUF];
    snprintf(session_path, sizeof(session_path), "%s/net/session.txt", project_root);
    char wallet_id[128];
    read_kv_str(session_path, "wallet_id", wallet_id, sizeof(wallet_id));

    /* REAL BUG, LIVE-CAUGHT: this used to be sized BOX_W+1, exactly the
     * visible width - that made snprintf() itself silently truncate any
     * longer content (e.g. the send confirmation message) BEFORE line()
     * ever got to run its own, correct %.*s-based width clamp, cutting
     * off the last character with no padding. line() below is what's
     * actually responsible for clamping to BOX_W - this buffer only
     * needs to hold the real content up to MAX_LINE. */
    char rowbuf[MAX_LINE];
    border();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  P A L - C H A I N   [%s]", active_menu_piece);
#pragma GCC diagnostic pop
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_menu_piece, "wallet_main") == 0 ||
        strcmp(active_menu_piece, "send_screen") == 0 ||
        strcmp(active_menu_piece, "receive_screen") == 0) {
        if (!wallet_id[0]) {
            line("  Not logged in. Use the Back to Login link below.");
        } else {
            snprintf(rowbuf, sizeof(rowbuf), "  Wallet: %s", wallet_id);
            line(rowbuf);

            if (strcmp(active_menu_piece, "receive_screen") == 0) {
                char bal_cmd[PATH_BUF];
                snprintf(bal_cmd, sizeof(bal_cmd), "cd '%s' && ./ops/+x/chain_balance.+x %s 2>/dev/null", project_root, wallet_id);
                FILE *p = popen(bal_cmd, "r");
                long bal = -1;
                if (p) { if (fscanf(p, "%ld", &bal) != 1) bal = -1; pclose(p); }
                if (bal >= 0) {
                    snprintf(rowbuf, sizeof(rowbuf), "  Balance: %ld millicones (%.3f cones)", bal, bal / 1000.0);
                    line(rowbuf);
                }
                blank();
                line("  Share your wallet ID above with anyone who wants");
                line("  to send you cones.");
            }
        }
    } else if (strcmp(active_menu_piece, "mining_status") == 0) {
        char status_path[PATH_BUF];
        snprintf(status_path, sizeof(status_path), "%s/net/miner_status.txt", project_root);
        int running = read_kv_int(status_path, "running", 0);
        long blocks = read_kv_int(status_path, "blocks_mined_this_session", 0);
        long last_index = read_kv_int(status_path, "last_block_index", -1);
        long long total_minted = read_kv_int(status_path, "total_supply_minted", 0);
        char last_hash[80], status_miner[128];
        read_kv_str(status_path, "last_block_hash", last_hash, sizeof(last_hash));
        read_kv_str(status_path, "miner_wallet_id", status_miner, sizeof(status_miner));

        snprintf(rowbuf, sizeof(rowbuf), "  Miner: %s   Status: %s", status_miner[0] ? status_miner : "(none)", running ? "RUNNING" : "stopped");
        line(rowbuf);
        blank();
        snprintf(rowbuf, sizeof(rowbuf), "  Blocks mined this session: %ld", blocks);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Last block index: %ld", last_index);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Last block hash:  %.20s...", last_hash[0] ? last_hash : "(none)");
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Total supply minted: %lld / 21000000 millicones", total_minted);
        line(rowbuf);
    } else if (strcmp(active_menu_piece, "login") == 0) {
        line("  Welcome to pal-chain.");
        line("  Log in with an existing wallet, or create a new one.");
    } else if (strcmp(active_menu_piece, "signup") == 0) {
        line("  Choose a wallet ID and password.");
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
    ping_chtpm_render_marker();
    return 0;
}
