/* fm_set_focus - point this widget instance at an editor session root.
 * Usage: fm_set_focus.+x <widget_state_dir> <editor_session_root>
 * Writes focus.txt with paths from editor's widget_bridge.txt (or constructs).
 */
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
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: fm_set_focus.+x <widget_state_dir> <editor_session_root>\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *ed = argv[2];
    mkdir(wdir, 0755);

    char bridge[PATH_BUF], inbox[PATH_BUF], buf[PATH_BUF], st[PATH_BUF], status[PATH_BUF];
    char kind[128], project_id[128], display[128], live[PATH_BUF], saves[PATH_BUF];
    snprintf(bridge, sizeof(bridge), "%s/pieces/system/widget_bridge.txt", ed);
    read_kv(bridge, "inbox_path", inbox, sizeof(inbox));
    read_kv(bridge, "buffer_path", buf, sizeof(buf));
    read_kv(bridge, "state_path", st, sizeof(st));
    read_kv(bridge, "status_path", status, sizeof(status));
    read_kv(bridge, "kind", kind, sizeof(kind));
    read_kv(bridge, "project_id", project_id, sizeof(project_id));
    read_kv(bridge, "display_name", display, sizeof(display));
    read_kv(bridge, "live_world", live, sizeof(live));
    read_kv(bridge, "saves_root", saves, sizeof(saves));
    if (!inbox[0]) snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", ed);
    if (!buf[0]) snprintf(buf, sizeof(buf), "%s/pieces/system/editor_buffer.txt", ed);
    if (!st[0]) snprintf(st, sizeof(st), "%s/pieces/system/editor_state.txt", ed);
    if (!status[0]) snprintf(status, sizeof(status), "%s/pieces/system/widget_cmds/status.txt", ed);
    if (!kind[0]) snprintf(kind, sizeof(kind), "text_buffer");
    if (!project_id[0]) snprintf(project_id, sizeof(project_id), "unknown");
    if (!display[0]) snprintf(display, sizeof(display), "%s", project_id);

    char focus[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/focus.txt", wdir);
    FILE *f = fopen(focus, "w");
    if (!f) return 1;
    fprintf(f, "session_root=%s\n", ed);
    fprintf(f, "editor_session=%s\n", ed); /* legacy alias */
    fprintf(f, "inbox_path=%s\n", inbox);
    fprintf(f, "buffer_path=%s\n", buf);
    fprintf(f, "state_path=%s\n", st);
    fprintf(f, "status_path=%s\n", status);
    fprintf(f, "kind=%s\n", kind);
    fprintf(f, "project_id=%s\n", project_id);
    fprintf(f, "display_name=%s\n", display);
    fprintf(f, "live_world=%s\n", live);
    fprintf(f, "saves_root=%s\n", saves);
    fclose(f);
    printf("FOCUS ok kind=%s project=%s -> %s\n", kind, project_id, ed);
    return 0;
}
