/* start_scan - regenerate section piece.pdl catalogs (TPMOS loader.pdl parity).
 *
 * Usage:
 *   start_scan.+x              # scan all sections
 *   start_scan.+x <section>    # system | widgets | apps | store | all
 *
 * Roots and skip lists come from config/start_button.pdl (or install copy).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_ENTRIES 64
#define MAX_NAME 512

static char project_root[MAX_PATH] = ".";   /* session (writable piece.pdl) */
static char install_root[MAX_PATH] = ".";   /* *.START_BUTTON install — config + relative roots */
static char scan_root[MAX_PATH] = "..";
static char widgets_root[MAX_PATH] = "../&.widgits";
static char apps_root[MAX_PATH] = "../@.apps";
static char store_root[MAX_PATH] = "../@.app-store";
static int scan_depth = 2;
static int require_button_sh = 1;
static int require_project_pdl = 0;
static int max_entries = MAX_ENTRIES;
static char skip_names[2048] =
    "*.START_BUTTON,@.apps,@.app-store,&.widgits,&.widgets";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *inst = getenv("PRISC_INSTALL_ROOT");
    if (inst && inst[0]) {
        snprintf(install_root, sizeof(install_root), "%s", inst);
    } else {
        /* Prefer config symlink target's parent (session → install). */
        char cfg[PATH_BUF], resolved[MAX_PATH];
        snprintf(cfg, sizeof(cfg), "%s/config/start_button.pdl", project_root);
        if (realpath(cfg, resolved)) {
            /* .../install/config/start_button.pdl → strip 2 components */
            char *slash = strrchr(resolved, '/');
            if (slash) {
                *slash = '\0'; /* /config */
                slash = strrchr(resolved, '/');
                if (slash) {
                    *slash = '\0'; /* install root */
                    snprintf(install_root, sizeof(install_root), "%s", resolved);
                }
            }
        } else {
            snprintf(install_root, sizeof(install_root), "%s", project_root);
        }
    }
}

