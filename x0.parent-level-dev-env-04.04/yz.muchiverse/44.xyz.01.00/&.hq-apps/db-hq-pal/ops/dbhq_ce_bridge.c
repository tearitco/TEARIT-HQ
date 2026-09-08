/* dbhq_ce_bridge.c — Common Events tab uses the live events-hq
 * projector/manager (same compile chain as entity events).
 *
 * Polls db-hq-pal state/active.pdl. When TAG=CE and a name is
 * selected, starts khtpm_events_hq_manager + evhq_projector on
 * common_events/<name>/event_pkg (no second renderer window) and
 * copies that pkg's .hq_manager/ui.txt into state/ui_ce.txt with
 * row_* renamed to cmd_* so it does not clobber the sidebar list.
 *
 * argv: house_root  xhtpm_pkg_dir
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char g_house[PATH_MAX], g_pkg[PATH_MAX];

static void read_pdl_key(const char *path, const char *key, char *out, size_t n) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SECTION", 7) == 0) continue;
        char *p = line;
        char *b1 = strchr(p, '|'); if (!b1) continue;
        char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
        char k[64];
        size_t kl = (size_t)(b2 - (b1 + 1));
        while (kl && (b1[1] == ' ' || b1[1] == '\t')) { b1++; kl--; }
        if (kl >= sizeof(k)) kl = sizeof(k) - 1;
        memcpy(k, b1 + 1, kl); k[kl] = 0;
        while (kl && (k[kl-1]==' '||k[kl-1]=='\t')) k[--kl] = 0;
        if (strcmp(k, key) != 0) continue;
        char *v = b2 + 1;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = 0;
        snprintf(out, n, "%s", v);
        break;
    }
    fclose(f);
}

static void ce_name_at(const char *list, int sel, char *out, size_t n) {
    out[0] = 0;
    FILE *f = fopen(list, "r");
    if (!f) return;
    char line[256];
    int i = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        if (i == sel) { snprintf(out, n, "%s", line); break; }
        i++;
    }
    fclose(f);
}

static int cmdline_has(pid_t pid, const char *needle) {
    char p[64], buf[8192];
    snprintf(p, sizeof(p), "/proc/%d/cmdline", (int)pid);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    for (size_t i = 0; i < n; i++) if (buf[i] == 0) buf[i] = '\n';
    return strstr(buf, needle) != NULL;
}

static int running_on_pkg(const char *bin_substr, const char *pkg) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *de;
    int hit = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] < '1' || de->d_name[0] > '9') continue;
        pid_t pid = (pid_t)atoi(de->d_name);
        if (pid <= 0) continue;
        if (!cmdline_has(pid, bin_substr)) continue;
        if (cmdline_has(pid, pkg) || (strstr(bin_substr, "evhq_projector") && cmdline_has(pid, pkg))) {
            hit = 1; break;
        }
        /* projector is env-only; match via /proc/pid/environ */
        if (strstr(bin_substr, "evhq_projector")) {
            char ep[64], env[16384];
            snprintf(ep, sizeof(ep), "/proc/%d/environ", (int)pid);
            FILE *f = fopen(ep, "r");
            if (f) {
                size_t n = fread(env, 1, sizeof(env) - 1, f);
                fclose(f);
                env[n] = 0;
                for (size_t i = 0; i < n; i++) if (env[i] == 0) env[i] = '\n';
                if (strstr(env, pkg)) { hit = 1; break; }
            }
        }
    }
    closedir(d);
    return hit;
}

static void ensure_ce_backend(const char *evpkg, const char *label) {
    char mgr[PATH_MAX], proj[PATH_MAX], cmd[PATH_MAX * 4];
    snprintf(mgr, sizeof(mgr),
             "%s/&.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x", g_house);
    snprintf(proj, sizeof(proj),
             "%s/&.widgits/events-hq/ops/+x/evhq_projector.+x", g_house);
    mkdir(evpkg, 0755);
    char hq[PATH_MAX];
    snprintf(hq, sizeof(hq), "%s/.hq_manager", evpkg);
    mkdir(hq, 0755);
    char lb[PATH_MAX];
    snprintf(lb, sizeof(lb), "%s/label.txt", hq);
    FILE *f = fopen(lb, "w");
    if (f) { fprintf(f, "%s\n", label); fclose(f); }

    if (access(mgr, X_OK) == 0 && !running_on_pkg("khtpm_events_hq_manager", evpkg)) {
        snprintf(cmd, sizeof(cmd),
                 "setsid nohup '%s' '%s' '%s' '%s' >/tmp/dbhq-ce-mgr-%s.log 2>&1 < /dev/null &",
                 mgr, g_house, evpkg, label, label);
        system(cmd);
    }
    if (access(proj, X_OK) == 0 && !running_on_pkg("evhq_projector", evpkg)) {
        snprintf(cmd, sizeof(cmd),
                 "KHTPM_ARG3='%s' KHTPM_HOUSE='%s' setsid nohup '%s' '%s' '%s' "
                 ">/tmp/dbhq-ce-proj-%s.log 2>&1 < /dev/null &",
                 evpkg, g_house, proj, g_house, g_pkg, label);
        system(cmd);
    }
}

