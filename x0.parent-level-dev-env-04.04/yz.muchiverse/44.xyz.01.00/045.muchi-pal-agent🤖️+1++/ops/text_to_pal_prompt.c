/* text_to_pal_prompt - converts this project's own context_log.txt
 * (role|kind|tool_name|content, pipe-escaped, one turn per line - see
 * send_message.c's header comment) into a raw Llama3 token-formatted
 * prompt for the llamacpp-completion provider's /completion endpoint.
 *
 * Same role cpp-llm's own text_to_llama3.c plays, and the same proven
 * token format (<|start_header_id|>role<|end_header_id|>\n\ncontent
 * <|eot_id|>...), but reading OUR pipe format directly instead of a JSON
 * array - simpler than cpp-llm's version needed to be, since pipe
 * splitting doesn't need a JSON parser.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: text_to_pal_prompt.+x (no args - reads context_log.txt/persona
 * itself, prints the raw prompt to stdout) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

int main(void) {
    resolve_root();

    char persona_path[PATH_BUF], log_path[PATH_BUF];
    /* llamacpp/completion has no native tool schema - it needs the
     * strict-JSON-with-few-shot-examples persona (proven in cpp-llm),
     * not the "just call the tool" persona native-tool-calling providers
     * (ollama/gemini) use. */
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/prompt_json.txt", project_root);
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);

    char *persona = read_full_file(persona_path);
    if (persona && strlen(persona) > 0) {
        printf("<|start_header_id|>system<|end_header_id|>\n\n%s<|eot_id|>", persona);
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

            if (strcmp(kind, "info") == 0) {
                /* UI-only listing (e.g. /model) - never real conversation
                 * content, same skip send_message.c's other three request
                 * builders apply - see that file's own header comment on
                 * the append_log_turn() call that produces this kind. */
                free(content);
                continue;
            }

            if (strcmp(role, "user") == 0) {
                printf("<|start_header_id|>user<|end_header_id|>\n\n%s<|eot_id|>", content);
            } else if (strcmp(role, "assistant") == 0 && strcmp(kind, "tool_call") == 0) {
                /* Reconstruct the same {"tool":...,"args":...} JSON shape
                 * the model itself is asked to produce (see the trailing
                 * "Respond ONLY with JSON" primer below and the persona),
                 * so replayed history matches what the model actually saw
                 * itself say, not a different representation. */
                printf("<|start_header_id|>assistant<|end_header_id|>\n\n{\"tool\":\"%s\",\"args\":%s}<|eot_id|>", tool_name, content);
            } else if (strcmp(role, "assistant") == 0) {
                printf("<|start_header_id|>assistant<|end_header_id|>\n\n%s<|eot_id|>", content);
            } else if (strcmp(role, "tool") == 0) {
                printf("<|start_header_id|>system<|end_header_id|>\n\nTOOL_RESULT: %s<|eot_id|>", content);
            }
            free(content);
        }
        fclose(lf);
    }

    printf("<|start_header_id|>system<|end_header_id|>\n\nRespond ONLY with JSON.<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n");

    return 0;
}
