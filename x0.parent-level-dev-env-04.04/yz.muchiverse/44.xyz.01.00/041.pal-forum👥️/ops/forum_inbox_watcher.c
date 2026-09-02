/* forum_inbox_watcher - PAL-FORUM-STANDARD.txt sec. 0/5. PERSISTENT
 * daemon, tails net/inbox.txt (palnet_peer.c's own output - lines
 * prefixed "<sender_node_id>|<content>", stripped before parsing here,
 * same convention chain_inbox_watcher.c already established) and
 * applies received POST/FOLLOW/UNFOLLOW/LIKE/RETWEET/DM lines to this
 * node's own local copy of the relevant user directory.
 *
 * v1 has NO signature/authenticity check (matching pal-chain's own
 * sec. 7 gap) - a line is applied as-is once its own author's user_id
 * is known/created locally; this stays real parsing of an unknown line
 * shape, so per PAL-FORUM-STANDARD sec. 5 it's a C op, not PAL.
 *
 * Auto-creates a MINIMAL local copy of an author's own user directory
 * (wall.txt/following.txt/likes.txt/retweets.txt + a placeholder
 * profile.txt using their own user_id as display_name) the first time
 * this node ever receives a line naming a user_id it has no local
 * directory for yet - required so a peer can follow/see posts from
 * someone whose real forum_create_user.+x call happened on a DIFFERENT
 * node; a real display_name sync is a named, not-yet-built follow-up
 * (this v1 just uses user_id as a readable fallback).
 *
 * Self-contained, no shared headers.
 * Usage: forum_inbox_watcher.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
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

static void ensure_user_dir(const char *user_id) {
    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) == 0) return;

    char users_root[PATH_BUF];
    snprintf(users_root, sizeof(users_root), "%s/users", project_root);
    mkdir(users_root, 0755);
    mkdir(user_dir, 0755);
    char dms_dir[PATH_BUF];
    snprintf(dms_dir, sizeof(dms_dir), "%s/dms", user_dir);
    mkdir(dms_dir, 0755);

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/profile.txt", user_dir);
    FILE *pf = fopen(path, "w");
    if (pf) {
        fprintf(pf, "user_id=%s\n", user_id);
        fprintf(pf, "display_name=%s\n", user_id);
        fprintf(pf, "created_at=%ld\n", (long)time(NULL));
        fprintf(pf, "bio=\n");
        fclose(pf);
    }
    snprintf(path, sizeof(path), "%s/wall.txt", user_dir); fclose(fopen(path, "w"));
    snprintf(path, sizeof(path), "%s/following.txt", user_dir); fclose(fopen(path, "w"));
    snprintf(path, sizeof(path), "%s/likes.txt", user_dir); fclose(fopen(path, "w"));
    snprintf(path, sizeof(path), "%s/retweets.txt", user_dir); fclose(fopen(path, "w"));
    snprintf(path, sizeof(path), "%s/feed_cache.txt", user_dir); fclose(fopen(path, "w"));
}

static void append_line(const char *path, const char *line) {
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", line); fclose(f); }
}

/* Dedup: refuses to re-apply a line already present verbatim in the
 * target file - a real, if simple, guard against re-processing the
 * same relayed message twice (palnet_peer's own backlog replay to a
 * newly-connected peer can otherwise redeliver an already-seen line). */
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
 * TWICE, in pal-chat-irc - see that project's own chat_inbox_watcher.c
 * for the full trace): a received line used to update the right file
 * but never told THIS session's own chtpm_parser_pal to redraw. FIRST
 * fix attempt just pinged frame_changed.txt (ported from p2p_manager.c's
 * own trigger_render()) - insufficient, confirmed live: that marker
 * only tells chtpm_parser_pal to re-read whatever's CURRENTLY in
 * view.txt, it doesn't regenerate view.txt itself - and view.txt is
 * only ever written by forum_compose_frame.c, normally invoked by this
 * project's own pal script in response to a local keypress, a path an
 * inbox watcher never goes through. Real fix: actually RUN
 * forum_compose_frame.+x from here so view.txt gets regenerated with
 * current data BEFORE anything gets told to redraw - that op already
 * pings frame_changed.txt itself at the end. PRISC_PROJECT_ROOT here is
 * this watcher's own launching session's directory, so this only ever
 * touches THAT session's own view.txt/frame_changed.txt. */
