/* rc_publish_ui.c - real, single-purpose op for robot_chat.pal's own
 * poll loop. Replaces the ui.txt-publishing half of
 * khtpm_robot_chat_manager.c (now retired) - see rc_check_request.c's
 * own header comment for the full rework context.
 *
 * No argv needed: reads PRISC_PROJECT_ROOT=<entity_dir> from the
 * environment (launch_module()'s own real setenv, inherited by every
 * op this pal script exec's - see rc_check_request.c's header).
 *
 * Publishes <entity_dir>/.hq_manager/ui.txt from the entity's own real
 * chat_history.txt, in the SAME key format open-hai's own
 * khtpm_open_hai_manager.c already uses for its transcript
 * (msg_%d_text / msg_%d_class, confirmed by direct read of that file -
 * this house's own "reuse open-hai's layout" instruction only works if
 * the var names actually match what that layout's <repeat> expects).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_TRANSCRIPT 30

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
static void load_entity_label(const char *entity_dir, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pal.pdl", entity_dir);
    FILE *f = fopen(path, "r");
    if (f) {
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
                    snprintf(out, out_sz, "%s", tok3);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }
    const char *base = strrchr(entity_dir, '/');
    snprintf(out, out_sz, "%s", base ? base + 1 : entity_dir);
}

/* Parse "[timestamp] SPEAKER: text" lines from chat_history.txt (the
 * exact format ai_chat.c's own fprintf already writes) into the last
 * MAX_TRANSCRIPT (speaker, text) pairs, oldest-first in the output. */
typedef struct { char speaker[128]; char text[512]; } ChatLine;
static int load_transcript(const char *entity_dir, ChatLine *out, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/chat_history.txt", entity_dir);
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

static void write_kv(FILE *f, const char *key, const char *val) {
    fprintf(f, "%s=", key);
    for (const char *p = val; *p; p++) {
        if (*p == '\n' || *p == '\r') continue;
        fputc(*p, f);
    }
    fputc('\n', f);
}

int main(void) {
    const char *entity_dir = getenv("PRISC_PROJECT_ROOT");
    const char *house_root = getenv("KHTPM_HOUSE");
    if (!entity_dir || !entity_dir[0]) return 0;

    char mgr_dir[PATH_BUF];
    snprintf(mgr_dir, sizeof(mgr_dir), "%s/.hq_manager", entity_dir);
    ensure_dir(mgr_dir);

    char ui_path[PATH_BUF];
    snprintf(ui_path, sizeof(ui_path), "%s/ui.txt", mgr_dir);
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", ui_path);
    FILE *f = fopen(tmp_path, "w");
    if (!f) return 0;

    ChatLine lines[MAX_TRANSCRIPT];
    int n = load_transcript(entity_dir, lines, MAX_TRANSCRIPT);
    fprintf(f, "transcript_count=%d\n", n);
    for (int i = 0; i < n; i++) {
        char key[32];
        snprintf(key, sizeof(key), "msg_%d_text", i);
        char full[768];
        snprintf(full, sizeof(full), "%s: %s", lines[i].speaker, lines[i].text);
        write_kv(f, key, full);
        snprintf(key, sizeof(key), "msg_%d_class", i);
        int is_user = strcmp(lines[i].speaker, "USER") == 0;
        write_kv(f, key, is_user ? "msg-user" : "msg-hai");
    }
    /* REAL FIX 2026-09-30, direct live request ("it should say which
     * backend its using somewhere in robot-chat") - status was always
     * blank; this is the one already-wired label slot
     * (robot-chat.xhtpm's <text id="status">) with nothing in it.
     * Mirrors rc_check_request.c's own real switch/default exactly -
     * same file, same fallback - so this can never claim a backend
     * that file isn't actually using. */
    {
        char backend[32] = "gemma";
        char bpath[PATH_BUF];
        snprintf(bpath, sizeof(bpath), "%s/.hq_manager/chat_backend.txt", entity_dir);
        FILE *bf = fopen(bpath, "r");
        if (bf) {
            if (fgets(backend, sizeof(backend), bf)) backend[strcspn(backend, "\r\n")] = '\0';
            fclose(bf);
        }
        if (!backend[0]) snprintf(backend, sizeof(backend), "gemma");
        char status[64];
        if (strcmp(backend, "openrouter") == 0)
            snprintf(status, sizeof(status), "backend: OpenRouter");
        else if (strcmp(backend, "bank") == 0)
            snprintf(status, sizeof(status), "backend: OpenRouter -> pipeline");
        else
            snprintf(status, sizeof(status), "backend: Gemma (LAN)");
        write_kv(f, "status", status);
    }
    char label[128];
    load_entity_label(entity_dir, label, sizeof(label));
    write_kv(f, "entity_label", label);
    if (house_root && house_root[0]) {
        char send_action[PATH_BUF];
        snprintf(send_action, sizeof(send_action), "'%s/&.widgits/robot-chat/ops/rc_write_send.sh'", house_root);
        write_kv(f, "send_action", send_action);
    }
    fclose(f);
    rename(tmp_path, ui_path);
    return 0;
}
