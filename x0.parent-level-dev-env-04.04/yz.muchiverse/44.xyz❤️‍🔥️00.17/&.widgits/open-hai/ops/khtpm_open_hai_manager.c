/* khtpm_open_hai_manager.c — open-hai's MANAGER binary (Stage 2d
 * shell/manager split, same real mechanism proven on db-hq/events-hq/
 * chat-hai - see local-2do-15.txt's own open-hai entry). Launched by
 * khtpm_open_hai_render.c's own main() via a real fork()+execv() reading
 * open-hai.chtpm-equivalent... actually open-hai has no .chtpm at all
 * (Group B - hand-rolled immediate-mode, no Elem/CSS parser, confirmed
 * in khtpm-merge-how2.md's own STATUS section) - the <module> tag
 * mechanism still applies (it's just a runtime child-process launch,
 * not tied to the Elem/parse_chtpm architecture), the shell just reads
 * the module path from a small dedicated config file instead of a
 * .chtpm tag - see khtpm_open_hai_render.c's own launch_module() call
 * site for the exact source.
 *
 * Owns EVERYTHING that used to be khtpm_open_hai_render.c's own business
 * logic (session persistence, the real Ollama HTTP call, tool
 * detection+execution, moved here near-verbatim - same "port, don't
 * rewrite" convention already used for db-hq/events-hq):
 *   - session list scan + create/load/delete, publishes to
 *     state/sessions.state.txt (one "dir|label" per line) and
 *     state/active_session.txt (which one is currently live).
 *   - the real async Ollama curl call + response parsing (send_to_
 *     ollama()/check_pending()/extract_response_field(), unchanged
 *     fire-a-child/poll-non-blocking shape) - appends results directly
 *     to the active session's own transcript.txt, same file/format the
 *     shell already knows how to read (no redundant state file needed
 *     for messages - transcript.txt IS the real data, matching chat-
 *     hai's own ledger-is-the-source-of-truth shape more closely than
 *     db-hq/events-hq's synthetic state files needed to).
 *   - real tool detection+execution (detect_tool()/tool_*()/
 *     execute_pending_tool_into()/start_tool_job(), unchanged) - the
 *     approve/deny gate now happens via state/pending_tool.state.txt
 *     (shell reads this to render the approve/deny UI) and
 *     state/request.txt's "APPROVE"/"DENY" commands (shell writes these
 *     instead of calling start_tool_job()/dismissing it directly).
 *   - state/busy.state.txt ("1"/"0") - replaces g_pending as something
 *     the shell can see (it used to be an in-process flag; now it's a
 *     published file, same shape as db-hq/events-hq's own state
 *     publishes).
 *
 * Real request protocol (state/request.txt, shell writes, manager polls
 * + clears, matching db-hq/events-hq's own action.txt convention):
 *   SEND|<escaped prompt>          - same escape_line() encoding
 *                                     persist_msg() already used for
 *                                     transcript.txt, reused here so a
 *                                     multi-line prompt survives one
 *                                     request line.
 *   APPROVE                        - run the pending tool
 *   DENY                           - dismiss it, no-op
 *   NEWSESSION                     - start a fresh session
 *   LOADSESSION|<dir>              - switch to an existing session
 *   DELETESESSION|<dir>            - delete a session
 *
 * model.txt (under state/sessions/, unchanged path/format) stays the
 * shell's own responsibility to WRITE (cycle_model() is a cheap, purely
 * local UI action) - this manager only ever READS it fresh before each
 * send_to_ollama() call, so a model switch takes effect on the very
 * next message with zero extra IPC needed. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define PATH_BUF 4096
#define MAX_MSGS 512
#define MSG_LEN 8192
#define MAX_SESSIONS 64
#define TOOL_MAX_ARG 512

#define AUDIT_DIR_REL "&.widgits/open-hai/pieces/audit"

static char g_house_root[PATH_BUF];
static char g_sessions_root[PATH_BUF];
static char g_audit_dir[PATH_BUF];
static char g_state_dir[PATH_BUF];
static char g_session_dir[PATH_BUF] = "";

/* ---------- IPC file paths ---------- */
static char g_request_path[PATH_BUF];
static char g_sessions_state_path[PATH_BUF];
static char g_active_session_path[PATH_BUF];
static char g_pending_tool_state_path[PATH_BUF];
static char g_busy_state_path[PATH_BUF];

/* ---------- session persistence (ported verbatim from
 * khtpm_open_hai_render.c - see that file's own header comment for the
 * real transcript.txt format/reasoning, unchanged here) ---------- */
static void escape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 2 < outsz; i++) {
        if (in[i] == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
        else if (in[i] == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

static void unescape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < outsz; i++) {
        if (in[i] == '\\' && in[i + 1] == 'n') { out[o++] = '\n'; i++; }
        else if (in[i] == '\\' && in[i + 1] == '\\') { out[o++] = '\\'; i++; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

/* ---------- incoming-message tone (2026-08-16, direct instruction:
 * "play a tone when a message is posted" - incoming only, toggleable
 * off via each app's own Settings) ----------
 * The renderer owns state/settings.pdl (GUI toggle in the Settings
 * submenu); this manager reads it FRESH at every incoming message so
 * a toggle goes live immediately - same "re-read every round" contract
 * chat-hai's own sleep_between() uses. Missing file = sound ON (never
 * hard-fails). All sound assets stay local to the app - this chain
 * needs no temp files at all: prefer a real notification sound, fall
 * back to a synthesized beep through sox's play. */
static int sound_on_enabled(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/settings.pdl", g_state_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[256];
    int v = 1;
    while (fgets(line, sizeof(line), f)) {
        /* require BOTH bars and match the key exactly - a bare strstr
         * on "sound_on" would hit the header comment's "sound_on is 1"
         * text first (v stays default ON), FOUND LIVE 2026-08-16. */
        char *bar = strchr(line, '|');
        if (!bar) continue;
        char *bar2 = strchr(bar + 1, '|');
        if (!bar2) continue;
        char *key = bar + 1;
        char *val = bar2 + 1;
        *bar2 = '\0'; /* cut the key at the second bar - the rest stays intact for atoi() */
        while (*key == ' ') key++;
        char *kend = key + strlen(key);
        while (kend > key && kend[-1] == ' ') *--kend = '\0';
        if (strcmp(key, "sound_on") == 0) { v = atoi(val); break; }
    }
    fclose(f);
    return v != 0;
}

static void play_incoming_tone(void) {
    if (!sound_on_enabled()) return;
    system("canberra-gtk-play --id=message 2>/dev/null || "
           "play -n synth 0.12 sine 880 vol 0.2 2>/dev/null");
}

static void persist_msg(int is_user, const char *text) {
    if (!g_session_dir[0]) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/transcript.txt", g_session_dir);
    FILE *f = fopen(path, "a");
    if (!f) return;
    char esc[MSG_LEN * 2];
    escape_line(text, esc, sizeof(esc));
    fprintf(f, "%c|%s\n", is_user ? 'U' : 'A', esc);
    fclose(f);
    if (!is_user) play_incoming_tone();
}

/* Disk-read equivalent of the shell's own last_assistant_text() (which
 * scanned its in-memory g_msgs[] - the manager has no such array, it
 * only ever writes transcript.txt, never holds it in memory). */
static void last_assistant_text(char *out, size_t outsz) {
    out[0] = '\0';
    if (!g_session_dir[0]) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/transcript.txt", g_session_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MSG_LEN * 2];
    char last_a[MSG_LEN] = "";
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n < 2 || line[1] != '|' || line[0] != 'A') continue;
        unescape_line(line + 2, last_a, sizeof(last_a));
    }
    fclose(f);
    snprintf(out, outsz, "%s", last_a);
}

static void write_active_session(void) {
    FILE *f = fopen(g_active_session_path, "w");
    if (!f) return;
    fprintf(f, "%s\n", g_session_dir);
    fclose(f);
}

static void start_new_session(void) {
    time_t now = time(NULL);
    snprintf(g_session_dir, sizeof(g_session_dir), "%s/%ld", g_sessions_root, (long)now);
    mkdir(g_sessions_root, 0755);
    mkdir(g_session_dir, 0755);
    write_active_session();
}

static void load_session(const char *dir) {
    snprintf(g_session_dir, sizeof(g_session_dir), "%s", dir);
    write_active_session();
}

static void delete_session(const char *dir) {
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    int rc = system(cmd);
    (void)rc;
    if (strcmp(dir, g_session_dir) == 0) g_session_dir[0] = '\0';
}

typedef struct { char dir[PATH_BUF]; char label[80]; } SessionEntry;
static int session_cmp_desc(const void *a, const void *b) {
    const SessionEntry *sa = a, *sb = b;
    return strcmp(sb->dir, sa->dir);
}

static void publish_sessions(void) {
    SessionEntry sessions[MAX_SESSIONS];
    int n_sessions = 0;
    DIR *d = opendir(g_sessions_root);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n_sessions < MAX_SESSIONS) {
            if (e->d_name[0] == '.') continue;
            char full[PATH_BUF];
            snprintf(full, sizeof(full), "%s/%s", g_sessions_root, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            SessionEntry *se = &sessions[n_sessions];
            snprintf(se->dir, sizeof(se->dir), "%s", full);
            time_t epoch = (time_t)atol(e->d_name);
            struct tm *tmv = localtime(&epoch);
            char tsbuf[32] = "";
            if (tmv) strftime(tsbuf, sizeof(tsbuf), "%m-%d %H:%M", tmv);
            char snippet[48] = "";
            char tpath[PATH_BUF];
            snprintf(tpath, sizeof(tpath), "%s/transcript.txt", full);
            FILE *tf = fopen(tpath, "r");
            if (tf) {
                char line[256];
                while (fgets(line, sizeof(line), tf)) {
                    if (line[0] == 'U' && line[1] == '|') {
                        char raw[256];
                        snprintf(raw, sizeof(raw), "%s", line + 2);
                        raw[strcspn(raw, "\r\n")] = '\0';
                        char un[64];
                        unescape_line(raw, un, sizeof(un));
                        snprintf(snippet, sizeof(snippet), "%s", un);
                        break;
                    }
                }
                fclose(tf);
            }
            if (snippet[0]) snprintf(se->label, sizeof(se->label), "%s %s", tsbuf, snippet);
            else snprintf(se->label, sizeof(se->label), "%s (empty)", tsbuf);
            n_sessions++;
        }
        closedir(d);
    }
    qsort(sessions, (size_t)n_sessions, sizeof(SessionEntry), session_cmp_desc);

    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_sessions_state_path);
    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    for (int i = 0; i < n_sessions; i++) fprintf(wf, "%s|%s\n", sessions[i].dir, sessions[i].label);
    fclose(wf);
    rename(tmp, g_sessions_state_path);
}

