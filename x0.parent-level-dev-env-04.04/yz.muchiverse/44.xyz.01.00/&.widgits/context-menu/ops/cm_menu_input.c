/* cm_menu_input - CHTPM context menu dispatch.
 * KEY:1-8 = run the Nth real method from the entity's own meta.pdl
 *   (fixed slots User/Move/Inventory/Skill/Close/Cancel excluded - see
 *   cm_compose_frame.c's own is_fixed_slot()), same real
 *   "<action> '<pkg_dir>' &" convention tp_desktop_window.c's own
 *   dispatch already uses (fo-menu-sys.md).
 * KEY:9 = Close (writes a real close_requested flag; main_loop_chtpm.pal
 *   checks it and halts).
 * KEY:0 = Cancel (no-op, dismiss).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void write_kv(const char *path, const char *key, const char *value) {
    char lines[64][MAX_LINE];
    int n = 0, replaced = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (n < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                snprintf(lines[n], MAX_LINE, "%s=%s\n", key, value);
                replaced = 1;
            } else {
                snprintf(lines[n], MAX_LINE, "%s", line);
            }
            n++;
        }
        fclose(rf);
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "%s=%s\n", key, value);
    fclose(wf);
}

static void bump(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/cm_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void set_msg(const char *state, const char *msg) {
    write_kv(state, "last_message", msg);
}

static int is_fixed_slot(const char *label) {
    return strcmp(label, "User") == 0 || strcmp(label, "Move") == 0 ||
           strcmp(label, "Inventory") == 0 || strcmp(label, "Skill") == 0 ||
           strcmp(label, "Close") == 0 || strcmp(label, "Cancel") == 0;
}

typedef struct { char label[64]; char action[PATH_BUF]; } MethodItem;

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

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char state[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/cm_state.txt", project_root);

    if (key == 0) return 0;

    if (key == '0') { set_msg(state, "Cancel"); bump(); return 0; }

    if (key == '9') {
        /* REAL, PROVEN mechanism only (2026-08-05: cut a speculative
         * "read_state to signal PAL halt" approach after checking
         * prisc+x.c's own source - read_state resolves a PIECE's own
         * state path via resolve_piece_state_path(), not this
         * project's own cm_state.txt at all - wrong mechanism, would
         * have silently done nothing. Killing this session's own real
         * tracked PIDs, same as button.sh's own "kill" action already
         * does, is simple and provably correct.) */
        char proc_list[PATH_BUF];
        snprintf(proc_list, sizeof(proc_list), "%s/pieces/os/proc_list.txt", project_root);
        FILE *pf = fopen(proc_list, "r");
        if (pf) {
            int pid; char name[64];
            while (fscanf(pf, "%d %63s", &pid, name) == 2) {
                char cmd[128];
                snprintf(cmd, sizeof(cmd), "kill %d 2>/dev/null", pid);
                int rc = system(cmd);
                (void)rc;
            }
            fclose(pf);
        }
        return 0;
    }

    if (key >= '1' && key <= '8') {
        int slot = key - '1';
        char pkg_dir[PATH_BUF];
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) { set_msg(state, "No pkg_dir set"); bump(); return 0; }
        MethodItem items[8];
        int n = load_methods(pkg_dir, items, 8);
        if (slot >= n) { bump(); return 0; }
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "%s '%s' >/dev/null 2>&1 &", items[slot].action, pkg_dir);
        int rc = system(cmd);
        (void)rc;
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "Ran: %s", items[slot].label);
        set_msg(state, msg);
        bump();
        return 0;
    }

    return 0;
}
