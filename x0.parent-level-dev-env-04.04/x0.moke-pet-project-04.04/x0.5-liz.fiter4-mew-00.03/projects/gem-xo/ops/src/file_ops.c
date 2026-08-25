// tools/file_ops.c - Self-contained file operations
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

static int is_safe(const char *path) {
    char sandbox_root[4096];
    if (!run_sandbox_scope_op("load-root", "config/context.txt", SANDBOX_SCOPE_DEFAULT_ROOT, sandbox_root, sizeof(sandbox_root))) {
        snprintf(sandbox_root, sizeof(sandbox_root), "%s", SANDBOX_SCOPE_DEFAULT_ROOT);
    }
    trim_eol(sandbox_root);

    char out[32] = "0";
    if (!run_sandbox_scope_op("is-within-root", path, sandbox_root, out, sizeof(out))) return 0;
    trim_eol(out);
    return atoi(out) != 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: file_ops [read|write] <file> [content]\n"); return 1; }
    const char* action = argv[1];
    const char* file = argv[2];

    if (!is_safe(file)) {
        fprintf(stderr, "Error: Path '%s' is outside the sandbox root.\n", file);
        return 1;
    }

    if (strcmp(action, "read") == 0) {
        FILE* f = fopen(file, "r");
        if (!f) { fprintf(stderr, "Error: Cannot open %s\n", file); return 1; }
        char buf[8192]; size_t n = fread(buf, 1, sizeof(buf)-1, f); buf[n] = '\0';
        fclose(f);
        printf("%s", buf);
        return 0;
    }
    
    if (strcmp(action, "write") == 0 && argc >= 4) {
        FILE* f = fopen(file, "w");
        if (!f) { perror("fopen"); return 1; }
        fputs(argv[3], f);
        fclose(f);
        printf("Written %lu bytes to %s\n", strlen(argv[3]), file);
        return 0;
    }
    
    fprintf(stderr, "Invalid action or missing arguments\n");
    return 1;
}
