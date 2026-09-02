/* runtime_kill - SIGTERM/SIGKILL or --soft quit_request
 * Usage: runtime_kill.+x <runtime_root> <pid> [--soft]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

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
        fprintf(stderr, "Usage: runtime_kill.+x <runtime_root> <pid> [--soft]\n");
        return 1;
    }
    const char *root = argv[1];
    const char *want_pid = argv[2];
    int soft = (argc >= 4 && strcmp(argv[3], "--soft") == 0);
    int pid = atoi(want_pid);

    char pdir[PATH_BUF], entry[PATH_BUF] = "", session[PATH_BUF] = "";
    snprintf(pdir, sizeof(pdir), "%s/processes", root);
    DIR *d = opendir(pdir);
    if (!d) return 1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char meta[PATH_BUF], pid_s[32];
        snprintf(meta, sizeof(meta), "%s/%s/meta.pdl", pdir, e->d_name);
        read_kv(meta, "pid", pid_s, sizeof(pid_s));
        if (strcmp(pid_s, want_pid) == 0) {
            snprintf(entry, sizeof(entry), "%s/%s", pdir, e->d_name);
            read_kv(meta, "session_path", session, sizeof(session));
            break;
        }
    }
    closedir(d);

    if (soft) {
        if (!session[0]) {
            fprintf(stderr, "soft kill: no session_path\n");
            return 1;
        }
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/pieces/system'", session);
        if (system(cmd) != 0) return 1;
        char qp[PATH_BUF];
        snprintf(qp, sizeof(qp), "%s/pieces/system/quit_request.txt", session);
        FILE *f = fopen(qp, "w");
        if (f) { fputs("1\n", f); fclose(f); }
        printf("SOFT_QUIT session=%s\n", session);
        return 0;
    }

    if (pid > 0) {
        kill(pid, SIGTERM);
        usleep(100000);
        if (kill(pid, 0) == 0 || (errno != ESRCH))
            kill(pid, SIGKILL);
    }
    if (entry[0]) {
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", entry);
        system(cmd);
    }
    printf("KILL pid=%s removed_registry=%s\n", want_pid, entry[0] ? "yes" : "no");
    return 0;
}
