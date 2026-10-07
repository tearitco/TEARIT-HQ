/* llm_brain — LLM Brain for XOD (C op, house pattern).
 *
 * Observes WSR state, builds Ollama payload, calls curl via run_tool pattern,
 * reads response via json_parser, emits LLM_DECISION to event bus.
 *
 * House pattern: payload → tmp file → curl → response tmp → json_parser → decision.
 *
 * Usage: llm_brain.+x <session_dir> <goal> [model] [tom_file]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define access _access
#define F_OK 0
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <unistd.h>
#define POPEN popen
#define PCLOSE pclose
#endif

#define MAX_LINE 8192
#define PATH_BUF (4096 + 512)

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_line(const char *path, const char *line) {
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}

static int read_file(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    size_t n = fread(buf, 1, sz - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n > 0;
}

static int read_first_line(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    if (!fgets(buf, (int)sz, f)) buf[0] = '\0';
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    return buf[0] != '\0';
}

/* run_tool pattern: fork + exec, capture stdout. */
static char *run_tool(const char *tool, char *const args[]) {
    FILE *p = POPEN(args[0], "r");
    if (!p) return NULL;
    char *out = malloc(16384);
    if (!out) { PCLOSE(p); return NULL; }
    size_t total = 0;
    char buf[1024];
    while (1) {
        size_t n = fread(buf, 1, sizeof(buf), p);
        if (n == 0) break;
        if (total + n < 16383) {
            memcpy(out + total, buf, n);
            total += n;
        }
    }
    out[total] = '\0';
    PCLOSE(p);
    char *nl = strchr(out, '\n');
    if (nl) *nl = '\0';
    return out;
}

/* Build payload JSON to a temp file. */
static void build_payload(const char *session_dir, const char *goal,
                          const char *model, const char *tom_file,
                          char *payload_file, size_t pf_sz,
                          char *response_file, size_t rf_sz) {
    snprintf(payload_file, pf_sz, "%s/state/llm_payload.json", session_dir);
    snprintf(response_file, rf_sz, "%s/state/llm_response.json", session_dir);

    /* Read state files */
    char frame[4096] = "", layout[256] = "";
    char corp_state[2048] = "", fitness[1024] = "", events[2048] = "";
    char tom_context[2048] = "{}";

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_frame.txt", session_dir);
    read_file(path, frame, sizeof(frame));
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", session_dir);
    read_first_line(path, layout, sizeof(layout));
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/pieces/corp_ORB/state.txt", session_dir);
    read_file(path, corp_state, sizeof(corp_state));
    snprintf(path, sizeof(path), "%s/pieces/display/fitness.txt", session_dir);
    read_file(path, fitness, sizeof(fitness));
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/interact_relay.txt", session_dir);
    read_file(path, events, sizeof(events));
    if (strlen(events) > 500) {
        memmove(events, events + strlen(events) - 500, 500);
        events[500] = '\0';
    }

    if (access("/dev/stdin", F_OK) == 0) {
        char tom_path[PATH_BUF];
        snprintf(tom_path, sizeof(tom_path), "%s/pieces/apps/player_app/tom_context.json", session_dir);
        if (access(tom_path, F_OK) == 0)
            read_file(tom_path, tom_context, sizeof(tom_context));
    }

    /* Escape JSON strings - minimal escaper */
    auto void json_escape(char *out, size_t out_sz, const char *in) {
        char *p = out;
        const char *end = out + out_sz - 1;
        for (; *in && p < end; in++) {
            switch (*in) {
                case '"': *p++ = '\\'; *p++ = '"'; break;
                case '\\': *p++ = '\\'; *p++ = '\\'; break;
                case '\n': *p++ = '\\'; *p++ = 'n'; break;
                case '\r': *p++ = '\\'; *p++ = 'r'; break;
                case '\t': *p++ = '\\'; *p++ = 't'; break;
                default: *p++ = *in; break;
            }
        }
        *p = '\0';
    }

    char esc_frame[8192], esc_corp[4096], esc_fit[2048], esc_evt[4096], esc_tom[4096];
    json_escape(esc_frame, sizeof(esc_frame), frame);
    json_escape(esc_corp, sizeof(esc_corp), corp_state);
    json_escape(esc_fit, sizeof(esc_fit), fitness);
    json_escape(esc_evt, sizeof(esc_evt), events);
    json_escape(esc_tom, sizeof(esc_tom), tom_context);

    /* Build payload JSON */
    FILE *f = fopen(payload_file, "w");
    if (!f) return;
    fprintf(f, "{"
        "\"model\":\"gemma3:1b\","
        "\"stream\":false,"
        "\"options\":{\"temperature\":0.3,\"num_ctx\":2048},"
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\"You are the decision-making brain of an autonomous agent driving a stock-market simulation (WSR).\\nYou observe the current screen frame, the active layout, the corporation state, and recent events.\\nYou must decide exactly ONE action to take right now.\\n\\nVALID ACTIONS (choose one):\\n- end_turn\\n- buy_stock\\n- sell_stock\\n- buy_sell\\n- new_game\\n- cycle_corp\\n- list_portfolio\\n- check_market\\n- back_to_main\\n- wait\\n\\nOUTPUT FORMAT (strict JSON, no extra text):\\n{\\\"action\\\":\\\"<action_name>\\\",\\\"reason\\\":\\\"<one-line reason>\\\",\\\"confidence\\\":0.0-1.0}\\n\\nDO NOT output anything except valid JSON.\"},"
        "{\"role\":\"user\",\"content\":\"GOAL: %s\\n\\nCURRENT LAYOUT: %s\\n\\nCURRENT FRAME:\\n%s\\n\\nCORP STATE:\\n%s\\n\\nFITNESS:\\n%s\\n\\nRECENT EVENTS:\\n%s\\n\\nTOM CONTEXT:\\n%s\\n\\nDecide the single best action now. Output only valid JSON.\"}"
        "]}",
        goal, layout, esc_frame, esc_corp, esc_fit, esc_evt, esc_tom);
    fclose(f);
}

