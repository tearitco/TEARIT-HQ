/* send_message - the "Enter" verb. Reads the live input_buffer, and
 * either delegates to switch_model.+x (if the buffer starts with
 * "/model ") or sends it as a chat turn.
 *
 * Builds one of three request shapes depending on state.txt's
 * provider_kind, each ported from a proven-working TPMOS fix:
 *   - ollama:   native tools[] + /api/chat (build_ollama_request) -
 *               proven in groq-ollama.
 *   - gemini:   systemInstruction+contents+tools[functionDeclarations] +
 *               non-streaming generateContent (build_gemini_request) -
 *               ported from gem-dev's gemini_payload_builder.c.
 *   - llamacpp: manually-built raw Llama3 prompt + /completion
 *               (build_llamacpp_request, via text_to_pal_prompt.+x) -
 *               proven in cpp-llm. No native tool schema on this path.
 *   - iqabod:   no HTTP, no JSON request at all - forks
 *               main_orchestrator.+x directly (build_iqabod_prompt,
 *               see model_list.txt's header comment for how api_url/
 *               model_name are repurposed as IQABOD's project root /
 *               curriculum path). IQABOD's curricula are raw
 *               word-continuation models, not instruction-tuned to
 *               emit the tool/args/response JSON convention the
 *               llamacpp path uses - see ROADMAP-models.txt §4 - so
 *               this path only ever produces plain assistant text,
 *               never a tool call.
 *
 * Non-blocking by design: for ollama/gemini/llamacpp, forks a child that
 * execs connect_op (which itself blocks on curl internally and exits
 * when done); for iqabod, forks a child that execs main_orchestrator.+x
 * directly (no network, no connect_op). Either way the child's PID is
 * recorded in state.txt's curl_pid field, and this op returns
 * immediately. check_response.c (called every tick from main_loop.pal
 * while ai_state=THINKING) polls that PID for liveness to know when to
 * parse the response - ops have no persistent memory between
 * invocations, so this is the only way a one-shot op can hand off a
 * long-running call to future ticks, mirroring how prisc+x's own tick
 * loop already polls everything else.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: send_message.+x (no args - reads/writes only via state files) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_BUFFER 2048
#define MAX_FIELD 256
#define MAX_IQABOD_PROMPT_CHARS 200
#define MAX_IQABOD_LOG_LINES 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Real, live-caught bug (2026-07-30, same day as PITFALL 60's own
 * first fix): button.sh always sets PRISC_PROJECT_ROOT to the SESSION
 * directory (SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID",
 * confirmed by direct read of button.sh) for every real run, not the
 * true top-level project directory - two extra path levels deeper than
 * where this project's own sibling projects (like IQABOD) actually
 * live. A cross-project relative path resolved against the raw
 * project_root (as PITFALL 60's own first pass did) works when invoked
 * directly from the top-level project dir (a manual CLI test, exactly
 * how that first fix was verified) but silently resolves to the WRONG,
 * nonexistent location for every REAL run through the actual app -
 * caught by test-harn-same/scenarios/demo_iqabod_chat.sh, a proper
 * level-2 harness scenario, after the CLI-only verification had
 * already reported success. Strips a trailing "/pieces/sessions/<id>"
 * suffix (button.sh's own exact, fixed session-dir shape) to recover
 * the true top-level project root before resolving any CROSS-PROJECT
 * relative reference - in-project paths should keep using project_root
 * directly (unaffected, session-local data legitimately lives under
 * the session dir). out_sz must be at least MAX_PATH. */
static void derive_true_project_root(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", project_root);
    const char *marker = "/pieces/sessions/";
    char *hit = strstr(out, marker);
    if (hit) *hit = '\0';
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

/* Reversible pipe-format escaping for context_log.txt fields: literal
 * backslash -> \\, literal pipe -> \|, literal newline -> \n. Keeps each
 * turn to exactly one line, one field per "|", matching mutaclsym's own
 * doctrine of plain pipe-delimited text over JSON for persisted state. */
static char *pipe_escape(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = malloc(len * 2 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (s[i] == '|') { *p++ = '\\'; *p++ = '|'; }
        else if (s[i] == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else *p++ = s[i];
    }
    *p = '\0';
    return out;
}

static char *pipe_unescape(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            i++;
            if (s[i] == 'n') *p++ = '\n';
            else *p++ = s[i];
        } else *p++ = s[i];
    }
    *p = '\0';
    return out;
}

/* JSON-string escaping, distinct from pipe_escape above - this one is for
 * embedding text inside a JSON string literal in the outgoing request. */
static void json_escaped(FILE *out, const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputs("\\\"", out);
        else if (*p == '\\') fputs("\\\\", out);
        else if (*p == '\n') fputs("\\n", out);
        else if (*p == '\r') fputs("\\r", out);
        else if (*p == '\t') fputs("\\t", out);
        else if ((unsigned char)*p < 32) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
}

/* REAL <cli_io> (2026-07-20 chtpm upgrade, CHAT-INTEGRATION-
 * ARCHITECTURE.txt sec. 1) writes the live typed text to gui_state.txt,
 * keyed by the cli_io's own target_id ("message_input" - chat.chtpm),
 * NOT state.txt's own input_buffer field anymore (buffer_key.c, which
 * used to own that field, is deleted). Same read shape pal-chat-irc's
 * own chat_menu_input.c already proved live. gui_state.txt lives at
 * projects/<project_id>/manager/gui_state.txt per chtpm_parser_pal.c's
 * own resolve_project_gui_state_path() - project_id is "muchi-pal-agent"
 * (project.pdl), hardcoded here matching every other op's own no-
 * shared-headers duplication of this same path shape. */
