// tools/cmd_exec.c - Fork/exec command runner
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int get_sandbox_depth() {
    int depth = 1;
    FILE* f = fopen("config/context.txt", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "sandbox_depth=", 14) == 0) {
                depth = atoi(line + 14);
                break;
            }
        }
        fclose(f);
    }
    return depth;
}

static int is_safe(const char* cmd) {
    int max_depth = get_sandbox_depth();
    int count = 0;
    const char* p = cmd;
    while ((p = strstr(p, "../"))) {
        count++;
        p += 3;
    }
    return count <= max_depth;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: cmd_exec <command>\n"); return 1; }
    
    if (!is_safe(argv[1])) {
        fprintf(stderr, "Error: Command exceeds sandbox depth ('../' limit).\n");
        return 1;
    }

    // TPMOS: Check YOLO marker file using access()
    int yolo_mode = (access("config/yolo.flag", F_OK) == 0);
    
    if (!yolo_mode) {
        printf("[SAFEGUARD] Run '%s'? (y/n): ", argv[1]);
        char confirm[4];
        if (!fgets(confirm, sizeof(confirm), stdin) || confirm[0] != 'y') {
            printf("Command aborted by user.\n"); return 0;
        }
    }
    
    printf("\033[90m[Action: exec_cmd]\033[0m\n");
    fflush(stdout);
    
    int pipefd[2];
    pipe(pipefd);
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", argv[1], NULL);
        _exit(127);
    }
    close(pipefd[1]);
    waitpid(pid, NULL, 0);
    
    char buf[4096];
    size_t n = read(pipefd[0], buf, sizeof(buf)-1);
    buf[n] = '\0';
    close(pipefd[0]);
    
    printf("STDOUT/ERR:\n%s\n", buf);
    return 0;
}