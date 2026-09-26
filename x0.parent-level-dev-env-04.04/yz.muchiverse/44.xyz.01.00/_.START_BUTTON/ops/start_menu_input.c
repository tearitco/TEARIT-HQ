/* start_menu_input - idle href bridge + KEY method dispatch + same-TTY handoff.
 *
 * Usage: start_menu_input.+x <keycode>
 *   0 = idle: map current_layout → active_target_id, rescan section if needed
 *   KEY:n digits → RUN / INFO / REFRESH
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU 64

typedef struct {
    char label[160];
    char command[512];
} MenuItem;

static char project_root[MAX_PATH] = ".";
static char install_root[MAX_PATH] = "."; /* where config + house live if session */

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *inst = getenv("PRISC_INSTALL_ROOT");
    if (inst && inst[0]) {
        snprintf(install_root, sizeof(install_root), "%s", inst);
    } else {
        char cfg[PATH_BUF], resolved[MAX_PATH];
        snprintf(cfg, sizeof(cfg), "%s/config/start_button.pdl", project_root);
        if (realpath(cfg, resolved)) {
            char *slash = strrchr(resolved, '/');
            if (slash) {
                *slash = '\0';
                slash = strrchr(resolved, '/');
                if (slash) {
                    *slash = '\0';
                    snprintf(install_root, sizeof(install_root), "%s", resolved);
                }
            }
        } else {
            snprintf(install_root, sizeof(install_root), "%s", project_root);
        }
    }
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
    FILE *f = fopen(path, "r");
    char lines[48][MAX_LINE];
    int n = 0;
    if (f) {
        while (n < 48 && fgets(lines[n], MAX_LINE, f)) n++;
        fclose(f);
    }
    size_t klen = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void set_message(const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/start_state.txt", project_root);
    write_kv(path, "last_message", msg);
}

static void bump_screen(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/start_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void clear_relay(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    FILE *f = fopen(p, "w");
    if (f) fclose(f);
}

static void write_bridge(const char *piece_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "module_path=system/prisc+x pal/main_loop_chtpm.pal\n");
    fprintf(f, "project_id=start-button\n");
    fprintf(f, "active_target_id=%s\n", piece_id && piece_id[0] ? piece_id : "home");
    fclose(f);
}

static const char *piece_from_layout(const char *layout) {
    if (!layout) return "home";
    if (strstr(layout, "system.chtpm")) return "system";
    if (strstr(layout, "widgets.chtpm")) return "widgets";
    if (strstr(layout, "apps.chtpm")) return "apps";
    if (strstr(layout, "store.chtpm")) return "store";
    return "home";
}

static void read_layout(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(out, out_sz, "pieces/chtpm/layouts/home.chtpm");
        return;
    }
    if (!fgets(out, (int)out_sz, f))
        snprintf(out, out_sz, "pieces/chtpm/layouts/home.chtpm");
    else
        out[strcspn(out, "\n")] = '\0';
    fclose(f);
}

static void run_scan(const char *section) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && ./ops/+x/start_scan.+x '%s' >/dev/null 2>&1",
             project_root, section ? section : "all");
    { int _rc = system(cmd); (void)_rc; }
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s) - 1;
    while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) *e-- = '\0';
    return s;
}

static int load_menu(const char *piece, MenuItem *items, int max) {
    char pdl[PATH_BUF];
    snprintf(pdl, sizeof(pdl),
             "%s/projects/start-button/pieces/%s/piece.pdl", project_root, piece);
    FILE *f = fopen(pdl, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        snprintf(items[n].label, sizeof(items[n].label), "%s", trim(p1 + 1));
        snprintf(items[n].command, sizeof(items[n].command), "%s", trim(p2 + 1));
        n++;
    }
    fclose(f);
    return n;
}

static void read_cfg_root(const char *key, char *out, size_t out_sz) {
    char path[PATH_BUF], line[MAX_LINE];
    out[0] = '\0';
    snprintf(path, sizeof(path), "%s/config/start_button.pdl", install_root);
    if (access(path, F_OK) != 0)
        snprintf(path, sizeof(path), "%s/config/start_button.pdl", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        if (!strstr(line, key)) continue;
        char *p = strrchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        p[strcspn(p, "\r\n")] = '\0';
        snprintf(out, out_sz, "%s", p);
        break;
    }
    fclose(f);
}

