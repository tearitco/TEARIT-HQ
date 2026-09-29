/* khtpm_robot_chat_manager.c - real manager for robot-chat.xhtpm
 * (AI-PUSH-ROADMAP-AND-NUANCES.md, "Track 1's Natural Test Surface: the
 * Drop-In Chatbot Entity"). Direct instruction 2026-09-28: real chat
 * dispatch first, before the personality-bank/template-delta mechanism
 * - replace robot_chat_001's hardcoded single test-line METHOD with a
 * real, typed, multi-turn window.
 *
 * Usage: khtpm_robot_chat_manager.+x <house_root> <entity_dir> [label]
 * - same real argv shape khtpm_events_hq_manager.c already requires
 * (confirmed via kh_launch_window_modules()'s own g_arg3_dir/
 * g_arg4_entity_label passthrough - this is a <module>, forked by the
 * shared renderer, not a standalone launch).
 *
 * Publishes: <entity_dir>/.hq_manager/ui.txt (transcript_count +
 * msgN_text/msgN_class + status + send_action) - auto-appended to the
 * window's vars= by the generic ARG3 instance-dir hook, no vars=
 * needed in robot-chat.xhtpm itself.
 * Polls: <entity_dir>/.hq_manager/request.txt for a composer SEND|<text>
 * line (written by rc_write_send.sh, the composer's own cli_io action=).
 *
 * Honest scope, matching every other checkpoint in this house's AI
 * track: this wires the real UI round trip on top of ai_chat.+x's
 * already-proven plumbing (gemma_ask()/chat_history.txt). It does NOT
 * touch the instance-scoped Concept-Bank personality mechanism, the
 * template/delta bank split, or the /command-vs-free-text HARNECIENT-
 * HACK dispatch layer - all three are still real, unbuilt designs (see
 * the roadmap doc's own "Design resolved this session (not yet built)"
 * section). Free text only for now; a literal "/command" typed into
 * the composer is sent to Gemma as plain text like anything else -
 * the dispatch layer that would intercept it first doesn't exist yet.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_TRANSCRIPT 30

static volatile sig_atomic_t g_running = 1;
static void on_term(int sig) { (void)sig; g_running = 0; }

static char g_house_root[PATH_BUF];
static char g_entity_dir[PATH_BUF];
static char g_label[128] = "Robot Chat";

static void ensure_dir(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

/* Real entity label from pal.pdl's own "PAL | name | <value>" line,
 * verbatim shape ai_chat.c's own load_entity_label() already uses. */
static void load_entity_label(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pal.pdl", g_entity_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *tok1 = strtok(line, "|");
        char *tok2 = tok1 ? strtok(NULL, "|") : NULL;
        char *tok3 = tok2 ? strtok(NULL, "|\r\n") : NULL;
        if (tok2 && tok3) {
            while (*tok2 == ' ') tok2++;
            size_t l2 = strlen(tok2); while (l2 > 0 && tok2[l2-1] == ' ') tok2[--l2] = '\0';
            if (strcmp(tok2, "name") == 0) {
                while (*tok3 == ' ') tok3++;
                size_t l3 = strlen(tok3); while (l3 > 0 && (tok3[l3-1]==' '||tok3[l3-1]=='\n'||tok3[l3-1]=='\r')) tok3[--l3] = '\0';
                snprintf(g_label, sizeof(g_label), "%s", tok3);
                break;
            }
        }
    }
    fclose(f);
}

/* Parse "[timestamp] SPEAKER: text" lines from chat_history.txt (the
 * exact format ai_chat.c's own fprintf already writes) into the last
 * MAX_TRANSCRIPT (speaker, text) pairs, oldest-first in the output. */
typedef struct { char speaker[128]; char text[512]; } ChatLine;
static int load_transcript(ChatLine *out, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/chat_history.txt", g_entity_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    ChatLine ring[MAX_TRANSCRIPT];
    int n = 0;
    char line[768];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        char *rb = strchr(line, ']');
        if (!rb) continue;
        char *rest = rb + 1;
        while (*rest == ' ') rest++;
        char *colon = strchr(rest, ':');
        if (!colon) continue;
        *colon = '\0';
        char *speaker = rest;
        char *text = colon + 1;
        while (*text == ' ') text++;
        ChatLine *slot = &ring[n % max];
        snprintf(slot->speaker, sizeof(slot->speaker), "%s", speaker);
        snprintf(slot->text, sizeof(slot->text), "%s", text);
        n++;
    }
    fclose(f);
    int count = n < max ? n : max;
    int start = n > max ? n % max : 0;
    for (int i = 0; i < count; i++) out[i] = ring[(start + i) % max];
    return count;
}

