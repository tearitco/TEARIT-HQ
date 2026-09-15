/* dsr_manager.c - real, separate manager process for the DSR (Desk
 * Street Raider) toy. Same house convention every other real HQ app
 * uses (db-hq-pal, chat-hai, network-browser): a real, compiled
 * manager owns state, publishes a plain key=value projection
 * (state/ui.txt) the generic khtpm_core_render.c renders through its
 * already-generic tag vocabulary - zero new per-project C in the
 * renderer itself.
 *
 * Direct instruction, 2026-09-14: "u can make the toy, and even try to
 * make the display. theres no difference between its gui and
 * 014.wsr-pal...+2/ cept it should be in x11-hq format... can we do
 * that first?" - this is the DISPLAY-FIRST pass: real field values
 * (Turn/Active corp/Wallet/Balance Sheet/News/World Status) sourced
 * from a real, editable state file (dsr_state.pdl, NOT hardcoded here)
 * matching the original wsr-pal's own wsr_compose_frame.c field shape
 * exactly (see that file's own "Cash.........." etc. line formats),
 * and the real 34-item action menu (order matches the direct frame
 * dump the user pasted, not piece.pdl's own unordered METHOD list -
 * see this file's own header for the sequence). Every action is a
 * real, honest `action="void"` stub for now (same convention castle's
 * own menu_gameplay.chtpm uses) - the ledger-driven economy mechanic
 * itself (TEST-GAMES-ROADMAP.md §6) is separate, staged, later work.
 * The fold/collapse ("[^] Actions" vs "[>] Actions [+]") interaction
 * the original ASCII UI has is NOT reproduced yet either - this pass
 * is a flat, always-visible action list, a real, deliberate
 * simplification recorded here and in
 * xyzfs/.../home/projects/dsr/NOTES.md, not a bug.
 *
 * Usage: dsr_manager.+x <house_root>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define PB 4096

static void read_pdl_kv(const char *path, const char *key, char *out, size_t sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, key);
        if (!p) continue;
        char *after = p + klen;
        while (*after == ' ') after++;
        if (*after != '|' && *after != '=') continue;
        after++;
        while (*after == ' ') after++;
        char *end = after + strlen(after);
        while (end > after && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) end--;
        size_t n = (size_t)(end - after);
        if (n >= sz) n = sz - 1;
        memcpy(out, after, n);
        out[n] = '\0';
        break;
    }
    fclose(f);
}

/* REAL, NEW 2026-09-15, direct instruction ("now lets fill in the op
 * buttons by category") - the flat 33-row list (kept below, unchanged,
 * for any future caller that still wants "every action in one list")
 * is now ALSO split into the 4 real category boxes the WSR 8.12
 * Trading Desk screenshot itself shows (Research Menus and Tools/
 * Transactions/Other/Quick Search Functions) - dsr.xhtpm's own real
 * template renders each as its own `<panel class="dsr-box">` in the
 * wrap-grid now, replacing the old single flat Actions panel (see
 * NOTES.md's own "Grouped-box layout" entry). Categorization is a real
 * semantic grouping by what each action DOES (research/reporting vs.
 * a real money-moving transaction vs. administrative/navigational vs.
 * the one real search action), not derived from any WSR data file -
 * piece.pdl's own METHOD list has no category field, this house's own
 * design-doc plan explicitly left the actual split to be worked out
 * here. File/Game Options/Settings/Help are deliberately left OUT of
 * every category box - dsr.xhtpm's own `<tabbar>` already represents
 * them, so keeping them ALSO in a category box would be a real, visible
 * duplication, not a fresh action. */
static const char *ACTION_LABELS[] = {
    "File", "Game Options", "Settings", "Help", "Select Player",
    "Culture", "Entity Info", "Select Corp.", "History", "General", "Tools",
    "Private", "Other Trans", "End Turn", "Misc. Menu", "Chart", "Auto",
    "Watchlist", "TICKER", "Research Report", "List Portfolio",
    "Financial Profile", "List Options", "Earnings Report",
    "Shareholder List", "My Corporations", "New Game",
    "Buy/Sell", "List Futures", "Financing", "Management", "DB Search", "Back"
};
#define N_ACTIONS (int)(sizeof(ACTION_LABELS) / sizeof(ACTION_LABELS[0]))
#define NAV_SEPARATOR_AFTER 27 /* real "--- Navigation ---" row appears before index 27 (27 real rows above it) */

static const char *RESEARCH_LABELS[] = {
    "Research Report", "List Portfolio", "Financial Profile",
    "List Options", "Earnings Report", "Shareholder List",
    "My Corporations", "Chart", "Watchlist", "TICKER", "History",
    "Entity Info", "Culture", "General", "Tools"
};
#define N_RESEARCH (int)(sizeof(RESEARCH_LABELS) / sizeof(RESEARCH_LABELS[0]))

static const char *TRANSACTION_LABELS[] = {
    "Buy/Sell", "Financing", "Private", "Other Trans", "List Futures",
    "Management"
};
#define N_TRANSACTIONS (int)(sizeof(TRANSACTION_LABELS) / sizeof(TRANSACTION_LABELS[0]))

/* REAL, NEW 2026-09-15, direct live report ("db search doesn't need
 * its own pane. just put it under other") - "Quick Search Functions"
 * was a real 1-item box (DB Search alone); folded into Other instead
 * of keeping a whole box for one row. */
