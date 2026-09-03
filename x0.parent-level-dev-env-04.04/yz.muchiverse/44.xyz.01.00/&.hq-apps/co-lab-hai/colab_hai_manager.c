#define _POSIX_C_SOURCE 200809L
/* colab_hai_manager.c - real manager for "collab-hai", a live,
 * human-supervised multi-agent chat channel (2026-09-02, direct
 * request: "u should plan and build this 'collab-hai' app under hai
 * tb ... it should allow for more agents in the convo").
 *
 * Real, deliberate difference from chat-hai: chat-hai orchestrates
 * several LLM personas via real API calls in a round-robin - it
 * DRIVES the conversation. This app is for real, already-autonomous
 * agents (Sonnet, Grok, later opencode/kilo) that each run their own
 * separate real process/session and just need a shared, visible
 * channel with a human approval gate - this manager never calls any
 * LLM itself, it only watches files and renders. Shape copied
 * directly from network_browser_manager.c's own proven contract
 * (poll loop, atomic publish, one pending request line consumed then
 * cleared, generic sidebar+panel+scrolllist+cli_io projection) per
 * CENTROID_GOLD_STD.md §3 rule 2 - no new UI concept, just new
 * business logic.
 *
 * Real state, all under <house_root>/#.desktop/collab_hai/:
 *   incoming.txt   - any agent APPENDS one real line here to speak:
 *                    "<agent_id>|<message>" (pipe/newline-escaped by
 *                    the poster - see ops/colab_hai_post.sh). This
 *                    manager is the ONLY consumer; it timestamps each
 *                    new line and moves it into pending.txt, then
 *                    truncates incoming.txt back to empty. Real,
 *                    deliberate design: an agent NEVER writes directly
 *                    to pending.txt or conversation.txt - only this
 *                    one, append-only, "yell into the room" file, so
 *                    two agents posting at once can never corrupt the
 *                    approval queue (this manager owns all ordering).
 *   pending.txt    - FIFO of "<ts>|<agent_id>|<message>" awaiting the
 *                    human's approval, oldest first, one line = one
 *                    message. Real approval gate: nothing here reaches
 *                    conversation.txt without a real "approve:"
 *                    request below.
 *   rejected.txt   - append-only audit log of rejected messages (real
 *                    file-based-state discipline - a decision, even a
 *                    "no," is a real, traceable event, not silently
 *                    dropped).
 *   conversation.txt - the real, permanent, approved conversation log,
 *                    append-only, same "<ts>|<agent_id>|<message>"
 *                    shape. This is what every agent should actually
 *                    read to see what's really been said.
 *
 * Consumes #.desktop/colab_hai_request.txt, one pending line at a
 * time, truncated back to empty after handling (same real contract as
 * every other khtpm manager in this house):
 *   approve:        - pop the oldest pending.txt line, append to
 *                      conversation.txt
 *   reject:          - pop the oldest pending.txt line, append to
 *                      rejected.txt instead (discarded from the real
 *                      conversation, not from the record)
 *   post:<message>   - the HUMAN's own composer submit. The human IS
 *                      the approver, so this goes straight into
 *                      conversation.txt as agent_id "owner", no
 *                      pending step - matching the real reason the
 *                      approval gate exists (checking AGENT output,
 *                      not the human's own typing).
 *
 * Real live .chtpm projection, regenerated every main-loop tick
 * (200ms, matching open-hai's own "more interactive" cadence) using
 * only generic tags already proven this session (sidebar/panel/
 * scrolllist/row class="toolbar"/cli_io) - zero new renderer C.
 *
 * REAL, NEW 2026-09-03 - sessions (direct request: "is there a way to
 * clear this session and prepare for the new one? what about saving
 * old sessions?"), same real convention khtpm_open_hai_manager.c
 * already proved (NEWSESSION/LOADSESSION, session dirs named by real
 * unix timestamp, a sidebar list + "+ New session" row) - not a new
 * pattern, reused verbatim. pending.txt/rejected.txt/conversation.txt
 * now live under sessions/<ts>/ instead of directly under state_dir;
 * incoming.txt stays global (agents don't know or care which session
 * is active - the manager drains it into whichever session currently
 * is). "newsession:" archives nothing (the OLD session dir is simply
 * left on disk, real and intact - "clear" means "start a new session,"
 * never "delete the old one"), "loadsession:<id>" switches the active
 * session (including future writes - matching open-hai's own real
 * semantics, not a read-only history view).
 *
 * REAL, NEW 2026-09-03 - addressed messages + real per-agent
 * visibility (direct request: "'@everyone' in front of message if
 * its for all or '@name' if its for a certain agent... and not let
 * the others read those"). A message's real text may start with
 * "@everyone " (or no @ prefix at all - same as @everyone) or
 * "@<agent_id> ". conversation.txt (this manager's own real source of
 * truth) ALWAYS holds the full, unfiltered transcript - the human
 * approver sees everything, always, on purpose (you cannot approve
 * what you cannot see). What's real and NEW is a per-participant
 * filtered view, sessions/<sid>/feed_<agent_id>.txt, regenerated every
 * tick from conversation.txt: a broadcast (@everyone/no prefix) line
 * appears in every agent's feed; an "@<agent_id>" line appears ONLY in
 * the sender's own feed and that one addressed agent's feed - other
 * agents' feeds skip it entirely. Real, deliberate fail-open rule: an
 * "@<name>" that doesn't match any known participant (typo, or
 * addressed to someone who hasn't spoken yet) is treated as a
 * broadcast rather than silently hidden from everyone - see
 * write_agent_feeds()'s own header comment for the full reasoning.
 * Agents should poll their own feed_<their_id>.txt, not conversation.txt
 * directly, once this lands in the onboarding doc.
 *
 * Usage: colab_hai_manager.+x <house_root> [--data-root <dir>]
 */
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <strings.h>

