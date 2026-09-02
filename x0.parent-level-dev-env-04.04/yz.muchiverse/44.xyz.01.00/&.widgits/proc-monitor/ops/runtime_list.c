/* runtime_list - list processes; optional --gc dead pids
 * Usage: runtime_list.+x <runtime_root> [--gc]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
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

static int pid_alive(int pid) {
    if (pid <= 0) return 0;
    if (kill(pid, 0) == 0) return 1;
    return errno != ESRCH ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: runtime_list.+x <runtime_root> [--gc]\n");
        return 1;
    }
    const char *root = argv[1];
    int do_gc = (argc >= 3 && strcmp(argv[2], "--gc") == 0);
    char pdir[PATH_BUF];
    snprintf(pdir, sizeof(pdir), "%s/processes", root);
    DIR *d = opendir(pdir);
    if (!d) {
        printf("list: 0 processes\n");
        return 0;
    }
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char meta[PATH_BUF];
        snprintf(meta, sizeof(meta), "%s/%s/meta.pdl", pdir, e->d_name);
        struct stat st;
        if (stat(meta, &st) != 0) continue;
        char pid_s[32], project[128], kind[64], gl[8], display[160], session[PATH_BUF];
        read_kv(meta, "pid", pid_s, sizeof(pid_s));
        read_kv(meta, "project_id", project, sizeof(project));
        read_kv(meta, "kind", kind, sizeof(kind));
        read_kv(meta, "gl_window", gl, sizeof(gl));
        read_kv(meta, "display_name", display, sizeof(display));
        read_kv(meta, "session_path", session, sizeof(session));
        int pid = atoi(pid_s);
        int alive = pid_alive(pid);
        if (!alive && do_gc) {
            char cmd[PATH_BUF + 64];
            snprintf(cmd, sizeof(cmd), "rm -rf '%s/%s'", pdir, e->d_name);
            if (system(cmd) == 0)
                printf("GC removed %s (dead pid %d)\n", e->d_name, pid);
            continue;
        }
        printf("%s\tpid=%d\talive=%d\tgl=%s\tkind=%s\tproject=%s\tname=%s\tsession=%s\n",
               e->d_name, pid, alive, gl[0] ? gl : "?",
               kind[0] ? kind : "?", project[0] ? project : "?",
               display[0] ? display : "?", session[0] ? session : "?");
        n++;
    }
    closedir(d);
    printf("list: %d processes\n", n);
    return 0;
}
