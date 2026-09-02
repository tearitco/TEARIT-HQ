/* broker_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for the
 * in-app Yahoo Finance broker-sim trading screen (broker.chtpm). Modeled
 * directly on yahoo_menu_input.c (house standard my-chara-txt).
 *
 * Reuses the generic yfin ops in ops/ (lookup_stock, buy_stock, ...) run
 * from the project root CWD so usr_acc.<hash>.txt and per-symbol files land
 * where yahoo_compose_frame / broker_compose_frame read them.
 *
 * Commands (broker piece.pdl METHODS):
 *   LOOKUP_STOCK      sym_input = symbol
 *   CHECK_BALANCE
 *   ADD_CREDIT        amt_input = dollars
 *   BUY_STOCK         sym_input = symbol, amt_input = shares
 *   SELL_STOCK        sym_input = symbol, amt_input = shares
 *   SELL_OPTIONS      sym_input = option index, amt_input = contracts
 *   VIEW_PORTFOLIO
 *   VIEW_PROFIT_LOSS
 *   WATCHLIST_ADD     sym_input = symbol
 *   OPTIONS_PRICING   sym_input = symbol, amt_input = strike
 *   PREDICT_STOCK     sym_input = symbol
 *   DEBUG_LEDGER      open data/master_ledger.txt in gedit (or print path)
 *
 * Self-contained.
 * Usage: broker_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[256];
} MenuItem;

static char project_root[MAX_PATH] = ".";
static char project_id[64] = "yahoo-app";

static char *trim(char *s);

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *pid = getenv("PRISC_PROJECT_ID");
    if (pid && pid[0]) snprintf(project_id, sizeof(project_id), "%s", pid);
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
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    FILE *orig = fopen(path, "r");
    if (orig) {
        char line[MAX_LINE];
        int found = 0;
        while (fgets(line, sizeof(line), orig)) {
            if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                fprintf(f, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, f);
            }
        }
        if (!found) fprintf(f, "%s=%s\n", key, val);
        fclose(orig);
    } else {
        fprintf(f, "%s=%s\n", key, val);
    }
    fclose(f);
    rename(tmp, path);
}

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/pieces/%s/piece.pdl", root, project_id, piece_id);
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

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static void get_gui_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/projects/%s/manager/gui_state.txt", project_root, project_id);
}

static void read_gui_state_str(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    get_gui_state_path(path, sizeof(path));
    read_kv_str_local(path, key, out, out_sz);
}

static void clear_gui_state_str(const char *key) {
    char path[PATH_BUF];
    get_gui_state_path(path, sizeof(path));
    write_kv(path, key, "");
}

static void append_message(const char *msg) {
    char path[PATH_BUF];
    get_gui_state_path(path, sizeof(path));
    write_kv(path, "last_message", msg);
}

static void mark_screen_changed(const char *name) {
    (void)name;
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/broker_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "broker");
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
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
    }
    fclose(f);
}

static void write_chtpm_bridge(const char *piece_id) {
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=%s\n", project_id);
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void get_user_hash(char *out, size_t out_sz) {
    out[0] = '\0';
    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    read_kv_str_local(config_path, "user_hash", out, out_sz);
}

/* cli_io target_id values land in projects/<id>/manager/gui_state.txt via
 * the parser's save_cli_io_gui_state() (chain_menu_input.c pattern). */
static void ensure_op_link(void) {
    char link[PATH_BUF], target[PATH_BUF];
    snprintf(link, sizeof(link), "%s/+x", project_root);
    snprintf(target, sizeof(target), "%s/ops/+x", project_root);
    struct stat st;
    if (stat(link, &st) != 0 && access(target, F_OK) == 0) {
        symlink(target, link);
    }
}

static char *last_nonempty_line(char *s) {
    char *last = NULL;
    char *p = s;
    while (p && *p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        if (*p) last = p;
        p = nl ? nl + 1 : NULL;
    }
    return last;
}

/* Run an arbitrary command from the project root CWD, capturing the last
 * non-empty stdout line into msg. The yfin ops call ./+x/... internally and
 * write CWD-relative files, so project_root must be their CWD. */