#define PATH_BUF 4352
#define MAX_LINES 4096
#define MAX_PARTICIPANTS 16

static char g_house[PATH_BUF];
static char g_package_dir[PATH_BUF];
static char g_chtpm_output_path[PATH_BUF];
static char g_state_dir[PATH_BUF];
static char g_incoming_path[PATH_BUF];
static char g_pending_path[PATH_BUF];
static char g_rejected_path[PATH_BUF];
static char g_conversation_path[PATH_BUF];
static char g_request_path[PATH_BUF];
static char g_sessions_root[PATH_BUF];
static char g_current_session_path[PATH_BUF];
static char g_session_id[64] = "";
#define MAX_SESSIONS 64
static char g_session_ids[MAX_SESSIONS][64];
static int g_n_sessions = 0;

static void path_join(char *out, size_t outsz, const char *a, const char *b) {
    snprintf(out, outsz, "%s/%s", a, b);
}

static void mkdir_p_local(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

/* Atomic publish - real tmp+rename, matching every other manager in
 * this house (khtpm_hq_manager.c's own publish_common_events() shape). */
static FILE *atomic_open(const char *final_path, char *tmp_out, size_t tmp_out_sz) {
    snprintf(tmp_out, tmp_out_sz, "%s.tmp", final_path);
    return fopen(tmp_out, "w");
}
static void atomic_commit(const char *final_path, const char *tmp_path) {
    rename(tmp_path, final_path);
}

/* Real XML escape for label= text - same reason every other manager
 * in this house does this (agent-authored text can legally contain
 * '<'/'&'/'"'). */
static void xml_escape(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const char *p = in; *p && o + 6 < outsz; p++) {
        switch (*p) {
            case '&': o += (size_t)snprintf(out + o, outsz - o, "&amp;"); break;
            case '<': o += (size_t)snprintf(out + o, outsz - o, "&lt;"); break;
            case '>': o += (size_t)snprintf(out + o, outsz - o, "&gt;"); break;
            case '"': o += (size_t)snprintf(out + o, outsz - o, "&quot;"); break;
            default: out[o++] = *p;
        }
    }
    out[o] = '\0';
}

