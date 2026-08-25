/* chat_inbox_watcher - PAL-CHAT-IRC-STANDARD.txt sec. 4. PERSISTENT
 * daemon, tails net/inbox.txt (palnet_peer.c's own output - lines
 * prefixed "<sender_node_id>|<content>", stripped before parsing,
 * same convention as chain_inbox_watcher.c/forum_inbox_watcher.c) and
 * applies received MSG lines to this node's own local copy of the
 * named room's messages.txt - mkdir -p's the room locally if this
 * node has never seen it before (sec. 0's own "a room is created
 * implicitly" rule).
 *
 * v1 has NO signature/authenticity check (matching pal-chain's own
 * sec. 7 gap / pal-forum's own sec. 4 gap).
 *
 * Self-contained, no shared headers.
 * Usage: chat_inbox_watcher.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_line(const char *path, const char *line) {
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", line); fclose(f); }
}

static int already_has_line(const char *path, const char *line) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[MAX_LINE];
    int found = 0;
    while (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strcmp(buf, line) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}

/* PAL-NET-STANDARD.txt sec. 5/6 - REAL BUG, LIVE-CAUGHT (2026-07-20,
 * TWICE): a received MSG used to update rooms/<room>/messages.txt
 * correctly but never told THIS session's own chtpm_parser_pal to
 * redraw. FIRST fix attempt just pinged pieces/display/frame_changed.txt
 * (ported from 1.TPMOS's own real p2p_manager.c's own trigger_render())
 * - necessary but NOT sufficient, confirmed by direct live re-test:
 * that marker only tells chtpm_parser_pal "something changed, re-read
 * whatever's currently in view.txt" - it does NOT regenerate view.txt
 * itself. view.txt is only ever written by chat_compose_frame.c, which
 * in EVERY other code path is invoked by this project's own pal script
 * (main_loop_chtpm.pal) in response to a local keypress - an inbox
 * watcher applying a NETWORK-received message never goes through that
 * path at all, so the marker got pinged but view.txt stayed stale
 * (still showing whatever the room looked like at this session's own
 * last local keypress). Real fix: actually RUN chat_compose_frame.+x
 * from here (same run_capture-style shellout every menu_input op in
 * this family already uses) so view.txt gets regenerated with current
 * room content BEFORE anything gets told to redraw - that op already
 * pings frame_changed.txt itself at the end, so nothing else is
 * needed. p2p_manager.c's own trigger_render() didn't need this second
 * step because THAT file computes its own view content directly
 * in-process (write_gui_state()) as part of the same call that also
 * pings the marker - we don't have that luxury since view-composition
 * lives in a separate op by design (XYZOS-STANDARDS sec. 20's own ONE
 * VISIBLE FRAME WRITER RULE), so this watcher has to invoke that op
 * explicitly instead. PRISC_PROJECT_ROOT here is this watcher's own
 * launching session's directory (button.sh launches one watcher per
 * session), so this only ever touches THAT session's own view.txt/
 * frame_changed.txt - never another session's. */
static void trigger_render(void) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/chat_compose_frame.+x >/dev/null 2>&1", project_root);
    if (system(cmd) != 0) { /* best-effort - a failed re-render just means the next real event will catch up */ }
}

static void handle_line(const char *line) {
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", line);
    char *bar = strchr(copy, '|');
    if (!bar) return;
    char verb[32];
    snprintf(verb, sizeof(verb), "%.*s", (int)(bar - copy), copy);

    if (strcmp(verb, "MSG") == 0) {
        /* MSG|<msg_id>|<room_name>|<user_id>|<timestamp>|<text> */
        char parse[MAX_LINE];
        snprintf(parse, sizeof(parse), "%s", line);
        char *fields[6]; int nf = 0; char *cursor = parse;
        for (; nf < 5; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 5) return;
        const char *room = fields[2];

        char rooms_root[PATH_BUF];
        snprintf(rooms_root, sizeof(rooms_root), "%s/rooms", project_root);
        mkdir(rooms_root, 0755);
        char room_dir[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(room_dir, sizeof(room_dir), "%s/rooms/%s", project_root, room);
#pragma GCC diagnostic pop
        mkdir(room_dir, 0755);
        char messages_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(messages_path, sizeof(messages_path), "%s/messages.txt", room_dir);
#pragma GCC diagnostic pop
        /* REAL BUG, LIVE-CAUGHT (2026-07-20, user's own two-terminal
         * test): trigger_render() used to live INSIDE the
         * !already_has_line guard, alongside append_line() - correct
         * for the append (never double-write the same line), WRONG for
         * the render trigger. rooms/<room>/messages.txt is SHARED,
         * REAL data across every session of the same real project
         * (symlinked into each pieces/sessions/<id>/, USER-PAL-
         * STANDARD.txt/xyzos-standards.txt sec. 23's own "what stays
         * shared" list) - when terminal 2 posts, its own
         * chat_post_message.c writes the line directly into that
         * SHARED file immediately, before the network round-trip ever
         * happens. By the time terminal 1's OWN chat_inbox_watcher
         * receives the network-relayed copy of that same line,
         * already_has_line() correctly says "already there" (it is -
         * terminal 2 wrote it locally) and skips the append - but that
         * also skipped the render trigger, so terminal 1 was never
         * told to redraw even though IT had never actually rendered
         * that line yet. The render trigger answers "did THIS session
         * just learn something new via its own inbox", which is true
         * every time handle_line() is reached here, regardless of
         * whether the underlying file already happened to have the
         * content - so it must NOT be gated on already_has_line.
         *
         * MASTER LEDGER (added alongside the above fix): data/
         * master_ledger.txt is the canonical global truth across ALL
         * rooms (shared/symlinked, same as rooms/ and users/), so the
         * dedup check is now made against IT rather than against the
         * per-room messages.txt - a line reaching this watcher is
         * "new to this node's ledger" or it isn't, independent of
         * whether the per-room view already happens to have it (it
         * always will in lockstep, since both are written together
         * below, but the ledger is the one source of truth this check
         * should be reasoning about). */
        char data_dir[PATH_BUF];
        snprintf(data_dir, sizeof(data_dir), "%s/data", project_root);
        mkdir(data_dir, 0755);
        char ledger_path[PATH_BUF];
        snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", project_root);
        if (!already_has_line(ledger_path, line)) {
            append_line(ledger_path, line);
            append_line(messages_path, line);
        }
        trigger_render();
    }
}

int main(void) {
    resolve_root();
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    char inbox_path[PATH_BUF];
    snprintf(inbox_path, sizeof(inbox_path), "%s/net/inbox.txt", project_root);

    long last_line = 0;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/net/inbox_watcher_state.txt", project_root);
    FILE *sf = fopen(state_path, "r");
    if (sf) { if (fscanf(sf, "last_line=%ld", &last_line) != 1) last_line = 0; fclose(sf); }

    while (!g_stop) {
        FILE *f = fopen(inbox_path, "r");
        if (f) {
            char line[MAX_LINE];
            long cur = 0;
            while (fgets(line, sizeof(line), f)) {
                cur++;
                if (cur <= last_line) continue;
                line[strcspn(line, "\n")] = '\0';
                char *content = strchr(line, '|');
                content = content ? content + 1 : line;
                handle_line(content);
            }
            last_line = cur;
            fclose(f);
            FILE *wf = fopen(state_path, "w");
            if (wf) { fprintf(wf, "last_line=%ld\n", last_line); fclose(wf); }
        }
        sleep(1);
    }
    return 0;
}