static void read_gui_state_str(const char *project_root_in, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/muchi-pal-agent/manager/gui_state.txt", project_root_in);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

/* Clears the gui_state.txt key itself (not state.txt's own vestigial
 * input_buffer field) - the cli_io's own displayed value is driven by
 * this file, so this is the real "clear the input box" operation now. */
static void clear_gui_state_str(const char *project_root_in, const char *key) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/muchi-pal-agent/manager/gui_state.txt", project_root_in);
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=\n", key);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=\n", key);
    fclose(f);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
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

static void append_log_turn(const char *log_path, const char *role, const char *kind, const char *tool_name, const char *content) {
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    char *esc_content = pipe_escape(content);
    fprintf(f, "%s|%s|%s|%s\n", role, kind, tool_name ? tool_name : "", esc_content);
    free(esc_content);
    fclose(f);
}

static char *run_tool_capture(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    char *buf = malloc(262144);
    size_t total = 0;
    size_t n;
    while ((n = fread(buf + total, 1, 262144 - total - 1, pipe)) > 0) {
        total += n;
        if (total >= 262143) break;
    }
    buf[total] = '\0';
    pclose(pipe);
    return buf;
}

static const char *OLLAMA_TOOLS_JSON =
    "{\"type\":\"function\",\"function\":{\"name\":\"exec_cmd\",\"description\":\"Run a shell command on the host system\",\"parameters\":{\"type\":\"object\",\"properties\":{\"cmd\":{\"type\":\"string\"}},\"required\":[\"cmd\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"read_file\",\"description\":\"Read the contents of a file\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"write_file\",\"description\":\"Create or overwrite a file with content\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"list_dir\",\"description\":\"List files and subdirectories in a directory\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"search_in_files\",\"description\":\"Search local files recursively for a text query\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"edit_file\",\"description\":\"Search and replace a block of text in a file\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"search\":{\"type\":\"string\"},\"replace\":{\"type\":\"string\"}},\"required\":[\"path\",\"search\",\"replace\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"web_search\",\"description\":\"Search the internet using DuckDuckGo\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"speak\",\"description\":\"Speak the given text using text-to-speech\",\"parameters\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}}}";

static const char *GEMINI_FUNCTION_DECLARATIONS =
    "{\"name\":\"exec_cmd\",\"description\":\"Run a shell command on the host system\",\"parameters\":{\"type\":\"object\",\"properties\":{\"cmd\":{\"type\":\"string\"}},\"required\":[\"cmd\"]}},"
    "{\"name\":\"read_file\",\"description\":\"Read the contents of a file\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
    "{\"name\":\"write_file\",\"description\":\"Create or overwrite a file with content\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}},"
    "{\"name\":\"list_dir\",\"description\":\"List files and subdirectories in a directory\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}}},"
    "{\"name\":\"search_in_files\",\"description\":\"Search local files recursively for a text query\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}},"
    "{\"name\":\"edit_file\",\"description\":\"Search and replace a block of text in a file\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"search\":{\"type\":\"string\"},\"replace\":{\"type\":\"string\"}},\"required\":[\"path\",\"search\",\"replace\"]}},"
    "{\"name\":\"web_search\",\"description\":\"Search the internet using DuckDuckGo\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}},"
    "{\"name\":\"speak\",\"description\":\"Speak the given text using text-to-speech\",\"parameters\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}}";

/* Ollama-native request body: flat "messages" array + native "tools[]" -
 * proven working in groq-ollama. Persona is the "just call the tool"
 * variant (native_tools.txt), since this provider has a real tool schema
 * and doesn't need prompt-engineered JSON. */
static void build_ollama_request(FILE *pf, const char *log_path, const char *model_name) {
    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/native_tools.txt", project_root);
    char *persona = read_full_file(persona_path);

    fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[", model_name);
    if (persona && strlen(persona) > 0) {
        fputs("{\"role\":\"system\",\"content\":\"", pf);
        json_escaped(pf, persona);
        fputs("\"}", pf);
    }
    free(persona);

    FILE *lf = fopen(log_path, "r");
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            line[strcspn(line, "\n")] = '\0';
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            const char *role = line;
            const char *kind = p1 + 1;
            if (strcmp(role, "system") == 0) continue; /* already emitted above */
            if (strcmp(kind, "info") == 0) continue; /* UI-only listing (e.g. /model), never real conversation content */
            char *content = pipe_unescape(p3 + 1);
            fprintf(pf, ",{\"role\":\"%s\",\"content\":\"", role);
            json_escaped(pf, content);
            fputs("\"}", pf);
            free(content);
        }
        fclose(lf);
    }

    fputs("],\"tools\":[", pf);
    fputs(OLLAMA_TOOLS_JSON, pf);
    fputs("]}", pf);
}

/* Gemini request body: systemInstruction + contents[] (role "user"/
 * "model"/"function", parts holding text/functionCall/functionResponse) +
 * tools[{functionDeclarations}]. Same native-tool-calling persona as
 * Ollama - Gemini has a real tool schema too. Ported from gem-dev's
 * proven gemini_payload_builder.c, adapted to read our pipe format
 * (which already carries tool_name on the relevant lines directly, so no
 * "last function name" tracking is needed the way that file needed it). */
