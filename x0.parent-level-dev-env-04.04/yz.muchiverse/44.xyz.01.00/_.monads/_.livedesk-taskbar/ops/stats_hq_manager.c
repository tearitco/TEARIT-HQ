/* stats_hq_manager.c — stats-hq's real MANAGER binary (2026-08-25, full
 * TPMOS-compliant rebuild — au11-hq/TPMOS-COMPLIANCE-DEBT.md's own worst
 * finding: the OLD stats-hq had no manager at all, just open_stats_hq.sh
 * doing inline `grep -oE` scraping + bash `printf`'d `<tabbar>` XML,
 * with tabs that rendered but never responded to clicks — direct
 * instruction "do this completely tpmos compliant").
 *
 * Real, direct precedent this is copied FROM (not invented):
 * khtpm_hq_manager.c (db-hq's own manager) — same init/poll-loop shape,
 * same atomic tmp-write-then-rename publish convention. Publishes into
 * the EXACT state-file format khtpm_core_render.c's own
 * dbhq_load_common_events() already parses (one plain-text line per
 * item) — stats-hq rides db-hq's real, already-proven sidebar+panel+
 * item-click machinery for free (see khtpm_core_render.c's
 * class-dispatch loop, g_is_stats_hq branch, 2026-08-25), rather than a
 * second bespoke rendering path.
 *
 * Real business logic owned here (moved out of open_stats_hq.sh
 * entirely): scans %.harnesses/harnecient-fsm/session-stats for .txt files,
 * parses each session's real fields (Date/Total Turns/User Messages/
 * AI Responses/Tool Calls Detected — same fields the old bash script
 * scraped, now real, testable C instead of shell regex), newest-first,
 * publishes one descriptive line per session. Clicking a sidebar item
 * (already a real, working, generic feature of db-hq mode) sets that
 * line as the panel's displayed text — a real single-session summary,
 * not the richer multi-field panel a Tier-B UI could eventually show,
 * but a real, testable, compliant, WORKING replacement for the old
 * dead tabs, achievable without a second full rendering-path build. */
#define _DEFAULT_SOURCE  /* usleep() under -std=c11 strict mode, matches khtpm_hq_manager.c's own convention */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PATH_BUF 4096
#define MAX_SESSIONS 64

static char g_house_root[PATH_BUF];
static char g_state_path[PATH_BUF];
static char g_stats_dir[PATH_BUF];
static char g_summary_path[PATH_BUF];
static char g_compute_script[PATH_BUF];

typedef struct {
    char date[64];
    long mtime;
    int total_turns;
    int user_msgs;
    int ai_msgs;
    int tool_calls;
} Session;

/* Minimal "Field: N" / "Field: text" line scanner — same fields the old
 * open_stats_hq.sh scraped via `grep -oE 'Field:\s*[0-9]+'`, now real C. */
static void scan_field_int(const char *buf, const char *field, int *out) {
    const char *p = strstr(buf, field);
    if (!p) return;
    p += strlen(field);
    while (*p == ' ' || *p == ':' ) p++;
    *out = atoi(p);
}
static void scan_field_str(const char *buf, const char *field, char *out, size_t outsz) {
    const char *p = strstr(buf, field);
    if (!p) return;
    p += strlen(field);
    while (*p == ' ' || *p == ':') p++;
    size_t i = 0;
    while (p[i] && p[i] != '\n' && p[i] != '\r' && i < outsz - 1) { out[i] = p[i]; i++; }
    out[i] = '\0';
}

/* Finds the first digit (or '.') after `field` and parses it as a
 * double - handles the real summary's own "~15300", "67.1%", "~$0.2295"
 * shapes (leading '~'/'$' before the number, trailing '%'/text after -
 * skip everything that isn't a digit, '.', or '-'). */
