/* chain_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever pal-chain screen is currently showing, modeled directly on
 * wsr-pal's own wsr_menu_input.c (that project's own real href +
 * ${piece_methods} rewrite, proven live earlier this session) - same
 * shape: screen SWITCHING is a real chtpm <button href="..."> handled
 * entirely by chtpm_parser_pal.c, never this op's job; "which screen is
 * current" is derived fresh every call from
 * pieces/display/current_layout.txt, never separately tracked mutable
 * state.
 *
 * pal-chain-specific commands (piece.pdl METHOD rows dispatch here):
 *   LOGIN / SIGNUP / SEND - reads real, hand-authored <cli_io
 *                          target_id="..."> fields (login.chtpm/
 *                          signup.chtpm/send_screen.chtpm) via
 *                          chtpm_parser_pal.c's own real
 *                          save_cli_io_gui_state() mechanism, landing in
 *                          projects/pal-chain/manager/gui_state.txt -
 *                          REPLACES an earlier prompt_active/
 *                          prompt_step/chain_wizard_input.c design that
 *                          turned out to be unreachable: chtpm's own
 *                          "normal nav mode" only ever forwards arrows/
 *                          digits/Enter to a module, silently dropping
 *                          every other key (confirmed live - typed
 *                          letters never reached that op at all). A
 *                          real <cli_io> element is chtpm's own actual,
 *                          working mechanism for this (same one wraith-
 *                          alpha's window-geom editor uses) - it
 *                          buffers keystrokes entirely inside
 *                          chtpm_parser_pal.c itself (no per-keystroke
 *                          module dispatch needed), so this op only
 *                          ever needs to read the ALREADY-TYPED value
 *                          back out of gui_state.txt once the real
 *                          LOGIN/SIGNUP/SEND button is clicked.
 *   CHAIN_BALANCE        - looks up net/session.txt's own wallet_id,
 *                          shells to chain_balance.+x, shows the result
 *   CHAIN_START_MINING   - launches chain_miner.+x for the logged-in
 *                          wallet as a detached background process
 *                          (and chain_inbox_watcher.+x alongside it, if
 *                          not already running - mining without a
 *                          watcher would never learn about peers' own
 *                          mined blocks, defeating the point of testing
 *                          "mine against other headless nodes")
 *   CHAIN_STOP_MINING     - kills net/miner.pid
 *   CHAIN_LOGOUT          - clears net/session.txt (real chtpm
 *                          navigation back to login.chtpm is a
 *                          separate, real href click - see that
 *                          screen's own "Back to Login" button,
 *                          matching this same split already used for
 *                          Startup New Corp's own wizard-vs-href
 *                          separation in wsr-pal)
 *
 * Self-contained, no shared headers.
 * Usage: chain_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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

static int load_menu_items(const char *project_root_, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/pal-chain/pieces/%s/piece.pdl", project_root_, piece_id);
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
        fprintf(cf, "project_id=pal-chain\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void get_current_piece_id(const char *project_root_, char *out, size_t out_sz) {
    snprintf(out, out_sz, "login");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root_);
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

static void session_wallet_id(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/session.txt", project_root);
    read_kv_str_local(path, "wallet_id", out, out_sz);
}

/* Reads a <cli_io target_id="key"> field's own typed value back out of
 * projects/pal-chain/manager/gui_state.txt - the exact file
 * chtpm_parser_pal.c's own save_cli_io_gui_state() writes to (confirmed
 * by direct read of that file's resolve_cli_io_project_id()/
 * save_to_gui_state_impl() - real project_id set, so it resolves to
 * projects/<project_id>/manager/gui_state.txt, not the legacy
 * pieces/apps/player_app/manager/ fallback). Matches that file's own
 * "last match wins" read semantics (a real, if odd, existing behavior -
 * duplicate keys accumulate rather than get deduped on write in some
 * paths there) rather than read_kv_str_local's own first-match-wins,
 * so a stale duplicate line never wins over a fresher one. */