static void build_gemini_request(FILE *pf, const char *log_path) {
    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/native_tools.txt", project_root);
    char *persona = read_full_file(persona_path);

    fputs("{", pf);
    if (persona && strlen(persona) > 0) {
        fputs("\"systemInstruction\":{\"parts\":[{\"text\":\"", pf);
        json_escaped(pf, persona);
        fputs("\"}]},", pf);
    }
    free(persona);

    fputs("\"contents\":[", pf);
    int first = 1;
    FILE *lf = fopen(log_path, "r");
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            line[strcspn(line, "\n")] = '\0';
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            const char *role = line;
            const char *kind = p1 + 1;
            const char *tool_name = p2 + 1;

            if (strcmp(role, "system") == 0) continue;
            if (strcmp(kind, "info") == 0) continue; /* UI-only listing (e.g. /model) - never a real functionResponse, see send_message.c's own header comment at that append_log_turn call */
            char *content = pipe_unescape(p3 + 1);

            if (!first) fputs(",", pf);
            first = 0;

            if (strcmp(role, "user") == 0) {
                fputs("{\"role\":\"user\",\"parts\":[{\"text\":\"", pf);
                json_escaped(pf, content);
                fputs("\"}]}", pf);
            } else if (strcmp(role, "assistant") == 0 && strcmp(kind, "tool_call") == 0) {
                fprintf(pf, "{\"role\":\"model\",\"parts\":[{\"functionCall\":{\"name\":\"%s\",\"args\":%s}}]}", tool_name, content);
            } else if (strcmp(role, "assistant") == 0) {
                fputs("{\"role\":\"model\",\"parts\":[{\"text\":\"", pf);
                json_escaped(pf, content);
                fputs("\"}]}", pf);
            } else if (strcmp(role, "tool") == 0) {
                fprintf(pf, "{\"role\":\"function\",\"parts\":[{\"functionResponse\":{\"name\":\"%s\",\"response\":{\"result\":\"", tool_name);
                json_escaped(pf, content);
                fputs("\"}}}]}", pf);
            } else {
                first = 1; /* unrecognized role - didn't actually emit, don't count it for comma placement */
            }
            free(content);
        }
        fclose(lf);
    }
    fputs("]", pf);

    fputs(",\"tools\":[{\"functionDeclarations\":[", pf);
    fputs(GEMINI_FUNCTION_DECLARATIONS, pf);
    fputs("]}]", pf);
    fputs("}", pf);
}

/* GEMMA REQUEST: Messages only, NO tools[] - gemma3:270m does not support tool calling */
static void build_gemma_request(FILE *pf, const char *log_path, const char *model_name) {
    char persona_path[PATH_BUF];
    /* gemma3:270m (gemma-lan) uses simpler persona; other gemma models use standard */
    const char *persona_file = (strcmp(model_name, "gemma3:270m") == 0)
        ? "prompt_keyword_simple.txt"
        : "prompt_keyword.txt";
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/%s", project_root, persona_file);
    char *persona = read_full_file(persona_path);

    fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[", model_name);
    int first = 1;
    if (persona && strlen(persona) > 0) {
        fputs("{\"role\":\"system\",\"content\":\"", pf);
        json_escaped(pf, persona);
        fputs("\"}", pf);
        first = 0;
    }
    free(persona);

    FILE *lf = fopen(log_path, "r");
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            line[strcspn(line, "\n")] = '\0';
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            const char *role = line;
            const char *kind = p1 + 1;
            const char *tool_name = p2 + 1;
            char *content = pipe_unescape(p3 + 1);

            if (strcmp(role, "system") == 0 || strcmp(kind, "info") == 0) {
                free(content);
                continue;
            }

            /* 2&3-jul31-sprint (jul-31 fix): skip stale model-driven
             * assistant|tool_call turns HERE - prompt-build only, the
             * persisted context_log template is never touched. Replaying
             * them to gemma3:270m as literal "assistant: TOOL: X {args}"
             * text taught the model to answer ordinary questions with
             * "TOOL: read file ..." lines (live-caught 2026-07-31:
             * llm_response.json contained exactly that). gemma_strategy
             * + strategy_execute_a handle all tool execution
             * deterministically before this call, and the persona
             * (prompt_keyword.txt) says "you never call tools yourself" -
             * for this 270M model a tool_call in history is pure noise.
             * Skipped BEFORE the comma placement below so no dangling
             * "{}," breaks the JSON. The matching tool|* result turns
             * stay: result-in-context is the whole design. */
            if (strcmp(role, "assistant") == 0 && strcmp(kind, "tool_call") == 0) {
                free(content);
                continue;
            }

            if (!first) fputs(",", pf);
            first = 0;

            if (strcmp(role, "user") == 0) {
                fputs("{\"role\":\"user\",\"content\":\"", pf);
                json_escaped(pf, content);
                fputs("\"}", pf);
            } else if (strcmp(role, "assistant") == 0) {
                fputs("{\"role\":\"assistant\",\"content\":\"", pf);
                json_escaped(pf, content);
                fputs("\"}", pf);
            } else if (strcmp(role, "tool") == 0) {
                fputs("{\"role\":\"user\",\"content\":\"TOOL_RESULT: ", pf);
                json_escaped(pf, content);
                fputs("\"}", pf);
            } else {
                first = 1;
            }
            free(content);
        }
        fclose(lf);
    }
    fputs("]}", pf);
}

/* llama.cpp/completion request body: a manually-built raw Llama3 prompt
 * (text_to_pal_prompt.+x) posted to /completion, matching cpp-llm's
 * proven pattern. No native tool schema on this path - prompt-engineered
 * JSON only, via the prompt_json.txt persona text_to_pal_prompt.+x
 * already selects. */
