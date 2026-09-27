// tools/cmd_exec.c - Fork/exec command runner
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define access(p, m) _access(p, m)
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

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
    
#ifdef _WIN32
    /* Windows counterpart of the pipe()+fork()+dup2() block below. The shell
       is cmd.exe rather than /bin/sh, so command syntax is whatever the
       Windows shell accepts -- a real behavioural difference from the POSIX
       build for any command using POSIX shell syntax, and unavoidable.

       Unlike the POSIX original this reads the pipe to EOF BEFORE waiting on
       the child. The POSIX order (waitpid, then a single read()) can deadlock
       if the child fills the 64K pipe buffer before exiting, and its single
       read() call silently truncates anything longer than one buffer's worth.
       Reading first is both deadlock-free and complete; the output format is
       unchanged. The read end must have HANDLE_FLAG_INHERIT cleared or the
       child holds a duplicate and the loop never sees EOF. */
    {
        SECURITY_ATTRIBUTES sa;
        HANDLE rd, wr;
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        char cmd[8192];
        char buf[4096];
        char out[65536];
        size_t total = 0;
        DWORD n;

        memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        if (!CreatePipe(&rd, &wr, &sa, 0)) { return 1; }
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

        snprintf(cmd, sizeof(cmd), "cmd.exe /c \"%s\"", argv[1]);

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = wr;
        si.hStdError = wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        memset(&pi, 0, sizeof(pi));

        if (!CreateProcessA("cmd.exe", cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(rd); CloseHandle(wr);
            return 1;
        }
        CloseHandle(wr);
        for (;;) {
            if (!ReadFile(rd, buf, (DWORD)sizeof(buf) - 1, &n, NULL) || n == 0) break;
            if (total + n < sizeof(out) - 1) { memcpy(out + total, buf, n); total += n; }
        }
        out[total] = '\0';
        CloseHandle(rd);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        printf("STDOUT/ERR:\n%s\n", out);
        return 0;
    }
#else
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
#endif
}