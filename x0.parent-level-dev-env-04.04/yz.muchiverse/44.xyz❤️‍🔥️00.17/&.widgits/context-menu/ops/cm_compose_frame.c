/* cm_compose_frame - CHTPM context menu: reads a real entity's own
 * meta.pdl METHOD rows (SAME file tp_desktop_window.c's own
 * load_methods() already reads - real precedent, not a new format) and
 * fills 8 static KEY:1-8 button labels. User/Move/Inventory/Skill/
 * Close/Cancel are fixed, static slots (not part of the dynamic 8) -
 * see cm_main.chtpm's own header comment for why.
 *
 * Usage: cm_compose_frame.+x (reads pkg_dir from pieces/system/
 * cm_state.txt, same convention as ee_/ez_compose_frame.c)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_SLOTS 8

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
    snprintf(p, sizeof(p), "%s/pieces/display/cm_screen_changed.txt", project_root);
    f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

typedef struct { char label[64]; char action[PATH_BUF]; } MethodItem;

static int is_fixed_slot(const char *label) {
    return strcmp(label, "User") == 0 || strcmp(label, "Move") == 0 ||
           strcmp(label, "Inventory") == 0 || strcmp(label, "Skill") == 0 ||
           strcmp(label, "Close") == 0 || strcmp(label, "Cancel") == 0;
}

/* Real precedent, not a new format: same parse shape as
 * tp_desktop_window.c's own load_methods() (SECTION|KEY|VALUE, METHOD
 * rows), reading the SAME real meta.pdl file. */
static int load_methods(const char *pkg_dir, MethodItem *items, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", pkg_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t llen = (size_t)(label_end - p);
        if (llen == 0 || llen >= sizeof(items[0].label)) continue;
        char label[64];
        memcpy(label, p, llen);
        label[llen] = '\0';
        if (is_fixed_slot(label)) continue;

        char *a = end + 1;
        while (*a == ' ') a++;
        char *a_end = a + strcspn(a, "\r\n");
        while (a_end > a && a_end[-1] == ' ') a_end--;
        size_t alen = (size_t)(a_end - a);
        if (alen == 0 || alen >= sizeof(items[0].action)) continue;

        snprintf(items[n].label, sizeof(items[0].label), "%s", label);
        memcpy(items[n].action, a, alen);
        items[n].action[alen] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

int main(void) {
    resolve_root();

    char state[PATH_BUF], gui[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/cm_state.txt", project_root);
    snprintf(gui, sizeof(gui), "%s/projects/context-menu/manager/gui_state.txt", project_root);

    char pkg_dir[PATH_BUF], pkg_name[128], msg[MAX_LINE];
    read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
    read_kv(state, "pkg_name", pkg_name, sizeof(pkg_name));
    read_kv(state, "last_message", msg, sizeof(msg));

    MethodItem items[MAX_SLOTS];
    int n = pkg_dir[0] ? load_methods(pkg_dir, items, MAX_SLOTS) : 0;

    {
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/projects/context-menu/manager'", project_root);
        if (system(cmd) != 0) { /* best-effort */ }
    }

    char keep[64][MAX_LINE];
    int n_keep = 0;
    FILE *rf = fopen(gui, "r");
    if (rf) {
        char line[MAX_LINE];
        while (n_keep < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "cm_header=", 10) == 0) continue;
            if (strncmp(line, "cm_label_", 9) == 0) continue;
            if (strncmp(line, "last_message=", 13) == 0) continue;
            snprintf(keep[n_keep], MAX_LINE, "%s", line);
            n_keep++;
        }
        fclose(rf);
    }
    FILE *g = fopen(gui, "w");
    if (!g) return 1;
    for (int i = 0; i < n_keep; i++) fputs(keep[i], g);
    fprintf(g, "cm_header=%s\n", pkg_name[0] ? pkg_name : "(entity)");
    for (int i = 0; i < MAX_SLOTS; i++) {
        fprintf(g, "cm_label_%d=%s\n", i + 1, i < n ? items[i].label : "");
    }
    fprintf(g, "last_message=%s\n", msg[0] ? msg : "");
    fclose(g);

    ping();
    return 0;
}