static int run_popen_cwd(char *msg, size_t msg_sz, const char *full_cmd) {
    char cwd[PATH_BUF];
    getcwd(cwd, sizeof(cwd));
    char out[2048] = "";
    if (chdir(project_root) == 0) {
        FILE *pipe = popen(full_cmd, "r");
        if (pipe) {
            size_t n = 0;
            char buf[1024];
            while (fgets(buf, sizeof(buf), pipe) && n < sizeof(out) - 1) {
                size_t l = strlen(buf);
                if (l > sizeof(out) - 1 - n) l = sizeof(out) - 1 - n;
                memcpy(out + n, buf, l);
                n += l;
            }
            pclose(pipe);
        }
        chdir(cwd);
    }
    char *last = last_nonempty_line(out);
    if (last && last[0]) {
        snprintf(msg, msg_sz, "%s", last);
        return 0;
    }
    return 1;
}

static int run_yfin(char *msg, size_t msg_sz, const char *op,
                    const char *arg1, const char *arg2, const char *arg3) {
    ensure_op_link();
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/ops/+x/%s.+x", project_root, op);
    if (access(path, X_OK) != 0) {
        if (msg && msg_sz) snprintf(msg, msg_sz, "op %s not built", op);
        return 1;
    }
    char full[PATH_BUF * 3];
    if (arg3 && arg3[0])
        snprintf(full, sizeof(full), "\"%s\" \"%s\" \"%s\" \"%s\"", path, arg1, arg2, arg3);
    else if (arg2 && arg2[0])
        snprintf(full, sizeof(full), "\"%s\" \"%s\" \"%s\"", path, arg1, arg2);
    else if (arg1 && arg1[0])
        snprintf(full, sizeof(full), "\"%s\" \"%s\"", path, arg1);
    else
        snprintf(full, sizeof(full), "\"%s\"", path);
    return run_popen_cwd(msg, msg_sz, full);
}

static int read_current_price(const char *sym, char *price_out, size_t price_sz) {
    price_out[0] = '\0';
    char msg[512];
    if (run_yfin(msg, sizeof(msg), "read_price", sym, NULL, NULL) == 0) {
        char *colon = strrchr(msg, ':');
        if (colon) {
            char *p = colon + 1;
            while (*p == ' ') p++;
            snprintf(price_out, price_sz, "%s", p);
            return 1;
        }
    }
    return 0;
}

static void sync_config_last_lookup(const char *user_hash) {
    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    char acc_path[PATH_BUF];
    snprintf(acc_path, sizeof(acc_path), "%s/usr_acc.%s.txt", project_root, user_hash);
    FILE *f = fopen(acc_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *copy = strdup(line);
        if (copy) {
            char *save = NULL;
            char *tok = strtok_r(copy, ",", &save);
            while (tok && strcmp(tok, "last_lookup") != 0) tok = strtok_r(NULL, ",", &save);
            if (tok) {
                char *sym = strtok_r(NULL, ",", &save);
                char *price = strtok_r(NULL, ",", &save);
                char *t = strtok_r(NULL, ",", &save);
                if (sym && sym[0]) write_kv(config_path, "last_lookup_symbol", sym);
                if (price && price[0]) write_kv(config_path, "last_lookup_price", price);
                if (t && t[0]) write_kv(config_path, "last_lookup_time", t);
            }
            free(copy);
        }
    }
    fclose(f);
}

static float account_balance(const char *user_hash) {
    char acc_path[PATH_BUF];
    snprintf(acc_path, sizeof(acc_path), "%s/usr_acc.%s.txt", project_root, user_hash);
    FILE *f = fopen(acc_path, "r");
    if (!f) return 0.0f;
    char line[MAX_LINE];
    float bal = 0.0f;
    if (fgets(line, sizeof(line), f)) {
        char *tok = strtok(line, ",");
        if (tok && strcmp(tok, "balance") == 0) {
            tok = strtok(NULL, ",");
            if (tok) bal = atof(tok);
        }
    }
    fclose(f);
    return bal;
}

