/* gemma_strategy.c - multi-strategy scaffolding for Gemma (270M model)
 * Gemma is too small to reliably follow TOOL: format, so we:
 * 1. Pre-parse user input to detect what tool is needed
 * 2. Select strategy (A/B/C) based on weights
 * 3. Execute that strategy
 *
 * Strategy A (weight: ~0.5): Pre-execute tool, send result to Gemma
 * Strategy B (weight: ~0.3): Send TOOL: format prompt, hope Gemma responds
 * Strategy C (weight: ~0.2): Handle locally (meta requests, help, etc)
 *
 * This is SCAFFOLDING - all strategies kept, weights tunable, no deletion.
 * Future: RL can adjust weights, nest in BT/FSM, combine strategies.
 *
 * Usage: gemma_strategy.+x (reads gui_state.txt's message_input)
 * Outputs: Sets state.txt fields:
 *   selected_strategy=[A|B|C]
 *   detected_tool=[tool|none]
 *   strategy_log=[appended record]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_gui_state(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/muchi-pal-agent/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *key, const char *value) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
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
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void to_lower(char *s) {
    for (int i = 0; s[i]; i++) s[i] = tolower((unsigned char)s[i]);
}

static int has_keyword(const char *text, const char *keyword) {
    if (!text || !keyword) return 0;
    size_t klen = strlen(keyword);
    for (const char *p = text; *p; p++) {
        if (strncmp(p, keyword, klen) == 0) {
            char before = (p == text) ? ' ' : *(p - 1);
            char after = *(p + klen);
            if (!isalnum((unsigned char)before) && !isalnum((unsigned char)after)) {
                return 1;
            }
        }
    }
    return 0;
}

static const char *detect_tool(const char *input) {
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s", input);
    to_lower(buf);

    /* W1 LT pipeline ops (checked first — specific before generic) */
    if (has_keyword(buf, "plan cells")) return "plan_cells";
    if (has_keyword(buf, "plan cell")) return "plan_cells";
    if (has_keyword(buf, "fill cells")) return "fill_cell";
    if (has_keyword(buf, "fill cell")) return "fill_cell";
    if (has_keyword(buf, "verify cells")) return "verify_cell";
    if (has_keyword(buf, "verify cell")) return "verify_cell";
    if (has_keyword(buf, "apply cells")) return "apply_cell";
    if (has_keyword(buf, "apply cell")) return "apply_cell";
    if (has_keyword(buf, "grade chapter")) return "grade_chapter";

    /* General tools */
    if (has_keyword(buf, "run") || has_keyword(buf, "exec") ||
        has_keyword(buf, "execute") || has_keyword(buf, "command") ||
        has_keyword(buf, "cmd")) return "exec_cmd";

    if (has_keyword(buf, "read") || has_keyword(buf, "view") ||
        has_keyword(buf, "cat") || has_keyword(buf, "open") ||
        has_keyword(buf, "display")) return "read_file";

    if (has_keyword(buf, "write") || has_keyword(buf, "create") ||
        has_keyword(buf, "save")) return "write_file";

    if (has_keyword(buf, "edit") || has_keyword(buf, "replace") ||
        has_keyword(buf, "modify") || has_keyword(buf, "append")) return "edit_file";

    if (has_keyword(buf, "search") || has_keyword(buf, "find") ||
        has_keyword(buf, "grep")) return "search_in_files";

    if (has_keyword(buf, "list") || has_keyword(buf, "dir") ||
        has_keyword(buf, "files") || has_keyword(buf, "show")) return "list_dir";

    if (has_keyword(buf, "speak") || has_keyword(buf, "say") ||
        has_keyword(buf, "voice")) return "speak";

    if (has_keyword(buf, "web") || has_keyword(buf, "search") ||
        has_keyword(buf, "google")) return "web_search";

    return "none";
}

static const char *detect_meta(const char *input) {
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s", input);
    to_lower(buf);

    if (has_keyword(buf, "help") || strchr(input, '?') ||
        has_keyword(buf, "tools") || has_keyword(buf, "capabilities"))
        return "meta_help";

    if ((has_keyword(buf, "what") || has_keyword(buf, "show")) &&
        (has_keyword(buf, "status") || has_keyword(buf, "state")))
        return "meta_status";

    if ((has_keyword(buf, "show") || has_keyword(buf, "display")) &&
        (has_keyword(buf, "history") || has_keyword(buf, "log")))
        return "meta_history";

    return "none";
}

static void append_log(const char *strategy, const char *tool, const char *meta) {
    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/strategy_log.txt", project_root);
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "[%ld] strategy=%s tool=%s meta=%s\n", now, strategy, tool ? tool : "none", meta ? meta : "none");
    fclose(f);
}

int main(void) {
    srand((unsigned)time(NULL));
    resolve_root();

    char message[MAX_LINE] = "";
    read_gui_state("message_input", message, sizeof(message));

    if (!message[0]) {
        return 0;
    }

    const char *meta = detect_meta(message);
    if (strcmp(meta, "none") != 0) {
        write_state_field("selected_strategy", "C");
        write_state_field("detected_tool", "none");
        write_state_field("detected_meta", meta);
        char msg[256];
        snprintf(msg, sizeof(msg), "[Strategy C] Meta: %s", meta);
        write_state_field("sys_msg", msg);
        append_log("C", NULL, meta);
        return 0;
    }

    const char *tool = detect_tool(message);
    write_state_field("detected_tool", tool);

    if (strcmp(tool, "none") == 0) {
        /* No tool keyword detected - ordinary chat, straight to Gemma as
         * plain conversation. Direct instruction: "gemma should never
         * parse tool etc" - this used to route here into Strategy B
         * (asking Gemma to try TOOL: format on every non-tool message,
         * including a bare "hi"), which is exactly the behavior that
         * instruction rules out. Gemma only ever sees plain text now;
         * see check_response.c's gemma branch and prompt_keyword.txt. */
        write_state_field("selected_strategy", "A");
        write_state_field("sys_msg", "Chat (no tool)");
        append_log("A", NULL, NULL);
    } else {
        /* A tool WAS detected - always pre-execute deterministically
         * (strategy_execute_a.c), never fall back to asking Gemma to try
         * TOOL: format itself (the old weighted A/B split - see
         * !.GRAND-PLAN-TOOL-SCAFFOLD.txt's own "Strategy B" - same
         * instruction applies: Gemma never parses/emits tool calls). */
        write_state_field("selected_strategy", "A");
        char msg[256];
        snprintf(msg, sizeof(msg), "[Strategy A] Tool: %s", tool);
        write_state_field("sys_msg", msg);
        append_log("A", tool, NULL);
    }

    return 0;
}