static double scan_number_after(const char *buf, const char *field) {
    const char *p = strstr(buf, field);
    if (!p) return -1.0;
    p += strlen(field);
    while (*p && *p != '\n' && !((*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) p++;
    if (!*p || *p == '\n') return -1.0;
    return atof(p);
}

/* REAL FIX 2026-08-25 (direct live report: "it used to show how much
 * money was saved from token calls") - the OLD open_stats_hq.sh had a
 * SEPARATE aggregate mode (no session_id given) reading compute_stats.sh's
 * own stats_summary.txt, never wired into this rebuild's per-session
 * scan at all (a real, separate data source, not a session record) -
 * this restores it as sidebar entry 0, "Overall Stats", reusing the
 * EXACT same 6-field pipe-delimited record shape every session line
 * already uses (see publish_sessions()) so the renderer's existing
 * stats_populate_panel() needs only a label swap for this one entry,
 * not a second parsing/display path. */
static void write_overall_line(FILE *f) {
    if (g_compute_script[0]) {
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "sh '%s' >/dev/null 2>&1", g_compute_script);
        int rc = system(cmd);
        (void)rc;
    }
    FILE *sf = fopen(g_summary_path, "r");
    if (!sf) return;
    char buf[4096];
    size_t rd = fread(buf, 1, sizeof(buf) - 1, sf);
    buf[rd] = '\0';
    fclose(sf);

    double rate = scan_number_after(buf, "Delegation rate:");
    double calls = scan_number_after(buf, "Real model calls delegated:");
    double passes = scan_number_after(buf, "PASS:");
    double tok_saved = scan_number_after(buf, "Estimated tokens saved:");
    double dollars = scan_number_after(buf, "Estimated $ saved");
    if (rate < 0 && calls < 0) return; /* no real summary yet - skip, don't publish garbage */

    fprintf(f, "Overall Stats|%.1f|%.0f|%.0f|%.0f|%.4f\n",
            rate < 0 ? 0.0 : rate, calls < 0 ? 0.0 : calls,
            passes < 0 ? 0.0 : passes, tok_saved < 0 ? 0.0 : tok_saved,
            dollars < 0 ? 0.0 : dollars);
}

static void publish_sessions(void) {
    Session sessions[MAX_SESSIONS];
    int n = 0;

    DIR *d = opendir(g_stats_dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && n < MAX_SESSIONS) {
            if (de->d_name[0] == '.') continue;
            size_t len = strlen(de->d_name);
            if (len < 4 || strcmp(de->d_name + len - 4, ".txt") != 0) continue;

            char path[PATH_BUF];
            snprintf(path, sizeof(path), "%s/%s", g_stats_dir, de->d_name);
            struct stat st;
            if (stat(path, &st) != 0) continue;

            FILE *f = fopen(path, "r");
            if (!f) continue;
            char buf[4096];
            size_t rd = fread(buf, 1, sizeof(buf) - 1, f);
            buf[rd] = '\0';
            fclose(f);

            Session *s = &sessions[n];
            memset(s, 0, sizeof(*s));
            s->mtime = (long)st.st_mtime;
            scan_field_str(buf, "Date:", s->date, sizeof(s->date));
            if (!s->date[0]) snprintf(s->date, sizeof(s->date), "%s", de->d_name);
            scan_field_int(buf, "Total Turns:", &s->total_turns);
            scan_field_int(buf, "User Messages:", &s->user_msgs);
            scan_field_int(buf, "AI Responses:", &s->ai_msgs);
            scan_field_int(buf, "Tool Calls Detected:", &s->tool_calls);
            n++;
        }
        closedir(d);
    }

    /* newest-first, by real mtime — not alphabetical (db-hq's own
     * common-events sort is alphabetical, correct for names; session
     * dates need real recency order, so this diverges deliberately,
     * not by oversight). */
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sessions[j].mtime > sessions[i].mtime) {
                Session t = sessions[i]; sessions[i] = sessions[j]; sessions[j] = t;
            }

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *f = fopen(tmp_path, "w");
    if (!f) return;
    write_overall_line(f); /* sidebar entry 0 - see its own header comment */
    if (n == 0) {
        fprintf(f, "(no sessions yet)\n");
    }
    /* REAL FIX 2026-08-25 (direct live report: "i was hoping it was more
     * human readable like before") - was one combined display string;
     * now raw pipe-delimited FIELDS instead, so the renderer can lay
     * them back out as separate itemized lines (Session:/User Messages:/
     * AI Responses:/Total Turns:/Tool Calls+Delegation:) matching the
     * OLD template's real multi-line panel exactly, not a condensed
     * one-liner. Sidebar labels (the part before the first "|") stay
     * short/clean either way - see dbhq_inject_sidebar_items()'s own
     * g_is_stats_hq branch. */
    for (int i = 0; i < n; i++) {
        double pct = sessions[i].total_turns > 0
            ? (100.0 * sessions[i].tool_calls / sessions[i].total_turns) : 0.0;
        fprintf(f, "%s|%d|%d|%d|%d|%.1f\n",
                sessions[i].date, sessions[i].total_turns, sessions[i].user_msgs,
                sessions[i].ai_msgs, sessions[i].tool_calls, pct);
    }
    fclose(f);
    rename(tmp_path, g_state_path);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "stats_hq_manager: usage: <house_root>\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_state_path, sizeof(g_state_path), "%s/#.desktop/stats_hq_common_events.state.txt", g_house_root);
    snprintf(g_stats_dir, sizeof(g_stats_dir), "%s/%%.harnesses/harnecient-fsm/session-stats", g_house_root);
    snprintf(g_summary_path, sizeof(g_summary_path), "%s/%%.harnesses/harnecient-fsm/stats_summary.txt", g_house_root);
    snprintf(g_compute_script, sizeof(g_compute_script), "%s/%%.harnesses/harnecient-fsm/compute_stats.sh", g_house_root);

    for (;;) {
        publish_sessions();
        usleep(1000000); /* 1s poll - a stats dashboard changes far less often than db-hq's own 400ms need */
    }
    return 0;
}
