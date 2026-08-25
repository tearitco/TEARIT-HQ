/* switch_model - the feature this whole project exists to add: a
 * standalone "/model <id>" command, independent of switching API
 * endpoints. In gem-dev/groq-ollama/cpp-llm, picking a model only ever
 * happens as a side effect of resolve_local_model() firing off the back
 * of a "Switch API" action - there's no way in any of them to say "use
 * this other model on the API I'm already connected to." Here it's its
 * own verb: look <id> up in the model registry and rewrite the session's
 * provider fields directly.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: switch_model.+x <model_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

int main(int argc, char **argv) {
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);

    if (argc < 2 || strlen(argv[1]) == 0) {
        write_state_field(state_path, "input_buffer", "");
        write_state_field(state_path, "sys_msg", "Usage: /model <id> - see pieces/registry/models/model_list.txt");
        return 0;
    }

    /* Trim trailing whitespace/newline off the requested id. */
    char want[MAX_FIELD];
    snprintf(want, sizeof(want), "%s", argv[1]);
    size_t wlen = strlen(want);
    while (wlen > 0 && (want[wlen - 1] == ' ' || want[wlen - 1] == '\n' || want[wlen - 1] == '\r')) want[--wlen] = '\0';

    char registry_path[PATH_BUF];
    snprintf(registry_path, sizeof(registry_path), "%s/pieces/registry/models/model_list.txt", project_root);
    FILE *f = fopen(registry_path, "r");
    int found = 0;
    char provider_kind[MAX_FIELD] = "", api_url[MAX_FIELD] = "", model_name[MAX_FIELD] = "";
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            line[strcspn(line, "\n")] = '\0';
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            if (strcmp(line, want) == 0) {
                snprintf(provider_kind, sizeof(provider_kind), "%s", p1 + 1);
                snprintf(api_url, sizeof(api_url), "%s", p2 + 1);
                snprintf(model_name, sizeof(model_name), "%s", p3 + 1);
                found = 1;
                break;
            }
        }
        fclose(f);
    }

    write_state_field(state_path, "input_buffer", "");
    if (found) {
        write_state_field(state_path, "current_model_id", want);
        write_state_field(state_path, "provider_kind", provider_kind);
        write_state_field(state_path, "current_api_url", api_url);
        write_state_field(state_path, "current_model_name", model_name);
        char msg[MAX_FIELD + 32];
        snprintf(msg, sizeof(msg), "Switched to %s", want);
        write_state_field(state_path, "sys_msg", msg);

        /* 2026-07-31 (2&3-jul31-sprint): REMEMBER the user's last chosen
         * model across runs. button.sh run has no copy-back of world_01
         * (sessions are wiped by reap_stale_sessions on every launch), so
         * state.txt alone can never persist this - write the id to the
         * top-level last_model.txt instead. In a real session project_root
         * is the session dir, and last_model.txt is SYMLINKED into it from
         * the real project root (button.sh's blanket top-level symlink
         * loop, guaranteed present because button.sh run touches the real
         * file first) - so this fopen writes the REAL file, which survives
         * session recreation and is read back by button.sh run at boot. */
        char last_path[PATH_BUF];
        snprintf(last_path, sizeof(last_path), "%s/last_model.txt", project_root);
        FILE *lf = fopen(last_path, "w");
        if (lf) { fprintf(lf, "%s\n", want); fclose(lf); }
    } else {
        char msg[MAX_FIELD + 32];
        snprintf(msg, sizeof(msg), "Unknown model: %s", want);
        write_state_field(state_path, "sys_msg", msg);
    }

    return 0;
}