/* Split one real "<agent>|<message>" or "<ts>|<agent>|<message>" line
 * into its fields. Returns 1 on success. Real, deliberate choice: the
 * message itself is the LAST field and may legally contain the
 * literal text after further '|' chars are NOT expected (posters
 * escape their own '|'/newlines - see ops/colab_hai_post.sh's own
 * header) - this keeps the parse trivial and matches every other
 * pipe-delimited state file in this house. */
static int split2(char *line, char **a, char **b) {
    char *p = strchr(line, '|');
    if (!p) return 0;
    *p = '\0';
    *a = line;
    *b = p + 1;
    return 1;
}
static int split3(char *line, char **a, char **b, char **c) {
    char *p1 = strchr(line, '|');
    if (!p1) return 0;
    char *p2 = strchr(p1 + 1, '|');
    if (!p2) return 0;
    *p1 = '\0'; *p2 = '\0';
    *a = line; *b = p1 + 1; *c = p2 + 1;
    return 1;
}

static void chomp(char *s) {
    size_t l = strlen(s);
    while (l > 0 && (s[l - 1] == '\n' || s[l - 1] == '\r')) s[--l] = '\0';
}

/* Move every new line in incoming.txt into pending.txt, timestamped,
 * then truncate incoming.txt. Real, deliberate ordering: this manager
 * is the only writer of pending.txt, so two agents appending to
 * incoming.txt at once can never race each other into the approval
 * queue out of order - both land in incoming.txt (fine, it's a plain
 * append), then get sequenced here, one process, one pass. */
static void drain_incoming(void) {
    FILE *inf = fopen(g_incoming_path, "r");
    if (!inf) return;
    fseek(inf, 0, SEEK_END);
    long sz = ftell(inf);
    if (sz <= 0) { fclose(inf); return; }
    fseek(inf, 0, SEEK_SET);

    FILE *pf = fopen(g_pending_path, "a");
    if (!pf) { fclose(inf); return; }
    char line[2048];
    time_t now = time(NULL);
    while (fgets(line, sizeof(line), inf)) {
        chomp(line);
        if (!line[0]) continue;
        char *agent, *msg;
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), "%s", line);
        if (!split2(tmp, &agent, &msg)) continue;
        fprintf(pf, "%ld|%s|%s\n", (long)now, agent, msg);
    }
    fclose(pf);
    fclose(inf);
    FILE *clr = fopen(g_incoming_path, "w");
    if (clr) fclose(clr);
}

/* Pop the oldest line of pending.txt (line 0), optionally appending it
 * to dest_path first. Real, simple "read all, write all-but-first"
 * rewrite - pending.txt is never expected to hold more than a handful
 * of real lines at once (a human approving a backlog of hundreds would
 * be a real product problem, not something to optimize file I/O for). */
static int pop_pending(const char *dest_path) {
    FILE *pf = fopen(g_pending_path, "r");
    if (!pf) return 0;
    /* REAL FIX 2026-09-02 (live crash, found via code review after
     * colab_hai_manager.+x turned up a zombie mid-test): MAX_LINES=4096
     * * 2048 bytes = 8MB, copy-pasted from network_browser_manager.c's
     * own line-count constant (sized for HTML page lines, a different
     * real context) without noticing THIS array is a stack-local
     * variable - 8MB blew the default 8MB stack outright, a real
     * guaranteed-or-near-guaranteed segfault on any real call, not an
     * edge case. `static` moves it to BSS instead of the stack - the
     * real fix, not just a smaller MAX_LINES (this function is not
     * reentrant/threaded, a static buffer is safe here). */
    static char lines[MAX_LINES][2048];
    int n = 0;
    while (n < MAX_LINES && fgets(lines[n], sizeof(lines[n]), pf)) {
        chomp(lines[n]);
        if (lines[n][0]) n++;
    }
    fclose(pf);
    if (n == 0) return 0;

    if (dest_path) {
        FILE *df = fopen(dest_path, "a");
        if (df) { fprintf(df, "%s\n", lines[0]); fclose(df); }
    }
    FILE *wf = fopen(g_pending_path, "w");
    if (wf) {
        for (int i = 1; i < n; i++) fprintf(wf, "%s\n", lines[i]);
        fclose(wf);
    }
    return 1;
}