static void read_gui_state_str(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/pal-chain/manager/gui_state.txt", project_root);
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

/* Clears a gui_state.txt key after it's been consumed (e.g. a password
 * shouldn't keep sitting in plain text in gui_state.txt, and a re-click
 * of Log In with an unchanged field shouldn't resubmit stale text the
 * user meant to have cleared) - reuses write_kv's own read-modify-write
 * shape against gui_state.txt specifically. */
static void clear_gui_state_str(const char *key) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/pal-chain/manager/gui_state.txt", project_root);
    write_kv(path, key, "");
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/pal-chain/pieces/chain_menu/state.txt", project_root);

    int key = atoi(argv[1]);

    if (key == 0) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_kv(state_path, "active_menu_piece", derived);
        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/chain_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_menu_piece[128];
    get_current_piece_id(project_root, active_menu_piece, sizeof(active_menu_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_menu_piece, items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "LOGIN") == 0) {
            char wallet_id[128], password[128];
            read_gui_state_str("wallet_id_input", wallet_id, sizeof(wallet_id));
            read_gui_state_str("password_input", password, sizeof(password));
            if (!wallet_id[0] || !password[0]) {
                snprintf(message, sizeof(message), "Enter a wallet ID and password, then click Log In.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "cd '%s' && ./ops/+x/chain_login.+x '%s' '%s' 2>&1", project_root, wallet_id, password);
                FILE *p = popen(cmdbuf, "r");
                char result[MAX_LINE] = "";
                if (p) { if (fgets(result, sizeof(result), p)) result[strcspn(result, "\n")] = '\0'; pclose(p); }
                clear_gui_state_str("password_input");
                if (strstr(result, "Logged in")) {
                    snprintf(message, sizeof(message), "%s - click Continue to Wallet.", result);
                } else {
                    snprintf(message, sizeof(message), "%s", result[0] ? result : "Login failed.");
                }
            }
        } else if (strcmp(cmd, "SIGNUP") == 0) {
            char wallet_id[128], password[128];
            read_gui_state_str("wallet_id_input", wallet_id, sizeof(wallet_id));
            read_gui_state_str("password_input", password, sizeof(password));
            if (!wallet_id[0] || !password[0]) {
                snprintf(message, sizeof(message), "Enter a wallet ID and password, then click Create Wallet.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "cd '%s' && ./ops/+x/chain_create_wallet.+x '%s' '%s' 2>&1", project_root, wallet_id, password);
                FILE *p = popen(cmdbuf, "r");
                char result[MAX_LINE] = "";
                if (p) { if (fgets(result, sizeof(result), p)) result[strcspn(result, "\n")] = '\0'; pclose(p); }
                clear_gui_state_str("password_input");
                if (strstr(result, "created")) {
                    snprintf(message, sizeof(message), "%s - click Back to Login.", result);
                } else {
                    snprintf(message, sizeof(message), "%s", result[0] ? result : "Could not create wallet.");
                }
            }
        } else if (strcmp(cmd, "SEND") == 0) {
            char to_wallet[128], amount[64], from_wallet[128];
            read_gui_state_str("to_wallet_input", to_wallet, sizeof(to_wallet));
            read_gui_state_str("amount_input", amount, sizeof(amount));
            session_wallet_id(from_wallet, sizeof(from_wallet));
            if (!from_wallet[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else if (!to_wallet[0] || !amount[0]) {
                snprintf(message, sizeof(message), "Enter a recipient wallet ID and amount, then click Send Cones.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "cd '%s' && ./ops/+x/chain_send.+x '%s' '%s' '%s' 2>&1", project_root, from_wallet, to_wallet, amount);
                FILE *p = popen(cmdbuf, "r");
                char result[MAX_LINE] = "";
                if (p) { if (fgets(result, sizeof(result), p)) result[strcspn(result, "\n")] = '\0'; pclose(p); }
                snprintf(message, sizeof(message), "%s", result[0] ? result : "Send failed.");
            }
        } else if (strcmp(cmd, "CHAIN_BALANCE") == 0) {
            char wallet_id[128];
            session_wallet_id(wallet_id, sizeof(wallet_id));
            if (!wallet_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else {
                char bal_cmd[PATH_BUF];
                snprintf(bal_cmd, sizeof(bal_cmd), "cd '%s' && ./ops/+x/chain_balance.+x %s 2>&1", project_root, wallet_id);
                FILE *p = popen(bal_cmd, "r");
                long bal = -1;
                if (p) { if (fscanf(p, "%ld", &bal) != 1) bal = -1; pclose(p); }
                if (bal >= 0) snprintf(message, sizeof(message), "%s balance: %ld millicones (%.3f cones).", wallet_id, bal, bal / 1000.0);
                else snprintf(message, sizeof(message), "Could not read balance.");
            }
        } else if (strcmp(cmd, "CHAIN_START_MINING") == 0) {
            char wallet_id[128];
            session_wallet_id(wallet_id, sizeof(wallet_id));
            if (!wallet_id[0]) {
                snprintf(message, sizeof(message), "Not logged in.");
            } else {
                char watcher_pid_path[PATH_BUF];
                snprintf(watcher_pid_path, sizeof(watcher_pid_path), "%s/net/inbox_watcher.pid", project_root);
                FILE *wf = fopen(watcher_pid_path, "r");
                int watcher_running = 0;
                if (wf) {
                    int pid = 0;
                    if (fscanf(wf, "%d", &pid) == 1 && pid > 0 && kill(pid, 0) == 0) watcher_running = 1;
                    fclose(wf);
                }
                if (!watcher_running) {
                    char watcher_cmd[PATH_BUF];
                    snprintf(watcher_cmd, sizeof(watcher_cmd),
                             "cd '%s' && ./ops/+x/chain_inbox_watcher.+x >/tmp/pal_chain_inbox_watcher.log 2>&1 & echo $! > net/inbox_watcher.pid",
                             project_root);
                    int rc = system(watcher_cmd);
                    (void)rc;
                }

                /* REAL BUG, LIVE-CAUGHT during a 2-node test: clicking
                 * Start Mining twice (or once per session after a crash
                 * left net/miner.pid stale) launched a SECOND
                 * chain_miner.+x racing the first against the same
                 * blockchain.txt - both append competing blocks around
                 * the same next_index, corrupting the chain with
                 * duplicate/conflicting entries. Guard the same way the
                 * watcher above already does: only launch if
                 * net/miner.pid doesn't name a still-alive process. */
                char miner_pid_path[PATH_BUF];
                snprintf(miner_pid_path, sizeof(miner_pid_path), "%s/net/miner.pid", project_root);
                FILE *mf = fopen(miner_pid_path, "r");
                int miner_running = 0;
                if (mf) {
                    int pid = 0;
                    if (fscanf(mf, "%d", &pid) == 1 && pid > 0 && kill(pid, 0) == 0) miner_running = 1;
                    fclose(mf);
                }
                if (miner_running) {
                    snprintf(message, sizeof(message), "Mining is already running.");
                } else {
                    char mine_cmd[PATH_BUF];
                    snprintf(mine_cmd, sizeof(mine_cmd),
                             "cd '%s' && ./ops/+x/chain_miner.+x %s >/tmp/pal_chain_miner.log 2>&1 &",
                             project_root, wallet_id);
                    int rc = system(mine_cmd);
                    (void)rc;
                    snprintf(message, sizeof(message), "Mining started for %s - see Mining Status.", wallet_id);
                }
            }
        } else if (strcmp(cmd, "CHAIN_STOP_MINING") == 0) {
            char pid_path[PATH_BUF];
            snprintf(pid_path, sizeof(pid_path), "%s/net/miner.pid", project_root);
            FILE *pf = fopen(pid_path, "r");
            int pid = 0;
            if (pf) { if (fscanf(pf, "%d", &pid) != 1) pid = 0; fclose(pf); }
            if (pid > 0) kill(pid, SIGTERM);
            snprintf(message, sizeof(message), "Mining stopped.");
        } else if (strcmp(cmd, "CHAIN_LOGOUT") == 0) {
            char session_path[PATH_BUF];
            snprintf(session_path, sizeof(session_path), "%s/net/session.txt", project_root);
            FILE *sf = fopen(session_path, "w");
            if (sf) fclose(sf);
            snprintf(message, sizeof(message), "Logged out - click Back to Login.");
        } else if (strcmp(cmd, "STUB") == 0) {
            snprintf(message, sizeof(message), "Not yet available in this build.");
        } else {
            char raw_cmd[PATH_BUF * 2];
            snprintf(raw_cmd, sizeof(raw_cmd), "cd '%s' && %s > /dev/null 2>&1", project_root, cmd);
            int rc = system(raw_cmd);
            (void)rc;
            snprintf(message, sizeof(message), "Ran: %s", items[resolved_item - 1].label);
        }
    }

    write_kv(state_path, "last_message", message);
    write_kv(state_path, "active_menu_piece", active_menu_piece);
    write_chtpm_bridge(active_menu_piece);
    return 0;
}
