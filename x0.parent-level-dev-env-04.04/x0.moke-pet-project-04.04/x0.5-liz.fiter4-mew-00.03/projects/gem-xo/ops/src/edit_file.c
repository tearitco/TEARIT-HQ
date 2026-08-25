// tools/edit_file.c - Self-contained surgical file editor
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
    if (argc < 4) { fprintf(stderr, "Usage: edit_file <path> <search> <replace>\n"); return 1; }
    const char* path = argv[1];

    if (!is_safe(path)) {
        fprintf(stderr, "Error: Path '%s' is outside the sandbox root.\n", path);
        return 1;
    }

    const char* search = argv[2];
    const char* replace = argv[3];

    FILE* f = fopen(path, "r");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* data = malloc(size + 1);
    size_t read_size = fread(data, 1, size, f);
    data[read_size] = '\0';
    fclose(f);

    if (strlen(search) == 0) { fprintf(stderr, "Error: Empty search string.\n"); free(data); return 1; }

    int count = 0;
    char* pos = data;
    while ((pos = strstr(pos, search)) != NULL) {
        count++;
        pos += strlen(search);
    }

    if (count == 0) {
        fprintf(stderr, "Error: Search block not found.\n");
        free(data); return 1;
    }

    size_t new_size = size + (strlen(replace) - strlen(search)) * count;
    char* new_data = malloc(new_size + 1);
    
    char* src = data;
    char* dst = new_data;
    while ((pos = strstr(src, search)) != NULL) {
        size_t len = pos - src;
        memcpy(dst, src, len);
        dst += len;
        memcpy(dst, replace, strlen(replace));
        dst += strlen(replace);
        src = pos + strlen(search);
    }
    strcpy(dst, src);

    f = fopen(path, "w");
    if (!f) { perror("fopen write"); free(data); free(new_data); return 1; }
    fwrite(new_data, 1, new_size, f);
    fclose(f);

    printf("Successfully edited %s (%d replacement%s).\n", path, count, count == 1 ? "" : "s");
    free(data); free(new_data);
    return 0;
}