/* Real session support (2026-09-03), same convention khtpm_open_hai_
 * manager.c already proved - see this file's own top-of-file header
 * comment for the full design. Recomputes the 4 real per-session paths
 * for whichever session id is now active, mkdir's it, and persists the
 * choice to current_session.txt so a manager restart resumes the same
 * session instead of silently starting over. */
static void switch_session(const char *sid) {
    snprintf(g_session_id, sizeof(g_session_id), "%s", sid);
    char session_dir[PATH_BUF];
    path_join(session_dir, sizeof(session_dir), g_sessions_root, sid);
    mkdir_p_local(session_dir);
    path_join(g_pending_path, sizeof(g_pending_path), session_dir, "pending.txt");
    path_join(g_rejected_path, sizeof(g_rejected_path), session_dir, "rejected.txt");
    path_join(g_conversation_path, sizeof(g_conversation_path), session_dir, "conversation.txt");
    { FILE *f = fopen(g_pending_path, "a"); if (f) fclose(f); }
    { FILE *f = fopen(g_rejected_path, "a"); if (f) fclose(f); }
    { FILE *f = fopen(g_conversation_path, "a"); if (f) fclose(f); }
    FILE *cs = fopen(g_current_session_path, "w");
    if (cs) { fprintf(cs, "%s\n", sid); fclose(cs); }
}

static void start_new_session(void) {
    char sid[64];
    snprintf(sid, sizeof(sid), "%ld", (long)time(NULL));
    switch_session(sid);
}

/* Real, generic session listing - scans sessions_root for real
 * subdirectories, newest first (session ids are unix timestamps, so a
 * numeric-descending sort is a real, correct "most recent first"
 * ordering, not a guess). Same real "+ New session first, existing
 * sessions below" sidebar shape open-hai's own publish_sessions()
 * already proved. */
static void list_sessions(void) {
    g_n_sessions = 0;
    DIR *d = opendir(g_sessions_root);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_n_sessions < MAX_SESSIONS) {
        if (e->d_name[0] == '.') continue;
        snprintf(g_session_ids[g_n_sessions], sizeof(g_session_ids[0]), "%s", e->d_name);
        g_n_sessions++;
    }
    closedir(d);
    for (int i = 0; i < g_n_sessions - 1; i++)
        for (int j = i + 1; j < g_n_sessions; j++)
            if (strcmp(g_session_ids[j], g_session_ids[i]) > 0) {
                char t[64];
                snprintf(t, sizeof(t), "%s", g_session_ids[i]);
                snprintf(g_session_ids[i], sizeof(g_session_ids[0]), "%s", g_session_ids[j]);
                snprintf(g_session_ids[j], sizeof(g_session_ids[0]), "%s", t);
            }
}

/* Real, human-readable session label - "Sep 2 23:51" style, matching
 * the real timestamp already stored as the session's own dir name
 * (unix seconds) rather than showing the raw epoch number to a human. */
static void session_label(const char *sid, char *out, size_t outsz) {
    time_t t = (time_t)atol(sid);
    struct tm *tmv = localtime(&t);
    if (!tmv) { snprintf(out, outsz, "%s", sid); return; }
    strftime(out, outsz, "%b %e %H:%M", tmv);
}

