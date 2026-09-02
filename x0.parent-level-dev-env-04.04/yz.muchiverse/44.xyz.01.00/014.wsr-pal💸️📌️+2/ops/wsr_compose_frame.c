/* wsr_compose_frame - REBUILT to render whichever piece.pdl is the
 * current active_menu_piece's METHOD table generically, matching
 * `wsr_menu_input.c`'s own rebuild (see that file's header comment for
 * the real TPMOS precedent this is modeled on -
 * `chtpm_parser.c`'s `load_dynamic_methods()` + `xlector.pdl`). No
 * per-screen hardcoded render function anymore - one generic renderer,
 * any number of screens, each just another piece.pdl.
 * Self-contained, no shared headers.
 * Usage: wsr_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define access _access
#define F_OK 0
#endif

#define MAX_LINE 512
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    /* Windows: prefer "." when CWD is the project (emoji abs paths break
     * ANSI fopen / MinGW opendir). Linux keeps env/getcwd as before. */
#ifdef _WIN32
    if (access("pieces", F_OK) == 0 && access("projects", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
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

/* Derives "which screen is current" straight from chtpm's own real
 * pieces/display/current_layout.txt export (parse_chtm()'s own "EXPORT
 * CURRENT LAYOUT FOR MODULE HEARTBEAT" write, confirmed by direct read
 * of chtpm_parser_pal.c - fires on the very first launch AND every real
 * `href` transition alike, see xyzos-standards.txt sec. 18) rather than a
 * separately-maintained active_menu_piece field that something has to
 * remember to keep in sync - the exact class of bug that motivated this
 * rewrite. Matches wsr_menu_input.c's own identical helper (duplicated,
 * not shared, per this family's no-shared-headers convention). Falls
 * back to wsr_main_menu if the file is missing/unreadable. */
static void get_current_piece_id(const char *project_root_, char *out, size_t out_sz) {
    snprintf(out, out_sz, "wsr_main_menu");
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

/* XYZOS-STANDARDS §16.3 (ONE WRITER RULE): chtpm_parser_pal is now the
 * SOLE writer of pieces/display/current_frame.txt (this project's own
 * "run" action fully consolidated onto the chtpm flow).
 *
 * CORRECTION (found by directly comparing against mutaclsym's own
 * compose_frame.c, which does NOT show this lag - see xyzos-standards.txt
 * §16.3, corrected): writing ONLY view.txt and waiting for chtpm's own
 * separate state_changed.txt-triggered reload to pick it up (a
 * different process, its own poll cycle) introduces a real, perceptible
 * one-step input lag - the screen only catches up to a keypress once
 * chtpm's OWN loop notices the marker grew, not the instant this op
 * finishes. mutaclsym's compose_frame.c writes BOTH current_frame.txt
 * AND view.txt directly, from the same in-memory buffer, precisely so
 * fresh data is visible IMMEDIATELY, without waiting on chtpm to catch
 * up - confirmed via that file's own header comment (open_memstream,
 * "no read-back and therefore no race window"). The real problem this
 * op actually had was never "two writers" per se - it was writing
 * CONTENT THAT VISIBLY CONFLICTED with chtpm's own composed frame (a
 * duplicate hand-drawn menu next to chtpm's real ${piece_methods}
 * buttons). Restored the dual-write (both outputs get the identical
 * bytes, no read-back, matching mutaclsym exactly); the numbered
 * METHOD-table menu block stays gone from BOTH outputs (chtpm's own
 * ${piece_methods} renders it as real buttons - this op has no reason
 * to draw it as text anywhere now). */
static FILE *g_out = NULL;
static FILE *g_view_out = NULL;
static void border(FILE *out) {
    (void)out;
    if (g_out) { fputc('+', g_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_out); fputc('+', g_out); fputc('\n', g_out); }
    if (g_view_out) { fputc('+', g_view_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out); fputc('+', g_view_out); fputc('\n', g_view_out); }
}
static void line(FILE *out, const char *content) {
    (void)out;
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    if (g_out) {
        fprintf(g_out, "|%.*s", len, content);
        for (int i = len; i < BOX_W; i++) fputc(' ', g_out);
        fputc('|', g_out);
        fputc('\n', g_out);
    }
    if (g_view_out) {
        fprintf(g_view_out, "|%.*s", len, content);
        for (int i = len; i < BOX_W; i++) fputc(' ', g_view_out);
        fputc('|', g_view_out);
        fputc('\n', g_view_out);
    }
}
static void blank(FILE *out) { line(out, ""); }

/* Real game.c reads data/news.txt and scrolls through its top movers
 * in the main menu banner (confirmed by reading game.c directly - the
 * "FINANCIAL NEWS HEADLINES [...]" line). Our render happens once per
 * End Turn rather than on a continuous terminal refresh, so there's no
 * scroll-counter to port - just show the single biggest mover (the
 * first data line after the 2-line header, since wsr_news_op.c already
 * sorts by |change%| descending). */
static void read_top_news_line(const char *project_root, char *out, size_t out_sz) {
    out[0] = '\0';
    char news_path[PATH_BUF];
    snprintf(news_path, sizeof(news_path), "%s/projects/wsr-pal/pieces/wsr_menu/news.txt", project_root);
    FILE *f = fopen(news_path, "r");
    if (!f) return;
    char line1[MAX_LINE];
    int n = 0;
    while (fgets(line1, sizeof(line1), f)) {
        n++;
        if (n <= 2) continue; /* skip the 2 header lines */
        line1[strcspn(line1, "\n")] = '\0';
        snprintf(out, out_sz, "%s", line1);
        break;
    }
    fclose(f);
}

/* Real bug, found by direct interface testing (2026-07-16): this used
 * to scan via opendir()/readdir(), which returns entries in raw
 * filesystem/inode order - NOT alphabetical. scripts/active_corp.sh
 * (what player_trade.+x/corp_action.+x/etc actually resolve "the
 * active corp" through, via piece.pdl RUN: commands) uses sorted `ls`.
 * The two could disagree on which corp index N means, so what a
 * player saw as "Active: corp_X" here was not always the corp their
 * Trade/Management/Financing actions actually hit. Fixed by using the
 * exact same sorted-name resolution both places share.
 *
 * Pure C (sorted dirent) — no shell — so Windows MinGW and Linux agree. */
static int cmp_strptr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int get_nth_corp(char *out, size_t out_sz, int n) {
    char *names[512];
    int count = 0;

#ifdef _WIN32
    /* MinGW opendir/readdir returns 0 entries on some OneDrive paths;
     * _findfirst is reliable (same enumeration PowerShell uses). */
    char pattern[PATH_BUF];
    snprintf(pattern, sizeof(pattern), "%s/projects/wsr-pal/pieces/*", project_root);
    for (char *p = pattern; *p; p++) if (*p == '/') *p = '\\';
    struct _finddata_t fi;
    intptr_t h = _findfirst(pattern, &fi);
    if (h != -1) {
        do {
            if (fi.name[0] == '.') continue;
            if (strncmp(fi.name, "corp_", 5) != 0) continue;
            if (count < 512) {
                names[count] = strdup(fi.name);
                if (names[count]) count++;
            }
        } while (_findnext(h, &fi) == 0);
        _findclose(h);
    }
#else
    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/wsr-pal/pieces", project_root);
    DIR *d = opendir(pieces_dir);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < 512) {
        if (strncmp(ent->d_name, "corp_", 5) != 0) continue;
        names[count] = strdup(ent->d_name);
        if (names[count]) count++;
    }
    closedir(d);
#endif
    if (count == 0) return 0;
    qsort(names, (size_t)count, sizeof(char *), cmp_strptr);

    int found = 0;
    if (n >= 0 && n < count) {
        snprintf(out, out_sz, "%s", names[n]);
        found = 1;
    }
    for (int i = 0; i < count; i++) free(names[i]);
    return found;
}

/* Real 5-step Startup New Corp wizard render (see wsr_wizard_input.c) -
 * shows the current step's prompt + (for the two numeric-list steps)
 * the real choices read live from the same industries/governments
 * files the original game itself reads, plus the current typed buffer
 * with a text cursor, matching TPMOS's own real cli_io visual
 * convention in spirit (researched directly, not a literal port). */
static void compose_wizard_frame(const char *menu_state_path, FILE *out, const char *last_message) {
    int step = read_kv_int(menu_state_path, "prompt_step", 1);
    char buf[MAX_LINE];
    read_kv_str(menu_state_path, "prompt_buffer", buf, sizeof(buf));

    char rowbuf[BOX_W + 1];
    border(out);
    line(out, "  Startup New Corp");
    border(out);
    blank(out);

    if (step == 1 || step == 2) {
        char list_path[PATH_BUF];
        if (step == 1) {
            snprintf(list_path, sizeof(list_path), "%s/Mar$.$treetRace.wsr]Q]k32/corporations/36_industries_wsr.txt", project_root);
        } else {
            snprintf(list_path, sizeof(list_path), "%s/Mar$.$treetRace.wsr]Q]k32/governments/generated/gov-list.txt", project_root);
        }
        FILE *lf = fopen(list_path, "r");
        if (lf) {
            char l[MAX_LINE];
            int n = 0, first = 1, shown = 0;
            while (fgets(l, sizeof(l), lf) && shown < 14) {
                if (step == 2 && first) { first = 0; continue; }
                l[strcspn(l, "\n")] = '\0';
                if (!l[0]) continue;
                n++;
                if (step == 1) {
                    char *dot = strchr(l, '.');
                    const char *name = dot ? dot + 1 : l;
                    while (*name == ' ') name++;
                    snprintf(rowbuf, sizeof(rowbuf), "  %2d. %s", n, name);
                } else {
                    char *tab = strchr(l, '\t');
                    if (!tab) tab = strchr(l, ' ');
                    char tmp[MAX_LINE];
                    snprintf(tmp, sizeof(tmp), "%s", tab ? tab + 1 : l);
                    char *tab2 = strchr(tmp, '\t');
                    if (tab2) *tab2 = '\0';
                    snprintf(rowbuf, sizeof(rowbuf), "  %2d. %s", n, tmp);
                }
                line(out, rowbuf);
                shown++;
            }
            fclose(lf);
        }
        blank(out);
    }

    const char *prompts[] = {
        "", "Enter industry number:", "Enter country number:",
        "Enter funding amount (millions):", "Enter corporation name (5-25 chars):",
        "Enter ticker symbol (1-6 chars):"
    };
    snprintf(rowbuf, sizeof(rowbuf), "  %s", (step >= 1 && step <= 5) ? prompts[step] : "");
    line(out, rowbuf);
    snprintf(rowbuf, sizeof(rowbuf), "  > %s_", buf);
    line(out, rowbuf);
    blank(out);
    snprintf(rowbuf, sizeof(rowbuf), "  %s", last_message);
    line(out, rowbuf);
    line(out, "  (type your answer, Enter to confirm, ESC to cancel)");
    border(out);
}

/* CHTPM VIEW BRIDGE (see chtpm-to-pal-layout-plan.txt §8 and
 * xyzos-standards.txt §7/!.wsr-pal-refactor.txt §2 for the why): a real
 * .chtpm menu shell's own `${game_map}` var is populated by load_vars()'s
 * real, unmodified GENERIC VIEW LOADING logic, which checks
 * pieces/apps/player_app/view.txt as one of its own candidate paths -
 * writing this file is the ONLY thing needed to let chtpm display this
 * project's live content, no chtpm_parser_pal.c patch required.
 *
 * view.txt is the ONLY output now (see border()/line()/blank() above,
 * §16.3) - the numbered METHOD-table menu block that used to render
 * here is dropped entirely, since chtpm's own ${piece_methods}
 * placeholder (wsr.chtpm) already renders that same menu as real,
 * clickable buttons; drawing it a second time as inert text would be a
 * visible duplicate. */
static void ping_chtpm_render_marker(void) {
    /* CHTPM RENDER-TRIGGER MARKER: chtpm_parser_pal.c's own main loop
     * only recomposes when one of a fixed set of marker files grows -
     * growing pieces/apps/player_app/state_changed.txt (one of the
     * markers it already checks) makes it re-run load_vars() (which is
     * what re-reads view.txt), so the world's own ticking shows up
     * even when chtpm itself received no keypress this cycle. */
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char menu_state_path[PATH_BUF], out_path[PATH_BUF];
    snprintf(menu_state_path, sizeof(menu_state_path), "%s/projects/wsr-pal/pieces/wsr_menu/state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/current_frame.txt", project_root);

    char view_path[PATH_BUF];
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    int prompt_active = read_kv_int(menu_state_path, "prompt_active", 0);
    if (prompt_active) {
        char last_message[MAX_LINE];
        read_kv_str(menu_state_path, "last_message", last_message, sizeof(last_message));
        g_out = fopen(out_path, "w");
        g_view_out = fopen(view_path, "w");
        if (!g_out || !g_view_out) return 1;
        compose_wizard_frame(menu_state_path, NULL, last_message);
        fclose(g_out); g_out = NULL;
        fclose(g_view_out); g_view_out = NULL;
        ping_chtpm_render_marker();
        return 0;
    }

    int active_corp_index = read_kv_int(menu_state_path, "active_corp_index", 0);
    int turn_number = read_kv_int(menu_state_path, "turn_number", 0);
    char last_message[MAX_LINE], active_menu_piece[128];
    read_kv_str(menu_state_path, "last_message", last_message, sizeof(last_message));
    get_current_piece_id(project_root, active_menu_piece, sizeof(active_menu_piece));

    char corp_id[64] = "";
    get_nth_corp(corp_id, sizeof(corp_id), active_corp_index);
    char corp_state_path[PATH_BUF];
    snprintf(corp_state_path, sizeof(corp_state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, corp_id);
    int cash = read_kv_int(corp_state_path, "cash", 0);
    char stock_price[MAX_LINE] = "", owned_by[MAX_LINE] = "";
    read_kv_str(corp_state_path, "stock_price", stock_price, sizeof(stock_price));
    read_kv_str(corp_state_path, "owned_by", owned_by, sizeof(owned_by));

    char pop_state_path[PATH_BUF], weather_state_path[PATH_BUF];
    snprintf(pop_state_path, sizeof(pop_state_path), "%s/projects/wsr-pal/pieces/pop_downtown/state.txt", project_root);
    snprintf(weather_state_path, sizeof(weather_state_path), "%s/projects/wsr-pal/pieces/weather_global/state.txt", project_root);
    int population = read_kv_int(pop_state_path, "total_population", 0);
    int temperature = read_kv_int(weather_state_path, "temperature", 0);

    /* The numbered menu row rendering that used to live here (reading
     * active_menu_piece's own piece.pdl via load_menu_items(), drawing a
     * hand-drawn "[>] N. [label]" text block) is gone entirely: chtpm's
     * own ${piece_methods} placeholder (wsr.chtpm) renders that same
     * METHOD table as real, clickable buttons now (§16 of
     * xyzos-standards.txt) - drawing it here too would be a visible
     * duplicate, and this op has no other use for that data. */

    g_out = fopen(out_path, "w");
    g_view_out = fopen(view_path, "w");
    if (!g_out || !g_view_out) return 1;

    char rowbuf[BOX_W + 1];
    border(NULL);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  %s   Turn:%d   Active: %s", active_menu_piece, turn_number, corp_id[0] ? corp_id : "(none)");
#pragma GCC diagnostic pop
    line(NULL, rowbuf);
    border(NULL);

    if (strcmp(active_menu_piece, "wsr_main_menu") == 0) {
        char player_state_path[PATH_BUF];
        snprintf(player_state_path, sizeof(player_state_path), "%s/projects/wsr-pal/pieces/player_you/state.txt", project_root);
        int player_cash = read_kv_int(player_state_path, "cash", 0);

        line(NULL, "----------------------------------");
        line(NULL, "Your Wallet:");
        snprintf(rowbuf, sizeof(rowbuf), "Cash..........           %d", player_cash);
        line(NULL, rowbuf);
        line(NULL, "----------------------------------");
        line(NULL, "Active Corp Balance Sheet:");
        snprintf(rowbuf, sizeof(rowbuf), "Cash (CD's)..            %d", cash);
        line(NULL, rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "Stock Price...           %s", stock_price[0] ? stock_price : "N/A");
        line(NULL, rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "Owned By......           %s", owned_by[0] ? owned_by : "(independent)");
        line(NULL, rowbuf);
        line(NULL, "----------------------------------------------");
        char news_line[MAX_LINE];
        read_top_news_line(project_root, news_line, sizeof(news_line));
        snprintf(rowbuf, sizeof(rowbuf), "FINANCIAL NEWS [%s]", news_line[0] ? news_line : "No significant news");
        line(NULL, rowbuf);
        blank(NULL);
        line(NULL, "WORLD STATUS");
        snprintf(rowbuf, sizeof(rowbuf), "Population: %d          Temperature: %dC", population, temperature);
        line(NULL, rowbuf);
    }
    blank(NULL);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  > %s", last_message[0] ? last_message : "");
#pragma GCC diagnostic pop
    line(NULL, rowbuf);
    line(NULL, "  (type digit(s), Enter to select, ctrl+c to quit)");
    border(NULL);

    fclose(g_out); g_out = NULL;
    fclose(g_view_out); g_view_out = NULL;
    ping_chtpm_render_marker();
    return 0;
}
