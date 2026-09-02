/* setup_set_focus - point the Match Setup WIDGIT at the host TSC_ELO
 * session root. Modeled directly on &.widgits/file-menu/ops/fm_set_focus.c
 * (the house's own proven widget-focus writer): the host passes its own
 * session root to the widget's run-widget action, this op turns it into a
 * focus.txt the widget's ops can read for the cmd-bus paths.
 *
 * Self-contained, no shared headers.
 * Usage: setup_set_focus.+x <widget_state_dir> <host_session_root> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: setup_set_focus.+x <widget_state_dir> <host_session_root>\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *host = argv[2];
    mkdir(wdir, 0755);

    char inbox[PATH_BUF], status[PATH_BUF], bridge[PATH_BUF];
    /* Optional host bridge overrides (host writes its own paths when it
     * wants something other than the defaults). */
    snprintf(bridge, sizeof(bridge), "%s/pieces/system/widget_bridge.txt", host);
    read_kv(bridge, "inbox_path", inbox, sizeof(inbox));
    read_kv(bridge, "status_path", status, sizeof(status));
    if (!inbox[0]) snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", host);
    if (!status[0]) snprintf(status, sizeof(status), "%s/pieces/system/widget_cmds/status.txt", host);

    /* host_session_root is the host's session dir; derive a stable
     * project label from its REAL project (strip /pieces/sessions/<id>). */
    char project_id[256] = "tsc-elo";
    {
        char real[PATH_BUF];
        snprintf(real, sizeof(real), "%s", host);
        char *sess = strstr(real, "/pieces/sessions/");
        if (sess) *sess = '\0';
        char *base = strrchr(real, '/');
        if (base && base[1]) snprintf(project_id, sizeof(project_id), "%s", base + 1);
    }

    char focus[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", wdir);
    FILE *f = fopen(focus, "w");
    if (!f) return 1;
    fprintf(f, "session_root=%s\n", host);
    fprintf(f, "inbox_path=%s\n", inbox);
    fprintf(f, "status_path=%s\n", status);
    fprintf(f, "kind=game_setup\n");
    fprintf(f, "project_id=%s\n", project_id);
    fprintf(f, "display_name=TSC ELO Match Setup\n");
    fclose(f);
    printf("FOCUS ok project=%s -> %s\n", project_id, host);
    return 0;
}
