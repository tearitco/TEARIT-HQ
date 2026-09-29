/* rc_check_request.c - real, single-purpose op for robot_chat.pal's
 * own poll loop (AI-PUSH-ROADMAP-AND-NUANCES.md, "Track 1's Natural
 * Test Surface: the Drop-In Chatbot Entity"). Direct instruction
 * 2026-09-28: "the manager should be a pal script instead of C, and
 * we will call event calling ops to do the chat" - this replaces the
 * request-polling half of khtpm_robot_chat_manager.c (now retired),
 * called bare (no args) every tick from robot_chat.pal's own loop, same
 * real shape wsr-pal's main_loop_chtpm.pal already uses for its own
 * per-tick ops (compose_frame/wsr_menu_input).
 *
 * No argv needed: launch_module() (khtpm_core_render.c) already sets
 * PRISC_PROJECT_ROOT=<entity_dir> and KHTPM_HOUSE=<house_root> as real
 * env vars on the prisc+x process this op is exec'd from (execvp
 * inherits environment) - the same real mechanism every OP_CUSTOM
 * handler in prisc+x.c already resolves paths against.
 *
 * Checks <entity_dir>/.hq_manager/request.txt for a composer's own
 * SEND|<text> line (written by rc_write_send.sh, the robot-chat.xhtpm
 * composer's cli_io action=). On a real one, unescapes it and runs the
 * already-proven ai_chat.+x <entity_dir> <house_root> "<text>" (the
 * real chat backend swap this whole rework was about - unchanged from
 * before), then consumes request.txt. A safe, cheap no-op when there
 * is nothing pending.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4352

static void unescape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < outsz; p++) {
        if (*p == '\\' && *(p + 1) == '\\') { out[o++] = '\\'; p++; }
        else out[o++] = *p;
    }
    out[o] = '\0';
}

int main(void) {
    const char *entity_dir = getenv("PRISC_PROJECT_ROOT");
    const char *house_root = getenv("KHTPM_HOUSE");
    if (!entity_dir || !entity_dir[0] || !house_root || !house_root[0]) return 0;

    char req_path[PATH_BUF];
    snprintf(req_path, sizeof(req_path), "%s/.hq_manager/request.txt", entity_dir);
    FILE *f = fopen(req_path, "r");
    if (!f) return 0;
    char line[1024];
    int got = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    if (!got) return 0;
    remove(req_path); /* one-shot consume, same convention every other
                          request.txt poll in this house uses */
    line[strcspn(line, "\r\n")] = '\0';
    if (strncmp(line, "SEND|", 5) != 0) return 0;

    char text[1024];
    unescape_line(line + 5, text, sizeof(text));
    if (!text[0]) return 0;

    char esc[1024];
    { size_t o = 0; for (const unsigned char *p = (const unsigned char *)text; *p && o + 5 < sizeof(esc); p++) {
        if (*p == '\'') { memcpy(esc + o, "'\\''", 4); o += 4; } else esc[o++] = (char)*p;
    } esc[o] = '\0'; }

    char bin[PATH_BUF];
    snprintf(bin, sizeof(bin), "%s/&.widgits/entity-cli/ops/+x/ai_chat.+x", house_root);
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' '%s' >/dev/null 2>&1", bin, entity_dir, house_root, esc);
    int rc = system(cmd);
    (void)rc;
    return 0;
}