static void post_owner_message(const char *msg) {
    time_t now = time(NULL);
    FILE *cf = fopen(g_conversation_path, "a");
    if (!cf) return;
    fprintf(cf, "%ld|owner|%s\n", (long)now, msg);
    fclose(cf);
}

/* One pending action line, same contract as every other manager's own
 * request.txt. */
static void handle_request(void) {
    FILE *f = fopen(g_request_path, "r");
    if (!f) return;
    char line[2048];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);
    chomp(line);
    if (!line[0]) return;

    if (strcmp(line, "approve:") == 0) {
        pop_pending(g_conversation_path);
    } else if (strcmp(line, "reject:") == 0) {
        pop_pending(g_rejected_path);
    } else if (strncmp(line, "post:", 5) == 0 && line[5]) {
        post_owner_message(line + 5);
    } else if (strcmp(line, "newsession:") == 0) {
        start_new_session();
    } else if (strncmp(line, "loadsession:", 12) == 0 && line[12]) {
        switch_session(line + 12);
    }

    FILE *clr = fopen(g_request_path, "w");
    if (clr) fclose(clr);
}

/* Real, generic per-agent color assignment - a small fixed palette
 * assigned in first-seen order, same idea chat-hai's own 12-persona
 * palette already proved, just derived from whichever real agent_ids
 * actually show up instead of a fixed roster (this app doesn't know
 * in advance who will join - opencode/kilo may appear later per
 * direct instruction). */
static char g_participants[MAX_PARTICIPANTS][64];
static int g_n_participants = 0;
static int participant_index(const char *agent) {
    for (int i = 0; i < g_n_participants; i++)
        if (strcmp(g_participants[i], agent) == 0) return i;
    if (g_n_participants < MAX_PARTICIPANTS) {
        snprintf(g_participants[g_n_participants], sizeof(g_participants[0]), "%s", agent);
        return g_n_participants++;
    }
    return 0;
}
static const char *agent_css_class(int idx) {
    static const char *classes[MAX_PARTICIPANTS] = {
        "agent-0", "agent-1", "agent-2", "agent-3", "agent-4", "agent-5",
        "agent-6", "agent-7", "agent-8", "agent-9", "agent-10", "agent-11",
        "agent-12", "agent-13", "agent-14", "agent-15"
    };
    return classes[idx % MAX_PARTICIPANTS];
}

/* Real message-target parsing: a leading "@<word> " token names who a
 * message is addressed to ("everyone" or a real, known agent_id); no
 * such token means the same thing as "@everyone" (a real, deliberate
 * default - most messages are genuinely for the room, not private).
 * target[0] is left '\0' for a plain broadcast either way, so callers
 * only need one check ("target[0] && strcasecmp(target,"everyone")")
 * to know whether real per-agent filtering applies at all. */
static void parse_message_target(const char *msg, char *target, size_t target_sz) {
    target[0] = '\0';
    if (msg[0] != '@') return;
    const char *sp = strchr(msg, ' ');
    size_t len = sp ? (size_t)(sp - (msg + 1)) : strlen(msg + 1);
    if (len >= target_sz) len = target_sz - 1;
    memcpy(target, msg + 1, len);
    target[len] = '\0';
    if (strcasecmp(target, "everyone") == 0) target[0] = '\0';
}

/* Real, per-participant filtered view of the full conversation - see
 * this file's own top-of-file header comment ("REAL, NEW 2026-09-03 -
 * addressed messages") for the full design. Rebuilt from
 * conversation.txt every tick (this house's own established "rebuild
 * from the real source of truth, don't try to incrementally patch a
 * derived file" convention, same as write_chtpm_projection() itself) -
 * conversation.txt stays the one real, permanent, unfiltered record;
 * these feed files are a disposable, always-current projection of it,
 * never a second source of truth. An "@<name>" that doesn't match any
 * currently-known participant is treated as a broadcast (fails open,
 * not closed) - a typo'd or not-yet-joined recipient should never
 * cause a message to silently vanish from everyone's view, that's a
 * real, worse failure mode than an unintended reader seeing it. */
