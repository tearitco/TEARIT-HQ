#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef _WIN32
    #include <sys/wait.h>
    #include <signal.h>
    #include <termios.h>
#else
    #include <conio.h>
    #include <windows.h>
    #include <direct.h>
    #include <process.h>
    #ifndef SIGHUP
        #define SIGHUP SIGTERM
    #endif
#endif
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>

/* --- Integrated win_spawn.h logic --- */
#ifdef _WIN32
typedef int win_pid_t;
static win_pid_t win_spawn(const char* exe_path, char* const argv[]) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char cmd_line[2048] = "";
    if (argv) {
        for (int i = 0; argv[i]; i++) {
            strcat(cmd_line, "\"");
            strcat(cmd_line, argv[i]);
            strcat(cmd_line, "\" ");
        }
    } else {
        snprintf(cmd_line, sizeof(cmd_line), "\"%s\"", exe_path);
    }

    if (CreateProcess(exe_path, cmd_line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        return (win_pid_t)pi.hProcess;
    }
    return -1;
}

static void win_kill_silent(const char* image_name) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "taskkill /F /IM %s 2>nul", image_name);
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 1000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static int win_kill(win_pid_t pid, int sig) {
    (void)sig; if (pid <= 0) return -1;
    HANDLE hProcess = (HANDLE)pid;
    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result ? 0 : -1;
}
#endif

// TPM Orchestrator (v3.1 - HOLY PROCESS MANAGEMENT)
// Responsibility: Platform detection, Brain-Muscle orchestration, process lifecycle.

char project_root[1024] = ".";
volatile sig_atomic_t should_exit = 0;

/* === PROCESS TRACKING (HOLY Pattern - File-Backed) === */

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
#else
    _commit(_fileno(f));
#endif
    fclose(f);
}

static void kill_all_tracked_processes(void) {
    FILE* f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    
    /* Phase 1: SIGTERM to all process groups */
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int pid;
        char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
#ifndef _WIN32
            kill(-pid, SIGTERM);
            kill(pid, SIGTERM);
#else
            win_kill(pid, 0);
#endif
        }
    }
    fclose(f);
    
    /* Wait 200ms for graceful shutdown */
#ifndef _WIN32
    usleep(200000);
#else
    Sleep(200);
#endif
    
    /* Phase 2: SIGKILL survivors */
    f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    
    while (fgets(line, sizeof(line), f)) {
        int pid;
        char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
#ifndef _WIN32
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, WNOHANG);
#else
            win_kill(pid, 0);
#endif
        }
    }
    fclose(f);
    
    /* Clear the file for next run */
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

#ifdef _WIN32
static int is_history_on(void) {
    FILE *f = fopen("pieces/display/state.txt", "r");
    if (!f) return 1;
    char line[128]; int on = 1;
    if (fgets(line, sizeof(line), f)) { if (strstr(line, "off")) on = 0; }
    fclose(f); return on;
}
#endif

void handle_sigint(int sig) {
    should_exit = 1;
#ifndef _WIN32
    /* Kill all child processes first (same process group as orchestrator) */
    kill(0, SIGTERM);
    usleep(100000); /* Give them 100ms to clean up */

    /* Use surgical kill script as final sweep */
    system("bash pieces/os/kill_all.sh > /dev/null 2>&1");
    _exit(0);
#else
    /* Windows: Kill tracked processes first */
    kill_all_tracked_processes();

    /* Explicitly kill known muscles/renderers (silently) */
    win_kill_silent("keyboard_input.+x");
    win_kill_silent("gl_renderer.+x");
    win_kill_silent("renderer.+x");
    win_kill_silent("chtpm_parser.+x");
    win_kill_silent("clock_daemon.+x");
    win_kill_silent("joystick_input.+x");
    win_kill_silent("response_handler.+x");
    
    /* Global sweep if available */
    if (access("pieces/os/kill_all.ps1", 0) == 0) {
        STARTUPINFO si; PROCESS_INFORMATION pi; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si); ZeroMemory(&pi, sizeof(pi));
        char cmd[] = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File pieces/os/kill_all.ps1";
        if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
    }
    _exit(0);
