/* runtime_focus - write current_focus.txt
 * Usage: runtime_focus.+x <runtime_root> <pid>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_BUF 4352

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *k = p1 + 1;
        while (*k == ' ' || *k == '\t') k++;
        char *ke = k + strlen(k);
        while (ke > k && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
        *ke = '\0';
        if (strcmp(k, key) != 0) continue;
        char *v = p2 + 1;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        snprintf(out, out_sz, "%s", v);
        break;
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: runtime_focus.+x <runtime_root> <pid>\n");
        return 1;
    }
    const char *root = argv[1];
    const char *want_pid = argv[2];
    char pdir[PATH_BUF];
    snprintf(pdir, sizeof(pdir), "%s/processes", root);
    DIR *d = opendir(pdir);
    if (!d) return 1;
    char found_meta[PATH_BUF] = "";
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char meta[PATH_BUF], pid_s[32];
        snprintf(meta, sizeof(meta), "%s/%s/meta.pdl", pdir, e->d_name);
        read_kv(meta, "pid", pid_s, sizeof(pid_s));
        if (strcmp(pid_s, want_pid) == 0) {
            snprintf(found_meta, sizeof(found_meta), "%s", meta);
            break;
        }
    }
    closedir(d);
    if (!found_meta[0]) {
        fprintf(stderr, "focus: pid %s not in registry\n", want_pid);
        return 1;
    }
    char project[128], session[PATH_BUF], kind[64], display[160], inbox[PATH_BUF];
    read_kv(found_meta, "project_id", project, sizeof(project));
    read_kv(found_meta, "session_path", session, sizeof(session));
    read_kv(found_meta, "kind", kind, sizeof(kind));
    read_kv(found_meta, "display_name", display, sizeof(display));
    snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", session);

    char mk[PATH_BUF];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", root);
    if (system(mk) != 0) return 1;

    char outp[PATH_BUF];
    snprintf(outp, sizeof(outp), "%s/current_focus.txt", root);
    FILE *f = fopen(outp, "w");
    if (!f) return 1;
    fprintf(f, "pid=%s\n", want_pid);
    fprintf(f, "project_id=%s\n", project);
    fprintf(f, "session_path=%s\n", session);
    fprintf(f, "kind=%s\n", kind);
    fprintf(f, "display_name=%s\n", display);
    fprintf(f, "inbox_path=%s\n", inbox);
    fclose(f);
    printf("FOCUS pid=%s project=%s\n", want_pid, project);
    return 0;
}