static const char *OTHER_LABELS[] = {
    "Select Player", "Select Corp.", "Misc. Menu", "Auto", "New Game",
    "End Turn", "Back", "DB Search"
};
#define N_OTHER (int)(sizeof(OTHER_LABELS) / sizeof(OTHER_LABELS[0]))

static void write_small_file(const char *path, const char *content) {
    char tmp[PB];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
    rename(tmp, path);
}

static void publish(const char *house_root, const char *state_path) {
    char turn[32] = "0", active_corp[64] = "", player_cash[32] = "0";
    char corp_cash[32] = "0", corp_stock_price[32] = "", corp_owned_by[64] = "";
    char news_ticker[16] = "", news_price[32] = "", news_pct[32] = "";
    char population[32] = "0", temperature[32] = "0";

    read_pdl_kv(state_path, "turn", turn, sizeof(turn));
    read_pdl_kv(state_path, "active_corp", active_corp, sizeof(active_corp));
    read_pdl_kv(state_path, "player_cash", player_cash, sizeof(player_cash));
    read_pdl_kv(state_path, "corp_cash", corp_cash, sizeof(corp_cash));
    read_pdl_kv(state_path, "corp_stock_price", corp_stock_price, sizeof(corp_stock_price));
    read_pdl_kv(state_path, "corp_owned_by", corp_owned_by, sizeof(corp_owned_by));
    read_pdl_kv(state_path, "news_ticker", news_ticker, sizeof(news_ticker));
    read_pdl_kv(state_path, "news_price", news_price, sizeof(news_price));
    read_pdl_kv(state_path, "news_pct", news_pct, sizeof(news_pct));
    read_pdl_kv(state_path, "population", population, sizeof(population));
    read_pdl_kv(state_path, "temperature", temperature, sizeof(temperature));

    char body[PB * 4];
    size_t off = 0;
    off += (size_t)snprintf(body + off, sizeof(body) - off,
        "turn=%s\n"
        "active_corp=%s\n"
        "player_cash=%s\n"
        "corp_cash=%s\n"
        "corp_stock_price=%s\n"
        "corp_owned_by=%s\n"
        "news_ticker=%s\n"
        "news_price=%s\n"
        "news_pct=%s\n"
        "population=%s\n"
        "temperature=%s\n"
        "n_actions=%d\n",
        turn, active_corp, player_cash, corp_cash, corp_stock_price,
        corp_owned_by, news_ticker, news_price, news_pct, population,
        temperature, N_ACTIONS);

    /* REAL FIX, direct live report ("missing chrome headers" led to
     * discovering this too, while dumping a real frame to verify): the
     * <repeat bind="act"> in dsr.xhtpm resolves each field as
     * ${act.label}, which this house's var-substitution engine matches
     * against a published `act_<i>_label` key - NOT `action_<i>_label`
     * (confirmed against db-hq-pal's own real bind="row" ->
     * `row_<i>_text` convention, dashboard.xhtpm line 54/74). Var
     * prefix must equal the `bind=` name exactly. */
    for (int i = 0; i < N_ACTIONS && off < sizeof(body) - 256; i++) {
        off += (size_t)snprintf(body + off, sizeof(body) - off,
            "act_%d_label=%s\n"
            "act_%d_sep_before=%d\n",
            i, ACTION_LABELS[i], i, (i == NAV_SEPARATOR_AFTER) ? 1 : 0);
    }

    /* REAL, NEW 2026-09-15 - the 3 real category boxes (see this file's
     * own header comment on RESEARCH_LABELS[] etc.); same var-prefix
     * convention as act_N_label above (bind= name must equal the
     * published prefix exactly). */
    off += (size_t)snprintf(body + off, sizeof(body) - off,
        "n_research=%d\nn_transactions=%d\nn_other=%d\n",
        N_RESEARCH, N_TRANSACTIONS, N_OTHER);
    for (int i = 0; i < N_RESEARCH && off < sizeof(body) - 256; i++)
        off += (size_t)snprintf(body + off, sizeof(body) - off,
            "res_%d_label=%s\n", i, RESEARCH_LABELS[i]);
    for (int i = 0; i < N_TRANSACTIONS && off < sizeof(body) - 256; i++)
        off += (size_t)snprintf(body + off, sizeof(body) - off,
            "trn_%d_label=%s\n", i, TRANSACTION_LABELS[i]);
    for (int i = 0; i < N_OTHER && off < sizeof(body) - 256; i++)
        off += (size_t)snprintf(body + off, sizeof(body) - off,
            "oth_%d_label=%s\n", i, OTHER_LABELS[i]);

    char ui_path[PB];
    snprintf(ui_path, sizeof(ui_path), "%s/&.hq-apps/dsr/state/ui.txt", house_root);
    write_small_file(ui_path, body);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: dsr_manager.+x <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];
    char state_path[PB];
    snprintf(state_path, sizeof(state_path), "%s/&.hq-apps/dsr/state/dsr_state.pdl", house_root);

    struct stat st;
    time_t last_mtime = 0;
    publish(house_root, state_path); /* always publish once on startup, even if the state file is untouched */
    if (stat(state_path, &st) == 0) last_mtime = st.st_mtime;

    for (;;) {
        if (stat(state_path, &st) == 0 && st.st_mtime != last_mtime) {
            last_mtime = st.st_mtime;
            publish(house_root, state_path);
        }
        struct timespec ts = {0, 400 * 1000 * 1000L};
        nanosleep(&ts, NULL);
    }
    return 0;
}
