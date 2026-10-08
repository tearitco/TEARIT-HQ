/**
 * model_api.c — Unified LLM interface implementation.
 * Supports: Ollama, OpenAI, Anthropic, local (llama.cpp), mock.
 * House pattern: curl via system(), JSON parsing via json_parser.+x.
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

#include "model_api.h"

#define MAX_LINE 8192
#define PATH_BUF (4096 + 512)

struct model_api {
    model_api_type_t type;
    char *base_url;
    char *model_name;
    char *api_key;
};

model_api_t *model_api_init(model_api_type_t type, const char *base_url, const char *model_name, const char *api_key) {
    model_api_t *api = calloc(1, sizeof(model_api_t));
    if (!api) return NULL;
    api->type = type;
    api->base_url = strdup(base_url ? base_url : "");
    api->model_name = strdup(model_name ? model_name : "");
    api->api_key = strdup(api_key ? api_key : "");
    return api;
}

void model_api_free(model_api_t *api) {
    if (!api) return;
    free(api->base_url);
    free(api->model_name);
    free(api->api_key);
    free(api);
}

/* Build curl command for given provider. */
static char *build_curl_cmd(model_api_t *api, const char *payload_file, const char *response_file) {
    char *cmd = malloc(4096);
    if (!cmd) return NULL;

    switch (api->type) {
        case MODEL_API_OLLAMA:
            snprintf(cmd, 4096,
                "curl -sS --max-time 60 -H 'Content-Type: application/json' "
                "-d @%s %s/api/chat -o %s 2>/dev/null",
                payload_file, api->base_url, response_file);
            break;
        case MODEL_API_OPENAI: {
            char *auth = api->api_key ? api->api_key : getenv("OPENAI_API_KEY");
            if (!auth) return NULL;
            snprintf(cmd, 4096,
                "curl -sS --max-time 60 -H 'Content-Type: application/json' -H 'Authorization: Bearer %s' "
                "-d @%s %s/v1/chat/completions -o %s 2>/dev/null",
                auth, payload_file, api->base_url, response_file);
            break;
        }
        case MODEL_API_ANTHROPIC: {
            char *auth = api->api_key ? api->api_key : getenv("ANTHROPIC_API_KEY");
            if (!auth) return NULL;
            snprintf(cmd, 4096,
                "curl -sS --max-time 60 -H 'Content-Type: application/json' -H 'x-api-key: %s' -H 'anthropic-version: 2023-06-01' "
                "-d @%s %s/v1/messages -o %s 2>/dev/null",
                auth, payload_file, api->base_url, response_file);
            break;
        }
        case MODEL_API_LOCAL:
            snprintf(cmd, 4096,
                "curl -sS --max-time 60 -H 'Content-Type: application/json' "
                "-d @%s %s/v1/chat/completions -o %s 2>/dev/null",
                payload_file, api->base_url, response_file);
            break;
        case MODEL_API_MOCK:
            /* Mock: return a deterministic response */
            return NULL;
    }
    return cmd;
}

/* Build payload JSON for given provider. */
static void build_payload(model_api_t *api, const char *system_prompt, const char *user_prompt,
                          double temperature, int max_tokens, const char *payload_file) {
    FILE *f = fopen(payload_file, "w");
    if (!f) return;

    char *esc_sys = malloc(strlen(system_prompt) * 2 + 1);
    char *esc_usr = malloc(strlen(user_prompt) * 2 + 1);
    char *p = esc_sys; const char *s = system_prompt;
    while (*s) { if (*s == '"' || *s == '\\') *p++ = '\\'; *p++ = *s++; } *p = '\0';
    p = esc_usr; s = (const char *)user_prompt;
    while (*s) { if (*s == '"' || *s == '\\') *p++ = '\\'; *p++ = *s++; } *p = '\0';

    switch (api->type) {
        case MODEL_API_OLLAMA:
        case MODEL_API_LOCAL:
            fprintf(f, "{"
                "\"model\":\"%s\","
                "\"stream\":false,"
                "\"options\":{\"temperature\":%.2f,\"num_ctx\":%d},"
                "\"messages\":["
                "{\"role\":\"system\",\"content\":\"%s\"},"
                "{\"role\":\"user\",\"content\":\"%s\"}"
                "]}",
                api->model_name, temperature, max_tokens, esc_sys, esc_usr);
            break;
        case MODEL_API_OPENAI:
            fprintf(f, "{"
                "\"model\":\"%s\","
                "\"temperature\":%.2f,"
                "\"max_tokens\":%d,"
                "\"messages\":["
                "{\"role\":\"system\",\"content\":\"%s\"},"
                "{\"role\":\"user\",\"content\":\"%s\"}"
                "]}",
                api->model_name, temperature, max_tokens, esc_sys, esc_usr);
            break;
        case MODEL_API_ANTHROPIC:
            fprintf(f, "{"
                "\"model\":\"%s\","
                "\"max_tokens\":%d,"
                "\"temperature\":%.2f,"
                "\"system\":\"%s\","
                "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
                api->model_name, max_tokens, temperature, esc_sys, esc_usr);
            break;
        default:
            break;
    }
    fclose(f);
    free(esc_sys);
    free(esc_usr);
}

static int run_curl(const char *cmd) {
    return system(cmd);
}