static void add_to_watchlist(const char *user_hash, const char *symbol, char *msg, size_t msg_sz) {
    char acc_path[PATH_BUF];
    snprintf(acc_path, sizeof(acc_path), "%s/usr_acc.%s.txt", project_root, user_hash);
    FILE *f = fopen(acc_path, "r");
    if (!f) {
        snprintf(msg, msg_sz, "No account file yet - run Lookup first");
        return;
    }
    char line[MAX_LINE];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        snprintf(msg, msg_sz, "Empty account file");
        return;
    }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';

    char sym[MAX_LINE];
    snprintf(sym, sizeof(sym), "%s", symbol);
    for (char *p = sym; *p; p++) *p = toupper((unsigned char)*p);

    char *wl = strstr(line, "watchlist");
    char *stocks = strstr(line, "stocks");
    if (!wl || !stocks || stocks < wl) {
        snprintf(msg, msg_sz, "Account file malformed");
        return;
    }

    char *cur = wl + 9;
    while (cur && cur < stocks) {
        char *comma = strchr(cur, ',');
        if (!comma) break;
        char tmp[128];
        size_t l = comma - cur;
        if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
        memcpy(tmp, cur, l);
        tmp[l] = '\0';
        if (strcmp(tmp, sym) == 0) {
            snprintf(msg, msg_sz, "%s already in watchlist", sym);
            return;
        }
        cur = comma + 1;
    }

    char *comma = strchr(wl, ',');
    if (!comma) {
        snprintf(msg, msg_sz, "Account file malformed");
        return;
    }
    size_t ins = (comma - line) + 1;
    char buf[MAX_LINE * 2];
    snprintf(buf, sizeof(buf), "%.*s%s,%s", (int)ins, line, sym, line + ins);
    FILE *w = fopen(acc_path, "w");
    if (!w) {
        snprintf(msg, msg_sz, "Cannot write account file");
        return;
    }
    fputs(buf, w);
    fputc('\n', w);
    fclose(w);
    snprintf(msg, msg_sz, "Added %s to watchlist", sym);
}