/* ---------- backend: raw Ollama HTTP (ported verbatim) ---------- */
/* REAL START 2026-08-16, direct instruction ("get the api work started
 * with just the basic code and connection logic it needs. and then
 * document it and the next agent can pick up where we left off") -
 * BACKEND_OPENROUTER is the router API key work flagged repeatedly
 * throughout this session's own local-2do-15.txt and never started
 * until now. This is a REAL, BUILDING, but DELIBERATELY MINIMAL first
 * increment - connection plumbing only (key loading, request/response
 * shape, curl dispatch), NOT wired into the live model-cycling UI or
 * tested against a real key yet. See au11-hq/openrouter-integration-
 * handoff.md for the full real status + exact next steps for whoever
 * picks this up. */
typedef enum { BACKEND_OLLAMA_RAW = 0, BACKEND_AGENT45_LEGACY = 1, BACKEND_HARNECIENT = 2, BACKEND_OPENROUTER = 3, BACKEND_TOKENROUTER = 4 } BackendMode;
typedef struct { const char *name; BackendMode mode; } ModelEntry;
/* REAL 2026-08-16, direct instruction ("i wanna make sure they work...
 * make sure we can get the models going"): both real keys confirmed
 * live (direct curl tests, see OPENROUTER-INTEGRATION-HANDOFF.md's own
 * "REAL VERIFIED 2026-08-16" section for the full transcripts) - both
 * models below are REAL, CURRENTLY-WORKING free-tier models as of this
 * test, not guesses. Free-tier model slugs on both services rotate
 * over time (OpenRouter's own error on a stale slug during this
 * session's testing confirmed this directly) - if a model here 404s
 * later, re-check GET https://openrouter.ai/api/v1/models for a
 * current ":free" entry with "tools" in supported_parameters. */
static const ModelEntry g_models[] = {
    { "stable-code:latest", BACKEND_HARNECIENT },
    { "gemma3:1b", BACKEND_HARNECIENT },
    { "gemma3:270m", BACKEND_HARNECIENT },
    { "llama3-groq-tool-use:8b", BACKEND_OLLAMA_RAW },
    { "llama2:latest", BACKEND_HARNECIENT },
    /* REAL 2026-08-16: both live-tested working (plain chat AND real
     * tool_calls confirmed for the OpenRouter entry; TokenRouter's own
     * tool-call test hit a real, distinct "cache-only admission
     * rejected a cold request" 503 during testing - plain chat
     * confirmed working, tool-calls unconfirmed, see handoff doc). Not
     * gated by key-availability yet (see current_model()'s own real
     * next-step note below) - if the key file is ever removed, cycling
     * onto these will fail per-message with a clear inline error
     * (send_to_openrouter()/send_to_tokenrouter() below), not silently. */
    { "google/gemma-4-26b-a4b-it:free", BACKEND_OPENROUTER },
    /* REAL FIX 2026-08-18, direct live test (curl, real tool_calls
     * response, not guessed): the entry above (gemma-4-26b-a4b-it:free)
     * hit a real, live 429 - "temporarily rate-limited upstream...
     * shared pool" (Google AI Studio's free-tier pool, congested at
     * test time, not a key problem - confirmed by testing the SAME key
     * against other free models seconds later, see below). Direct user
     * report matches this exactly ("my key doesn't work with any
     * openrouter agent apis for free"). Tested 3 other real, current
     * (GET https://openrouter.ai/api/v1/models, 2026-08-18) ":free"
     * models with the SAME real tools-array shape send_to_openrouter()
     * already sends: z-ai/glm-5.2:free ALSO hit the same shared-pool
     * 429; these two below both returned a real, live
     * finish_reason:"tool_calls" response on the first try - added as
     * more-reliable alternates, gemma entry kept (its congestion is
     * plausibly temporary, not a reason to remove a previously-working
     * model). Slugs, like the comment above already warns, rotate over
     * time - re-verify with the same GET before assuming these are
     * still live months from now. */
    { "nvidia/nemotron-3.5-lightning:free", BACKEND_OPENROUTER },
    { "cohere/north-mini-code:free", BACKEND_OPENROUTER },
    { "qwen/qwen3.8-max-free", BACKEND_TOKENROUTER }
};
static const int g_n_models = sizeof(g_models) / sizeof(g_models[0]);
static const char *g_ollama_host = "10.0.0.144:11434";

/* Real key loading - a plain local file, never hardcoded/committed
 * (matches every other real secret-adjacent convention in this house -
 * e.g. this file's own g_sessions_root-relative state/ files). Real
 * keys ARE now in place (2026-08-16, from /home/no/Desktop/🤖️🪤️🏠️/
 * &.FREE-AI-KEY/&.Secret-Keys.txt, intentionally kept out of any zip/
 * archive per the user's own storage) - chmod 600, state/
 * openrouter_api_key.txt / state/tokenrouter_api_key.txt. Absence is
 * still treated as a normal, expected state (not an error) for anyone
 * else's checkout without these files - openrouter_key_available()/
 * tokenrouter_key_available() below are the real gates every caller
 * checks first. */
static char g_openrouter_key_path[PATH_BUF];
static void init_openrouter_key_path(void) {
    snprintf(g_openrouter_key_path, sizeof(g_openrouter_key_path), "%s/openrouter_api_key.txt", g_state_dir);
}
static int load_openrouter_key(char *out, size_t outsz) {
    FILE *f = fopen(g_openrouter_key_path, "r");
    if (!f) return 0;
    char buf[512] = "";
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    if (!buf[0]) return 0;
    snprintf(out, outsz, "%s", buf);
    return 1;
}
static int openrouter_key_available(void) {
    char key[512];
    return load_openrouter_key(key, sizeof(key));
}

/* Real TokenRouter key/endpoint - a SEPARATE real service from
 * OpenRouter (confirmed directly by the user after this session
 * initially assumed "TokenRouter" might just be an alternate name for
 * OpenRouter - it is not; different key, different base URL
 * (api.tokenrouter.com vs openrouter.ai), different real request/
 * response JSON shape, see send_to_tokenrouter()'s own header comment
 * and OPENROUTER-INTEGRATION-HANDOFF.md's protocol notes). */