static void write_ui_ce(const char *src_ui, const char *ce_pkg, const char *label) {
    char dst[PATH_MAX], tmp[PATH_MAX];
    snprintf(dst, sizeof(dst), "%s/state/ui_ce.txt", g_pkg);
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *out = fopen(tmp, "w");
    if (!out) return;
    fprintf(out, "ce_pkg=%s\n", ce_pkg);
    fprintf(out, "ce_label=%s\n", label);
    FILE *in = fopen(src_ui, "r");
    if (in) {
        char line[1024];
        while (fgets(line, sizeof(line), in)) {
            if (!strncmp(line, "row_", 4)) {
                fprintf(out, "cmd_%s", line + 4);
            } else if (!strncmp(line, "rows_count=", 11)) {
                fprintf(out, "n_cmds=%s", line + 11);
            } else if (!strncmp(line, "list_title=", 11)) {
                fprintf(out, "ce_list_title=%s", line + 11);
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    } else {
        fprintf(out, "n_cmds=0\npicker_open=0\nfields_open=0\nlist_open=1\nempty_list=1\n");
        fprintf(out, "trigger=(none)\nn_picker=0\nn_fields=0\n");
        fprintf(out, "ce_list_title=Commands\ndetail_hint=\n");
    }
    fclose(out);
    rename(tmp, dst);
}

static void write_idle_ui_ce(void) {
    char dst[PATH_MAX], tmp[PATH_MAX];
    snprintf(dst, sizeof(dst), "%s/state/ui_ce.txt", g_pkg);
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *out = fopen(tmp, "w");
    if (!out) return;
    fprintf(out,
            "ce_pkg=\nce_label=\nn_cmds=0\npicker_open=0\nfields_open=0\n"
            "list_open=0\nempty_list=1\ntrigger=\nn_picker=0\nn_fields=0\n"
            "ce_list_title=\ndetail_hint=\nis_scratch=0\nis_blueprints=0\n");
    fclose(out);
    rename(tmp, dst);
}

int main(int argc, char **argv) {
    snprintf(g_house, sizeof(g_house), "%s",
             argc > 1 ? argv[1] : (getenv("KHTPM_HOUSE") ? getenv("KHTPM_HOUSE") : "."));
    snprintf(g_pkg, sizeof(g_pkg), "%s",
             argc > 2 ? argv[2] : (getenv("KHTPM_PKG") ? getenv("KHTPM_PKG") : "."));
    char st[PATH_MAX];
    snprintf(st, sizeof(st), "%s/state", g_pkg);
    mkdir(st, 0755);

    for (;;) {
        char active[PATH_MAX], tag[64], sel_s[32];
        snprintf(active, sizeof(active), "%s/state/active.pdl", g_pkg);
        read_pdl_key(active, "tag", tag, sizeof(tag));
        read_pdl_key(active, "sel", sel_s, sizeof(sel_s));
        if (strcmp(tag, "CE") != 0) {
            write_idle_ui_ce();
            usleep(400000);
            continue;
        }
        char list[PATH_MAX], name[128];
        snprintf(list, sizeof(list), "%s/#.desktop/db_hq_common_events.state.txt", g_house);
        ce_name_at(list, atoi(sel_s), name, sizeof(name));
        if (!name[0]) {
            write_idle_ui_ce();
            usleep(400000);
            continue;
        }
        char evpkg[PATH_MAX], src[PATH_MAX];
        snprintf(evpkg, sizeof(evpkg), "%s/common_events/%s/event_pkg", g_house, name);
        ensure_ce_backend(evpkg, name);
        snprintf(src, sizeof(src), "%s/.hq_manager/ui.txt", evpkg);
        write_ui_ce(src, evpkg, name);
        usleep(400000);
    }
    return 0;
}