static void transfer_funds(const char *direction, const char *amount_str) {
    char broker_state_path[PATH_BUF];
    snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
    char focused_root[PATH_BUF] = "";
    read_kv_str_local(broker_state_path, "focused_project_root", focused_root, sizeof(focused_root));
    if (!focused_root[0]) return;

    char bank_config[PATH_BUF];
    snprintf(bank_config, sizeof(bank_config), "%s/pieces/system/config.txt", focused_root);
    char bank_balance_str[64] = "0.00";
    read_kv_str_local(bank_config, "bank_balance", bank_balance_str, sizeof(bank_balance_str));
    float bank_balance = atof(bank_balance_str);

    char broker_balance_str[64] = "0.00";
    read_kv_str_local(broker_state_path, "broker_balance", broker_balance_str, sizeof(broker_balance_str));
    float broker_balance = atof(broker_balance_str);

    float amount = atof(amount_str);
    if (strcmp(direction, "deposit") == 0) {
        if (amount > bank_balance) amount = bank_balance;
        bank_balance -= amount;
        broker_balance += amount;
    } else {
        if (amount > broker_balance) amount = broker_balance;
        bank_balance += amount;
        broker_balance -= amount;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", bank_balance);
    write_kv(bank_config, "bank_balance", buf);
    snprintf(buf, sizeof(buf), "%.2f", broker_balance);
    write_kv(broker_state_path, "broker_balance", buf);
}

static void handle_command(const char *cmd, char *message, size_t message_sz) {
    char sym[128] = "";
    char amt[128] = "";
    char hash[64] = "";
    get_user_hash(hash, sizeof(hash));

    if (strcmp(cmd, "CHECK_BALANCE") == 0) {
        char broker_state_path[PATH_BUF];
        snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
        char bbal[64] = "0.00";
        read_kv_str_local(broker_state_path, "broker_balance", bbal, sizeof(bbal));
        char bank_config[PATH_BUF];
        snprintf(bank_config, sizeof(bank_config), "%s/pieces/system/config.txt", project_root);
        char kbal[64] = "0.00";
        read_kv_str_local(bank_config, "bank_balance", kbal, sizeof(kbal));
        float acc = hash[0] ? account_balance(hash) : 0.0f;
        snprintf(message, message_sz, "Bank: $%s | Broker: $%s | Broker account: $%.2f", kbal, bbal, acc);
        return;
    }

    if (strcmp(cmd, "DEBUG_LEDGER") == 0) {
        char ledger_path[PATH_BUF];
        snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", project_root);
        const char *display = getenv("DISPLAY");
        if (display && display[0]) {
            char gcmd[PATH_BUF * 2];
            snprintf(gcmd, sizeof(gcmd), "gedit \"%s\" >/dev/null 2>&1 &", ledger_path);
            system(gcmd);
            snprintf(message, message_sz, "Opened master ledger in gedit: %s", ledger_path);
        } else {
            snprintf(message, message_sz, "Ledger: %s", ledger_path);
        }
        return;
    }

    if (!hash[0]) {
        snprintf(message, message_sz, "No user account yet - open Bank first to create one");
        return;
    }

    if (strcmp(cmd, "LOOKUP_STOCK") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        if (!sym[0]) {
            snprintf(message, message_sz, "Type a symbol in the Symbol field first.");
            return;
        }
        run_yfin(NULL, 0, "lookup_stock", hash, sym, NULL);
        char price[64];
        if (read_current_price(sym, price, sizeof(price))) {
            snprintf(message, message_sz, "%s current price: $%s", sym, price);
            sync_config_last_lookup(hash);
        } else {
            snprintf(message, message_sz, "No price cached for %s yet - try again in a moment", sym);
        }
        clear_gui_state_str("sym_input");
        return;
    }

    if (strcmp(cmd, "ADD_CREDIT") == 0) {
        read_gui_state_str("amt_input", amt, sizeof(amt));
        if (!amt[0]) {
            snprintf(message, message_sz, "Enter an amount in the Amount field first.");
            return;
        }
        run_yfin(NULL, 0, "add_credit", hash, amt, NULL);
        snprintf(message, message_sz, "Added $%s credit to broker account", amt);
        clear_gui_state_str("amt_input");
        return;
    }

    if (strcmp(cmd, "BUY_STOCK") == 0 || strcmp(cmd, "SELL_STOCK") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        read_gui_state_str("amt_input", amt, sizeof(amt));
        if (!sym[0] || !amt[0]) {
            snprintf(message, message_sz, "Enter a symbol AND share count (Amount = shares).");
            return;
        }
        if (strcmp(cmd, "BUY_STOCK") == 0) {
            run_yfin(NULL, 0, "buy_stock", hash, sym, amt);
            snprintf(message, message_sz, "Bought %s shares of %s", amt, sym);
        } else {
            run_yfin(NULL, 0, "sell_stock", hash, sym, amt);
            snprintf(message, message_sz, "Sold %s shares of %s", amt, sym);
        }
        clear_gui_state_str("sym_input");
        clear_gui_state_str("amt_input");
        return;
    }

    if (strcmp(cmd, "SELL_OPTIONS") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        read_gui_state_str("amt_input", amt, sizeof(amt));
        if (!sym[0] || !amt[0]) {
            snprintf(message, message_sz, "Enter option index (Symbol) AND contracts (Amount).");
            return;
        }
        run_yfin(NULL, 0, "sell_option_inventory", hash, sym, amt);
        snprintf(message, message_sz, "Sold %s contracts of option %s", amt, sym);
        clear_gui_state_str("sym_input");
        clear_gui_state_str("amt_input");
        return;
    }

    if (strcmp(cmd, "VIEW_PORTFOLIO") == 0) {
        if (run_yfin(message, message_sz, "portfolio_new", hash, NULL, NULL) != 0)
            snprintf(message, message_sz, "Portfolio: no holdings yet");
        return;
    }

    if (strcmp(cmd, "VIEW_PROFIT_LOSS") == 0) {
        if (run_yfin(message, message_sz, "profit_loss", hash, NULL, NULL) != 0)
            snprintf(message, message_sz, "Profit/Loss: (no transactions)");
        return;
    }

    if (strcmp(cmd, "WATCHLIST_ADD") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        if (!sym[0]) {
            snprintf(message, message_sz, "Type a symbol in the Symbol field first.");
            return;
        }
        add_to_watchlist(hash, sym, message, message_sz);
        clear_gui_state_str("sym_input");
        return;
    }

    if (strcmp(cmd, "OPTIONS_PRICING") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        read_gui_state_str("amt_input", amt, sizeof(amt));
        if (!sym[0] || !amt[0]) {
            snprintf(message, message_sz, "Enter symbol (Symbol) AND strike (Amount).");
            return;
        }
        char price[64];
        if (!read_current_price(sym, price, sizeof(price))) {
            snprintf(message, message_sz, "No price for %s - run Lookup first", sym);
            clear_gui_state_str("sym_input");
            clear_gui_state_str("amt_input");
            return;
        }
        ensure_op_link();
        char full[PATH_BUF * 3];
        snprintf(full, sizeof(full),
            "\"%s/ops/+x/options_pricing.+x\" -s \"%s\" -p \"%s\" -k \"%s\" -r 0.05 -v 0.30 -d 0.0 -t 0",
            project_root, sym, price, amt);
        if (run_popen_cwd(message, message_sz, full) != 0)
            snprintf(message, message_sz, "Wrote option_prices.%s.csv (see Sell Options index)", sym);
        clear_gui_state_str("sym_input");
        clear_gui_state_str("amt_input");
        return;
    }

    if (strcmp(cmd, "BUY_OPTIONS") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        read_gui_state_str("amt_input", amt, sizeof(amt));
        if (!sym[0] || !amt[0]) {
            snprintf(message, message_sz, "Enter symbol (Symbol) AND option index (Amount).");
            return;
        }
        char price[64];
        if (!read_current_price(sym, price, sizeof(price))) {
            snprintf(message, message_sz, "No price for %s - run Lookup first", sym);
            clear_gui_state_str("sym_input");
            clear_gui_state_str("amt_input");
            return;
        }
        /* Amount = "index[,contracts]" (contracts default 1). buy_option needs
         * all four args (hash, symbol, index, contracts) - run_yfin only
         * carries three, so build the command directly like OPTIONS_PRICING. */
        char index[64], contracts[64] = "1";
        snprintf(index, sizeof(index), "%s", amt);
        char *comma = strchr(index, ',');
        if (comma) {
            *comma = '\0';
            if (comma[1]) snprintf(contracts, sizeof(contracts), "%s", comma + 1);
        }
        ensure_op_link();
        char full[PATH_BUF * 3];
        snprintf(full, sizeof(full),
            "\"%s/ops/+x/buy_option.+x\" \"%s\" \"%s\" \"%s\" \"%s\"",
            project_root, hash, sym, index, contracts);
        if (run_popen_cwd(message, message_sz, full) != 0)
            snprintf(message, message_sz, "Buy option index %s of %s - broker reported failure", index, sym);
        clear_gui_state_str("sym_input");
        clear_gui_state_str("amt_input");
        return;
    }

    if (strcmp(cmd, "PREDICT_STOCK") == 0) {
        read_gui_state_str("sym_input", sym, sizeof(sym));
        if (!sym[0]) {
            snprintf(message, message_sz, "Type a symbol in the Symbol field first.");
            return;
        }
        if (run_yfin(message, message_sz, "predictions", hash, sym, NULL) != 0)
            snprintf(message, message_sz, "No prediction for %s yet", sym);
        clear_gui_state_str("sym_input");
        return;
    }

    if (strncmp(cmd, "DEPOSIT:", 8) == 0) {
        transfer_funds("deposit", cmd + 8);
        snprintf(message, message_sz, "Deposited $%s to broker", cmd + 8);
        return;
    }

    if (strncmp(cmd, "WITHDRAW:", 9) == 0) {
        transfer_funds("withdraw", cmd + 9);
        snprintf(message, message_sz, "Withdrew $%s from broker", cmd + 9);
        return;
    }

    snprintf(message, message_sz, "Command: %s", cmd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    int keycode = atoi(argv[1]);
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/state.txt", project_root);

    if (keycode == 0) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char current_target[128];
        read_kv_str_local(state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        /* Single-broker simplification: navigation into the broker screen
         * is a static href (no SELECT_BROKER METHOD fires), so seed
         * broker_state.txt idempotently on first entry. Not a marker ping -
         * one-time config init. */
        char broker_state_path[PATH_BUF];
        snprintf(broker_state_path, sizeof(broker_state_path), "%s/pieces/system/broker_state.txt", project_root);
        char selected[64] = "";
        read_kv_str_local(broker_state_path, "selected_broker", selected, sizeof(selected));
        if (!selected[0]) {
            write_kv(broker_state_path, "selected_broker", "yahoo_finance");
            write_kv(broker_state_path, "focused_project_root", project_root);
            write_kv(broker_state_path, "focused_project_id", project_id);
            char bbal[64] = "0.00";
            read_kv_str_local(broker_state_path, "broker_balance", bbal, sizeof(bbal));
            if (!bbal[0]) write_kv(broker_state_path, "broker_balance", "0.00");
        }

        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/broker_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    /* Enter/ESC on a cli_io field are consumed by the parser itself (it saves
     * the target_id to gui_state and toggles typing). The relay still forwards
     * the bare 13/27 to us; dispatching them would mis-fire a METHOD (13 ->
     * the 12th item = DEBUG_LEDGER). They are never button activations. */
    if (keycode == 13 || keycode == 27) return 0;

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    char message[256] = "";

    int resolved_item = 0;
    if (keycode >= '0' && keycode <= '9') resolved_item = (keycode - '0') - 1;
    else if (keycode >= ':' && keycode <= '>') resolved_item = (keycode - ':') + 9;
    else if (keycode > 9 && keycode < 1000) resolved_item = keycode - 1;
    if (resolved_item >= 1 && resolved_item <= item_count) {
        handle_command(items[resolved_item - 1].command, message, sizeof(message));
    }

    if (message[0]) {
        append_message(message);
        mark_screen_changed("broker");
    }

    char piece_id[64] = "broker";
    get_current_piece_id(project_root, piece_id, sizeof(piece_id));
    write_chtpm_bridge(piece_id);
    ping_chtpm_render_marker(project_root);

    return 0;
}
