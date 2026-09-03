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

static void write_chtpm_projection(void) {
    char *buf = malloc(262144);
    if (!buf) return;
    size_t cap = 262144, len = 0;
#define CH_APPEND(...) do { \
        int _n = snprintf(buf + len, cap - len, __VA_ARGS__); \
        if (_n > 0) len += (size_t)_n < cap - len ? (size_t)_n : cap - len - 1; \
    } while (0)

    /* Real pending-message count + oldest pending line, for the
     * approval toolbar. */
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

    CH_APPEND("<!-- co-lab-hai.chtpm - REAL, GENERATED PROJECTION.\n");
    CH_APPEND("     Written by colab_hai_manager.c's own write_chtpm_projection()\n");
    CH_APPEND("     every real main-loop tick - DO NOT HAND-EDIT. -->\n");
    CH_APPEND("<window label=\"Co-lab-h-ai\" class=\"co-lab-hai\">\n");
    CH_APPEND("  <module src=\"&.hq-apps/co-lab-hai/+x/colab_hai_manager.+x\"/>\n");
    CH_APPEND("  <page name=\"main\">\n");
    CH_APPEND("    <sidebar>\n");
    CH_APPEND("      <text label=\"Participants\" class=\"quiet\"/>\n");

    /* Real participant roster, scanned from the real conversation +
     * pending logs, not a hardcoded list - reflects whoever has
     * actually spoken. */
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
    for (int i = 0; i < g_n_participants; i++) {
        char esc[128];
        xml_escape(g_participants[i], esc, sizeof(esc));
        CH_APPEND("      <text label=\"%s\" class=\"%s\"/>\n", esc, agent_css_class(i));
    }
    if (n_pending > 0) {
        CH_APPEND("      <text label=\"Pending: %d\" class=\"pending-count\"/>\n", n_pending);
    }
    CH_APPEND("    </sidebar>\n");
    CH_APPEND("    <panel>\n");

    /* Real, standing toolbar - always visible, not gated on pending.
     * "Dir" opens this app's own real state dir for log inspection
     * (direct request, 2026-09-02). A full dropdown "Menu" (matching
     * piececraft-hq's own convention) + centered title is a real,
     * separate, house-wide task (it touches the shared chrome-bar
     * renderer every khtpm window uses, not just this app) - scoped
     * out of this first cut on purpose, tracked in the roadmap doc. */
    CH_APPEND("      <row class=\"toolbar\">\n");
    CH_APPEND("        <item id=\"ch-dir\" label=\"Dir\" action=\"'%s/ops/colab_hai_open_dir.sh'\"/>\n", g_package_dir);
    CH_APPEND("      </row>\n");

    if (n_pending > 0) {
        char esc_agent[128], esc_msg[1200];
        xml_escape(pend_agent, esc_agent, sizeof(esc_agent));
        xml_escape(pend_msg, esc_msg, sizeof(esc_msg));
        /* REAL FIX 2026-09-02 (found live testing this app's own first
         * cut): layout_toolbar_row() only positions <item> children -
         * a bare <text> inside class="toolbar" was silently pushed
         * off-screen (t->x/y = -100000), same "non-item children get
         * hidden" contract every other toolbar row already relies on.
         * A plain, unwrapped <text> flows normally in the panel
         * instead - it doesn't need toolbar's own horizontal-item
         * layout at all, just to be visible before the button row. */
        CH_APPEND("      <text label=\"PENDING (%s): %s\" class=\"pending-text\"/>\n", esc_agent, esc_msg);
        CH_APPEND("      <row class=\"toolbar\">\n");
        CH_APPEND("        <item id=\"ch-approve\" label=\"Approve\" action=\"'%s/ops/colab_hai_action.sh' 'approve'\"/>\n", g_package_dir);
        CH_APPEND("        <item id=\"ch-reject\" label=\"Reject\" action=\"'%s/ops/colab_hai_action.sh' 'reject'\"/>\n", g_package_dir);
        CH_APPEND("      </row>\n");
    }

    CH_APPEND("      <scrolllist id=\"conv\" class=\"from-bottom conv-list\">\n");
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
                char esc_agent[128], esc_msg[1600];
                xml_escape(agent, esc_agent, sizeof(esc_agent));
                xml_escape(msg, esc_msg, sizeof(esc_msg));
                CH_APPEND("        <text label=\"%s: %s\" class=\"%s\"/>\n",
                          esc_agent, esc_msg, agent_css_class(idx));
            }
            fclose(f);
        }
    }
    CH_APPEND("      </scrolllist>\n");
    CH_APPEND("      <cli_io id=\"composer\" target_id=\"composer\" label=\"&gt; \" "
              "action=\"'%s/ops/colab_hai_action.sh' 'post'\"/>\n", g_package_dir);
    CH_APPEND("    </panel>\n");
    CH_APPEND("  </page>\n</window>\n");
#undef CH_APPEND

    /* Real "only write when content actually changed" guard - same
     * fix network_browser_manager.c/khtpm_open_hai_manager.c already
     * needed, for the exact same reason (an unconditional write every
     * tick would tear down/rebuild the Elem tree - and any currently-
     * armed cli_io field with it - for no real reason). */
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
    path_join(g_pending_path, sizeof(g_pending_path), g_state_dir, "pending.txt");
    path_join(g_rejected_path, sizeof(g_rejected_path), g_state_dir, "rejected.txt");
    path_join(g_conversation_path, sizeof(g_conversation_path), g_state_dir, "conversation.txt");
    path_join(g_request_path, sizeof(g_request_path), g_state_dir, "request.txt");
    snprintf(g_chtpm_output_path, sizeof(g_chtpm_output_path), "%s/co-lab-hai.chtpm", g_package_dir);

    /* Never assume, always create - same discipline as every other
     * manager's own startup. */
    for (const char *p = g_incoming_path; p == g_incoming_path;) {
        FILE *f = fopen(g_incoming_path, "a"); if (f) fclose(f);
        f = fopen(g_pending_path, "a"); if (f) fclose(f);
        f = fopen(g_rejected_path, "a"); if (f) fclose(f);
        f = fopen(g_conversation_path, "a"); if (f) fclose(f);
        f = fopen(g_request_path, "w"); if (f) fclose(f);
        break;
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
