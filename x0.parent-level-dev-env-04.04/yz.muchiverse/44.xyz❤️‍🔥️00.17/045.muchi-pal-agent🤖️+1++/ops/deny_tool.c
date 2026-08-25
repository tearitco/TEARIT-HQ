/* deny_tool - the "n" verb. Dispatched from main_loop.pal when
 * input_buffer is exactly "n"/"no" while ai_state=PENDING_PERM.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: deny_tool.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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
    fprintf(f, "%s|%s|%s|%s\n", role, kind, tool_name ? tool_name : "", content);
    fclose(f);
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], log_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);

    char tool_name[MAX_FIELD];
    read_state_field(state_path, "pending_tool_name", tool_name, sizeof(tool_name));
    if (strlen(tool_name) == 0) return 0;

    append_log_turn(log_path, "tool", "text", tool_name, "Error: Permission denied by user.");

    write_state_field(state_path, "pending_tool_name", "");
    write_state_field(state_path, "pending_tool_args", "");
    write_state_field(state_path, "ai_state", "IDLE");
    write_state_field(state_path, "sys_msg", "Tool execution denied.");

    return 0;
}
