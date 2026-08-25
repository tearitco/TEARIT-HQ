// tools/cmd_exec.c - Fork/exec command runner
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#ifndef SANDBOX_SCOPE_DEFAULT_ROOT
#define SANDBOX_SCOPE_DEFAULT_ROOT "projects/gem-xo/sandbox"
#endif

static int run_sandbox_scope_op(const char *action, const char *arg1, const char *arg2, char *out, size_t out_size) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return 0;

    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execl("ops/+x/sandbox_scope_op", "sandbox_scope_op", action, arg1 ? arg1 : "", arg2 ? arg2 : "", NULL);
        _exit(127);
    }

    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
    }

    close(pipefd[1]);
    ssize_t n = read(pipefd[0], out, out_size - 1);
    if (n < 0) n = 0;
    out[n] = '\0';
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    return 1;
}

static void trim_eol(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void sandbox_scope_load_root(const char *config_path, char *out, size_t out_size) {
    if (!run_sandbox_scope_op("load-root", config_path ? config_path : "", SANDBOX_SCOPE_DEFAULT_ROOT, out, out_size)) {
        snprintf(out, out_size, "%s", SANDBOX_SCOPE_DEFAULT_ROOT);
    }
    trim_eol(out);
}

static int sandbox_scope_is_within_root(const char *path, const char *root) {
    char out[32] = "0";
    if (!run_sandbox_scope_op("is-within-root", path ? path : "", root ? root : "", out, sizeof(out))) return 0;
    trim_eol(out);
    return atoi(out) != 0;
}

static int token_is_path_like(const char *token) {
    if (!token || !*token) return 0;
    return strchr(token, '/') != NULL || token[0] == '.' || token[0] == '~';
}

static void strip_shell_quotes(char *token) {
    size_t len = strlen(token);
    while (len > 0 && (token[0] == '"' || token[0] == '\'')) {
        memmove(token, token + 1, len);
        len--;
    }
    while (len > 0 && (token[len - 1] == '"' || token[len - 1] == '\'' || token[len - 1] == ';' || token[len - 1] == ',')) {
        token[len - 1] = '\0';
        len--;
    }
}

static int is_safe(const char *cmd) {
    char sandbox_root[PATH_MAX];
    sandbox_scope_load_root("config/context.txt", sandbox_root, sizeof(sandbox_root));

    char copy[4096];
    snprintf(copy, sizeof(copy), "%s", cmd);
    char *saveptr = NULL;
    for (char *token = strtok_r(copy, " \t\r\n;&|()<>", &saveptr); token; token = strtok_r(NULL, " \t\r\n;&|()<>", &saveptr)) {
        strip_shell_quotes(token);
        if (!token_is_path_like(token)) continue;
        if (!sandbox_scope_is_within_root(token, sandbox_root)) return 0;
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: cmd_exec <command>\n"); return 1; }
    
    if (!is_safe(argv[1])) {
        fprintf(stderr, "Error: Command references a path outside the sandbox root.\n");
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
    if (pipe(pipefd) != 0) {
        perror("pipe");
        return 1;
    }
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
    ssize_t n = read(pipefd[0], buf, sizeof(buf)-1);
    if (n < 0) n = 0;
    buf[n] = '\0';
    close(pipefd[0]);
    
    printf("STDOUT/ERR:\n%s\n", buf);
    return 0;
}
