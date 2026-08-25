#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LEN 1024
#define MAX_LINE_LEN 256

typedef struct {
    int counter;
    char last_script[64];
    char last_result[128];
} ScriptState;

static void path_join(char *out, size_t out_sz, const char *root, const char *rel) {
    snprintf(out, out_sz, "%s/%s", root, rel);
}

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void defaults(ScriptState *st) {
    memset(st, 0, sizeof(*st));
    snprintf(st->last_script, sizeof(st->last_script), "none");
    snprintf(st->last_result, sizeof(st->last_result), "Script op ready");
}

static void load_state(const char *root, ScriptState *st) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;

    defaults(st);
    path_join(path, sizeof(path), root, "session/script_state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "counter=", 8) == 0) st->counter = atoi(line + 8);
        else if (strncmp(line, "last_script=", 12) == 0) snprintf(st->last_script, sizeof(st->last_script), "%s", line + 12);
        else if (strncmp(line, "last_result=", 12) == 0) snprintf(st->last_result, sizeof(st->last_result), "%s", line + 12);
    }
    fclose(f);
}

static void save_state(const char *root, const ScriptState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/script_state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "counter=%d\n", st->counter);
    fprintf(f, "last_script=%s\n", st->last_script);
    fprintf(f, "last_result=%s\n", st->last_result);
    fclose(f);
}

int main(int argc, char **argv) {
    ScriptState st;
    const char *root;
    const char *action;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <project_root> <action>\n", argc > 0 ? argv[0] : "browser_exec_js");
        return 1;
    }

    root = argv[1];
    action = argv[2];
    load_state(root, &st);

    if (strcmp(action, "counter_inc") == 0) {
        st.counter++;
        snprintf(st.last_script, sizeof(st.last_script), "counter_inc");
        snprintf(st.last_result, sizeof(st.last_result), "counter=%d", st.counter);
    } else if (strcmp(action, "counter_reset") == 0) {
        st.counter = 0;
        snprintf(st.last_script, sizeof(st.last_script), "counter_reset");
        snprintf(st.last_result, sizeof(st.last_result), "counter=%d", st.counter);
    } else {
        snprintf(st.last_script, sizeof(st.last_script), "%s", action);
        snprintf(st.last_result, sizeof(st.last_result), "unknown_action");
    }

    save_state(root, &st);
    printf("%s\n", st.last_result);
    return 0;
}