static void build_llamacpp_request(FILE *pf, const char *log_path) {
    (void)log_path; /* text_to_pal_prompt.+x reads context_log.txt itself */
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/text_to_pal_prompt.+x'", project_root);
    char *raw_prompt = run_tool_capture(cmd);

    fputs("{\"prompt\":\"", pf);
    json_escaped(pf, raw_prompt ? raw_prompt : "");
    fputs("\",\"n_predict\":512,\"stream\":false,\"stop\":[\"<|eot_id|>\",\"<|end_of_text|>\"]}", pf);
    free(raw_prompt);
}

/* iqabod prompt: plain concatenated turn text, most recent turns first,
 * NO role markers ("user:"/"assistant:") and NO persona/instruction
 * preamble - unlike the other three providers. IQABOD's vocabulary is
 * closed-world/exact-match only (see ROADMAP-models.txt §4), so any
 * token that isn't already in the trained curriculum's vocab becomes
 * <UNK> noise rather than being understood; synthetic markers and
 * english instructions would just dilute the budget below with garbage
 * the model can't use. Only "text"-kind log lines are included -
 * tool_call/tool entries are JSON-shaped and equally meaningless to it.
 *
 * Budget is a rough char-based proxy for generation_module.c's
 * SEQ_LEN=128 (prompt tokens + generated tokens share that one cap) -
 * not exact token accounting, just enough headroom left for the model
 * to actually generate something instead of hitting the cap on prompt
 * alone. */
static char *build_iqabod_prompt(const char *log_path) {
    char *raw_turns[MAX_IQABOD_LOG_LINES];
    int n = 0;

    FILE *lf = fopen(log_path, "r");
    if (lf) {
        char line[MAX_LINE];
        while (n < MAX_IQABOD_LOG_LINES && fgets(line, sizeof(line), lf)) {
            line[strcspn(line, "\n")] = '\0';
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            const char *kind = p1 + 1;
            if (strcmp(kind, "text") != 0) continue;
            raw_turns[n++] = pipe_unescape(p3 + 1);
        }
        fclose(lf);
    }

    /* Walk backward from the most recent turn, keeping only what fits
     * the char budget, then re-emit in oldest-first order so the
     * concatenation still reads as a coherent transcript. */
    size_t budget = MAX_IQABOD_PROMPT_CHARS;
    int first_kept = n;
    for (int i = n - 1; i >= 0; i--) {
        size_t tlen = strlen(raw_turns[i]) + 1; /* +1 for the joining space */
        if (tlen > budget) break;
        budget -= tlen;
        first_kept = i;
    }

    size_t out_len = 1;
    for (int i = first_kept; i < n; i++) out_len += strlen(raw_turns[i]) + 1;
    char *out = malloc(out_len);
    out[0] = '\0';
    for (int i = first_kept; i < n; i++) {
        strcat(out, raw_turns[i]);
        if (i < n - 1) strcat(out, " ");
    }
    for (int i = 0; i < n; i++) free(raw_turns[i]);
    return out;
}

static void trim_inplace(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r')) s[--len] = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

/* Case-insensitive whole-word search - "cat" matches "the CAT sat" but
 * not "category", so short/common trigger words don't misfire on
 * unrelated messages. */
static int message_has_word(const char *message, const char *word) {
    size_t wlen = strlen(word);
    if (wlen == 0) return 0;
    size_t mlen = strlen(message);
    char *lower_msg = malloc(mlen + 1);
    char *lower_word = malloc(wlen + 1);
    if (!lower_msg || !lower_word) { free(lower_msg); free(lower_word); return 0; }
    for (size_t i = 0; i < mlen; i++) lower_msg[i] = (char)tolower((unsigned char)message[i]);
    lower_msg[mlen] = '\0';
    for (size_t i = 0; i < wlen; i++) lower_word[i] = (char)tolower((unsigned char)word[i]);
    lower_word[wlen] = '\0';

    int found = 0;
    char *p = lower_msg;
    while ((p = strstr(p, lower_word)) != NULL) {
        int before_ok = (p == lower_msg) || !isalnum((unsigned char)p[-1]);
        int after_ok = !isalnum((unsigned char)p[wlen]);
        if (before_ok && after_ok) { found = 1; break; }
        p++;
    }
    free(lower_msg);
    free(lower_word);
    return found;
}

/* Runs an executable directly - fork/exec with an argv array, NEVER
 * popen/system with an interpolated string. `arg` is untrusted
 * free-typed chat text (the user's own message), and this project
 * already has one preexisting shell-interpolation spot (the /model
 * dispatch below) that this deliberately does NOT copy - a single
 * quote in a chat message must not be able to break out into a shell
 * command. Captures the child's stdout, trims trailing newlines, and
 * treats a nonzero exit or empty output as failure. */
static int run_script_capture(const char *script_path, const char *arg, char *out, size_t out_sz) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        char *argv[] = { (char *)script_path, (char *)arg, NULL };
        execv(script_path, argv);
        _exit(127);
    }
    close(pipefd[1]);
    size_t total = 0;
    ssize_t n;
    while (total + 1 < out_sz && (n = read(pipefd[0], out + total, out_sz - 1 - total)) > 0) {
        total += (size_t)n;
    }
    out[total] = '\0';
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    while (total > 0 && (out[total - 1] == '\n' || out[total - 1] == '\r')) out[--total] = '\0';
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;
    return 0;
}

