#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

// Usage: ./connect_op <api_url> <input_file> <output_file>
int main(int argc, char *argv[]) {
    if (argc < 4) return 1;
    char *url = argv[1];
    char *input_file = argv[2];
    char *output_file = argv[3];

    /* 600s: this hardware's inference latency is slow and variable (observed
     * up to ~118s for a single completion with the current persona length),
     * so leave generous headroom above worst-case observed latency. */
#ifdef _WIN32
    /* Windows: system(curl ...) — no fork/execvp. curl must be on PATH
     * (Git for Windows or MSYS2 curl). */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "curl -s --max-time 600 -H \"Content-Type: application/json\" \"%s\" -d \"@%s\" -o \"%s\"",
             url, input_file, output_file);
    int rc = system(cmd);
    return (rc == 0) ? 0 : 1;
#else
    char *input_arg = NULL;
    if (asprintf(&input_arg, "@%s", input_file) == -1) return 1;

    char *curl_args[] = {"curl", "-s", "--max-time", "600", "-H", "Content-Type: application/json", url, "-d", input_arg, "-o", output_file, NULL};

    pid_t pid = fork();
    if (pid == 0) {
        execvp("curl", curl_args);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    free(input_arg);
    return WEXITSTATUS(status);
#endif
}
