#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
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

    char *input_arg = NULL;
    if (asprintf(&input_arg, "@%s", input_file) == -1) return 1;

    // Using curl to connect
    // 600s: this hardware's inference latency is slow and variable (observed
    // up to ~118s for a single completion with the current persona length),
    // so leave generous headroom above worst-case observed latency.
    char *curl_args[] = {"curl", "-s", "--max-time", "600", "-H", "Content-Type: application/json", url, "-d", input_arg, "-o", output_file, NULL};

#ifdef _WIN32
    /* Windows counterpart of the fork/execvp/waitpid trio below. curl's
       output goes to a file via -o rather than to a pipe, so unlike the
       manager's run_tool() this needs no stdout capture -- just spawn it, wait
       for it, and hand back the exit code, which is what the caller's
       WEXITSTATUS(status) check was reading. PATH is walked explicitly
       because CreateProcess does not search PATH for a bare name the way
       execvp does. */
    {
        char exe[MAX_PATH];
        char cmd[4096];
        char *dup_path, *saveptr = NULL, *dir;
        const char *path_env = getenv("PATH");
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        DWORD exit_code = 127;
        int found = 0;

        exe[0] = '\0';
        if (path_env && asprintf(&dup_path, "%s", path_env) != -1) {
            for (dir = strtok_r(dup_path, ";", &saveptr); dir; dir = strtok_r(NULL, ";", &saveptr)) {
                char *cand = NULL;
                if (asprintf(&cand, "%s/curl.exe", dir) != -1) {                    if (GetFileAttributesA(cand) != INVALID_FILE_ATTRIBUTES) {
                        snprintf(exe, sizeof(exe), "%s", cand);
                        found = 1;
                    }
                    free(cand);
                    if (found) break;
                }
            }
            free(dup_path);
        }
        if (!found) { free(input_arg); return 127; }

        snprintf(cmd, sizeof(cmd), "\"%s\"", exe);
        for (int i = 1; curl_args[i]; i++) {
            size_t used = strlen(cmd);
            /* Quote every argument: the URL and the -o path routinely contain
               characters (spaces, ':', '\\') that would otherwise be re-split
               by the child's command-line parser. */
            if (used + 3 < sizeof(cmd)) cmd[used++] = ' ';
            if (used + 1 < sizeof(cmd)) cmd[used++] = '"';
            for (const char *q = curl_args[i]; *q && used + 2 < sizeof(cmd); q++) {
                if (*q == '"') { cmd[used++] = '\\'; if (used + 1 >= sizeof(cmd)) break; }
                cmd[used++] = *q;
            }
            if (used + 1 < sizeof(cmd)) cmd[used++] = '"';
            cmd[used] = '\0';
        }

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));
        if (CreateProcessA(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &exit_code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        free(input_arg);
        return (int)exit_code;
    }
#else
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
