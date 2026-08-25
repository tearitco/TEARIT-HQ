/* generate_corpus - a standalone CLI tool (NOT a tick-loop op, run
 * directly by a human/script) that asks an LLM (any of the 3 already-
 * proven provider shapes - ollama/gemini/llamacpp) to author a plain-
 * text training corpus for IQABOD, using corpus_author.txt as the
 * persona, and appends the result to IQABOD's own corpuses/<name>.txt.
 * See ROADMAP-models.txt §11.1 and TODO.txt Phase A2.
 *
 * Unlike send_message.c, this calls connect_op.+x SYNCHRONOUSLY -
 * connect_op.c already blocks internally (waitpid on curl), and there
 * is no prisc+x tick loop to avoid blocking here since this is a
 * plain CLI invocation, not an op dispatched from main_loop.pal. No
 * fork/detach dance needed.
 *
 * Self-contained: own root resolution, own request builders, no shared
 * headers - matching every other op in this project.
 * Usage: generate_corpus.+x <provider_kind> <api_url> <model_name>
 *        <topic_instruction> <line_count> <output_name>
 *   provider_kind: ollama | gemini | llamacpp
 *   api_url: same meaning as model_list.txt's api_url field for that
 *            provider_kind (NOT an iqabod-style repurposed path - this
 *            tool only ever talks to a real HTTP backend)
 *   output_name: written to <IQABOD_ROOT>/corpuses/<output_name>.txt
 *                (IQABOD_ROOT read from the IQABOD_PROJECT_ROOT env var,
 *                or defaults to the path used throughout this session) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_FIELD 512

static char project_root[MAX_PATH] = ".";
/* No hardcoded fallback on purpose - IQABOD_PROJECT_ROOT must be set by
 * the caller (matching PRISC_PROJECT_ROOT's own convention elsewhere in
 * this project). Baking a specific absolute path into source here would
 * silently break the moment IQABOD moves, and this op has no other way
 * to discover it (unlike chat's model_list.txt, which already stores it
 * per-entry - this tool runs standalone, before any curriculum exists
 * to look that path up from). */
static char iqabod_root[MAX_PATH] = ".";

static void resolve_roots(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *iq = getenv("IQABOD_PROJECT_ROOT");
    if (iq && iq[0]) snprintf(iqabod_root, sizeof(iqabod_root), "%s", iq);
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

static char *run_capture(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    char *buf = malloc(65536);
    size_t total = 0, n;
    while ((n = fread(buf + total, 1, 65535 - total, pipe)) > 0) {
        total += n;
        if (total >= 65535) break;
    }
    buf[total] = '\0';
    pclose(pipe);
    while (total > 0 && (buf[total - 1] == '\n' || buf[total - 1] == '\r')) buf[--total] = '\0';
    return buf;
}

int main(int argc, char *argv[]) {
    resolve_roots();
    if (argc < 7) {
        fprintf(stderr, "Usage: generate_corpus.+x <provider_kind> <api_url> <model_name> "
                         "<topic_instruction> <line_count> <output_name>\n");
        return 1;
    }
    const char *provider_kind = argv[1];
    const char *api_url = argv[2];
    const char *model_name = argv[3];
    const char *topic = argv[4];
    const char *line_count = argv[5];
    const char *output_name = argv[6];

    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/corpus_author.txt", project_root);
    char *persona = read_full_file(persona_path);
    if (!persona) { fprintf(stderr, "corpus_author.txt not found\n"); return 1; }

    char user_turn[MAX_FIELD];
    snprintf(user_turn, sizeof(user_turn), "Topic: %s\nNumber of sentences: %s", topic, line_count);

    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/generate_corpus_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/generate_corpus_response_%d.json", getpid());

    FILE *pf = fopen(request_path, "w");
    if (!pf) return 1;

    char full_url[MAX_FIELD * 2 + 128];

    if (strcmp(provider_kind, "gemini") == 0) {
        char *gemini_key = getenv("GEMINI_API_KEY");
        fputs("{\"systemInstruction\":{\"parts\":[{\"text\":\"", pf);
        json_escaped(pf, persona);
        fputs("\"}]},\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"", pf);
        json_escaped(pf, user_turn);
        fputs("\"}]}]}", pf);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(full_url, sizeof(full_url), "%s/v1beta/models/%s:generateContent?key=%s",
                 api_url, model_name, gemini_key ? gemini_key : "");
#pragma GCC diagnostic pop
    } else if (strcmp(provider_kind, "llamacpp") == 0) {
        /* Manually-built raw Llama3 prompt, one-shot, no history -
         * matches cpp-llm's proven pattern, simplified since there's
         * no context_log.txt to fold in here. */
        fputs("{\"prompt\":\"<|start_header_id|>system<|end_header_id|>\\n\\n", pf);
        json_escaped(pf, persona);
        fputs("<|eot_id|><|start_header_id|>user<|end_header_id|>\\n\\n", pf);
        json_escaped(pf, user_turn);
        fputs("<|eot_id|><|start_header_id|>assistant<|end_header_id|>\\n\\n\","
              "\"n_predict\":1024,\"stream\":false,\"stop\":[\"<|eot_id|>\",\"<|end_of_text|>\"]}", pf);
        snprintf(full_url, sizeof(full_url), "%s/completion", api_url);
    } else {
        fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"", model_name);
        json_escaped(pf, persona);
        fputs("\"},{\"role\":\"user\",\"content\":\"", pf);
        json_escaped(pf, user_turn);
        fputs("\"}]}", pf);
        snprintf(full_url, sizeof(full_url), "%s/api/chat", api_url);
    }
    fclose(pf);
    free(persona);

    char connect_cmd[PATH_BUF * 3];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(connect_cmd, sizeof(connect_cmd), "'%s/ops/+x/connect_op.+x' '%s' '%s' '%s'",
             project_root, full_url, request_path, response_path);
#pragma GCC diagnostic pop
    int rc = system(connect_cmd);
    if (rc != 0) { fprintf(stderr, "connect_op failed (rc=%d)\n", rc); remove(request_path); return 1; }

    char json_parser_cmd[PATH_BUF * 2];
    char *text = NULL;
    if (strcmp(provider_kind, "gemini") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'candidates[0].content.parts[0].text'", project_root, response_path);
    } else if (strcmp(provider_kind, "llamacpp") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'content'", project_root, response_path);
    } else {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'message.content'", project_root, response_path);
    }
    text = run_capture(json_parser_cmd);
    remove(request_path);
    remove(response_path);

    if (!text || strlen(text) == 0) {
        fprintf(stderr, "Empty/unparseable response from provider\n");
        free(text);
        return 1;
    }

    char corpus_path[PATH_BUF];
    snprintf(corpus_path, sizeof(corpus_path), "%s/corpuses/%s.txt", iqabod_root, output_name);
    FILE *cf = fopen(corpus_path, "a");
    if (!cf) { fprintf(stderr, "Could not open %s for writing\n", corpus_path); free(text); return 1; }
    fputs(text, cf);
    fputc('\n', cf);
    fclose(cf);

    printf("Wrote corpus to %s\n", corpus_path);
    free(text);
    return 0;
}