static void read_kv_file(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        /* STATE | key | value  OR  key=value */
        char *p = line;
        if (strncmp(p, "STATE", 5) == 0) {
            char *p1 = strchr(p, '|');
            if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|');
            if (!p2) continue;
            *p2 = '\0';
            char *k = p1 + 1;
            while (*k == ' ' || *k == '\t') k++;
            char *ke = k + strlen(k) - 1;
            while (ke > k && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';
            if (strcmp(k, key) != 0) continue;
            char *v = p2 + 1;
            while (*v == ' ' || *v == '\t') v++;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void load_config(void) {
    char path[PATH_BUF], val[MAX_LINE];
    /* Prefer install_root/config (absolute roots relative to install). */
    snprintf(path, sizeof(path), "%s/config/start_button.pdl", install_root);
    if (access(path, F_OK) != 0)
        snprintf(path, sizeof(path), "%s/config/start_button.pdl", project_root);

    read_kv_file(path, "scan_root", val, sizeof(val));
    if (val[0]) snprintf(scan_root, sizeof(scan_root), "%s", val);
    read_kv_file(path, "widgets_root", val, sizeof(val));
    if (val[0]) snprintf(widgets_root, sizeof(widgets_root), "%s", val);
    read_kv_file(path, "apps_root", val, sizeof(val));
    if (val[0]) snprintf(apps_root, sizeof(apps_root), "%s", val);
    read_kv_file(path, "store_root", val, sizeof(val));
    if (val[0]) snprintf(store_root, sizeof(store_root), "%s", val);
    read_kv_file(path, "scan_depth", val, sizeof(val));
    if (val[0]) scan_depth = atoi(val);
    if (scan_depth < 1) scan_depth = 1;
    if (scan_depth > 3) scan_depth = 3;
    read_kv_file(path, "require_button_sh", val, sizeof(val));
    if (val[0]) require_button_sh = atoi(val);
    read_kv_file(path, "require_project_pdl", val, sizeof(val));
    if (val[0]) require_project_pdl = atoi(val);
    read_kv_file(path, "max_entries", val, sizeof(val));
    if (val[0]) max_entries = atoi(val);
    if (max_entries < 1) max_entries = 8;
    if (max_entries > MAX_ENTRIES) max_entries = MAX_ENTRIES;
    read_kv_file(path, "skip_names", val, sizeof(val));
    if (val[0]) snprintf(skip_names, sizeof(skip_names), "%s", val);
}

static int name_skipped(const char *name) {
    if (!name || !name[0]) return 1;
    if (name[0] == '.') return 1;
    if (name[0] == '#') return 1;
    if (strncmp(name, "test-harn", 9) == 0) return 1;
    /* comma-separated skip_names */
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", skip_names);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;
        char *e = tok + strlen(tok) - 1;
        while (e > tok && (*e == ' ' || *e == '\n')) *e-- = '\0';
        if (strcmp(tok, name) == 0) return 1;
    }
    return 0;
}

static void abspath_from(const char *base, const char *rel, char *out, size_t out_sz) {
    if (rel[0] == '/') {
        snprintf(out, out_sz, "%s", rel);
        return;
    }
    /* base is project_root (session); rel like .. or ../@.apps */
    char joined[PATH_BUF];
    snprintf(joined, sizeof(joined), "%s/%s", base, rel);
    if (!realpath(joined, out)) {
        /* realpath fails if missing — keep joined */
        snprintf(out, out_sz, "%s", joined);
    }
}

typedef struct {
    char label[MAX_NAME];
    char relpath[MAX_PATH]; /* relative to section root, or for system: relative to scan_root */
} Entry;

static int entry_cmp(const void *a, const void *b) {
    return strcmp(((const Entry *)a)->label, ((const Entry *)b)->label);
}

static int has_file(const char *dir, const char *name) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/%s", dir, name);
    return access(p, F_OK) == 0;
}

/* Collect direct children (and optional depth-2) under root_abs. */
static int collect_entries(const char *root_abs, int depth, int need_button,
                           int need_pdl, Entry *out, int max_out) {
    DIR *d = opendir(root_abs);
    if (!d) return 0;
    int n = 0;
    struct dirent *ent;
    while (n < max_out && (ent = readdir(d)) != NULL) {
        if (name_skipped(ent->d_name)) continue;
        char child[PATH_BUF * 2];
        snprintf(child, sizeof(child), "%s/%s", root_abs, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        int ok = 1;
        if (need_button && !has_file(child, "button.sh")) ok = 0;
        if (need_pdl && !has_file(child, "project.pdl")) ok = 0;

        if (ok) {
            snprintf(out[n].label, sizeof(out[n].label), "%s", ent->d_name);
            snprintf(out[n].relpath, sizeof(out[n].relpath), "%s", ent->d_name);
            n++;
        }

        /* depth 2: one level of nested button.sh apps (e.g. 0.user-pal/00.login) */
        if (depth >= 2 && n < max_out) {
            DIR *d2 = opendir(child);
            if (!d2) continue;
            struct dirent *e2;
            while (n < max_out && (e2 = readdir(d2)) != NULL) {
                if (name_skipped(e2->d_name)) continue;
                char c2[PATH_BUF * 3];
                snprintf(c2, sizeof(c2), "%s/%s", child, e2->d_name);
                struct stat st2;
                if (stat(c2, &st2) != 0 || !S_ISDIR(st2.st_mode)) continue;
                if (need_button && !has_file(c2, "button.sh")) continue;
                if (need_pdl && !has_file(c2, "project.pdl")) continue;
                /* skip if parent already listed as runnable (avoid dup noise) */
                snprintf(out[n].label, sizeof(out[n].label), "%s/%s", ent->d_name, e2->d_name);
                snprintf(out[n].relpath, sizeof(out[n].relpath), "%s/%s", ent->d_name, e2->d_name);
                n++;
            }
            closedir(d2);
        }
    }
    closedir(d);
    qsort(out, (size_t)n, sizeof(Entry), entry_cmp);
    return n;
}

static void write_section_pdl(const char *section, const Entry *ents, int n, int is_store) {
    char pdl[PATH_BUF];
    snprintf(pdl, sizeof(pdl),
             "%s/projects/start-button/pieces/%s/piece.pdl", project_root, section);
    /* ensure dir */
    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/projects/start-button/pieces/%s", project_root, section);
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    { int _rc = system(cmd); (void)_rc; }

    FILE *f = fopen(pdl, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | %s\n", section);
    fprintf(f, "META         | version            | 1.0\n\n");
    if (n == 0) {
        fprintf(f, "METHOD       | (empty)                  | NOOP\n");
    } else {
        for (int i = 0; i < n; i++) {
            if (is_store)
                fprintf(f, "METHOD       | %s | INFO:%s\n", ents[i].label, ents[i].relpath);
            else
                fprintf(f, "METHOD       | %s | RUN:%s:%s\n",
                        ents[i].label, section, ents[i].relpath);
        }
    }
    fprintf(f, "METHOD       | Refresh list             | REFRESH\n");
    fclose(f);
}

static void scan_section(const char *section) {
    char root_abs[PATH_BUF];
    int need_btn = require_button_sh;
    int need_pdl = require_project_pdl;
    int is_store = 0;
    int depth = 1;

    /* Roots are relative to install_root (*.START_BUTTON), NOT the session. */
    if (strcmp(section, "system") == 0) {
        abspath_from(install_root, scan_root, root_abs, sizeof(root_abs));
        depth = scan_depth;
        need_btn = 1; /* system: must be runnable */
    } else if (strcmp(section, "widgets") == 0) {
        abspath_from(install_root, widgets_root, root_abs, sizeof(root_abs));
        need_btn = 0; /* widgets may be stubs without button.sh yet */
        need_pdl = 0;
    } else if (strcmp(section, "apps") == 0) {
        abspath_from(install_root, apps_root, root_abs, sizeof(root_abs));
        need_btn = 1;
    } else if (strcmp(section, "store") == 0) {
        abspath_from(install_root, store_root, root_abs, sizeof(root_abs));
        need_btn = 0;
        is_store = 1;
    } else {
        return;
    }

    Entry ents[MAX_ENTRIES];
    int n = collect_entries(root_abs, depth, need_btn, need_pdl, ents, max_entries);
    write_section_pdl(section, ents, n, is_store);
    printf("scan %s: %d entries under %s\n", section, n, root_abs);
}

int main(int argc, char **argv) {
    resolve_root();
    load_config();

    const char *sec = (argc >= 2) ? argv[1] : "all";
    if (strcmp(sec, "all") == 0) {
        scan_section("system");
        scan_section("widgets");
        scan_section("apps");
        scan_section("store");
    } else {
        scan_section(sec);
    }
    return 0;
}
