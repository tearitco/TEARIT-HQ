/* orchestrator.c - background service launcher + cascading kill
 * for piececraft-xyz pal-chain sessions.
 * Linux: fork/exec (unchanged shape). Windows: CreateProcessA.
 * Surgical #ifdef _WIN32 — design stays one codepath. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#define usleep(us) Sleep((DWORD)((us) / 1000))
#define getcwd _getcwd
#define getpid _getpid
typedef intptr_t child_pid_t;
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
typedef pid_t child_pid_t;
#endif

static volatile int should_exit = 0;
static volatile int shutdown_done = 0;

static char project_root_path[1024] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root_path, sizeof(project_root_path), "%s", env);
    } else {
        if (!getcwd(project_root_path, sizeof(project_root_path)))
            strncpy(project_root_path, ".", sizeof(project_root_path) - 1);
    }
}

static void resolve_path(char *buf, size_t bufsz, const char *rel) {
    snprintf(buf, bufsz, "%s/%s", project_root_path, rel);
}

static void kill_process_group(void) {
#ifdef _WIN32
    usleep(100000);
#else
    kill(0, SIGTERM);
    usleep(100000);
#endif
}

static void log_pid(int pid, const char* name) {
    FILE* f = fopen("pieces/os/proc_list.txt", "a");
    if (!f) return;
#ifndef _WIN32
    flock(fileno(f), LOCK_EX);
#endif
    fprintf(f, "%d %s\n", pid, name);
    fflush(f);
#ifndef _WIN32
    fsync(fileno(f));
    flock(fileno(f), LOCK_UN);
#endif
    fclose(f);
}

static void win_terminate_pid(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (h) {
        TerminateProcess(h, 1);
        CloseHandle(h);
    }
#else
    (void)pid;
#endif
}

static void kill_all_tracked(void) {
    FILE* f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
#ifdef _WIN32
            win_terminate_pid(pid);
#else
            kill(pid, SIGTERM);
#endif
        }
    }
    fclose(f);
    usleep(200000);
    f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
#ifdef _WIN32
            win_terminate_pid(pid);
#else
            kill(pid, SIGKILL);
            waitpid(pid, NULL, WNOHANG);
#endif
        }
    }
    fclose(f);
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

static void run_final_kill_sweep(void) {
#ifdef _WIN32
    /* button.ps1 kill is the real Win sweep; no kill_all.sh on Win */
    (void)0;
#else
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
    char kill_path[2048];
    resolve_path(kill_path, sizeof(kill_path), "pieces/os/kill_all.sh");
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", kill_path, cwd, NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
#endif
}

static void do_shutdown(void) {
    if (shutdown_done) return;
    shutdown_done = 1;
    fprintf(stderr, "[Orchestrator] Shutting down...\n");
    kill_process_group();
    kill_all_tracked();
    run_final_kill_sweep();
    fprintf(stderr, "[Orchestrator] Cleanup complete.\n");
}

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
    do_shutdown();
    _exit(0);
}

/* launch path + up to 6 optional args (aomorai palnet shape) */
static child_pid_t launch(const char *path, const char **argv, int argc) {
#ifdef _WIN32
    char use[512];
    {
        const char *src = path;
        if (src[0] == '.' && (src[1] == '/' || src[1] == '\\')) src += 2;
        snprintf(use, sizeof(use), "%s", src);
        for (char *p = use; *p; p++) if (*p == '/') *p = '\\';
        size_t n = strlen(use);
        int has_exe = (n > 4 && _stricmp(use + n - 4, ".exe") == 0);
        /* Prefer .exe; keep . +x ops as-is if present */
        if (!has_exe) {
            char with_exe[512];
            snprintf(with_exe, sizeof(with_exe), "%s.exe", use);
            struct stat st;
            if (stat(with_exe, &st) == 0)
                snprintf(use, sizeof(use), "%s", with_exe);
        }
        {
            struct stat st;
            if (stat(use, &st) != 0) {
                fprintf(stderr, "[Orchestrator] missing binary: %s\n", use);
                return -1;
            }
        }
    }
    char cmd_line[1536];
    int off = 0;
    off += snprintf(cmd_line + off, sizeof(cmd_line) - (size_t)off, "\"%s\"", use);
    int i;
    for (i = 0; i < argc && i < 14; i++) {
        if (!argv[i]) break;
        off += snprintf(cmd_line + off, sizeof(cmd_line) - (size_t)off, " \"%s\"", argv[i]);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    /* CREATE_NO_WINDOW: console apps (renderer, parser, …) must NOT open a
     * second conhost. Terminal frames are owned by button.ps1's same-console
     * renderer (-NoNewWindow). Without this flag, Win allocates a new console
     * per child when the orchestrator itself is Hidden. */
    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "[Orchestrator] CreateProcess failed: %s (err=%lu)\n",
                use, (unsigned long)GetLastError());
        return -1;
    }
    int pid = (int)pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    log_pid(pid, path);
    fprintf(stderr, "[Orchestrator] Launched %s (PID %d)\n", use, pid);
    return (child_pid_t)pid;
