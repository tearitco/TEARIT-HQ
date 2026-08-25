/* irc_agent_poll - the "irc-agent-0000" bridge, direct user ask
 * 2026-07-20: "we could have a pal script 'irc-agent-0000' inside the
 * chat that hooks up to chat, says hi and waits for new messages and
 * responds to everything from random." Called every tick from main_
 * loop_chtpm.pal (same shape as check_response/compose_frame), but is
 * a near-instant no-op unless muchi-pal-agent's own state.txt has
 * irc_agent_active=yes - so this costs nothing on ordinary chat ticks.
 *
 * Uses pal-chat-irc's own direct_ops backend (CHAT-INTEGRATION-
 * ARCHITECTURE.txt sec. 2/3's "direct_ops" - shell straight into the
 * target project's own ops and read its own data files directly, no
 * chtpm/UI involvement at all) rather than ui_drive: this is a bot
 * doing bulk background polling, not something that needs to look
 * like a human pressing keys - sec. 2's own "WHEN TO USE WHICH"
 * guidance names this exact case as the direct_ops one.
 *
 * FIRST ACTIVATION for a given room (tracked in the local irc_agent_
 * state.txt below, separate from muchi-pal-agent's own state.txt):
 *   1. chat_create_user.+x <user_id> "IRC Agent" against the target
 *      root - idempotent, a nonzero exit (user already exists) is
 *      expected and ignored on every activation after the first.
 *   2. Records the CURRENT line count of the target's own
 *      rooms/<room>/messages.txt as the starting point, so old room
 *      history is never replied to - only genuinely new messages.
 *   3. Posts one greeting via chat_post_message.+x, then re-counts
 *      lines (now including the greeting itself) so the bot never
 *      replies to its own greeting on the very next tick.
 *
 * EVERY TICK AFTER: re-counts messages.txt; for each line beyond the
 * last-seen count, skips anything authored by irc_agent_user_id
 * itself (its own prior replies), and for everything else, posts a
 * random 5-10 word reply (identical word-picking logic to send_
 * message.c's own provider_kind=script branch, duplicated here per
 * this family's "no shared headers, self-contained op" convention)
 * built from the SAME wordbank muchi-pal-agent's own /model script
 * mode is currently configured with (current_model_name, when
 * provider_kind=script - falls back to the default sample.txt
 * wordbank otherwise, e.g. if a network provider is active).
 *
 * Self-contained: own root resolution, own constants, no shared
 * headers. Usage: irc_agent_poll.+x (no args). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_FIELD 256

/* Sibling-directory fallback, same override-with-hardcoded-default
 * pattern already established family-wide for this exact class of
 * problem (palnet_peer.c's own PRISC_NET_ROOT, zoo_0000/button.sh's
 * own PRISC_EXCHANGE_ROOT shim) - session isolation means muchi-pal-
 * agent's own PRISC_PROJECT_ROOT is usually a pieces/sessions/<id>/
 * dir, not a real sibling of pal-chat-irc, so "../pal-chat-irc"
 * relative to it would silently resolve to nonsense. */
#define PAL_CHAT_IRC_DEFAULT_ROOT "/home/no/Desktop/🤖️🪤️🏠️/🚽️🧻️/🚽️🥡️-00.00/ZEST-10.18/x0.parent-level-dev-env-03.01/yz.muchiverse/2.muchi-verse/pal-chat-irc"

static char project_root[MAX_PATH] = ".";
static char target_root[MAX_PATH] = PAL_CHAT_IRC_DEFAULT_ROOT;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *tenv = getenv("PRISC_IRC_TARGET_ROOT");
    if (tenv && tenv[0]) snprintf(target_root, sizeof(target_root), "%s", tenv);
}

static char *read_full_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

static void read_field(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *val = line + klen + 1;
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r')) val[--vlen] = '\0';
            snprintf(out, out_sz, "%s", val);
            break;
        }
    }
    fclose(f);
}