static char g_tokenrouter_key_path[PATH_BUF];
static void init_tokenrouter_key_path(void) {
    snprintf(g_tokenrouter_key_path, sizeof(g_tokenrouter_key_path), "%s/tokenrouter_api_key.txt", g_state_dir);
}
static int load_tokenrouter_key(char *out, size_t outsz) {
    FILE *f = fopen(g_tokenrouter_key_path, "r");
    if (!f) return 0;
    char buf[512] = "";
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    if (!buf[0]) return 0;
    snprintf(out, outsz, "%s", buf);
    return 1;
}
static int tokenrouter_key_available(void) {
    char key[512];
    return load_tokenrouter_key(key, sizeof(key));
}

/* Reads state/sessions/model.txt fresh - the shell owns WRITING this
 * (cycle_model() stays shell-side, cheap local UI action), this manager
 * only ever reads it right before a send, so a model switch takes
 * effect on the very next message. */
static void current_model(char *name_out, size_t name_outsz, BackendMode *mode_out) {
    snprintf(name_out, name_outsz, "%s", g_models[0].name);
    *mode_out = g_models[0].mode;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/model.txt", g_sessions_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char buf[128];
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        for (int i = 0; i < g_n_models; i++) {
            if (strcmp(buf, g_models[i].name) == 0) {
                snprintf(name_out, name_outsz, "%s", g_models[i].name);
                *mode_out = g_models[i].mode;
                break;
            }
        }
    }
    fclose(f);
}

static int load_persona(const char *model_name, char *out, size_t outsz) {
    char slug[128];
    snprintf(slug, sizeof(slug), "%s", model_name);
    for (char *p = slug; *p; p++) if (*p == ':' || *p == '/') *p = '_';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.widgits/open-hai/pieces/registry/personas/%s.txt", g_house_root, slug);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, outsz - 1, f);
    out[n] = '\0';
    fclose(f);
    return n > 0;
}

static void escape_json_string(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 2 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c >= 32) { out[o++] = (char)c; }
    }
    out[o] = '\0';
}

static int g_pending = 0;
static pid_t g_pending_pid = -1;
static char g_pending_outfile[PATH_BUF];
static int g_pending_is_tool = 0;
/* REAL START 2026-08-16: which extractor check_pending() should use for
 * the current in-flight request - Ollama's /api/generate and
 * OpenRouter's /chat/completions have different real JSON response
 * shapes, set at dispatch time (send_to_ollama()/send_to_openrouter())
 * so the response-check code doesn't need to re-derive it. */
static BackendMode g_pending_backend_mode = BACKEND_OLLAMA_RAW;
/* REAL FIX 2026-08-28, direct live bug: every send path below used to
 * silently `if (g_pending) return;` with zero feedback - a message
 * typed while a previous request was still in flight just vanished,
 * and when the OLD request finally resolved (possibly with an error,
 * e.g. a bad OpenRouter model), that stale result showed up instead,
 * looking exactly like "I switched models and the new one didn't
 * respond." Root-caused live: a real "hi" sent to OpenRouter, model
 * switched to gemma3:1b ~29s later while still pending, and the
 * eventual OpenRouter parse-error is what landed in the transcript -
 * gemma3 was never actually asked anything. Two real fixes: (1) a
 * dropped send now persists a real, visible message instead of
 * vanishing; (2) switching models while a request is pending now
 * cancels the stale request immediately (real SIGTERM + reap, not a
 * freeze/wait) so the new model's send goes through right away -
 * direct instruction: "i dont want it to freeze... if model is
 * switched it should automatically unfreeze." g_pending_model_name
 * remembers which model a still-in-flight request was actually sent
 * under, so dispatch_send() can tell "still on the same model, just
 * slow" (drop + message) apart from "model changed since this was
 * sent" (cancel + proceed). */
static char g_pending_model_name[128] = "";

static void write_busy_state(void) {
    FILE *f = fopen(g_busy_state_path, "w");
    if (f) { fprintf(f, "%d\n", g_pending ? 1 : 0); fclose(f); }
}

static void cancel_pending(const char *reason) {
    if (!g_pending) return;
    kill(g_pending_pid, SIGTERM);
    waitpid(g_pending_pid, NULL, 0);
    if (g_pending_outfile[0]) unlink(g_pending_outfile);
    g_pending = 0;
    g_pending_is_tool = 0;
    write_busy_state();
    if (reason && reason[0]) persist_msg(0, reason);
}