static void trigger_render(void) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/forum_compose_frame.+x >/dev/null 2>&1", project_root);
    if (system(cmd) != 0) { /* best-effort - a failed re-render just means the next real event will catch up */ }
}

static void handle_line(const char *line) {
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", line);
    char *bar = strchr(copy, '|');
    if (!bar) return;
    char verb[32];
    snprintf(verb, sizeof(verb), "%.*s", (int)(bar - copy), copy);

    if (strcmp(verb, "POST") == 0) {
        /* POST|<post_id>|<user_id>|<timestamp>|<text>|<image_id_or_-> */
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
        const char *author = fields[2];
        ensure_user_dir(author);
        char wall_path[PATH_BUF];
        snprintf(wall_path, sizeof(wall_path), "%s/users/%s/wall.txt", project_root, author);
        if (!already_has_line(wall_path, line)) append_line(wall_path, line);
        trigger_render();
    } else if (strcmp(verb, "FOLLOW") == 0 || strcmp(verb, "UNFOLLOW") == 0) {
        /* FOLLOW|<user_id>|<followed_user_id>|<timestamp> */
        char parse[MAX_LINE];
        snprintf(parse, sizeof(parse), "%s", line);
        char *fields[4]; int nf = 0; char *cursor = parse;
        for (; nf < 3; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 3) return;
        const char *user_id = fields[1];
        ensure_user_dir(user_id);
        char following_path[PATH_BUF];
        snprintf(following_path, sizeof(following_path), "%s/users/%s/following.txt", project_root, user_id);
        if (!already_has_line(following_path, line)) append_line(following_path, line);
        trigger_render();
    } else if (strcmp(verb, "LIKE") == 0) {
        /* LIKE|<user_id>|<post_id>|<timestamp> */
        char parse[MAX_LINE];
        snprintf(parse, sizeof(parse), "%s", line);
        char *fields[4]; int nf = 0; char *cursor = parse;
        for (; nf < 3; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 3) return;
        const char *user_id = fields[1];
        ensure_user_dir(user_id);
        char likes_path[PATH_BUF];
        snprintf(likes_path, sizeof(likes_path), "%s/users/%s/likes.txt", project_root, user_id);
        if (!already_has_line(likes_path, line)) append_line(likes_path, line);
        trigger_render();
    } else if (strcmp(verb, "RETWEET") == 0) {
        char parse[MAX_LINE];
        snprintf(parse, sizeof(parse), "%s", line);
        char *fields[4]; int nf = 0; char *cursor = parse;
        for (; nf < 3; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 3) return;
        const char *user_id = fields[1];
        ensure_user_dir(user_id);
        char retweets_path[PATH_BUF];
        snprintf(retweets_path, sizeof(retweets_path), "%s/users/%s/retweets.txt", project_root, user_id);
        if (!already_has_line(retweets_path, line)) append_line(retweets_path, line);
        trigger_render();
    } else if (strcmp(verb, "DM") == 0) {
        /* DM|<from_user_id>|<to_user_id>|<timestamp>|<text> - applies
         * to the RECIPIENT's own local copy (users/<to>/dms/<from>.txt)
         * - the sender already appended to their own copy directly in
         * forum_dm.c, this daemon is what completes the OTHER side. */
        char parse[MAX_LINE];
        snprintf(parse, sizeof(parse), "%s", line);
        char *fields[6]; int nf = 0; char *cursor = parse;
        for (; nf < 4; nf++) {
            char *p = strchr(cursor, '|');
            if (!p) break;
            *p = '\0';
            fields[nf] = cursor;
            cursor = p + 1;
        }
        if (nf < 4) return;
        const char *from_id = fields[1];
        const char *to_id = fields[2];
        ensure_user_dir(to_id);
        char dms_dir[PATH_BUF];
        snprintf(dms_dir, sizeof(dms_dir), "%s/users/%s/dms", project_root, to_id);
        mkdir(dms_dir, 0755);
        char thread_path[PATH_BUF];
        snprintf(thread_path, sizeof(thread_path), "%s/%s.txt", dms_dir, from_id);
        if (!already_has_line(thread_path, line)) append_line(thread_path, line);
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
                /* Strip palnet_peer's own "<sender_node_id>|" prefix -
                 * see chain_inbox_watcher.c's own identical fix for why
                 * (content is self-describing, sender identity isn't
                 * needed for v1's own no-signature trust model). */
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