static void write_agent_feeds(void) {
    if (g_n_participants == 0) return;
    char session_dir[PATH_BUF];
    path_join(session_dir, sizeof(session_dir), g_sessions_root, g_session_id);
    for (int i = 0; i < g_n_participants; i++) {
        char fname[96], feed_path[PATH_BUF], tmp_path[PATH_BUF];
        snprintf(fname, sizeof(fname), "feed_%s.txt", g_participants[i]);
        path_join(feed_path, sizeof(feed_path), session_dir, fname);
        FILE *wf = atomic_open(feed_path, tmp_path, sizeof(tmp_path));
        if (!wf) continue;
        FILE *cf = fopen(g_conversation_path, "r");
        if (cf) {
            char line[2048];
            while (fgets(line, sizeof(line), cf)) {
                chomp(line);
                if (!line[0]) continue;
                char tmpline[2048];
                snprintf(tmpline, sizeof(tmpline), "%s", line);
                char *ts, *agent, *msg;
                if (!split3(tmpline, &ts, &agent, &msg)) continue;
                (void)ts;
                char target[64];
                parse_message_target(msg, target, sizeof(target));
                int visible = 1;
                if (target[0]) {
                    int target_known = 0;
                    for (int k = 0; k < g_n_participants; k++)
                        if (strcmp(g_participants[k], target) == 0) target_known = 1;
                    if (target_known) {
                        visible = (strcmp(agent, g_participants[i]) == 0) ||
                                  (strcmp(target, g_participants[i]) == 0);
                    }
                    /* unknown target -> visible stays 1, real fail-open */
                }
                if (visible) fprintf(wf, "%s\n", line);
            }
            fclose(cf);
        }
        fclose(wf);
        atomic_commit(feed_path, tmp_path);
    }
}

