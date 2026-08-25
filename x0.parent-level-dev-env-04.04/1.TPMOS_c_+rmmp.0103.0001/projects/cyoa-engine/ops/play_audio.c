#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

char project_root[1024] = ".";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "project_root") == 0) {
                    char *v = eq + 1; v[strcspn(v, "\n\r")] = 0;
                    strncpy(project_root, v, 1023);
                }
            }
        }
        fclose(kvp);
    }
}

void kill_previous() {
    char pid_path[1024];
    snprintf(pid_path, sizeof(pid_path), "%s/projects/cyoa-engine/pieces/audio.pid", project_root);
    FILE *fp = fopen(pid_path, "r");
    if (fp) {
        pid_t pid;
        if (fscanf(fp, "%d", &pid) == 1) kill(pid, SIGTERM);
        fclose(fp);
        remove(pid_path);
    }
}

int main(int argc, char *argv[]) {
    resolve_paths();
    if (argc < 2) return 1;
    if (strcmp(argv[1], "--stop") == 0) { kill_previous(); return 0; }
    kill_previous();
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/mpg123", "mpg123", "-q", argv[1], (char *)NULL);
        exit(1);
    } else if (pid > 0) {
        char pid_path[1024];
        snprintf(pid_path, sizeof(pid_path), "%s/projects/cyoa-engine/pieces/audio.pid", project_root);
        FILE *fp = fopen(pid_path, "w");
        if (fp) { fprintf(fp, "%d\n", pid); fclose(fp); }
    }
    return 0;
}