static void send_to_ollama(const char *prompt) {
    /* Real fix 2026-08-28: dispatch_send() already cancels a pending
     * request when the MODEL changed - reaching this guard still true
     * means the same model is genuinely still answering. Say so instead
     * of silently eating the new message (see g_pending_model_name's
     * own header comment for the full real bug this fixes). */
    if (g_pending) { persist_msg(0, "[dropped: previous request to this model is still in flight - wait for it, or switch models to cancel it]"); return; }

    char model_name[128];
    BackendMode backend_mode;
    current_model(model_name, sizeof(model_name), &backend_mode);

    char full_prompt[MSG_LEN + 2048];
    const char *prompt_to_send = prompt;
    if (backend_mode == BACKEND_HARNECIENT) {
        char persona[2048];
        if (load_persona(model_name, persona, sizeof(persona))) {
            snprintf(full_prompt, sizeof(full_prompt), "%s\n\n%s", persona, prompt);
            prompt_to_send = full_prompt;
        }
    }

    char esc[MSG_LEN * 2 + 4096];
    escape_json_string(prompt_to_send, esc, sizeof(esc));

    char payload_path[PATH_BUF];
    snprintf(payload_path, sizeof(payload_path), "%s/payload-%d.json", g_audit_dir, (int)getpid());
    FILE *pf = fopen(payload_path, "w");
    if (!pf) return;
    fprintf(pf, "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}", model_name, esc);
    fclose(pf);

    snprintf(g_pending_outfile, sizeof(g_pending_outfile), "%s/response-%d.json", g_audit_dir, (int)getpid());
    unlink(g_pending_outfile);

    pid_t pid = fork();
    if (pid == 0) {
        char url[256];
        snprintf(url, sizeof(url), "http://%s/api/generate", g_ollama_host);
        int fd = open(g_pending_outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        char data_arg[PATH_BUF + 2];
        snprintf(data_arg, sizeof(data_arg), "@%s", payload_path);
        execlp("curl", "curl", "-s", "-m", "60", "-X", "POST", url,
               "-H", "Content-Type: application/json", "-d", data_arg,
               (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        g_pending = 1;
        g_pending_pid = pid;
        g_pending_backend_mode = BACKEND_OLLAMA_RAW;
        snprintf(g_pending_model_name, sizeof(g_pending_model_name), "%s", model_name);
        write_busy_state();
    }
}

/* REAL START 2026-08-16 - see this file's own BACKEND_OPENROUTER header
 * comment for full context/scope. Real, basic connection logic only:
 * OpenRouter's real chat-completions endpoint (OpenAI-compatible shape,
 * https://openrouter.ai/docs), same async fork+curl+response-file
 * pattern as send_to_ollama() above - not a parallel design, the same
 * one. NOT YET CALLED from anywhere real (no caller wires backend_mode
 * == BACKEND_OPENROUTER to this yet - that's current_model()/g_models'
 * own real next step, see the handoff doc). Written and ready for that
 * wiring, not exercised against a real key/response yet. */
static void send_to_openrouter(const char *prompt, const char *model_name) {
    /* See send_to_ollama()'s own real-fix comment - dispatch_send()
     * already cancels a stale pending request on a model switch. */
    if (g_pending) { persist_msg(0, "[dropped: previous request to this model is still in flight - wait for it, or switch models to cancel it]"); return; }

    char key[512];
    if (!load_openrouter_key(key, sizeof(key))) {
        persist_msg(0, "[error: no OpenRouter API key - create &.widgits/open-hai/state/openrouter_api_key.txt with a real key from https://openrouter.ai/keys]");
        return;
    }

    char esc[MSG_LEN * 2 + 4096];
    escape_json_string(prompt, esc, sizeof(esc));

    char payload_path[PATH_BUF];
    snprintf(payload_path, sizeof(payload_path), "%s/or-payload-%d.json", g_audit_dir, (int)getpid());
    FILE *pf = fopen(payload_path, "w");
    if (!pf) return;
    /* REAL 2026-08-16, direct instruction ("test chat using relay
     * injection... to do tool calls with new api (if they do toolcalls
     * we can bypass tools harnesses used for gemma)") - real OpenAI-
     * style `tools` array, matching open-hai's own REAL local tool names
     * (list_dir/read_file - see detect_tool()/tool_list_dir()/
     * tool_read_file() elsewhere in this file) so a genuine API-native
     * tool_calls response can be compared directly against what the
     * local Harnecient-hack dispatcher already produces for the same
     * request shape. Real, deliberate scope limit: this sends the
     * tools param and the response gets a real tool_calls DETECTION
     * (see extract_openrouter_content() below), but does NOT execute
     * the tool or feed a result back yet - that's a real, separate,
     * larger round-trip (system prompt needs a tool_call_id + role:
     * tool follow-up message) not attempted in this pass. */
    fprintf(pf, "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
                "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"list_dir\","
                "\"description\":\"List files in a directory\",\"parameters\":{\"type\":\"object\","
                "\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}},"
                "{\"type\":\"function\",\"function\":{\"name\":\"read_file\","
                "\"description\":\"Read a file's contents\",\"parameters\":{\"type\":\"object\","
                "\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}}]}",
            model_name, esc);
    fclose(pf);

    snprintf(g_pending_outfile, sizeof(g_pending_outfile), "%s/or-response-%d.json", g_audit_dir, (int)getpid());
    unlink(g_pending_outfile);

    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(g_pending_outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        char auth_hdr[600];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", key);
        char data_arg[PATH_BUF + 2];
        snprintf(data_arg, sizeof(data_arg), "@%s", payload_path);
        execlp("curl", "curl", "-s", "-m", "60", "-X", "POST",
               "https://openrouter.ai/api/v1/chat/completions",
               "-H", "Content-Type: application/json",
               "-H", auth_hdr,
               "-d", data_arg,
               (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        g_pending = 1;
        g_pending_pid = pid;
        g_pending_backend_mode = BACKEND_OPENROUTER;
        snprintf(g_pending_model_name, sizeof(g_pending_model_name), "%s", model_name);
        write_busy_state();
    }
}

/* REAL 2026-08-16, direct instruction ("test chat using relay
 * injection... to do tool calls with new api") - real tool_calls
 * DETECTION (not execution - see send_to_openrouter()'s own header
 * comment for the real scope line). When the model picks a tool,
 * OpenAI-compatible APIs set message.content to JSON null (not a
 * string) and populate message.tool_calls instead - real, minimal
 * strstr-based check for that shape, same style as every other
 * extractor in this file. Returns 1 and fills a real, readable
 * "[tool_call] name(args)" summary if found. */
/* REAL 2026-08-16, direct instruction ("it says not executed. pls do
 * execution pass so i can see it in gui") - raw name+path extraction
 * so the real caller (check_pending() below) can build a real
 * PendingTool and hand it to the SAME real start_tool_job()/
 * execute_pending_tool_into() the local Harnecient-hack path already
 * uses - one real execution engine, two ways to arrive at a PendingTool
 * (local keyword detection vs a real API-native tool_calls response),
 * not a second parallel implementation. Only extracts a single "path"
 * argument (matches the 2 real tools currently offered - list_dir/
 * read_file, see send_to_openrouter()'s own tools array) - a real,
 * documented scope limit, not an oversight. */
static int extract_openrouter_tool_call_raw(const char *json, char *name_out, size_t name_outsz, char *path_out, size_t path_outsz) {
    const char *tc = strstr(json, "\"tool_calls\":[{");
    if (!tc) return 0;
    const char *name_key = strstr(tc, "\"name\":\"");
    if (!name_key) return 0;
    name_key += 8;
    size_t ni = 0;
    while (name_key[ni] && name_key[ni] != '"' && ni + 1 < name_outsz) { name_out[ni] = name_key[ni]; ni++; }
    name_out[ni] = '\0';
    path_out[0] = '\0';
    /* REAL FIX 2026-08-16, caught before shipping: "arguments" is a
     * JSON-STRING-ENCODED JSON object (real OpenAI shape - see this
     * file's own extract_openrouter_tool_call() a few lines up, which
     * already handles this for its own summary string), so its own
     * quotes appear BACKSLASH-ESCAPED in the raw response bytes -
     * \"path\":\" - not a bare "path":" like a real, unescaped JSON
     * key. Searching for the unescaped form would never match real
     * live responses (confirmed live: the un-harnessed relay test
     * this fix was written to support). */
    const char *path_key = strstr(name_key, "\\\"path\\\":\\\"");
    if (path_key) {
        path_key += strlen("\\\"path\\\":\\\"");
        size_t pi = 0;
        while (*path_key && pi + 1 < path_outsz) {
            if (path_key[0] == '\\' && path_key[1] == '"') break; /* end of the JSON-string-encoded value */
            path_out[pi++] = *path_key++;
        }
        path_out[pi] = '\0';
    }
    return name_out[0] != '\0';
}

static int extract_openrouter_tool_call(const char *json, char *out, size_t outsz) {
    const char *tc = strstr(json, "\"tool_calls\":[{");
    if (!tc) return 0;
    const char *name_key = strstr(tc, "\"name\":\"");
    const char *args_key = strstr(tc, "\"arguments\":\"");
    if (!name_key) return 0;
    name_key += 8;
    char name[128] = "";
    size_t ni = 0;
    while (name_key[ni] && name_key[ni] != '"' && ni + 1 < sizeof(name)) { name[ni] = name_key[ni]; ni++; }
    name[ni] = '\0';
    char args[512] = "";
    if (args_key) {
        args_key += strlen("\"arguments\":\"");
        size_t ai = 0;
        while (*args_key && ai + 1 < sizeof(args)) {
            if (*args_key == '\\' && args_key[1] == '"') { args[ai++] = '"'; args_key += 2; }
            else if (*args_key == '"') break;
            else args[ai++] = *args_key++;
        }
        args[ai] = '\0';
    }
    snprintf(out, outsz, "[tool_call requested by model] %s(%s) - real API-native tool call, NOT executed (detection only this pass)", name, args);
    return 1;
}

/* Real OpenAI-compatible response shape: choices[0].message.content -
 * different key/nesting than Ollama's own flat "response" field, same
 * minimal strstr-based extraction style as extract_response_field()
 * below (this codebase doesn't use a real JSON parser anywhere yet -
 * not introduced here either, consistency over a bigger unrelated
 * change). */
static void extract_openrouter_content(const char *json, char *out, size_t outsz) {
    if (extract_openrouter_tool_call(json, out, outsz)) return;
    const char *key = "\"content\":\"";
    const char *p = strstr(json, key);
    out[0] = '\0';
    if (!p) return;
    p += strlen(key);
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < outsz) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') { out[o++] = '\n'; }
            else if (*p == 't') { out[o++] = '\t'; }
            else { out[o++] = *p; }
            p++;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
}

/* REAL 2026-08-16, direct instruction ("make sure we can get the
 * models going... doing the std toolcalls, and documenting their
 * unique protocols if any") - TokenRouter (api.tokenrouter.com) is a
 * SEPARATE real service from OpenRouter, confirmed via direct live
 * curl testing this session (both real request AND response shapes
 * verified, not guessed - see OPENROUTER-INTEGRATION-HANDOFF.md for
 * the full transcripts). Two REAL, CONFIRMED protocol differences from
 * OpenRouter/plain-OpenAI:
 *   1. Request "content" is an ARRAY of {"type":"text","text":".."}
 *      objects, not a bare string - see send_to_tokenrouter() below.
 *   2. Response JSON uses "key": "value" (space after colon) where
 *      OpenRouter's is compact "key":"value" (confirmed byte-exact
 *      from both services' real live responses this session) -
 *      extract_tokenrouter_content() below searches for the
 *      space-included key for this reason, NOT copy-paste carelessness.
 * Real, separate finding: the response message also carries a genuine
 * "reasoning_content" field (the model's real chain-of-thought,
 * separate from its final answer) AND a top-level "tool_calls" field
 * on every message (null when no tool was called) - TokenRouter
 * appears to support real OpenAI-style tool_calls natively, but this
 * session's own live tool-call test against it hit a real, distinct
 * 503 "cache-only admission rejected a cold or overloaded request"
 * (code "cache_only_cold") - plain chat completions confirmed working,
 * tool-calling itself was NOT successfully confirmed end-to-end this
 * session (see handoff doc's own open item). */
static void send_to_tokenrouter(const char *prompt, const char *model_name) {
    /* See send_to_ollama()'s own real-fix comment - dispatch_send()
     * already cancels a stale pending request on a model switch. */
    if (g_pending) { persist_msg(0, "[dropped: previous request to this model is still in flight - wait for it, or switch models to cancel it]"); return; }

    char key[512];
    if (!load_tokenrouter_key(key, sizeof(key))) {
        persist_msg(0, "[error: no TokenRouter API key - create &.widgits/open-hai/state/tokenrouter_api_key.txt with a real key]");
        return;
    }

    char esc[MSG_LEN * 2 + 4096];
    escape_json_string(prompt, esc, sizeof(esc));

    char payload_path[PATH_BUF];
    snprintf(payload_path, sizeof(payload_path), "%s/tr-payload-%d.json", g_audit_dir, (int)getpid());
    FILE *pf = fopen(payload_path, "w");
    if (!pf) return;
    /* real TokenRouter request shape - content is an ARRAY, confirmed
     * live (see this function's own header comment point 1). */
    fprintf(pf, "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"%s\"}]}]}", model_name, esc);
    fclose(pf);

    snprintf(g_pending_outfile, sizeof(g_pending_outfile), "%s/tr-response-%d.json", g_audit_dir, (int)getpid());
    unlink(g_pending_outfile);

    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(g_pending_outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        char auth_hdr[600];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", key);
        char data_arg[PATH_BUF + 2];
        snprintf(data_arg, sizeof(data_arg), "@%s", payload_path);
        execlp("curl", "curl", "-s", "-m", "60", "-X", "POST",
               "https://api.tokenrouter.com/v1/chat/completions",
               "-H", "Content-Type: application/json",
               "-H", auth_hdr,
               "-d", data_arg,
               (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        g_pending = 1;
        g_pending_pid = pid;
        g_pending_backend_mode = BACKEND_TOKENROUTER;
        snprintf(g_pending_model_name, sizeof(g_pending_model_name), "%s", model_name);
        write_busy_state();
    }
}

static void extract_tokenrouter_content(const char *json, char *out, size_t outsz) {
    /* real key with the space TokenRouter's own JSON formatting uses -
     * see this file's own send_to_tokenrouter() header comment point 2.
     * "content" (real answer) appears before "reasoning_content" (chain
     * of thought) in every real response seen this session, so the
     * FIRST match is the real one - real but fragile if key order ever
     * changes upstream (documented, not silently trusted). */
    const char *key = "\"content\": \"";
    const char *p = strstr(json, key);
    out[0] = '\0';
    if (!p) return;
    p += strlen(key);
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < outsz) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') { out[o++] = '\n'; }
            else if (*p == 't') { out[o++] = '\t'; }
            else { out[o++] = *p; }
            p++;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    /* real live responses had leading "\n\n" before the actual answer
     * (confirmed: "content": "\n\npong" for a one-word reply) - trim
     * leading whitespace/newlines so persist_msg() doesn't show a
     * message that visually starts with blank lines. */
    size_t lead = 0;
    while (out[lead] == '\n' || out[lead] == '\r' || out[lead] == ' ' || out[lead] == '\t') lead++;
    if (lead) memmove(out, out + lead, o - lead + 1);
}

static void extract_response_field(const char *json, char *out, size_t outsz) {
    const char *key = "\"response\":\"";
    const char *p = strstr(json, key);
    out[0] = '\0';
    if (!p) return;
    p += strlen(key);
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < outsz) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') { out[o++] = '\n'; }
            else if (*p == 't') { out[o++] = '\t'; }
            else if (*p == 'u' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2]) &&
                     isxdigit((unsigned char)p[3]) && isxdigit((unsigned char)p[4])) {
                char hex[5] = { p[1], p[2], p[3], p[4], '\0' };
                unsigned int cp = (unsigned int)strtoul(hex, NULL, 16);
                p += 4;
                if (cp < 0x80) {
                    if (o + 1 < outsz) out[o++] = (char)cp;
                } else if (cp < 0x800) {
                    if (o + 2 < outsz) {
                        out[o++] = (char)(0xC0 | (cp >> 6));
                        out[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                } else {
                    if (o + 3 < outsz) {
                        out[o++] = (char)(0xE0 | (cp >> 12));
                        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                }
            }
            else { out[o++] = *p; }
        } else {
            out[o++] = *p;
        }
        p++;
    }
    out[o] = '\0';
}

/* REAL 2026-08-16, moved earlier in the file (was declared further
 * down, right before its own original single use site) - check_pending()
 * below now needs it too, to build a real PendingTool from an
 * OpenRouter-native tool_calls response and hand it to the SAME real
 * execution engine (start_tool_job()) the local Harnecient-hack path
 * already uses. Forward declarations for the functions still defined
 * later in this file (unmoved - only the type needed to move). */
typedef struct {
    char name[32];
    char arg[TOOL_MAX_ARG];
    char search[TOOL_MAX_ARG];
    char content[MSG_LEN];
} PendingTool;
static int tool_requires_approval(const char *name);
static void start_tool_job(PendingTool *pt);
static void write_pending_tool_state(void);
static PendingTool g_pending_tool;
static int g_tool_pending = 0;

static void check_pending(void) {
    if (!g_pending) return;
    int status;
    pid_t r = waitpid(g_pending_pid, &status, WNOHANG);
    if (r != g_pending_pid) return;
    g_pending = 0;
    write_busy_state();

    if (g_pending_is_tool) {
        g_pending_is_tool = 0;
        FILE *f = fopen(g_pending_outfile, "r");
        if (!f) { persist_msg(0, "[tool error: no output file]"); return; }
        char buf[MSG_LEN * 2];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        unlink(g_pending_outfile);
        persist_msg(0, buf[0] ? buf : "[tool: no output]");
        return;
    }

    FILE *f = fopen(g_pending_outfile, "r");
    if (!f) { persist_msg(0, "[error: curl produced no output]"); return; }
    char buf[MSG_LEN * 4];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    unlink(g_pending_outfile);

    char resp[MSG_LEN];
    if (g_pending_backend_mode == BACKEND_OPENROUTER) {
        /* REAL 2026-08-16, direct instruction ("it says not executed.
         * pls do execution pass so i can see it in gui") - a real
         * tool_calls response now gets ACTUALLY EXECUTED via the same
         * real start_tool_job()/execute_pending_tool_into() engine the
         * local Harnecient-hack path already uses, not just detected
         * and reported as inert. Same real approval gate
         * (tool_requires_approval()) applies - list_dir/read_file
         * (the only 2 tools currently offered to the API, see
         * send_to_openrouter()'s own tools array) are read-only and
         * auto-run; if a future tools array ever adds write_file/
         * cmd_exec, this same real gate stops it from silently
         * auto-executing an API-originated request. */
        char tool_name[32], tool_path[TOOL_MAX_ARG];
        if (extract_openrouter_tool_call_raw(buf, tool_name, sizeof(tool_name), tool_path, sizeof(tool_path))) {
            PendingTool pt;
            memset(&pt, 0, sizeof(pt));
            snprintf(pt.name, sizeof(pt.name), "%s", tool_name);
            snprintf(pt.arg, sizeof(pt.arg), "%s", tool_path);
            if (tool_requires_approval(pt.name)) {
                g_pending_tool = pt;
                g_tool_pending = 1;
                write_pending_tool_state();
                char banner[MSG_LEN];
                snprintf(banner, sizeof(banner), "[tool request from model] %s %s - approve/deny in the sidebar", pt.name, pt.arg);
                persist_msg(0, banner);
            } else {
                start_tool_job(&pt);
            }
            return;
        }
        extract_openrouter_content(buf, resp, sizeof(resp));
        if (resp[0]) persist_msg(0, resp);
        else persist_msg(0, "[error: no 'content' field in OpenRouter reply - check model name / key / raw response in or-response-*.json under the audit dir]");
        return;
    }
    if (g_pending_backend_mode == BACKEND_TOKENROUTER) {
        extract_tokenrouter_content(buf, resp, sizeof(resp));
        if (resp[0]) persist_msg(0, resp);
        else persist_msg(0, "[error: no 'content' field in TokenRouter reply - check model name / key / raw response in tr-response-*.json under the audit dir]");
        return;
    }
    extract_response_field(buf, resp, sizeof(resp));
    if (resp[0]) persist_msg(0, resp);
    else persist_msg(0, "[error: no 'response' field in Ollama reply - check model name / endpoint]");
}

/* ---------- REAL TOOLS (ported verbatim - see khtpm_open_hai_render.c's
 * own header comment above detect_tool() for the agent-45 provenance) ---------- */
static void write_pending_tool_state(void) {
    FILE *f = fopen(g_pending_tool_state_path, "w");
    if (!f) return;
    if (g_tool_pending) {
        char banner_preview[64] = "";
        snprintf(banner_preview, sizeof(banner_preview), "%.60s", g_pending_tool.content);
        for (size_t i = 0; banner_preview[i]; i++) if (banner_preview[i] == '\n') banner_preview[i] = ' ';
        fprintf(f, "%s|%s|%s\n", g_pending_tool.name, g_pending_tool.arg, banner_preview);
    }
    fclose(f);
}

static void to_lower_str(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static char *find_word(const char *text, const char *word) {
    size_t klen = strlen(word);
    for (char *p = (char *)text; *p; p++) {
        if (strncasecmp(p, word, klen) == 0) {
            char before = (p == text) ? ' ' : *(p - 1);
            char after = *(p + klen);
            if (!isalnum((unsigned char)before) && !isalnum((unsigned char)after)) return p;
        }
    }
    return NULL;
}

static int earliest_kw(const char *text, const char *kws[], int *klen_out) {
    int best = -1, best_len = 0;
    for (int i = 0; kws[i]; i++) {
        char *h = find_word(text, kws[i]);
        if (h && (best < 0 || (h - text) < best || ((h - text) == best && (int)strlen(kws[i]) > best_len))) {
            best = (int)(h - text);
            best_len = (int)strlen(kws[i]);
        }
    }
    *klen_out = best_len;
    return best;
}

static void trim_ws(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void strip_outer_quotes(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static int next_token(char **cursor, char *out, size_t outsz) {
    char *p = *cursor;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    char *end = p;
    while (*end && !isspace((unsigned char)*end)) end++;
    size_t len = (size_t)(end - p);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    *cursor = end;
    return 1;
}

static int tok_is_pathish(const char *tok) {
    return tok[0] && (strchr(tok, '/') != NULL || strchr(tok, '.') != NULL);
}

static int extract_path_arg(const char *start, char *out, size_t outsz) {
    char tok[256], found[256] = "";
    char *cursor = (char *)start;
    int hit = 0;
    while (next_token(&cursor, tok, sizeof(tok))) {
        if (tok_is_pathish(tok)) { snprintf(found, sizeof(found), "%s", tok); hit = 1; }
    }
    if (!hit) return 0;
    strip_outer_quotes(found);
    snprintf(out, outsz, "%s", found);
    return 1;
}

static int tool_requires_approval(const char *name) {
    return strcmp(name, "write_file") == 0 ||
           strcmp(name, "edit_file") == 0 ||
           strcmp(name, "cmd_exec") == 0;
}

static int detect_tool(const char *msg, PendingTool *pt) {
    char lower[MSG_LEN];
    snprintf(lower, sizeof(lower), "%s", msg);
    to_lower_str(lower);
    PendingTool t;
    memset(&t, 0, sizeof(t));
    int off, klen;

    {
        const char *kws[] = {"read file", "open file", "cat file", "view file",
                             "read", "open", "cat", "view", "display", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        if (extract_path_arg(tail, t.arg, sizeof(t.arg))) {
            snprintf(t.name, sizeof(t.name), "read_file");
            *pt = t;
            return 1;
        }
    }

    {
        const char *kws[] = {"write file", "create file", "save file",
                             "write", "create", "save", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        if (extract_path_arg(tail, t.arg, sizeof(t.arg))) {
            snprintf(t.name, sizeof(t.name), "write_file");
            char *bestc = NULL;
            int bestclen = 0;
            const char *cps[] = {"containing", "that says", NULL};
            for (int i = 0; cps[i]; i++) {
                char *h = find_word(lower, cps[i]);
                if (h && (!bestc || h < bestc)) { bestc = h; bestclen = (int)strlen(cps[i]); }
            }
            if (bestc) {
                char *c = (char *)msg + (bestc - lower) + bestclen;
                trim_ws(c);
                strip_outer_quotes(c);
                snprintf(t.content, sizeof(t.content), "%s", c);
            } else {
                char last[MSG_LEN];
                last_assistant_text(last, sizeof(last));
                if (last[0]) snprintf(t.content, sizeof(t.content), "%s", last);
            }
            *pt = t;
            return 1;
        }
    }

    {
        const char *kws[] = {"edit", "modify", "append", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        if (extract_path_arg(tail, t.arg, sizeof(t.arg))) {
            snprintf(t.name, sizeof(t.name), "edit_file");
            char *rkw = find_word(lower, "replace");
            char *wkw = find_word(lower, "with");
            if (rkw && wkw && wkw > rkw) {
                char *so = (char *)msg + (rkw - lower) + 7;
                size_t n = (size_t)(wkw - rkw) - 7;
                if (n > 0 && n < sizeof(t.search)) {
                    memcpy(t.search, so, n);
                    t.search[n] = '\0';
                    trim_ws(t.search);
                    strip_outer_quotes(t.search);
                }
                char *co = (char *)msg + (wkw - lower) + 4;
                trim_ws(co);
                strip_outer_quotes(co);
                snprintf(t.content, sizeof(t.content), "%s", co);
            } else {
                const char *line_kws[] = {"the line", "the text", NULL};
                char *bc = NULL;
                int bclen = 0;
                for (int i = 0; line_kws[i]; i++) {
                    char *h = find_word(lower, line_kws[i]);
                    if (h && (!bc || h < bc)) { bc = h; bclen = (int)strlen(line_kws[i]); }
                }
                if (bc) {
                    char *c = (char *)msg + (bc - lower) + bclen;
                    trim_ws(c);
                    strip_outer_quotes(c);
                    snprintf(t.content, sizeof(t.content), "%s", c);
                } else {
                    char last[MSG_LEN];
                    last_assistant_text(last, sizeof(last));
                    if (last[0]) snprintf(t.content, sizeof(t.content), "%s", last);
                }
            }
            *pt = t;
            return 1;
        }
    }

    {
        const char *kws[] = {"search", "grep", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        trim_ws(tail);
        if (tail[0]) {
            char *in = find_word(lower, " in ");
            if (in && in > lower + off) {
                size_t qn = (size_t)(in - lower) - (size_t)(off + klen);
                if (qn > 0 && qn < sizeof(t.arg)) {
                    memcpy(t.arg, tail, qn);
                    t.arg[qn] = '\0';
                    trim_ws(t.arg);
                    strip_outer_quotes(t.arg);
                }
                char *tp = (char *)msg + (in - lower) + 4;
                trim_ws(tp);
                extract_path_arg(tp, t.search, sizeof(t.search));
                if (t.arg[0]) {
                    snprintf(t.name, sizeof(t.name), "search_in_files");
                    *pt = t;
                    return 1;
                }
            } else {
                strip_outer_quotes(tail);
                snprintf(t.arg, sizeof(t.arg), "%s", tail);
                snprintf(t.name, sizeof(t.name), "search_in_files");
                *pt = t;
                return 1;
            }
        }
    }

    {
        const char *kws[] = {"list", "show", "dir", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        snprintf(t.name, sizeof(t.name), "list_dir");
        if (!extract_path_arg(tail, t.arg, sizeof(t.arg))) t.arg[0] = '\0';
        *pt = t;
        return 1;
    }

    {
        const char *kws[] = {"run", "execute", "command", "cmd", "exec", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        trim_ws(tail);
        if (tail[0]) {
            snprintf(t.name, sizeof(t.name), "cmd_exec");
            snprintf(t.arg, sizeof(t.arg), "%s", tail);
            *pt = t;
            return 1;
        }
    }

    return 0;
}

static void tool_list_dir(const char *path, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (path[0] == '/') snprintf(resolved, sizeof(resolved), "%s", path);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, path);
    DIR *d = opendir(resolved);
    if (!d) { snprintf(out, outsz, "[list_dir] cannot open: %s", resolved); return; }
    char buf[MSG_LEN / 2] = "";
    snprintf(buf, sizeof(buf), "[list_dir] %s:\n", resolved);
    struct dirent *e;
    while ((e = readdir(d)) && strlen(buf) < sizeof(buf) - 512) {
        char line[300];
        snprintf(line, sizeof(line), "  %s%s\n", e->d_name, (e->d_type == DT_DIR) ? "/" : "");
        if (strlen(buf) + strlen(line) >= sizeof(buf)) {
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  ... (truncated)");
            break;
        }
        strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
    }
    closedir(d);
    snprintf(out, outsz, "%s", buf);
}

static void tool_read_file(const char *path, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (path[0] == '/') snprintf(resolved, sizeof(resolved), "%s", path);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, path);
    FILE *f = fopen(resolved, "rb");
    if (!f) { snprintf(out, outsz, "[read_file] cannot open: %s", resolved); return; }
    char buf[MSG_LEN / 2];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    snprintf(out, outsz, "[read_file] %s:\n%s", resolved, buf);
}

static void tool_write_file(const PendingTool *pt, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (pt->arg[0] == '/') snprintf(resolved, sizeof(resolved), "%s", pt->arg);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, pt->arg);
    if (!pt->content[0]) { snprintf(out, outsz, "[write_file] nothing to write (no content given, no prior ai answer)"); return; }
    FILE *f = fopen(resolved, "wb");
    if (!f) { snprintf(out, outsz, "[write_file] cannot create: %s", resolved); return; }
    size_t n = fwrite(pt->content, 1, strlen(pt->content), f);
    fclose(f);
    snprintf(out, outsz, "[write_file] wrote %zu bytes to %s", n, resolved);
}

static void tool_edit_file(const PendingTool *pt, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (pt->arg[0] == '/') snprintf(resolved, sizeof(resolved), "%s", pt->arg);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, pt->arg);
    if (pt->search[0]) {
        FILE *f = fopen(resolved, "rb");
        if (!f) { snprintf(out, outsz, "[edit_file] cannot open: %s", resolved); return; }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > MSG_LEN * 2) {
            fclose(f);
            snprintf(out, outsz, "[edit_file] file too big to edit in place");
            return;
        }
        char *data = malloc((size_t)sz + 1);
        if (!data) { fclose(f); snprintf(out, outsz, "[edit_file] out of memory"); return; }
        size_t rd = fread(data, 1, (size_t)sz, f);
        (void)rd;
        fclose(f);
        data[sz] = '\0';
        char *hit = strstr(data, pt->search);
        if (!hit) { free(data); snprintf(out, outsz, "[edit_file] pattern not found: %s", pt->search); return; }
        size_t hoff = (size_t)(hit - data);
        size_t slen = strlen(pt->search);
        size_t clen = strlen(pt->content);
        char *newd = malloc((size_t)sz - slen + clen + 1);
        if (!newd) { free(data); snprintf(out, outsz, "[edit_file] out of memory"); return; }
        memcpy(newd, data, hoff);
        memcpy(newd + hoff, pt->content, clen);
        memcpy(newd + hoff + clen, hit + slen, (size_t)sz - hoff - slen + 1);
        free(data);
        FILE *wf = fopen(resolved, "wb");
        if (!wf) { free(newd); snprintf(out, outsz, "[edit_file] cannot write: %s", resolved); return; }
        fwrite(newd, 1, strlen(newd), wf);
        fclose(wf);
        free(newd);
        snprintf(out, outsz, "[edit_file] replaced in %s", resolved);
    } else {
        if (!pt->content[0]) { snprintf(out, outsz, "[edit_file] nothing to append"); return; }
        FILE *f = fopen(resolved, "ab");
        if (!f) { snprintf(out, outsz, "[edit_file] cannot open: %s", resolved); return; }
        size_t n = fwrite(pt->content, 1, strlen(pt->content), f);
        fclose(f);
        snprintf(out, outsz, "[edit_file] appended %zu bytes to %s", n, resolved);
    }
}

static void escape_single_quote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        if (in[i] == '\'') { memcpy(out + o, "'\\''", 4); o += 4; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

static void tool_search(const PendingTool *pt, char *out, size_t outsz) {
    char q[MSG_LEN * 2], ts[PATH_BUF * 2], cmd[PATH_BUF * 4];
    escape_single_quote(pt->arg, q, sizeof(q));
    if (pt->search[0]) {
        char rp[PATH_BUF];
        if (pt->search[0] == '/') snprintf(rp, sizeof(rp), "%s", pt->search);
        else snprintf(rp, sizeof(rp), "%s/%s", g_house_root, pt->search);
        escape_single_quote(rp, ts, sizeof(ts));
        snprintf(cmd, sizeof(cmd), "grep -rn -- '%s' '%s' 2>&1 | head -30", q, ts);
    } else {
        escape_single_quote(g_house_root, ts, sizeof(ts));
        snprintf(cmd, sizeof(cmd), "grep -rn -- '%s' '%s' 2>&1 | head -30", q, ts);
    }
    FILE *pipe = popen(cmd, "r");
    if (!pipe) { snprintf(out, outsz, "[search_in_files] failed to start grep"); return; }
    char buf[MSG_LEN / 2];
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    buf[n] = '\0';
    pclose(pipe);
    if (!n) snprintf(out, outsz, "[search_in_files] no matches for: %s", pt->arg);
    else snprintf(out, outsz, "[search_in_files] %s:\n%s", pt->arg, buf);
}

static void tool_exec(const PendingTool *pt, char *out, size_t outsz) {
    FILE *pipe = popen(pt->arg, "r");
    if (!pipe) { snprintf(out, outsz, "[cmd_exec] failed to start"); return; }
    char buf[MSG_LEN / 2];
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    buf[n] = '\0';
    pclose(pipe);
    if (!n) snprintf(out, outsz, "[cmd_exec] OK (no output)");
    else snprintf(out, outsz, "[cmd_exec] %s\n%s", pt->arg, buf);
}

static void execute_pending_tool_into(const PendingTool *pt, char *out, size_t outsz) {
    if (strcmp(pt->name, "list_dir") == 0) tool_list_dir(pt->arg, out, outsz);
    else if (strcmp(pt->name, "read_file") == 0) tool_read_file(pt->arg, out, outsz);
    else if (strcmp(pt->name, "write_file") == 0) tool_write_file(pt, out, outsz);
    else if (strcmp(pt->name, "edit_file") == 0) tool_edit_file(pt, out, outsz);
    else if (strcmp(pt->name, "search_in_files") == 0) tool_search(pt, out, outsz);
    else if (strcmp(pt->name, "cmd_exec") == 0) tool_exec(pt, out, outsz);
    else snprintf(out, outsz, "[tool] unknown tool: %s", pt->name);
}

static void start_tool_job(PendingTool *pt) {
    /* Same real fix as the send_* functions above - a tool run isn't
     * tied to a specific model, so a model switch does not auto-cancel
     * it (nothing stale to reconcile), but a silently-dropped tool
     * request is the same confusing symptom - say so. */
    if (g_pending) { persist_msg(0, "[dropped: a previous request is still in flight - wait for it to finish before running another tool]"); return; }
    g_pending_tool = *pt;
    snprintf(g_pending_outfile, sizeof(g_pending_outfile), "%s/toolout-%d.txt", g_audit_dir, (int)getpid());
    unlink(g_pending_outfile);
    pid_t pid = fork();
    if (pid == 0) {
        char result[MSG_LEN];
        result[0] = '\0';
        execute_pending_tool_into(&g_pending_tool, result, sizeof(result));
        int fd = open(g_pending_outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { ssize_t wr = write(fd, result, strlen(result)); (void)wr; close(fd); }
        _exit(0);
    } else if (pid > 0) {
        g_pending = 1;
        g_pending_pid = pid;
        g_pending_is_tool = 1;
        write_busy_state();
    }
}

/* ported verbatim from khtpm_open_hai_render.c's own tool_request_banner() -
 * the shell used to persist this into the chat immediately so the user
 * sees WHY approval is being asked for; kept here since the manager now
 * owns persist_msg(). */
static void tool_request_banner(const PendingTool *pt, char *out, size_t outsz) {
    char preview[48] = "";
    if (pt->content[0]) {
        snprintf(preview, sizeof(preview), "%.40s", pt->content);
        for (size_t i = 0; preview[i]; i++) if (preview[i] == '\n') preview[i] = ' ';
    }
    snprintf(out, outsz, "[tool request] %s %s%s", pt->name, pt->arg,
             preview[0] ? "  (+content: " : "");
    if (preview[0]) { size_t l = strlen(out); snprintf(out + l, outsz - l, "%s)", preview); }
    strncat(out, " - approve/deny in the sidebar", outsz - strlen(out) - 1);
}

/* REAL 2026-08-16 - real dispatch by backend_mode, the piece that was
 * previously missing entirely: send_to_ollama() existed and worked,
 * send_to_openrouter()/send_to_tokenrouter() existed and worked (both
 * live-verified this session), but nothing ever branched between them -
 * every send unconditionally went to Ollama regardless of what
 * current_model() actually returned. This is that real wiring. */
static void dispatch_send(const char *prompt) {
    char model_name[128];
    BackendMode backend_mode;
    current_model(model_name, sizeof(model_name), &backend_mode);
    /* REAL FIX 2026-08-28, direct instruction ("i dont want it to
     * freeze... if model is switched it should automatically
     * unfreeze"): if something is still pending AND it was sent under
     * a DIFFERENT model than the one we're about to send to, that
     * pending request is stale relative to what the human is doing
     * right now - cancel it (real SIGTERM + reap, not a wait) so this
     * new send proceeds immediately instead of getting silently
     * dropped by the g_pending guards inside send_to_*(). A pending
     * request under the SAME model is genuinely still relevant - that
     * one is left alone and will produce the real "[dropped: ...]"
     * message from inside send_to_*() if this call races it. */
    if (g_pending && strcmp(g_pending_model_name, model_name) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[cancelled: switched away from %s while it was still answering]", g_pending_model_name);
        cancel_pending(msg);
    }
    if (backend_mode == BACKEND_OPENROUTER) { send_to_openrouter(prompt, model_name); return; }
    if (backend_mode == BACKEND_TOKENROUTER) { send_to_tokenrouter(prompt, model_name); return; }
    send_to_ollama(prompt);
}

/* ---------- request handling ---------- */
static void handle_submit(const char *prompt) {
    persist_msg(1, prompt);

    /* REAL 2026-08-16, direct instruction ("lets get it going in
     * open-hai, using inject and 'un-harnessed'"): OpenRouter's own
     * confirmed-working native tool_calls (see send_to_openrouter()'s
     * real tools param + extract_openrouter_tool_call() - live-verified
     * this session, naked curl AND now this real dispatch path) means
     * the local Harnecient-hack keyword detector (detect_tool() below)
     * is real dead weight for this specific backend - it would
     * otherwise intercept messages BEFORE they ever reach the API,
     * silently preventing the real native tool-calling from ever being
     * exercised (confirmed live: an earlier relay test asking to
     * "list_dir" got caught here and never reached OpenRouter at all).
     * Skip the local harness entirely when the current model is routed
     * through BACKEND_OPENROUTER - let the model's own real tool_calls
     * response decide, matching the "un-harnessed" instruction exactly.
     * TokenRouter is NOT included here - real tool-calling was never
     * successfully confirmed against it this session (blocked by a
     * real $0 account credit limit, not a protocol issue - see
     * OPENROUTER-INTEGRATION-HANDOFF.md's own "TokenRouter: paywalled,
     * non-starter for now" section) - it keeps using the local harness
     * until that's actually verified. */
    char model_name[128];
    BackendMode backend_mode;
    current_model(model_name, sizeof(model_name), &backend_mode);
    if (backend_mode == BACKEND_OPENROUTER) {
        dispatch_send(prompt);
        return;
    }

    PendingTool pt;
    if (detect_tool(prompt, &pt)) {
        if (tool_requires_approval(pt.name)) {
            g_pending_tool = pt;
            g_tool_pending = 1;
            write_pending_tool_state();
            char banner[MSG_LEN];
            tool_request_banner(&g_pending_tool, banner, sizeof(banner));
            persist_msg(0, banner);
        } else {
            start_tool_job(&pt);
        }
    } else {
        dispatch_send(prompt);
    }
}

static void handle_request(void) {
    FILE *f = fopen(g_request_path, "r");
    if (!f) return;
    char line[MSG_LEN * 2 + 32] = "";
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    if (!line[0]) return;

    FILE *cw = fopen(g_request_path, "w"); /* clear immediately - never re-fire the same request */
    if (cw) fclose(cw);

    if (strncmp(line, "SEND|", 5) == 0) {
        if (g_pending) { persist_msg(0, "[h-ai busy - previous request still running, send again when it finishes]"); return; }
        if (g_tool_pending) { persist_msg(0, "[a tool request is awaiting approve/deny in the sidebar]"); return; }
        char prompt[MSG_LEN * 2];
        unescape_line(line + 5, prompt, sizeof(prompt));
        handle_submit(prompt);
    } else if (strcmp(line, "APPROVE") == 0) {
        if (g_tool_pending) {
            g_tool_pending = 0;
            write_pending_tool_state();
            start_tool_job(&g_pending_tool);
        }
    } else if (strcmp(line, "DENY") == 0) {
        if (g_tool_pending) {
            char note[MSG_LEN];
            snprintf(note, sizeof(note), "[tool denied] %s %s", g_pending_tool.name, g_pending_tool.arg);
            persist_msg(0, note);
            g_tool_pending = 0;
            write_pending_tool_state();
        }
    } else if (strcmp(line, "NEWSESSION") == 0) {
        start_new_session();
        publish_sessions();
    } else if (strncmp(line, "LOADSESSION|", 12) == 0) {
        load_session(line + 12);
    } else if (strncmp(line, "DELETESESSION|", 14) == 0) {
        /* REAL FIX 2026-08-16: the shell used to follow a delete-the-
         * active-session action with its own separate new_chat() call -
         * two requests in a row would race on this app's single-line
         * request.txt (the second overwrites the first before the next
         * poll). Real fix: do both here, in one request, matching the
         * shell's original combined behavior exactly but without the
         * race - see khtpm_open_hai_render.c's own delete_focused_if_
         * session() comment for the full reasoning. */
        int was_active = g_session_dir[0] && strcmp(line + 14, g_session_dir) == 0;
        delete_session(line + 14);
        if (was_active) start_new_session();
        publish_sessions();
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "khtpm_open_hai_manager: usage: <house_root> [--data-root <dir>]\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    /* PER-INSTANCE DATA ROOT (2026-08-24, cursword chat): optional
     * --data-root redirects sessions/state/audit to one self-contained dir
     * (the calling entity pal's own chat/ dir) so a SECOND instance of this
     * same shared binary can run next to plain open-hai with its OWN session
     * history - same interface, same binary, separate data. Personas and the
     * emoji tile registry stay house-shared on purpose (assets, not data).
     * The render shell parses and forwards this exact flag via launch_module(). */
    char data_root[PATH_BUF] = "";
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--data-root") == 0 && i + 1 < argc) {
            snprintf(data_root, sizeof(data_root), "%s", argv[++i]);
        }
    }
    if (data_root[0] == '/') {
        snprintf(g_sessions_root, sizeof(g_sessions_root), "%s/sessions", data_root);
    } else {
        snprintf(g_sessions_root, sizeof(g_sessions_root), "%s/&.widgits/open-hai/sessions", g_house_root);
    }
    if (data_root[0] == '/') {
        snprintf(g_audit_dir, sizeof(g_audit_dir), "%s/audit", data_root);
    } else {
        snprintf(g_audit_dir, sizeof(g_audit_dir), "%s/%s", g_house_root, AUDIT_DIR_REL);
    }
    mkdir(g_audit_dir, 0755);
    if (data_root[0] == '/') {
        snprintf(g_state_dir, sizeof(g_state_dir), "%s/state", data_root);
    } else {
        snprintf(g_state_dir, sizeof(g_state_dir), "%s/&.widgits/open-hai/state", g_house_root);
    }
    mkdir(g_state_dir, 0755);
    init_openrouter_key_path(); /* REAL START 2026-08-16 - see this file's own BACKEND_OPENROUTER header comment */
    init_tokenrouter_key_path();

    snprintf(g_request_path, sizeof(g_request_path), "%s/request.txt", g_state_dir);
    snprintf(g_sessions_state_path, sizeof(g_sessions_state_path), "%s/sessions.state.txt", g_state_dir);
    snprintf(g_active_session_path, sizeof(g_active_session_path), "%s/active_session.txt", g_state_dir);
    snprintf(g_pending_tool_state_path, sizeof(g_pending_tool_state_path), "%s/pending_tool.state.txt", g_state_dir);
    snprintf(g_busy_state_path, sizeof(g_busy_state_path), "%s/busy.state.txt", g_state_dir);

    FILE *w = fopen(g_request_path, "w"); /* start clean */
    if (w) fclose(w);
    write_pending_tool_state(); /* g_tool_pending starts 0 - clears any stale file */
    write_busy_state();

    publish_sessions();
    /* real session bootstrap: use the most recent session if any exist,
     * matching the shell's own old startup behavior (loaded whichever
     * session was on top of the sorted list), else start fresh. */
    {
        FILE *sf = fopen(g_sessions_state_path, "r");
        char first_line[PATH_BUF] = "";
        if (sf) { if (fgets(first_line, sizeof(first_line), sf)) { /* dir|label */ } fclose(sf); }
        char *bar = strchr(first_line, '|');
        if (bar) {
            *bar = '\0';
            load_session(first_line);
        } else {
            start_new_session();
        }
    }

    for (;;) {
        check_pending();
        handle_request();
        usleep(200000); /* 200ms - a bit faster than db-hq/events-hq's 400ms, this app is more interactive/request-driven */
    }
    return 0;
}