static void write_chtpm_projection(void) {
    /* CHTPM-ARCHITECTURE-FIX.md: emit plain key=value to state/ui.txt;
     * the STATIC co-lab-hai.chtpm template does the layout (repeat
     * blocks for participants / sessions / conversation, show= for the
     * pending gate). This manager writes no markup. Values are
     * xml_escape()'d so &, ", <, > round-trip through the renderer's
     * decode_entities() after ${var} substitution. */
    char *buf = malloc(262144);
    if (!buf) return;
    size_t cap = 262144, len = 0;
#define CH_APPEND(...) do { \
        int _n = snprintf(buf + len, cap - len, __VA_ARGS__); \
        if (_n > 0) len += (size_t)_n < cap - len ? (size_t)_n : cap - len - 1; \
    } while (0)

    /* oldest pending line + count (approval gate) */
    char pend_agent[128] = "", pend_msg[1024] = "";
    int n_pending = 0;
    {
        FILE *pf = fopen(g_pending_path, "r");
        if (pf) {
            char line[2048];
            while (fgets(line, sizeof(line), pf)) {
                chomp(line);
                if (!line[0]) continue;
                if (n_pending == 0) {
                    char tmp[2048];
                    snprintf(tmp, sizeof(tmp), "%s", line);
                    char *ts, *agent, *msg;
                    if (split3(tmp, &ts, &agent, &msg)) {
                        snprintf(pend_agent, sizeof(pend_agent), "%s", agent);
                        snprintf(pend_msg, sizeof(pend_msg), "%s", msg);
                    }
                    (void)ts;
                }
                n_pending++;
            }
            fclose(pf);
        }
    }

    /* participant roster (scan conversation + pending, same as before) */
    g_n_participants = 0;
    for (int pass = 0; pass < 2; pass++) {
        const char *path = pass == 0 ? g_conversation_path : g_pending_path;
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[2048];
        while (fgets(line, sizeof(line), f)) {
            chomp(line);
            if (!line[0]) continue;
            char tmp[2048];
            snprintf(tmp, sizeof(tmp), "%s", line);
            char *ts, *agent, *msg;
            if (split3(tmp, &ts, &agent, &msg)) participant_index(agent);
            (void)ts; (void)msg;
        }
        fclose(f);
    }
    write_agent_feeds();

    CH_APPEND("participants_count=%d\n", g_n_participants);
    for (int i = 0; i < g_n_participants; i++) {
        char esc[128];
        xml_escape(g_participants[i], esc, sizeof(esc));
        CH_APPEND("p_%d_name=%s\n", i, esc);
        CH_APPEND("p_%d_class=%s\n", i, agent_css_class(i));
    }

    CH_APPEND("has_pending=%d\n", n_pending > 0 ? 1 : 0);
    CH_APPEND("n_pending=%d\n", n_pending);
    {
        char esc_agent[128], esc_msg[1200];
        xml_escape(pend_agent, esc_agent, sizeof(esc_agent));
        xml_escape(pend_msg, esc_msg, sizeof(esc_msg));
        CH_APPEND("pend_agent=%s\n", esc_agent);
        CH_APPEND("pend_msg=%s\n", esc_msg);
    }

    CH_APPEND("newsession_action='%s/ops/colab_hai_action.sh' 'newsession'\n", g_package_dir);
    CH_APPEND("dir_action='%s/ops/colab_hai_open_dir.sh'\n", g_package_dir);
    CH_APPEND("faq_action='%s/ops/colab_hai_open_faq.sh'\n", g_package_dir);
    CH_APPEND("approve_action='%s/ops/colab_hai_action.sh' 'approve'\n", g_package_dir);
    CH_APPEND("reject_action='%s/ops/colab_hai_action.sh' 'reject'\n", g_package_dir);
    CH_APPEND("post_action='%s/ops/colab_hai_action.sh' 'post'\n", g_package_dir);

    /* sessions list */
    list_sessions();
    CH_APPEND("sessions_count=%d\n", g_n_sessions);
    for (int i = 0; i < g_n_sessions; i++) {
        char label[96], esc[128];
        session_label(g_session_ids[i], label, sizeof(label));
        int is_current = (strcmp(g_session_ids[i], g_session_id) == 0);
        char label_full[128];
        snprintf(label_full, sizeof(label_full), "%s%s", is_current ? "* " : "", label);
        xml_escape(label_full, esc, sizeof(esc));
        CH_APPEND("session_%d_label=%s\n", i, esc);
        CH_APPEND("session_%d_action='%s/ops/colab_hai_action.sh' 'loadsession' '%s'\n",
                  i, g_package_dir, g_session_ids[i]);
    }

    /* conversation feed */
    int convn = 0;
    {
        FILE *f = fopen(g_conversation_path, "r");
        if (f) {
            char line[2048];
            while (fgets(line, sizeof(line), f)) {
                chomp(line);
                if (!line[0]) continue;
                char tmp[2048];
                snprintf(tmp, sizeof(tmp), "%s", line);
                char *ts, *agent, *msg;
                if (!split3(tmp, &ts, &agent, &msg)) continue;
                (void)ts;
                int idx = participant_index(agent);
                char row_raw[1800], row_esc[2200];
                snprintf(row_raw, sizeof(row_raw), "%s: %s", agent, msg);
                xml_escape(row_raw, row_esc, sizeof(row_esc));
                CH_APPEND("msg_%d_text=%s\n", convn, row_esc);
                CH_APPEND("msg_%d_class=%s\n", convn, agent_css_class(idx));
                convn++;
            }
            fclose(f);
        }
    }
    CH_APPEND("conv_count=%d\n", convn);
#undef CH_APPEND

    static char *g_last_projection = NULL;
    if (g_last_projection && strcmp(g_last_projection, buf) == 0) { free(buf); return; }
    free(g_last_projection);
    g_last_projection = buf;

    char tmp_path[PATH_BUF];
    FILE *wf = atomic_open(g_chtpm_output_path, tmp_path, sizeof(tmp_path));
    if (!wf) return;
    fputs(buf, wf);
    fclose(wf);
    atomic_commit(g_chtpm_output_path, tmp_path);
}

