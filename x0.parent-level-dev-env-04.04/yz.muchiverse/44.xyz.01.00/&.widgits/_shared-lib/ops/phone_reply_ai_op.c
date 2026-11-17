/* phone_reply_ai_op - the model-backed reply for the on-message event (registry command phone_reply_ai; Texting design 3.5e, owner 2026-11-16 "hybrid").
 *
 * Usage: phone_reply_ai_op.+x <phone_dir> <entity_dir> <house> <to_number> <fallback_text>      (the incoming text is read from $MUCHI_MSG_TEXT)
 * It asks the entity's own model for a reply, then sends it as an ordinary outbox line through phone_send_op (kind say, ref t:<to>), ALWAYS starting with "[auto] "
 * so no two bots chain. If the model cannot answer (call failed, no history line, empty reply, or the daily cap is reached) it sends <fallback_text> instead, so a
 * message is never left without the reply the page promised. Prints AI-REPLY ok | AI-REPLY fallback <why>.
 * How the model is called (BOOK 17.ai/ENTITY-LLM-ENDPOINTS-HOW-TO-TEST-AND-USE-2026-11-16.md): <house>/&.widgits/entity-cli/ops/ai_chat.+x <entity_dir> <house> <text>
 * (the shared LAN Ollama on the Mac by default; appends to <entity_dir>/chat_history.txt, whose LAST line is the reply). fork+exec+waitpid with a 30 s watchdog.
 * PRIVACY: the incoming text goes to the model. A page uses this command only when the owner chose it for that entity (opt-in, never the default page).
 * QUOTA: at most 50 model calls per entity per 24 h (<entity_dir>/ai_reply_log.txt, one epoch_ms per line, trimmed to the last 24 h when over 32 KiB); the free API
 * backends are never used by this op (it calls ai_chat, the LAN one). Exit: 0 sent, 1 refused by phone_send_op, 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define DAILY_MAX 50
static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static long long fsize(const char *p) { struct stat s; return stat(p, &s) ? -1 : (long long)s.st_size; }

static int calls_today(const char *log, long long now, int trim) {
    FILE *f = fopen(log, "r"); char line[64]; int n = 0; long long keep[4096]; int nk = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) { long long v = atoll(line); if (v >= now - 86400000LL) { n++; if (nk < 4096) keep[nk++] = v; } }
    fclose(f);
    if (trim && fsize(log) > 32768 && (f = fopen(log, "w"))) { for (int i = 0; i < nk; i++) fprintf(f, "%lld\n", keep[i]); fclose(f); }
    return n;
}
static int run_ai(const char *house, const char *ent, const char *text) {
    char op[2048]; snprintf(op, sizeof op, "%s/&.widgits/entity-cli/ops/ai_chat.+x", house);
    pid_t pid = fork(); if (pid < 0) return 125;
    if (pid == 0) { int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); } execl(op, op, ent, house, text, (char *)0); _exit(127); }
    for (int i = 0; i < 300; i++) { int st; if (waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : 128; usleep(100000); }
    kill(pid, SIGKILL); waitpid(pid, NULL, 0); return 124;
}
/* last line of the history -> the reply text after "[date] label: " ; returns 1 if a usable reply was found */
static int last_reply(const char *hist, char *out, size_t n) {
    FILE *f = fopen(hist, "r"); char line[4096], last[4096] = ""; if (!f) return 0;
    while (fgets(line, sizeof line, f)) if (line[0] == '[') snprintf(last, sizeof last, "%s", line);
    fclose(f); last[strcspn(last, "\r\n")] = 0;
    char *b = strstr(last, "] "); if (!b) return 0; b += 2;
    char *c = strstr(b, ": "); if (!c) return 0; c += 2;
    while (*c == ' ') c++;
    size_t k = 0; for (; *c && k + 1 < n && k < 300; c++) out[k++] = (*c == '|' || *c == '\t') ? '/' : *c;
    while (k > 0 && out[k - 1] == ' ') k--;
    out[k] = 0;
    return k > 0;
}
static int send_text(const char *house, const char *phone, const char *to, const char *text) {
    char op[2048], ref[64]; snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/phone_send_op.+x", house); snprintf(ref, sizeof ref, "t:%s", to);
    pid_t pid = fork(); if (pid < 0) return 1;
    if (pid == 0) { execl(op, op, phone, to, "say", ref, text, (char *)0); _exit(127); }
    int st; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "usage: phone_reply_ai_op <phone_dir> <entity_dir> <house> <to_number> <fallback_text>\n"); return 2; }
    const char *phone = argv[1], *ent = argv[2], *house = argv[3], *to = argv[4], *fb = argv[5], *in = getenv("MUCHI_MSG_TEXT");
    char log[2200], hist[2200], reply[400], msg[512]; const char *why = NULL; long long now = now_ms();
    snprintf(log, sizeof log, "%s/ai_reply_log.txt", ent); snprintf(hist, sizeof hist, "%s/chat_history.txt", ent);
    if (!in || !*in) why = "no incoming text";
    else if (calls_today(log, now, 1) >= DAILY_MAX) why = "daily cap";
    else {
        long long before = fsize(hist); if (before < 0) before = 0;
        char ent_row[64]; snprintf(ent_row, sizeof ent_row, "%lld\n", now); FILE *lf = fopen(log, "a"); if (lf) { fputs(ent_row, lf); fclose(lf); }   /* count the attempt, success or not */
        int rc = run_ai(house, ent, in);
        if (rc != 0) why = "model call failed";
        else if (fsize(hist) <= before) why = "model wrote no history line";
        else if (!last_reply(hist, reply, sizeof reply)) why = "empty reply";
    }
    if (why) { snprintf(msg, sizeof msg, "%s", fb); printf("AI-REPLY fallback %s\n", why); }
    else { snprintf(msg, sizeof msg, "[auto] %s", reply); printf("AI-REPLY ok\n"); }
    if (strncmp(msg, "[auto]", 6)) { char t[600]; snprintf(t, sizeof t, "[auto] %s", msg); snprintf(msg, sizeof msg, "%.511s", t); }   /* a fallback text without the marker still gets it */
    return send_text(house, phone, to, msg) == 0 ? 0 : 1;
}
