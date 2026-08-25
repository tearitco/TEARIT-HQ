/* start_compose_frame - HOUSE LOADER chrome into view.txt */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 55

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

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

static void ping(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void row(FILE *o, const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    fprintf(o, "║  %.*s", len, content);
    for (int i = len; i < BOX_W; i++) fputc(' ', o);
    fprintf(o, "║\n");
}

static void sep(FILE *o) {
    fprintf(o, "╠");
    for (int i = 0; i < BOX_W + 2; i++) fputs("═", o);
    fprintf(o, "╣\n");
}

static void top(FILE *o, const char *title) {
    fprintf(o, "╔");
    for (int i = 0; i < BOX_W + 2; i++) fputs("═", o);
    fprintf(o, "╗\n");
    row(o, title);
    sep(o);
}

static void bot(FILE *o) {
    fprintf(o, "╚");
    for (int i = 0; i < BOX_W + 2; i++) fputs("═", o);
    fprintf(o, "╝\n");
}

static const char *piece_from_layout(const char *layout) {
    if (strstr(layout, "system.chtpm")) return "system";
    if (strstr(layout, "widgets.chtpm")) return "widgets";
    if (strstr(layout, "apps.chtpm")) return "apps";
    if (strstr(layout, "store.chtpm")) return "store";
    return "home";
}

int main(void) {
    resolve_root();

    char layout_path[PATH_BUF], state_path[PATH_BUF], view_path[PATH_BUF], msg_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(msg_path, sizeof(msg_path), "%s/pieces/system/start_state.txt", project_root);

    char layout[MAX_PATH] = "pieces/chtpm/layouts/home.chtpm";
    char target[128] = "home";
    char message[MAX_LINE] = "";
    read_kv(layout_path, "", layout, sizeof(layout)); /* may fail — try raw */
    {
        FILE *f = fopen(layout_path, "r");
        if (f) {
            if (fgets(layout, sizeof(layout), f))
                layout[strcspn(layout, "\n")] = '\0';
            fclose(f);
        }
    }
    read_kv(state_path, "active_target_id", target, sizeof(target));
    if (!target[0]) snprintf(target, sizeof(target), "%s", piece_from_layout(layout));
    read_kv(msg_path, "last_message", message, sizeof(message));

    const char *section = piece_from_layout(layout);
    FILE *o = fopen(view_path, "w");
    if (!o) return 1;

    if (strcmp(section, "home") == 0) {
        top(o, " H O U S E   L O A D E R ");
        row(o, "Pre-screen — pick a category:");
        row(o, "");
        row(o, "  System     house programs under ./");
        row(o, "  Widgets    &.widgits/ installed widgets");
        row(o, "  Apps       @.apps/ installed apps");
        row(o, "  App Store  @.app-store/ available packages");
        row(o, "");
        if (message[0]) row(o, message);
        bot(o);
    } else {
        char title[80];
        if (strcmp(section, "system") == 0) snprintf(title, sizeof(title), " S Y S T E M   P R O G R A M S ");
        else if (strcmp(section, "widgets") == 0) snprintf(title, sizeof(title), " W I D G E T S ");
        else if (strcmp(section, "apps") == 0) snprintf(title, sizeof(title), " A P P S ");
        else snprintf(title, sizeof(title), " A P P   S T O R E ");
        top(o, title);
        row(o, "Select an entry below (numbered methods).");
        row(o, "");
        if (message[0]) row(o, message);
        else row(o, "(scan builds the list into methods)");
        bot(o);
    }

    fclose(o);
    ping();
    return 0;
}