/* Same real "renderer's own pid vanished, stop existing" check every
 * other manager in this house uses. */
static int parent_still_alive(const char *parent_package_dir) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), parent_package_dir[0] ? parent_package_dir : g_package_dir, "module_parent.pid");
    FILE *f = fopen(path, "r");
    if (!f) return 1; /* no pid file yet = don't assume dead */
    long pid = 0;
    if (fscanf(f, "%ld", &pid) != 1) { fclose(f); return 1; }
    fclose(f);
    if (pid <= 0) return 1;
    return kill((int)pid, 0) == 0 || errno == EPERM;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root>\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s/&.hq-apps/co-lab-hai", g_house);

    char desktop[PATH_BUF];
    path_join(desktop, sizeof(desktop), g_house, "#.desktop");
    path_join(g_state_dir, sizeof(g_state_dir), desktop, "colab_hai");
    mkdir_p_local(g_state_dir);
    path_join(g_incoming_path, sizeof(g_incoming_path), g_state_dir, "incoming.txt");
    path_join(g_request_path, sizeof(g_request_path), g_state_dir, "request.txt");
    path_join(g_sessions_root, sizeof(g_sessions_root), g_state_dir, "sessions");
    mkdir_p_local(g_sessions_root);
    path_join(g_current_session_path, sizeof(g_current_session_path), g_state_dir, "current_session.txt");
    { char sd[PATH_BUF]; snprintf(sd, sizeof(sd), "%s/state", g_package_dir); mkdir_p_local(sd); }
    snprintf(g_chtpm_output_path, sizeof(g_chtpm_output_path), "%s/state/ui.txt", g_package_dir);

    { FILE *f = fopen(g_incoming_path, "a"); if (f) fclose(f); }
    { FILE *f = fopen(g_request_path, "w"); if (f) fclose(f); }

    /* REAL, NEW 2026-09-03 - session startup. Resume the last active
     * session if current_session.txt says so; else, if this house
     * still has the OLD, pre-sessions flat conversation.txt directly
     * under state_dir (real, live test data from before this feature
     * existed), migrate it into a real first session instead of
     * silently orphaning it - "clear" must never mean "lose." A
     * genuinely fresh install with neither gets a real new session. */
    {
        char saved_sid[64] = "";
        FILE *cs = fopen(g_current_session_path, "r");
        if (cs) { if (fgets(saved_sid, sizeof(saved_sid), cs)) chomp(saved_sid); fclose(cs); }
        if (saved_sid[0]) {
            switch_session(saved_sid);
        } else {
            char old_conv[PATH_BUF];
            path_join(old_conv, sizeof(old_conv), g_state_dir, "conversation.txt");
            struct stat st;
            if (stat(old_conv, &st) == 0 && st.st_size > 0) {
                char sid[64];
                snprintf(sid, sizeof(sid), "%ld", (long)time(NULL));
                switch_session(sid);
                char old_pending[PATH_BUF], old_rejected[PATH_BUF];
                path_join(old_pending, sizeof(old_pending), g_state_dir, "pending.txt");
                path_join(old_rejected, sizeof(old_rejected), g_state_dir, "rejected.txt");
                rename(old_conv, g_conversation_path);
                rename(old_pending, g_pending_path);
                rename(old_rejected, g_rejected_path);
            } else {
                start_new_session();
            }
        }
    }

    write_chtpm_projection();

    for (;;) {
        drain_incoming();
        handle_request();
        write_chtpm_projection();
        if (!parent_still_alive("")) {
            fprintf(stderr, "colab_hai_manager: real parent renderer is gone - exiting\n");
            break;
        }
        usleep(200000);
    }
    return 0;
}