#endif
}

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        handle_sigint(SIGINT); return TRUE;
    }
    return FALSE;
}
#endif

void build_path(char* dst, size_t sz, const char* fmt, ...) {
    va_list args; va_start(args, fmt); vsnprintf(dst, sz, fmt, args); va_end(args);
}

void resolve_root() {
    getcwd(project_root, sizeof(project_root));
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13; v[strcspn(v, "\n\r")] = 0;
                if (strlen(v) > 0) snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
}

void proc_mgr_call(const char* action, const char* name, int pid) {
    char *pm_path = NULL;
    if (asprintf(&pm_path, "%s/pieces/os/plugins/+x/proc_manager.+x", project_root) == -1) return;
#ifndef _WIN32
    pid_t p = fork();
    if (p == 0) {
        if (pid >= 0) { char ps[32]; snprintf(ps, sizeof(ps), "%d", pid); execl(pm_path, pm_path, action, name, ps, NULL); }
        else execl(pm_path, pm_path, action, name, NULL);
        exit(1);
    } else if (p > 0) waitpid(p, NULL, 0);
#else
    if (pid >= 0) { char ps[32]; snprintf(ps, sizeof(ps), "%d", pid); _spawnl(_P_DETACH, pm_path, pm_path, action, name, ps, NULL); }
    else _spawnl(_P_DETACH, pm_path, pm_path, action, name, NULL);
#endif
    free(pm_path);
}

void launch_and_register(const char* name, const char* path, const char* args, bool quiet) {
    char abs_path[1024];
    if (path[0] == '/') strcpy(abs_path, path);
    else build_path(abs_path, sizeof(abs_path), "%s/%s", project_root, path);
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        if (project_root[0] != '\0') chdir(project_root);
        if (quiet) { freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr); }
        if (args && strlen(args) > 0) execl(abs_path, abs_path, args, NULL);
        else execl(abs_path, abs_path, NULL);
        exit(1);
    } else if (pid > 0) {
        /* Log to proc_list.txt for cleanup on Ctrl+C */
        log_pid(pid, name);
        int status; waitpid(pid, &status, 0);
    }
#else
    int pid; char ocwd[1024]; _getcwd(ocwd, sizeof(ocwd));
    char win_path[1024]; strcpy(win_path, abs_path);
    /* Convert forward slashes to backslashes for Windows native spawn if needed */
    for (int i = 0; win_path[i]; i++) if (win_path[i] == '/') win_path[i] = '\\';

    int mode = quiet ? _P_DETACH : _P_NOWAIT;

    if (project_root[0] != '\0') _chdir(project_root);
    if (args && strlen(args) > 0) pid = _spawnl(mode, win_path, win_path, args, NULL);
    else pid = _spawnl(mode, win_path, win_path, NULL);
    _chdir(ocwd);
    if (pid > 0) log_pid(pid, name);
#endif
}

void* joystick_thread_func(void* arg) {
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execl("./pieces/joystick/plugins/+x/joystick_input.+x", "joystick_input.+x", ".", NULL);
        exit(1);
    } else if (pid > 0) {
        log_pid(pid, "joystick_input");
    }
#else
    int pid = _spawnl(_P_DETACH, "./pieces/joystick/plugins/+x/joystick_input.+x", "joystick_input.+x", ".", NULL);
    if (pid > 0) log_pid(pid, "joystick_input");
#endif
    return NULL;
}

void* keyboard_thread_func(void* arg) {
    /* Muscle Selection */
#ifndef _WIN32
    launch_and_register("keyboard_input", "pieces/keyboard/plugins/+x/keyboard_input.+x", NULL, false);
#else
    launch_and_register("keyboard_input", "pieces\\keyboard\\plugins\\+x\\keyboard_input.+x", NULL, false);
#endif
    return NULL;
}