#else
    pid_t pid = fork();
    if (pid == 0) {
        char *args[16];
        args[0] = (char *)path;
        int i;
        for (i = 0; i < argc && i < 14; i++) args[i + 1] = (char *)argv[i];
        args[i + 1] = NULL;
        execv(path, args);
        fprintf(stderr, "[Orchestrator] exec failed: %s\n", path);
        _exit(1);
    }
    if (pid > 0) {
        log_pid(pid, path);
        fprintf(stderr, "[Orchestrator] Launched %s (PID %d)\n", path, pid);
    }
    return pid;
#endif
}

static void ensure_directories(void) {
#ifdef _WIN32
    _mkdir("pieces");
    _mkdir("pieces\\os");
    _mkdir("pieces\\system");
    _mkdir("pieces\\display");
    _mkdir("pieces\\keyboard");
    _mkdir("pieces\\apps");
    _mkdir("pieces\\apps\\player_app");
#else
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/mkdir", "mkdir", "-p", "pieces/os", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
#endif
}

static int quit_requested(void) {
    struct stat st;
    /* REAL BUG FIX 2026-08-20 (direct user report: closing the GL window
     * left every other process still running): x11_mirror.c (shared, its
     * own project_root = argv[1] = the SESSION dir, unaffected by this
     * project's own PRISC_PROJECT_ROOT=$SCRIPT_DIR post-symlink-
     * elimination change) writes quit_flag.txt to the SESSION dir on
     * both Ctrl+C and window-close - but this function was only ever
     * checking the PROJECT-root copy (resolve_path()), a real path
     * mismatch introduced by the same category-B refactor documented in
     * SIMLINK_PITFALL.md. Check both: session-relative (CWD, matching
     * x11_mirror.c's own real write target) first, then project-root
     * (matching button.sh's own clear-at-launch site) as a fallback. */
    char qpath[2048];
    if (stat("pieces/system/quit_flag.txt", &st) == 0 && st.st_size > 0) return 1;
    resolve_path(qpath, sizeof(qpath), "pieces/system/quit_flag.txt");
    if (stat(qpath, &st) != 0) return 0;
    return st.st_size > 0;
}

int main(void) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    resolve_root();

    fprintf(stderr, "[Orchestrator] Starting pal-chain session from %s (root=%s)\n", cwd, project_root_path);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    ensure_directories();

    FILE *pl = fopen("pieces/os/proc_list.txt", "w");
    if (pl) fclose(pl);
    log_pid(getpid(), "orchestrator");

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    char rpath[2048];

    resolve_path(rpath, sizeof(rpath), "system/renderer");
    launch(rpath, NULL, 0);

    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        const char *args[] = { pal_layout };
        resolve_path(rpath, sizeof(rpath), "system/chtpm_parser_pal");
        launch(rpath, args, 1);
    }

    struct stat st;
    resolve_path(rpath, sizeof(rpath), "system/chtpm_rgb_render");
    if (stat(rpath, &st) == 0) {
        launch(rpath, NULL, 0);
    }

    if (!getenv("NO_NET")) {
        resolve_path(rpath, sizeof(rpath), "ops/+x/palnet_peer.+x");
        if (stat(rpath, &st) == 0) {
            const char *args[] = { "chain_node", "pal-chain", "-", "net/outbox.txt", "net/inbox.txt", "chain_node" };
            launch(rpath, args, 6);
        }
    }

    fprintf(stderr, "[Orchestrator] Ready. Waiting for quit_flag.txt or signal.\n");

    while (!should_exit) {
        if (quit_requested()) {
            should_exit = 1;
            do_shutdown();
            break;
        }
#ifdef _WIN32
        usleep(200000);
#else
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            fprintf(stderr, "[Orchestrator] Child %d exited\n", dead);
        }
        usleep(200000);
#endif
    }

    fprintf(stderr, "[Orchestrator] Exit.\n");
    return 0;
}