/* 2&3-jul31-sprint (jul-31 fix): flushes strategy_execute_a.c's stashed
 * tool_result.pending into context_log as a real tool|result turn, AFTER
 * the user's message that triggered it (strategy_execute_a runs before
 * this op, so appending directly there placed the result above the
 * user's message - see the demo_list_dir_tool harness + 2&3-jul31-sprint.md).
 * Pending file layout (written by strategy_execute_a.c): line 1 = tool
 * name, remainder = raw result (pipe-escaped here by append_log_turn).
 * Returns 1 if a pending result was flushed this turn. */
static int flush_pending_tool_result(const char *state_path, const char *log_path) {
    char pending_path[PATH_BUF];
    snprintf(pending_path, sizeof(pending_path), "%s/pieces/world_01/session_01/chat/tool_result.pending", project_root);
    FILE *pf = fopen(pending_path, "r");
    if (!pf) return 0;

    char *buf = malloc(MAX_BUFFER + 1);
    if (!buf) { fclose(pf); return 0; }
    size_t n = fread(buf, 1, MAX_BUFFER, pf);
    buf[n] = '\0';
    fclose(pf);

    char *first_nl = strchr(buf, '\n');
    if (!first_nl) { free(buf); return 0; }
    *first_nl = '\0';
    append_log_turn(log_path, "tool", "result", buf, first_nl + 1);
    free(buf);

    remove(pending_path);
    write_state_field(state_path, "tool_result_pending", "0");
    return 1;
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], log_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);

    char buffer[MAX_BUFFER];
    read_gui_state_str(project_root, "message_input", buffer, sizeof(buffer));
    if (strlen(buffer) == 0) return 0;

    /* This is the single Enter-handler op main_loop.pal calls
     * unconditionally on every Enter keypress (mirrors mutaclsym's own
     * choice.c self-filtering on every tick rather than the .pal script
     * branching on state - prisc+x's beq is exact-int-equality only, it
     * can't compare strings or piece state, so any "what does Enter mean
     * right now" decision has to live in C, not the script). If a tool
     * permission is pending, Enter means y/n, not a new message. */
    char ai_state[MAX_FIELD];
    read_state_field(state_path, "ai_state", ai_state, sizeof(ai_state));
    if (strcmp(ai_state, "PENDING_PERM") == 0) {
        char trimmed[MAX_BUFFER];
        snprintf(trimmed, sizeof(trimmed), "%s", buffer);
        size_t tlen = strlen(trimmed);
        while (tlen > 0 && (trimmed[tlen - 1] == ' ' || trimmed[tlen - 1] == '\n')) trimmed[--tlen] = '\0';
        for (size_t i = 0; trimmed[i]; i++) trimmed[i] = (char)tolower((unsigned char)trimmed[i]);

        clear_gui_state_str(project_root, "message_input");
        if (strcmp(trimmed, "y") == 0 || strcmp(trimmed, "yes") == 0) {
            char cmd[PATH_BUF];
            snprintf(cmd, sizeof(cmd), "'%s/ops/+x/execute_tool.+x'", project_root);
            int rc = system(cmd);
            (void)rc;
        } else if (strcmp(trimmed, "n") == 0 || strcmp(trimmed, "no") == 0) {
            char cmd[PATH_BUF];
            snprintf(cmd, sizeof(cmd), "'%s/ops/+x/deny_tool.+x'", project_root);
            int rc = system(cmd);
            (void)rc;
        } else {
            write_state_field(state_path, "sys_msg", "Invalid response. Type 'y' or 'n'.");
        }
        return 0;
    }

    /* "/model <id>" switches model, "/model" alone lists available models */
    if (strcmp(buffer, "/model") == 0) {
        /* List available models from model_list.txt */
        char model_list_path[PATH_BUF];
        snprintf(model_list_path, sizeof(model_list_path), "%s/pieces/registry/models/model_list.txt", project_root);
        FILE *mf = fopen(model_list_path, "r");
        if (mf) {
            char line[MAX_LINE];
            char models_msg[MAX_LINE * 10] = "Available models:\n";
            while (fgets(line, sizeof(line), mf)) {
                line[strcspn(line, "\n")] = '\0';
                if (line[0] && line[0] != '#') {
                    char *pipe = strchr(line, '|');
                    if (pipe) {
                        *pipe = '\0';
                        strncat(models_msg, "  - ", sizeof(models_msg) - strlen(models_msg) - 1);
                        strncat(models_msg, line, sizeof(models_msg) - strlen(models_msg) - 1);
                        strncat(models_msg, "\n", sizeof(models_msg) - strlen(models_msg) - 1);
                    }
                }
            }
            fclose(mf);
            /* kind="info", NOT "text" - this is a UI listing, never a real
             * tool_name/functionResponse pair. Every request builder below
             * must skip role="tool" kind="info" turns, same as it already
             * skips role="system" - a REAL BUG, LIVE-CAUGHT: build_gemini_
             * request() used to treat ANY role="tool" line as a genuine
             * functionResponse and emitted {"name":"", ...} for this one
             * (tool_name is NULL here, there's no actual tool), which
             * Gemini rejects with a permanent 400 ("functionResponse.name:
             * Name cannot be empty") - and because context is cumulative,
             * one /model command poisoned every later Gemini call for the
             * rest of the session, not just the one that triggered it. */
            append_log_turn(log_path, "tool", "info", NULL, models_msg);
            write_state_field(state_path, "sys_msg", "Models listed above.");
            clear_gui_state_str(project_root, "message_input");
            return 0;
        }
        return 0;
    }

    /* "/model <id>" switches model */
    if (strncmp(buffer, "/model ", 7) == 0) {
        char cmd[PATH_BUF + MAX_BUFFER];
        snprintf(cmd, sizeof(cmd), "'%s/ops/+x/switch_model.+x' '%s'", project_root, buffer + 7);
        int rc = system(cmd);
        (void)rc;
        clear_gui_state_str(project_root, "message_input");
        return 0;
    }

    /* "/irc-agent <room>|off" - direct user ask, 2026-07-20: turns on
     * irc_agent_poll.c's own tick-driven bridge into pal-chat-irc (see
     * that op's own header comment for the full design). Dispatched
     * the same way /model is - a command, not a chat message. */
    if (strncmp(buffer, "/irc-agent ", 11) == 0) {
        char cmd[PATH_BUF + MAX_BUFFER];
        snprintf(cmd, sizeof(cmd), "'%s/ops/+x/set_irc_agent.+x' '%s'", project_root, buffer + 11);
        int rc = system(cmd);
        (void)rc;
        clear_gui_state_str(project_root, "message_input");
        return 0;
    }

    /* Ordinary chat message. */
    append_log_turn(log_path, "user", "text", NULL, buffer);
    clear_gui_state_str(project_root, "message_input");
    write_state_field(state_path, "ai_state", "THINKING");
    write_state_field(state_path, "sys_msg", "Querying AI...");

    /* thinking_start - gem-dev_manager.c's own g_thinking_start/
     * g_thinking_secs pattern (direct user request: "per second counter
     * so user will know we are waiting, not frozen" - the existing
     * [poll #N, frame #M] display doesn't actually tell a user how much
     * real time has passed). compose_frame.c computes elapsed seconds
     * from this on every render while ai_state=THINKING. */
    char thinking_start_str[32];
    snprintf(thinking_start_str, sizeof(thinking_start_str), "%ld", (long)time(NULL));
    write_state_field(state_path, "thinking_start", thinking_start_str);

    /* 2&3-jul31-sprint (jul-31 fix): if strategy_execute_a stashed a
     * tool result this same turn, flush it into context_log right after
     * the user's message (so the listing renders BELOW "You: ...", not
     * above it), and - unless model_after_tool=yes - skip the LLM call
     * entirely: the tool already answered deterministically, no 270M
     * model needed to (mis)echo "TOOL: ..." back at us. Toggle lives in
     * state.txt; default =no when absent. */
    if (flush_pending_tool_result(state_path, log_path)) {
        char mat[16] = "no";
        read_state_field(state_path, "model_after_tool", mat, sizeof(mat));
        if (strcmp(mat, "yes") != 0) {
            write_state_field(state_path, "ai_state", "IDLE");
            write_state_field(state_path, "sys_msg", "Tool result shown.");
            return 0;
        }
    }

    char provider_kind[MAX_FIELD], api_url[MAX_FIELD], model_name[MAX_FIELD];
    read_state_field(state_path, "provider_kind", provider_kind, sizeof(provider_kind));
    read_state_field(state_path, "current_api_url", api_url, sizeof(api_url));
    read_state_field(state_path, "current_model_name", model_name, sizeof(model_name));

    /* provider_kind=script - direct user request, 2026-07-20: "very
     * basic responses to chat, without the llm apis, just using
     * script/fsm w/e to giving random words from curriculum bank."
     * The simplest real instance of CHAT-INTEGRATION-ARCHITECTURE.txt
     * sec. 5's own planned /model script responder kind - no LLM API,
     * no network, no forked child, no THINKING wait at all (unlike
     * every other provider_kind, which forks a background call and
     * relies on check_response.c to notice it finished): this one
     * finishes synchronously, in this same process, since picking
     * random words from a plain text file is effectively instant.
     * model_name is repurposed as a wordbank file path relative to
     * project_root (same "reuse the two existing fields for a
     * different meaning" precedent iqabod's own api_url/model_name
     * repurposing already established - see model_list.txt's own
     * header comment) - a plain newline-delimited word list
     * (pieces/registry/wordbanks/<name>.txt), NOT tied to any real
     * IQABOD curriculum's own row format, so this works with zero
     * external dependencies at all, unlike the real provider_kind=
     * iqabod path. api_url is unused for this provider_kind (kept "-"
     * in model_list.txt, matching the family's own "-" placeholder
     * convention for an unused positional field). */
    if (strcmp(provider_kind, "script") == 0) {
        /* Trigger-word routing - direct user request, 2026-07-20: "if
         * certain words are entered it will pick from a different
         * corpus, and if certain words are entered it will run a
         * particular script." Scans pieces/registry/triggers/
         * trigger_list.txt (same pipe-delimited convention as
         * model_list.txt) for a whole-word, case-insensitive match
         * against the user's own message. First matching row wins,
         * checked top to bottom - no match at all falls through to the
         * default wordbank (model_name), unchanged from before this
         * feature existed.
         *   trigger|corpus|<wordbank path>  - same random 5-10-word
         *     pick below, just reads a different file.
         *   trigger|script|<executable path> - runs that program
         *     directly (argv, not shell - see run_script_capture's own
         *     comment for why) with the user's message as argv[1], and
         *     uses its stdout verbatim as the response. Word-picking is
         *     skipped entirely for a script match. */
        char active_wordbank_rel[PATH_BUF];
        snprintf(active_wordbank_rel, sizeof(active_wordbank_rel), "%s", model_name);
        char script_target_rel[PATH_BUF] = "";

        char triggers_path[PATH_BUF];
        snprintf(triggers_path, sizeof(triggers_path), "%s/pieces/registry/triggers/trigger_list.txt", project_root);
        char *tb = read_full_file(triggers_path);
        if (tb) {
            char *saveptr = NULL;
            char *line = strtok_r(tb, "\n", &saveptr);
            while (line) {
                while (*line == ' ' || *line == '\t') line++;
                if (line[0] && line[0] != '#') {
                    char trig[128] = "", kind[32] = "", target[512] = "";
                    if (sscanf(line, "%127[^|]|%31[^|]|%511[^\r\n]", trig, kind, target) == 3) {
                        trim_inplace(trig);
                        trim_inplace(kind);
                        trim_inplace(target);
                        if (trig[0] && message_has_word(buffer, trig)) {
                            if (strcmp(kind, "corpus") == 0) {
                                snprintf(active_wordbank_rel, sizeof(active_wordbank_rel), "%s", target);
                            } else if (strcmp(kind, "script") == 0) {
                                snprintf(script_target_rel, sizeof(script_target_rel), "%s", target);
                            }
                            break;
                        }
                    }
                }
                line = strtok_r(NULL, "\n", &saveptr);
            }
            free(tb);
        }

        char response[MAX_BUFFER] = "";

        if (script_target_rel[0]) {
            char script_full_path[PATH_BUF];
            snprintf(script_full_path, sizeof(script_full_path), "%s/%s", project_root, script_target_rel);
            if (run_script_capture(script_full_path, buffer, response, sizeof(response)) != 0 || response[0] == '\0') {
                snprintf(response, sizeof(response), "[script: '%s' failed or produced no output]", script_target_rel);
            }
        } else {
            char wordbank_path[PATH_BUF];
            snprintf(wordbank_path, sizeof(wordbank_path), "%s/%s", project_root, active_wordbank_rel);
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
                snprintf(response, sizeof(response), "[script: wordbank '%s' is empty or missing]", active_wordbank_rel);
            } else {
                /* /dev/urandom seed, not time() - several rapid messages in
                 * the same second would otherwise draw the identical
                 * "random" picks, confirmed a real risk given how fast this
                 * synchronous path runs (unlike the network providers,
                 * where a second's worth of latency makes a time()-based
                 * seed collision practically impossible anyway). */
                unsigned int seed = (unsigned int)getpid() ^ (unsigned int)time(NULL);
                FILE *rf = fopen("/dev/urandom", "rb");
                if (rf) { unsigned int r; if (fread(&r, sizeof(r), 1, rf) == 1) seed ^= r; fclose(rf); }
                srand(seed);
                int pick_count = 5 + (rand() % 6); /* 5-10 words */
                for (int i = 0; i < pick_count; i++) {
                    if (i > 0) strcat(response, " ");
                    strcat(response, words[rand() % nwords]);
                }
            }
            free(wb);
        }

        append_log_turn(log_path, "assistant", "text", NULL, response);
        write_state_field(state_path, "ai_state", "IDLE");
        write_state_field(state_path, "sys_msg", "Response received.");
        return 0;
    }

    char prompt_path[PATH_BUF];
    snprintf(prompt_path, sizeof(prompt_path), "%s/pieces/world_01/session_01/chat/prompt.json", project_root);
    FILE *pf = fopen(prompt_path, "w");
    if (!pf) return 1;

    char full_url[MAX_FIELD * 2 + 128];

    if (strcmp(provider_kind, "iqabod") == 0) {
        /* Wholly different shape from the other three: no JSON request
         * file (prompt.json stays empty), no connect_op/curl, no HTTP.
         * api_url/model_name are repurposed per model_list.txt's header
         * comment - api_url is IQABOD's own project root (this process
         * must chdir() there since main_orchestrator.c resolves
         * config.txt/curriculum paths/./+x/generation_module.+x all
         * relative to CWD, with no root-resolution of its own), and
         * model_name is the curriculum file path relative to that root.
         *
         * api_url is resolved relative to THIS project's own
         * project_root when it doesn't start with '/' - real fix for a
         * real, confirmed-dead bug (2026-07-30): model_list.txt used to
         * hardcode api_url as an ABSOLUTE path from a prior top-level
         * house reorg (.../ZEST-10.00/.../^.IQABOD-llm-06.00) that no
         * longer exists at all - every iqabod call failed at this
         * chdir() (silently, straight to _exit(127), no error surfaced
         * anywhere a human would see it) until this was caught and
         * fixed. IQABOD lives as a SIBLING of this project under the
         * same house root (both directly under 44.xyz.../), so a path
         * relative to project_root (e.g. "../#.z.mirror_llm]z5]
         * IQABOD🪞️+4") survives the NEXT house-wide rename too, since
         * sibling projects move together as a unit - an absolute path
         * does not survive that, confirmed by this exact incident.
         * Absolute paths (starting with '/') are still honored as-is
         * for backward compatibility / manual overrides. See
         * !.xyzos-standards+1.txt's own "relative paths house-wide"
         * section and !.xyzos-pitfalls+1.txt for the full incident.
         *
         * Resolved against derive_true_project_root(), NOT raw
         * project_root directly - see that function's own header
         * comment for the second, real bug a level-2 harness caught
         * the SAME day as the first fix: raw project_root is the
         * SESSION dir for every actual run, two levels deeper than
         * where sibling projects live, so resolving against it
         * directly silently landed on a nonexistent path again despite
         * looking correct in a manual CLI-only test. */
        char true_root[MAX_PATH];
        derive_true_project_root(true_root, sizeof(true_root));
        char resolved_root[PATH_BUF];
        if (api_url[0] == '/') {
            snprintf(resolved_root, sizeof(resolved_root), "%s", api_url);
        } else {
            snprintf(resolved_root, sizeof(resolved_root), "%s/%s", true_root, api_url);
        }
        fclose(pf);
        char *prompt_text = build_iqabod_prompt(log_path);

        char iqabod_response_path[PATH_BUF], iqabod_prompt_path[PATH_BUF];
        snprintf(iqabod_response_path, sizeof(iqabod_response_path), "%s/pieces/world_01/session_01/chat/iqabod_response.txt", project_root);
        snprintf(iqabod_prompt_path, sizeof(iqabod_prompt_path), "%s/pieces/world_01/session_01/chat/iqabod_prompt.tmp", project_root);

        /* check_response.c needs the exact prompt text back to strip it
         * off the front of "Final generated text: <prompt> <tokens...>"
         * (generation_module.c builds output as strcpy(output, prompt)
         * before appending generated tokens) - persisted here since
         * ops have no memory between invocations. */
        FILE *ppf = fopen(iqabod_prompt_path, "w");
        if (ppf) { fputs(prompt_text ? prompt_text : "", ppf); fclose(ppf); }

        pid_t iqabod_pid = fork();
        if (iqabod_pid == 0) {
            setsid();
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) { dup2(devnull, STDIN_FILENO); dup2(devnull, STDERR_FILENO); }
            int outfd = open(iqabod_response_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (outfd >= 0) dup2(outfd, STDOUT_FILENO);
            if (devnull >= 0 && devnull > STDERR_FILENO) close(devnull);
            if (outfd >= 0 && outfd > STDERR_FILENO) close(outfd);
            if (chdir(resolved_root) != 0) _exit(127);
            char *orch_path = NULL;
            if (asprintf(&orch_path, "%s/+x/main_orchestrator.+x", resolved_root) == -1) _exit(127);
            /* Passed as a single raw execl() argv element, NOT through a
             * shell - unlike main_orchestrator.c's own internal re-exec
             * of generation_module.+x (via system(), which embeds this
             * same prompt unescaped inside a quoted string), so quotes/
             * backticks/etc. in the prompt can't break parsing here. */
            execl(orch_path, orch_path, "generate", model_name, "0.8", "60", prompt_text ? prompt_text : "", (char *)NULL);
            _exit(127);
        }
        free(prompt_text);

        char iqabod_pid_str[32];
        snprintf(iqabod_pid_str, sizeof(iqabod_pid_str), "%d", iqabod_pid);
        write_state_field(state_path, "curl_pid", iqabod_pid_str);
        return 0;
    } else if (strcmp(provider_kind, "gemini") == 0) {
        build_gemini_request(pf, log_path);
        char *gemini_key = getenv("GEMINI_API_KEY");
        /* getenv()'s result is unbounded from gcc's static view, so
         * -Wformat-truncation can't be fully satisfied by sizing alone -
         * full_url is already sized with headroom above any real API key
         * length, snprintf truncates safely in the extreme case regardless. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(full_url, sizeof(full_url), "%s/v1beta/models/%s:generateContent?key=%s",
                 api_url, model_name, gemini_key ? gemini_key : "");
#pragma GCC diagnostic pop
    } else if (strcmp(provider_kind, "gemma") == 0) {
        build_gemma_request(pf, log_path, model_name);
        snprintf(full_url, sizeof(full_url), "%s/api/chat", api_url);
    } else if (strcmp(provider_kind, "llamacpp") == 0) {
        build_llamacpp_request(pf, log_path);
        snprintf(full_url, sizeof(full_url), "%s/completion", api_url);
    } else {
        build_ollama_request(pf, log_path, model_name);
        snprintf(full_url, sizeof(full_url), "%s/api/chat", api_url);
    }
    fclose(pf);

    char response_path[PATH_BUF], status_path[PATH_BUF];
    snprintf(response_path, sizeof(response_path), "%s/pieces/world_01/session_01/chat/llm_response.json", project_root);
    snprintf(status_path, sizeof(status_path), "%s.status", response_path);
    /* REAL BUG, LIVE-CAUGHT (2026-07-21): a failed/refused connection
     * leaves llm_response.json untouched (curl never gets far enough to
     * open its -o destination at all), and nothing here ever checked
     * connect_op's own exit status - so check_response.c would silently
     * re-parse WHATEVER the previous, possibly totally unrelated call
     * left behind (a stale error from a different provider, in the case
     * that exposed this) and misreport it as THIS turn's answer. Clear
     * both files before starting a fresh attempt so an unwritten
     * response_path after connect_op exits unambiguously means "this
     * attempt produced nothing," not "read whatever's still there." */
    remove(response_path);
    remove(status_path);

    pid_t pid = fork();
    if (pid == 0) {
        /* send_message.+x is itself invoked via prisc+x's popen(), so
         * stdout here is the write end of that pipe. Without detaching it,
         * this grandchild (and curl under it) would keep that pipe open
         * long after send_message.+x itself exits, and prisc+x's
         * popen()/fgets() loop would block on it for the whole network
         * call - exactly the blocking behavior this design exists to
         * avoid. Redirect to /dev/null and start a new session so this
         * subprocess is fully independent of the short-lived parent. */
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        char *connect_op_path = NULL;
        if (asprintf(&connect_op_path, "%s/ops/+x/connect_op.+x", project_root) == -1) _exit(127);
        execl(connect_op_path, connect_op_path, full_url, prompt_path, response_path, (char *)NULL);
        _exit(127);
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    write_state_field(state_path, "curl_pid", pid_str);

    return 0;
}