/* Rewrite-whole-file field writer, same shape as every other op's own
 * write_state_field (send_message.c/switch_model.c) - state files in
 * this family are always small enough that this is fine. */
static void write_field(const char *path, const char *key, const char *value) {
    char fields[32][MAX_FIELD];
    char values[32][MAX_LINE];
    int n = 0;
    int found = 0;

    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && n < 32) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *val = eq + 1;
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r')) val[--vlen] = '\0';
            snprintf(fields[n], sizeof(fields[n]), "%s", line);
            if (strcmp(fields[n], key) == 0) {
                snprintf(values[n], sizeof(values[n]), "%s", value);
                found = 1;
            } else {
                snprintf(values[n], sizeof(values[n]), "%s", val);
            }
            n++;
        }
        fclose(f);
    }
    if (!found && n < 32) {
        snprintf(fields[n], sizeof(fields[n]), "%s", key);
        snprintf(values[n], sizeof(values[n]), "%s", value);
        n++;
    }

    FILE *out = fopen(path, "w");
    if (!out) return;
    for (int i = 0; i < n; i++) fprintf(out, "%s=%s\n", fields[i], values[i]);
    fclose(out);
}

static long count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long count = 0;
    int c;
    while ((c = fgetc(f)) != EOF) if (c == '\n') count++;
    fclose(f);
    return count;
}