/* Run curl via system (fire-and-forget, writes response to file). */
static int run_ollama(const char *payload_file, const char *response_file) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "curl -sS --max-time 30 -H 'Content-Type: application/json' "
        "-d @%s http://10.0.0.144:11434/api/chat -o %s 2>/dev/null",
        payload_file, response_file);
    return system(cmd);
}

/* Extract action, reason, confidence from Ollama response JSON.
 * Two-step: extract message.content, strip markdown, parse inner JSON.
 * Uses simple string manipulation (avoids json_parser issues). */
static void parse_response(const char *response_file,
                           char *action, size_t action_sz,
                           char *reason, size_t reason_sz,
                           double *conf,
                           const char *session_dir,
                           const char *project_root) {
    *conf = 0.5;
    action[0] = '\0';
    reason[0] = '\0';

    char response[16384];
    if (!read_file(response_file, response, sizeof(response))) return;

    /* Find message.content field in the response */
    char *content_start = strstr(response, "\"message\"");
    if (!content_start) return;
    content_start = strstr(content_start, "\"content\"");
    if (!content_start) return;
    content_start = strchr(content_start, ':');
    if (!content_start) return;
    content_start++;
    while (*content_start && (*content_start == ' ' || *content_start == '"')) content_start++;

    /* Extract the content string (handle escaped quotes) */
    char content[8192];
    char *c = content;
    char *s = content_start;
    while (*s && *s != '"') {
        if (*s == '\\' && *(s+1) == '"') {
            *c++ = '"';
            s += 2;
        } else if (*s == '\\' && *(s+1) == '\\') {
            *c++ = '\\';
            s += 2;
        } else if (*s == '\\' && *(s+1) == 'n') {
            *c++ = '\n';
            s += 2;
        } else {
            *c++ = *s++;
        }
    }
    *c = '\0';

    /* Strip markdown code fences if present */
    char *json_start = content;
    if (strncmp(content, "```json", 7) == 0) {
        json_start = content + 7;
    } else if (strncmp(content, "```", 3) == 0) {
        json_start = content + 3;
    }
    char *json_end = strstr(json_start, "```");
    if (json_end) *json_end = '\0';

    /* Parse action, reason, confidence from inner JSON */
    char *p = json_start;
    char *action_ptr = strstr(p, "\"action\"");
    if (action_ptr) {
        char *colon = strchr(action_ptr, ':');
        if (colon) {
            char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            char *end = q;
            while (*end && *end != '"' && *end != ',' && *end != '}') end++;
            if (end > q) {
                size_t len = end - q;
                if (len < action_sz) memcpy(action, q, len), action[len] = '\0';
            }
        }
    }
    char *reason_ptr = strstr(p, "\"reason\"");
    if (reason_ptr) {
        char *colon = strchr(reason_ptr, ':');
        if (colon) {
            char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            char *end = q;
            while (*end && *end != '"' && *end != ',' && *end != '}') end++;
            if (end > q) {
                size_t len = end - q;
                if (len < reason_sz) memcpy(reason, q, len), reason[len] = '\0';
            }
        }
    }
    char *conf_ptr = strstr(p, "\"confidence\"");
    if (conf_ptr) {
        char *colon = strchr(conf_ptr, ':');
        if (colon) *conf = atof(colon + 1);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <session_dir> <goal> [model] [tom_file]\n", argv[0]);
        return 1;
    }
    resolve_root();

    const char *session_dir = argv[1];
    const char *goal = argv[2];
    const char *model = argc >= 4 ? argv[3] : "gemma3:1b";
    const char *tom_file = argc >= 5 ? argv[4] : NULL;

    char payload_file[PATH_BUF], response_file[PATH_BUF];
    build_payload(session_dir, goal, model, tom_file, payload_file, sizeof(payload_file),
                  response_file, sizeof(response_file));

    int rc = run_ollama(payload_file, response_file);
    if (rc != 0) {
        fprintf(stderr, "Ollama call failed\n");
        return 1;
    }

    char action[64] = "wait";
    char reason[256] = "";
    double conf = 0.5;
    parse_response(response_file, action, sizeof(action), reason, sizeof(reason), &conf, session_dir, project_root);

    /* Emit LLM_DECISION to event bus */
    char event_path[PATH_BUF];
    snprintf(event_path, sizeof(event_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    char event[MAX_LINE];
    snprintf(event, sizeof(event), "LLM_DECISION|%s|%.2f|%s", action, conf, reason);
    append_line(event_path, event);

    printf("{\"action\":\"%s\",\"reason\":\"%s\",\"confidence\":%.2f}\n", action, reason, conf);
    return 0;
}