static void write_kv_escaped(FILE *f, const char *key, const char *val) {
    /* Plain key=value, one line - same convention every other manager's
     * ui.txt already uses. A value can't hold a real newline (chat
     * lines are single-line by construction, see ai_chat.c's own
     * trim), so no escaping needed beyond stripping any stray one. */
    fprintf(f, "%s=", key);
    for (const char *p = val; *p; p++) {
        if (*p == '\n' || *p == '\r') continue;
        fputc(*p, f);
    }
    fputc('\n', f);
}

static void publish_ui(const char *status) {
    char ui_path[PATH_BUF];
    snprintf(ui_path, sizeof(ui_path), "%s/.hq_manager/ui.txt", g_entity_dir);
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", ui_path);
    FILE *f = fopen(tmp_path, "w");
    if (!f) return;

    ChatLine lines[MAX_TRANSCRIPT];
    int n = load_transcript(lines, MAX_TRANSCRIPT);
    fprintf(f, "transcript_count=%d\n", n);
    for (int i = 0; i < n; i++) {
        char key[32];
        snprintf(key, sizeof(key), "msg%d_text", i);
        char full[768];
        snprintf(full, sizeof(full), "%s: %s", lines[i].speaker, lines[i].text);
        write_kv_escaped(f, key, full);
        snprintf(key, sizeof(key), "msg%d_class", i);
        int is_user = strcmp(lines[i].speaker, "USER") == 0;
        write_kv_escaped(f, key, is_user ? "chat-user" : "chat-bot");
    }
    write_kv_escaped(f, "status", status ? status : "");
    char send_action[PATH_BUF];
    snprintf(send_action, sizeof(send_action), "'%s/&.widgits/robot-chat/ops/rc_write_send.sh'", g_house_root);
    write_kv_escaped(f, "send_action", send_action);
    fclose(f);
    rename(tmp_path, ui_path);
}

/* Verbatim doubled-backslash decode, matching rc_write_send.sh's own
 * doubled-backslash encode (same real scheme oh_write_send.sh/
 * khtpm_open_hai_manager.c already use for this exact purpose). */
static void unescape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < outsz; p++) {
        if (*p == '\\' && *(p + 1) == '\\') { out[o++] = '\\'; p++; }
        else out[o++] = *p;
    }
    out[o] = '\0';
}

static void run_ai_chat(const char *text) {
    char bin[PATH_BUF];
    snprintf(bin, sizeof(bin), "%s/&.widgits/entity-cli/ops/+x/ai_chat.+x", g_house_root);
    char esc[1024];
    { size_t o = 0; for (const unsigned char *p = (const unsigned char *)text; *p && o + 5 < sizeof(esc); p++) {
        if (*p == '\'') { memcpy(esc + o, "'\\''", 4); o += 4; } else esc[o++] = (char)*p;
    } esc[o] = '\0'; }
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' '%s' >/dev/null 2>&1", bin, g_entity_dir, g_house_root, esc);
    publish_ui("thinking...");
    int rc = system(cmd);
    (void)rc;
    publish_ui("");
}

static void poll_request(void) {
    char req_path[PATH_BUF];
    snprintf(req_path, sizeof(req_path), "%s/.hq_manager/request.txt", g_entity_dir);
    FILE *f = fopen(req_path, "r");
    if (!f) return;
    char line[1024];
    int got = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    if (!got) return;
    remove(req_path); /* consume once - same one-shot convention every
                          other request.txt poll in this house uses */
    line[strcspn(line, "\r\n")] = '\0';
    if (strncmp(line, "SEND|", 5) != 0) return;
    char text[1024];
    unescape_line(line + 5, text, sizeof(text));
    if (text[0]) run_ai_chat(text);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <entity_dir> [label]\n", argv[0]);
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_entity_dir, sizeof(g_entity_dir), "%s", argv[2]);
    if (argc >= 4 && argv[3][0]) snprintf(g_label, sizeof(g_label), "%s", argv[3]);
    load_entity_label();

    char mgr_dir[PATH_BUF];
    snprintf(mgr_dir, sizeof(mgr_dir), "%s/.hq_manager", g_entity_dir);
    ensure_dir(mgr_dir);

    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);

    publish_ui("");
    while (g_running) {
        poll_request();
        usleep(200000);
    }
    return 0;
}