/* Extract message.content from Ollama/OpenAI response. */
static char *extract_content(const char *response_file, char *out, size_t out_sz) {
    char parser_path[PATH_BUF];
    char *proj = getenv("PRISC_PROJECT_ROOT");
    if (!proj) proj = ".";
    snprintf(parser_path, sizeof(parser_path), "%s/ops/+x/json_parser.+x", proj);
    
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "%s %s message.content", parser_path, response_file);
    FILE *pipe = POPEN(cmd, "r");
    if (!pipe) return NULL;
    size_t total = 0;
    while (total < out_sz - 1) {
        size_t n = fread(out + total, 1, out_sz - 1 - total, pipe);
        if (n == 0) break;
        total += n;
    }
    out[total] = '\0';
    PCLOSE(pipe);
    if (total == 0) return NULL;
    return out;
}

model_response_t *model_api_chat(model_api_t *api,
                                 const char *system_prompt,
                                 const char *user_prompt,
                                 double temperature,
                                 int max_tokens) {
    if (!api) return NULL;

    char payload_file[PATH_BUF];
    char response_file[PATH_BUF];
    char tmp_dir[PATH_BUF];
    char *proj = getenv("PRISC_PROJECT_ROOT");
    if (!proj) proj = ".";
    snprintf(tmp_dir, sizeof(tmp_dir), "%s/state", proj);
    mkdir(tmp_dir, 0755);
    snprintf(payload_file, sizeof(payload_file), "%s/llm_payload_%d.json", tmp_dir, (int)time(NULL));
    snprintf(response_file, sizeof(response_file), "%s/llm_response_%d.json", tmp_dir, (int)time(NULL));

    model_response_t *resp = calloc(1, sizeof(model_response_t));
    if (!resp) return NULL;

    build_payload(api, system_prompt, user_prompt, temperature, max_tokens, payload_file);

    char *cmd = build_curl_cmd(api, payload_file, response_file);
    if (!cmd) {
        resp->error_code = 1;
        resp->error_msg = strdup("unsupported provider or missing api key");
        return resp;
    }

    int rc = run_curl(cmd);
    free(cmd);
    if (rc != 0) {
        resp->error_code = 1;
        resp->error_msg = strdup("curl failed");
        return resp;
    }

    char content[16384];
    if (!extract_content(response_file, content, sizeof(content))) {
        resp->error_code = 1;
        resp->error_msg = strdup("failed to extract content");
        return resp;
    }

    resp->content = strdup(content);
    resp->model = strdup(api->model_name);

    /* Parse tokens if available */
    char *p = strstr(content, "prompt_eval_count");
    if (p) resp->prompt_tokens = atoi(strchr(p, ':') + 1);
    p = strstr(content, "eval_count");
    if (p) resp->completion_tokens = atoi(strchr(p, ':') + 1);
    resp->total_tokens = resp->prompt_tokens + resp->completion_tokens;

    return resp;
}

model_response_t *model_api_complete(model_api_t *api,
                                     const char *prompt,
                                     double temperature,
                                     int max_tokens) {
    return model_api_chat(api, "", prompt, temperature, max_tokens);
}

void model_response_free(model_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->model);
    free(resp->error_msg);
    free(resp);
}

/* Extract action/reason/confidence from response JSON. */
int model_extract_action(const char *content, char *action, size_t action_sz,
                         char *reason, size_t reason_sz, double *confidence) {
    if (!content) return 0;
    *confidence = 0.5;
    action[0] = '\0';
    reason[0] = '\0';

    /* Try json_parser first */
    char parser_path[PATH_BUF];
    char *proj = getenv("PRISC_PROJECT_ROOT");
    if (!proj) proj = ".";
    snprintf(parser_path, sizeof(parser_path), "%s/ops/+x/json_parser.+x", proj);
    
    if (access(parser_path, F_OK) == 0) {
        char tmp_content[PATH_BUF];
        snprintf(tmp_content, sizeof(tmp_content), "/tmp/llm_content_%d.json", (int)time(NULL));
        
        /* Write content to temp file */
        FILE *f = fopen(tmp_content, "w");
        if (f) {
            fputs(content, f);
            fclose(f);
        }
        
        const char *fields[] = {"action", "reason", "confidence"};
        for (int i = 0; i < 3; i++) {
            char cmd[PATH_BUF];
            snprintf(cmd, sizeof(cmd), "%s %s %s 2>/dev/null",
                     parser_path, tmp_content, fields[i]);
            FILE *p = POPEN(cmd, "r");
            if (p) {
                char buf[256];
                if (fgets(buf, sizeof(buf), p)) {
                    buf[strcspn(buf, "\r\n")] = '\0';
                    if (i == 0) snprintf(action, action_sz, "%s", buf);
                    else if (i == 1) snprintf(reason, reason_sz, "%s", buf);
                    else *confidence = atof(buf);
                }
                PCLOSE(p);
            }
        }
        unlink(tmp_content);
        return 1;
    }

    /* Fallback: simple string extraction */
    const char *p = content;
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
                if (len < 64) memcpy(action, q, len), action[len] = '\0';
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
                if (len < 256) memcpy(reason, q, len), reason[len] = '\0';
            }
        }
    }
    char *conf_ptr = strstr(p, "\"confidence\"");
    if (conf_ptr) {
        char *colon = strchr(conf_ptr, ':');
        if (colon) *confidence = atof(colon + 1);
    }
    return 1;
}