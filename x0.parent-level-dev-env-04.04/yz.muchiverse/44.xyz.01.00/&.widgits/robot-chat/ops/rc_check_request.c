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
 * chat backend, then consumes request.txt. A safe, cheap no-op when
 * there is nothing pending.
 *
 * REAL FIX 2026-09-30, direct live report ("chat-api, chat-bank were
 * meant to open the same open-hai style layout robot-chat was using
 * but have the openrouter-api or pipeline bank backend, instead it's
 * opening a real linux terminal window, very out of character") - the
 * ONLY backend this op ever called was ai_chat.+x (Gemma/LAN), same as
 * the original single "Chat" button. Chat-api/Chat-bank's own
 * terminal-loop scripts (chat_openrouter_loop.sh/chat_bank_loop.sh)
 * were a wrong, disconnected shortcut - this house never opens a bare
 * gnome-terminal for anything, every chat surface is a real khtpm
 * window (this one). Real fix: <entity_dir>/.hq_manager/
 * chat_backend.txt (written by the meta.pdl METHOD row before this
 * SAME robot-chat window launches, see button.sh/rc_write_backend.sh)
 * selects which binary this op runs - "gemma" (default, absent file =
 * unchanged prior behavior) -> ai_chat.+x, "openrouter" ->
 * ai_chat_openrouter.+x, "bank" -> ai_chat.+x followed by a real
 * ai_describe.+x pass (the actual TEARIT-pipeline entry point
 * ROBOT-CHAT-BLUEPRINT.md §4 describes - Chat-bank's whole point is
 * feeding chat_history.txt into DESCRIBE, not a different LLM). */
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

    char backend[32] = "gemma";
    {
        char bpath[PATH_BUF];
        snprintf(bpath, sizeof(bpath), "%s/.hq_manager/chat_backend.txt", entity_dir);
        FILE *bf = fopen(bpath, "r");
        if (bf) {
            if (fgets(backend, sizeof(backend), bf)) backend[strcspn(backend, "\r\n")] = '\0';
            fclose(bf);
        }
        if (!backend[0]) snprintf(backend, sizeof(backend), "gemma");
    }

    /* "bank" uses the SAME OpenRouter round trip as "openrouter" - the
     * two loop scripts this replaces (chat_openrouter_loop.sh/
     * chat_bank_loop.sh) both called ai_chat_openrouter.+x; bank's own
     * distinction is the ai_describe.+x follow-up below, not a
     * different chat model. */
    char bin[PATH_BUF];
    if (strcmp(backend, "groq") == 0)   /* 2026-10-07: free Groq tier, same file contract as ai_chat_openrouter */
        snprintf(bin, sizeof(bin), "%s/&.widgits/entity-cli/ops/+x/ai_chat_groq.+x", house_root);
    else if (strcmp(backend, "openrouter") == 0 || strcmp(backend, "bank") == 0)
        snprintf(bin, sizeof(bin), "%s/&.widgits/entity-cli/ops/+x/ai_chat_openrouter.+x", house_root);
    else
        snprintf(bin, sizeof(bin), "%s/&.widgits/entity-cli/ops/+x/ai_chat.+x", house_root);

    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' '%s' >/dev/null 2>&1", bin, entity_dir, house_root, esc);
    int rc = system(cmd);
    (void)rc;

    if (strcmp(backend, "bank") == 0) {
        char dbin[PATH_BUF];
        snprintf(dbin, sizeof(dbin), "%s/&.widgits/entity-cli/ops/+x/ai_describe.+x", house_root);
        char dcmd[PATH_BUF * 2];
        snprintf(dcmd, sizeof(dcmd), "'%s' '%s' '%s' >/dev/null 2>&1", dbin, entity_dir, house_root);
        int rc2 = system(dcmd);
        (void)rc2;
    }
    return 0;
}