/* RUN:section:relpath → resolve button.sh, write handoff, request quit */
static void do_run(const char *section, const char *relpath) {
    char root_key[32];
    char root_rel[MAX_PATH];
    if (strcmp(section, "system") == 0) snprintf(root_key, sizeof(root_key), "scan_root");
    else if (strcmp(section, "widgets") == 0) snprintf(root_key, sizeof(root_key), "widgets_root");
    else if (strcmp(section, "apps") == 0) snprintf(root_key, sizeof(root_key), "apps_root");
    else {
        set_message("Cannot RUN this section.");
        bump_screen();
        return;
    }
    read_cfg_root(root_key, root_rel, sizeof(root_rel));
    if (!root_rel[0]) {
        if (strcmp(section, "system") == 0) snprintf(root_rel, sizeof(root_rel), "..");
        else if (strcmp(section, "widgets") == 0) snprintf(root_rel, sizeof(root_rel), "../&.widgits");
        else snprintf(root_rel, sizeof(root_rel), "../@.apps");
    }

    char joined[PATH_BUF], absdir[PATH_BUF];
    /* root_rel is relative to install_root (*.START_BUTTON) */
    snprintf(joined, sizeof(joined), "%s/%s/%s", install_root, root_rel, relpath);
    if (!realpath(joined, absdir)) {
        snprintf(absdir, sizeof(absdir), "%s", joined);
    }

    char button[PATH_BUF + 32];
    snprintf(button, sizeof(button), "%s/button.sh", absdir);
    if (access(button, X_OK) != 0 && access(button, F_OK) != 0) {
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "No button.sh: %s", relpath);
        set_message(msg);
        bump_screen();
        return;
    }

    /* handoff file: absolute dir to chdir + run */
    char hp[PATH_BUF];
    snprintf(hp, sizeof(hp), "%s/pieces/system/handoff_launch.txt", project_root);
    FILE *hf = fopen(hp, "w");
    if (hf) {
        fprintf(hf, "%s\n", absdir);
        fclose(hf);
    }
    char qp[PATH_BUF];
    snprintf(qp, sizeof(qp), "%s/pieces/system/quit_request.txt", project_root);
    FILE *qf = fopen(qp, "w");
    if (qf) { fputs("1\n", qf); fclose(qf); }

    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Launching %s (same terminal)...", relpath);
    set_message(msg);
    bump_screen();
}

static void do_info(const char *relpath) {
    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Store item '%s' — install not wired yet.", relpath);
    set_message(msg);
    bump_screen();
}

/* Track last bridged piece to avoid rescanning every idle tick. */
static void idle_bridge(void) {
    char layout[MAX_PATH], cur[128], piece[64];
    read_layout(layout, sizeof(layout));
    snprintf(piece, sizeof(piece), "%s", piece_from_layout(layout));

    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/pieces/apps/player_app/state.txt", project_root);
    read_kv(sp, "active_target_id", cur, sizeof(cur));

    char bridge_flag[PATH_BUF];
    snprintf(bridge_flag, sizeof(bridge_flag), "%s/pieces/system/last_bridged.txt", project_root);
    char last[64] = "";
    read_kv(bridge_flag, "piece", last, sizeof(last));

    if (strcmp(cur, piece) != 0 || strcmp(last, piece) != 0) {
        write_bridge(piece);
        if (strcmp(piece, "home") != 0)
            run_scan(piece);
        else
            run_scan("all"); /* keep catalogs warm */
        write_kv(bridge_flag, "piece", piece);
        clear_relay();
        bump_screen();
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    int key = atoi(argv[1]);
    if (key == 0) {
        idle_bridge();
        return 0;
    }

    char layout[MAX_PATH], piece[64];
    read_layout(layout, sizeof(layout));
    snprintf(piece, sizeof(piece), "%s", piece_from_layout(layout));
    if (strcmp(piece, "home") == 0) {
        /* home has no KEY methods — href only */
        return 0;
    }

    MenuItem items[MAX_MENU];
    int count = load_menu(piece, items, MAX_MENU);

    /* method_idx starts at 2 for non-loader pieces → KEY:2 = first METHOD */
    int resolved = 0;
    if (key >= '0' && key <= '9') resolved = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved = key - 1;

    if (resolved < 1 || resolved > count) return 0;
    const char *cmd = items[resolved - 1].command;

    if (strcmp(cmd, "NOOP") == 0) {
        set_message("(empty section)");
        bump_screen();
        return 0;
    }
    if (strcmp(cmd, "REFRESH") == 0) {
        run_scan(piece);
        set_message("Catalog refreshed.");
        bump_screen();
        return 0;
    }
    if (strncmp(cmd, "RUN:", 4) == 0) {
        /* RUN:section:relpath */
        const char *rest = cmd + 4;
        char sec[64], rel[MAX_PATH];
        sec[0] = rel[0] = '\0';
        const char *colon = strchr(rest, ':');
        if (colon) {
            size_t sl = (size_t)(colon - rest);
            if (sl >= sizeof(sec)) sl = sizeof(sec) - 1;
            memcpy(sec, rest, sl);
            sec[sl] = '\0';
            snprintf(rel, sizeof(rel), "%s", colon + 1);
        } else {
            snprintf(sec, sizeof(sec), "%s", piece);
            snprintf(rel, sizeof(rel), "%s", rest);
        }
        do_run(sec, rel);
        return 0;
    }
    if (strncmp(cmd, "INFO:", 5) == 0) {
        do_info(cmd + 5);
        return 0;
    }

    return 0;
}