void* response_thread_func(void* arg) { launch_and_register("response_handler", "pieces/master_ledger/plugins/+x/response_handler.+x", NULL, true); return NULL; }

void* render_thread_func(void* arg) {
#ifndef _WIN32
    launch_and_register("renderer", "pieces/display/plugins/+x/renderer.+x", NULL, false);
#else
    off_t lp = 0; struct stat st;
    const char* pp = "pieces/display/renderer_pulse.txt";
    const char* fp = "pieces/display/current_frame.txt";
    if (stat(pp, &st) == 0) lp = st.st_size;
    while (!should_exit) {
        if (stat(pp, &st) == 0 && st.st_size != lp) {
            if (is_history_on()) { printf("\n\n\n\n\n--- FRAME UPDATE ---\n"); }
            else { system("cls"); }
            FILE *f = fopen(fp, "r");
            if (f) { char buf[4096]; while (fgets(buf, sizeof(buf), f)) printf("%s", buf); fclose(f); }
            fflush(stdout); lp = st.st_size;
        }
        Sleep(17);
    }
#endif
    return NULL;
}

void* gl_render_thread_func(void* arg) { launch_and_register("gl_renderer", "pieces/display/plugins/+x/gl_renderer.+x", NULL, true); return NULL; }
void* clock_daemon_thread_func(void* arg) { launch_and_register("clock_daemon", "pieces/system/clock_daemon/plugins/+x/clock_daemon.+x", NULL, true); return NULL; }
void* chtpm_thread_func(void* arg) { launch_and_register("chtpm_parser", "pieces/chtpm/plugins/+x/chtpm_parser.+x", "pieces/chtpm/layouts/os.chtpm", true); return NULL; }

int main() {
    resolve_root();

    /* Clear stale proc_list.txt from previous run (Cross-platform) */
    FILE* init_f = fopen("pieces/os/proc_list.txt", "w");
    if (init_f) fclose(init_f);

    /* Reset project state to clean defaults */
    FILE* sf = fopen("pieces/apps/player_app/manager/state.txt", "w");
    if (sf) {
        fprintf(sf, "project_id=playrm\n");
        fprintf(sf, "active_target_id=loader\n");
        fprintf(sf, "current_map=map_01.txt\n");
        fprintf(sf, "current_z=0\n");
        fprintf(sf, "last_key=None\n");
        fclose(sf);
    }
    FILE* lf = fopen("pieces/display/layout_changed.txt", "w");
    if (lf) fclose(lf);

    /* Register orchestrator itself */
#ifndef _WIN32
    log_pid(getpid(), "orchestrator");
#else
    log_pid(_getpid(), "orchestrator");
#endif
    
    signal(SIGINT, handle_sigint);
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif

    pthread_t kb_t, joy_t, res_t, ren_t, gl_t, clock_t, parser_t;
    pthread_create(&kb_t, NULL, keyboard_thread_func, NULL);
    pthread_create(&joy_t, NULL, joystick_thread_func, NULL);
    pthread_create(&res_t, NULL, response_thread_func, NULL);
    pthread_create(&parser_t, NULL, chtpm_thread_func, NULL);
    usleep(50000);
    pthread_create(&ren_t, NULL, render_thread_func, NULL);
    pthread_create(&gl_t, NULL, gl_render_thread_func, NULL);
    pthread_create(&clock_t, NULL, clock_daemon_thread_func, NULL);

    /* Wait for shutdown signal */
    while (!should_exit) {
#ifndef _WIN32
        /* On POSIX, we still join kb_t to stay compatible with existing design */
        pthread_join(kb_t, NULL);
        should_exit = 1;
#else
        /* On Windows, all processes are spawned detached, so we just wait here */
        Sleep(100);
#endif
    }
    
    /* Trigger graceful shutdown of all children */
    handle_sigint(0);
    return 0;
}