static void pick_random_words(const char *wordbank_path, char *response, size_t response_sz) {
    response[0] = '\0';
    char *words[512];
    int nwords = 0;
    char *wb = read_full_file(wordbank_path);
    if (wb) {
        char *saveptr = NULL;
        char *line = strtok_r(wb, "\n", &saveptr);
        while (line && nwords < 512) {
            while (*line == ' ' || *line == '\t') line++;
            size_t llen = strlen(line);
            while (llen > 0 && (line[llen - 1] == '\r' || line[llen - 1] == ' ')) line[--llen] = '\0';
            if (line[0] && line[0] != '#') words[nwords++] = line;
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }
    if (nwords == 0) {
        snprintf(response, response_sz, "[irc-agent: wordbank '%s' is empty or missing]", wordbank_path);
    } else {
        unsigned int seed = (unsigned int)getpid() ^ (unsigned int)time(NULL);
        FILE *rf = fopen("/dev/urandom", "rb");
        if (rf) { unsigned int r; if (fread(&r, sizeof(r), 1, rf) == 1) seed ^= r; fclose(rf); }
        srand(seed);
        int pick_count = 5 + (rand() % 6);
        for (int i = 0; i < pick_count; i++) {
            if (i > 0) strncat(response, " ", response_sz - strlen(response) - 1);
            strncat(response, words[rand() % nwords], response_sz - strlen(response) - 1);
        }
    }
    free(wb);
}

static void sanitize_shell_arg(char *s) {
    /* chat_post_message.c/chat_create_user.c argv text ends up inside
     * a system()-built shell command below - single quotes must be
     * neutralized, not just spaces (this op's own text is either the
     * fixed literal greeting or randomly-picked wordbank words, never
     * untrusted chat text, but this is cheap insurance against a
     * wordbank file someone edits to include a quote). */
    for (char *p = s; *p; p++) {
        if (*p == '\'') *p = ' ';
    }
}

int main(void) {
    resolve_root();

    char own_state_path[PATH_BUF];
    snprintf(own_state_path, sizeof(own_state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);

    char active[MAX_FIELD];
    read_field(own_state_path, "irc_agent_active", active, sizeof(active));
    if (strcmp(active, "yes") != 0) return 0;

    char room[MAX_FIELD];
    read_field(own_state_path, "irc_agent_room", room, sizeof(room));
    if (room[0] == '\0') return 0;

    char user_id[MAX_FIELD];
    read_field(own_state_path, "irc_agent_user_id", user_id, sizeof(user_id));
    if (user_id[0] == '\0') snprintf(user_id, sizeof(user_id), "irc-agent-0000");

    char wordbank_rel[MAX_FIELD];
    char provider_kind[MAX_FIELD];
    read_field(own_state_path, "provider_kind", provider_kind, sizeof(provider_kind));
    if (strcmp(provider_kind, "script") == 0) {
        read_field(own_state_path, "current_model_name", wordbank_rel, sizeof(wordbank_rel));
    }
    if (wordbank_rel[0] == '\0') {
        snprintf(wordbank_rel, sizeof(wordbank_rel), "pieces/registry/wordbanks/sample.txt");
    }
    char wordbank_path[PATH_BUF];
    snprintf(wordbank_path, sizeof(wordbank_path), "%s/%s", project_root, wordbank_rel);

    char local_state_path[PATH_BUF];
    snprintf(local_state_path, sizeof(local_state_path), "%s/pieces/world_01/session_01/chat/irc_agent_state.txt", project_root);

    char joined_room[MAX_FIELD];
    read_field(local_state_path, "joined_room", joined_room, sizeof(joined_room));

    char messages_path[PATH_BUF];
    snprintf(messages_path, sizeof(messages_path), "%s/rooms/%s/messages.txt", target_root, room);

    if (strcmp(joined_room, room) != 0) {
        /* First activation for this room. */
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd),
                 "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/chat_create_user.+x' '%s' 'IRC Agent' > /dev/null 2>&1",
                 target_root, target_root, user_id);
        int rc = system(cmd);
        (void)rc; /* nonzero = "already exists", expected after the first join */

        char greeting[512];
        snprintf(greeting, sizeof(greeting), "hi! I am %s - say something and I will reply.", user_id);
        snprintf(cmd, sizeof(cmd),
                 "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/chat_post_message.+x' '%s' '%s' '%s' > /dev/null 2>&1",
                 target_root, target_root, room, user_id, greeting);
        rc = system(cmd);
        (void)rc;

        long start_count = count_lines(messages_path);
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%ld", start_count);
        write_field(local_state_path, "joined_room", room);
        write_field(local_state_path, "last_line_count", count_str);
        return 0;
    }

    char last_count_str[32];
    read_field(local_state_path, "last_line_count", last_count_str, sizeof(last_count_str));
    long last_count = last_count_str[0] ? atol(last_count_str) : 0;

    char *content = read_full_file(messages_path);
    if (!content) return 0;

    long line_no = 0;
    char *saveptr = NULL;
    char *line = strtok_r(content, "\n", &saveptr);
    while (line) {
        line_no++;
        if (line_no > last_count) {
            /* MSG|<msg_id>|<room>|<user_id>|<timestamp>|<text> - text
             * is guaranteed pipe-free (chat_post_message.c's own
             * sanitize_line already strips '|' at write time), so a
             * fixed 6-field split is safe. */
            char *fields[6];
            int nf = 0;
            char *tok = line;
            fields[nf++] = tok;
            for (char *p = line; *p && nf < 6; p++) {
                if (*p == '|') {
                    *p = '\0';
                    fields[nf++] = p + 1;
                }
            }
            if (nf == 6 && strcmp(fields[0], "MSG") == 0) {
                const char *msg_user = fields[3];
                const char *msg_text = fields[5];
                if (strcmp(msg_user, user_id) != 0) {
                    char response[512];
                    pick_random_words(wordbank_path, response, sizeof(response));
                    sanitize_shell_arg(response);
                    char cmd[PATH_BUF * 2];
                    snprintf(cmd, sizeof(cmd),
                             "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/chat_post_message.+x' '%s' '%s' '%s' > /dev/null 2>&1",
                             target_root, target_root, room, user_id, response);
                    int rc = system(cmd);
                    (void)rc;
                    (void)msg_text;
                }
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(content);

    long final_count = count_lines(messages_path);
    char final_count_str[32];
    snprintf(final_count_str, sizeof(final_count_str), "%ld", final_count);
    write_field(local_state_path, "last_line_count", final_count_str);
    return 0;
}
