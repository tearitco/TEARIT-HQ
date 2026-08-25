/* set_irc_agent - the "/irc-agent <room>" command. Turns on irc_agent_
 * poll.c's own polling loop (called every tick from main_loop_chtpm.
 * pal) by writing irc_agent_active=yes/irc_agent_room=<room> into
 * muchi-pal-agent's own state.txt - irc_agent_poll.c does all the real
 * work of joining pal-chat-irc and auto-responding; this op only ever
 * flips the switch. "/irc-agent off" turns it back off (irc_agent_
 * poll.c's own next-tick no-op check reads irc_agent_active, not
 * irc_agent_room, so turning it off doesn't need to touch the room
 * field or the local join-tracking state at all - turning it back on
 * for the SAME room later resumes without re-greeting, since irc_
 * agent_state.txt's own joined_room still matches).
 *
 * Self-contained: own root resolution, own constants, no shared
 * headers, mirrors switch_model.c's own write_state_field shape.
 * Usage: set_irc_agent.+x <room>|off */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
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

static int valid_room_name(const char *s) {
    if (!s[0]) return 0;
    for (const char *p = s; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);

    char want[MAX_FIELD] = "";
    if (argc >= 2) snprintf(want, sizeof(want), "%s", argv[1]);
    size_t wlen = strlen(want);
    while (wlen > 0 && (want[wlen - 1] == ' ' || want[wlen - 1] == '\n' || want[wlen - 1] == '\r')) want[--wlen] = '\0';

    write_state_field(state_path, "input_buffer", "");

    if (strcmp(want, "off") == 0) {
        write_state_field(state_path, "irc_agent_active", "no");
        write_state_field(state_path, "sys_msg", "irc-agent stopped.");
        return 0;
    }

    if (!valid_room_name(want)) {
        write_state_field(state_path, "sys_msg", "Usage: /irc-agent <room>|off - room letters/digits/_/- only.");
        return 0;
    }

    write_state_field(state_path, "irc_agent_active", "yes");
    write_state_field(state_path, "irc_agent_room", want);
    write_state_field(state_path, "irc_agent_user_id", "irc-agent-0000");
    char msg[MAX_FIELD + 32];
    snprintf(msg, sizeof(msg), "irc-agent joining pal-chat-irc room '%s'...", want);
    write_state_field(state_path, "sys_msg", msg);
    return 0;
}
