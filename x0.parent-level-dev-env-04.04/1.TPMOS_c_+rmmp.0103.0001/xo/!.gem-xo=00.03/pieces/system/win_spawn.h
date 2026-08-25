#ifndef WIN_SPAWN_H
#define WIN_SPAWN_H

#ifdef _WIN32
/*
 * Windows Process Spawning Wrapper
 * Emulates fork()+exec() behavior using _spawn() + _cwait()
 * 
 * Usage:
 *   pid_t pid = win_spawn("/path/to/exe", "arg1", "arg2", NULL);
 *   if (pid > 0) {
 *       // Parent: can wait on pid
 *       _cwait(NULL, pid, WAIT_CHILD);
 *   }
 */

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Pseudo-PID type - on Windows this is the process handle/ID from spawn */
typedef int win_pid_t;

/*
 * Spawn a process (like fork+exec)
 * 
 * @param exe_path  Path to executable
 * @param args      NULL-terminated array of arguments (argv style)
 * @return          Process ID (>0) on success, -1 on failure
 */
static win_pid_t win_spawn(const char* exe_path, char* const argv[]) {
    char cmd_line[4096] = "";
    size_t offset = 0;
    
    /* Build command line from argv array */
    for (int i = 0; argv[i] != NULL && offset < sizeof(cmd_line) - 1; i++) {
        if (i > 0) {
            cmd_line[offset++] = ' ';
        }
        /* Add quotes around paths with spaces */
        if (strchr(argv[i], ' ') != NULL) {
            cmd_line[offset++] = '"';
            size_t arg_len = strlen(argv[i]);
            if (offset + arg_len < sizeof(cmd_line) - 2) {
                strcpy(cmd_line + offset, argv[i]);
                offset += arg_len;
            }
            cmd_line[offset++] = '"';
        } else {
            size_t arg_len = strlen(argv[i]);
            if (offset + arg_len < sizeof(cmd_line) - 1) {
                strcpy(cmd_line + offset, argv[i]);
                offset += arg_len;
            }
        }
    }
    cmd_line[offset] = '\0';
    
    /* Spawn the process detached (DETACHED = no console window inheritance) */
    win_pid_t pid = _spawnl(_P_DETACH, exe_path, exe_path, NULL);
    
    /* If _P_DETACH fails, try _P_NOWAIT for better compatibility */
    if (pid == -1) {
        pid = _spawnl(_P_NOWAIT, exe_path, exe_path, NULL);
    }
    
    return pid;
}

/*
 * Spawn with explicit arguments (variadic version)
 * 
 * Usage: win_spawn_exe("/path/to/exe", "arg1", "arg2", NULL);
 */
static win_pid_t win_spawn_exe(const char* exe_path, ...) {
    va_list args;
    va_start(args, exe_path);
    
    char* argv[64];
    int argc = 0;
    
    /* First argument is the executable itself */
    argv[argc++] = (char*)exe_path;
    
    /* Collect remaining arguments */
    while (argc < 63) {
        char* arg = va_arg(args, char*);
        if (arg == NULL) break;
        argv[argc++] = arg;
    }
    argv[argc] = NULL;
    
    va_end(args);
    
    return win_spawn(exe_path, argv);
}

/*
 * Wait for a spawned process (like waitpid)
 * 
 * @param pid   Process ID from win_spawn
 * @param status Output status (can be NULL)
 * @return      pid on success, -1 on failure
 */
static win_pid_t win_waitpid(win_pid_t pid, int* status) {
    if (pid <= 0) return -1;
    
    int wait_result = _cwait(NULL, pid, WAIT_CHILD);
    if (status != NULL) {
        *status = wait_result;
    }
    return pid;
}

/*
 * Check if process is still running
 * 
 * @param pid   Process ID from win_spawn
 * @return      1 if running, 0 if exited, -1 on error
 */
static int win_process_running(win_pid_t pid) {
    if (pid <= 0) return -1;
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return 0;  /* Process doesn't exist */
    }
    
    DWORD exit_code;
    GetExitCodeProcess(hProcess, &exit_code);
    CloseHandle(hProcess);
    
    return (exit_code == STILL_ACTIVE) ? 1 : 0;
}

/*
 * Kill a process (like kill())
 * 
 * @param pid   Process ID from win_spawn
 * @return      0 on success, -1 on failure
 */
static int win_kill(win_pid_t pid, int sig) {
    (void)sig;  /* Signal ignored on Windows */
    if (pid <= 0) return -1;
    
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        return -1;
    }
    
    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    
    return result ? 0 : -1;
}

/* Convenience macros to match existing code patterns */
#define fork() (-1)  /* Keep for compatibility, but don't use directly */
#define win_fork_and_exec(exe, ...) win_spawn_exe(exe, __VA_ARGS__)

#else
/* POSIX/Linux - use native fork/exec */
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

static inline pid_t win_spawn(const char* exe_path, char* const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: exec the program */
        execv(exe_path, argv);
        exit(127);  /* exec failed */
    }
    return pid;
}

static inline pid_t win_waitpid(pid_t pid, int* status) {
    return waitpid(pid, status, 0);
}

static inline int win_process_running(pid_t pid) {
    return (kill(pid, 0) == 0) ? 1 : 0;
}

static inline int win_kill(pid_t pid, int sig) {
    return kill(pid, sig);
}

#endif /* _WIN32 */

#endif /* WIN_SPAWN_H */
