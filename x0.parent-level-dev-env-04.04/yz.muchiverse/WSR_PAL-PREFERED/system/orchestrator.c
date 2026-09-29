// orchestrator.c - TPMOS launcher for WSR-PAL
// Stock market simulation game orchestrator
// Linux: fork/exec (Bible §3). Windows: CreateProcess/_spawn (TPMOS parity).
// Process management: x0.moke 3-layer cascading kill pattern
// Surgical #ifdef _WIN32 wraps — Linux path left byte-identical in shape.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#define usleep(us) Sleep((DWORD)((us) / 1000))
#define getcwd _getcwd
#define getpid _getpid
#define SETENV(n, v) _putenv_s((n), (v))
typedef intptr_t child_pid_t;
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#define SETENV(n, v) setenv((n), (v), 1)
typedef pid_t child_pid_t;
#endif

#define MAX_CHILDREN 8

static volatile int should_exit = 0;

/* === LAYER 1: Process Group Kill (fast path) === */

static void kill_process_group(void) {
#ifdef _WIN32
    /* Windows has no process groups like POSIX; Layer 2 handles tracked PIDs. */
    usleep(100000);
#else
    /* Send SIGTERM to entire process group (all forked children) */
    kill(0, SIGTERM);
    usleep(100000); /* 100ms grace period */
#endif
}

/* === LAYER 2: File-Backed PID Tracking (x0.moke HOLY Pattern) === */

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
    /* Phase 1: soft kill */
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
    /* Phase 2: hard kill survivors */
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
    /* Clear for next run */
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

/* === LAYER 3: kill_all final sweep === */

static void run_final_kill_sweep(void) {
#ifdef _WIN32
    /* PowerShell kill script (parity with kill_all.sh) */
    system("powershell -NoProfile -ExecutionPolicy Bypass -File pieces\\os\\kill_all.ps1 >nul 2>&1");
#else
    pid_t pid = fork();
    if (pid == 0) {
        /* Redirect to /dev/null, run kill_all.sh */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "pieces/os/kill_all.sh", NULL);
        _exit(1);
    }
    if (pid > 0) {
        /* Wait up to 2 seconds for kill_all.sh to finish */
        int status;
        waitpid(pid, &status, WNOHANG);
    }
#endif
}

/* === SIGNAL HANDLER === */

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;

    fprintf(stderr, "\n[Orchestrator] Shutting down (signal %d)...\n", sig);

    /* 3-layer cascading kill (x0.moke pattern) */
    kill_process_group();    /* Layer 1: process group SIGTERM */
    kill_all_tracked();      /* Layer 2: file-backed PID sweep */
    run_final_kill_sweep();  /* Layer 3: kill_all nuclear option */

    fprintf(stderr, "[Orchestrator] Cleanup complete.\n");
    _exit(0);
}

/* === PROCESS LAUNCH === */

static child_pid_t launch(const char *path, const char *arg1, const char *arg2) {
#ifdef _WIN32
    /* Normalize ./system/foo -> system\foo.exe for CreateProcessA. */
    char use[512];
    {
        const char *src = path;
        if (src[0] == '.' && (src[1] == '/' || src[1] == '\\')) src += 2;
        snprintf(use, sizeof(use), "%s", src);
        for (char *p = use; *p; p++) if (*p == '/') *p = '\\';
        size_t n = strlen(use);
        int has_exe = (n > 4 && _stricmp(use + n - 4, ".exe") == 0);
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
    char cmd_line[1024];
    if (arg2)
        snprintf(cmd_line, sizeof(cmd_line), "\"%s\" \"%s\" \"%s\"", use, arg1, arg2);
    else if (arg1)
        snprintf(cmd_line, sizeof(cmd_line), "\"%s\" \"%s\"", use, arg1);
    else
        snprintf(cmd_line, sizeof(cmd_line), "\"%s\"", use);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    /* Share console so keyboard_input can read stdin (_P_NOWAIT style). */
    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
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
        if (arg2)
            execl(path, path, arg1, arg2, NULL);
        else if (arg1)
            execl(path, path, arg1, NULL);
        else
            execl(path, path, NULL);
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

/* === BINARY COMPILATION === */

static void compile_single(const char *src, const char *dst, const char *extra_flags) {
#ifdef _WIN32
    char cmd[768];
    if (extra_flags)
        snprintf(cmd, sizeof(cmd), "gcc -o %s %s %s", dst, src, extra_flags);
    else
        snprintf(cmd, sizeof(cmd), "gcc -o %s %s", dst, src);
    int rc = system(cmd);
    (void)rc;
#else
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        char cmd[512];
        if (extra_flags)
            snprintf(cmd, sizeof(cmd), "gcc -o %s %s %s", dst, src, extra_flags);
        else
            snprintf(cmd, sizeof(cmd), "gcc -o %s %s", dst, src);
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
#endif
}

/* Compile ops/*.c to ops/+x/*.+x via loop (Bible section 3 compliant) */
static void compile_ops(void) {
    DIR *d = opendir("ops");
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        size_t len = strlen(entry->d_name);
        if (len < 3 || strcmp(entry->d_name + len - 2, ".c") != 0) continue;

        char src[512], dst[512];
        snprintf(src, sizeof(src), "ops/%s", entry->d_name);

        /* Strip .c, replace with +x extension */
        char base[256];
        strncpy(base, entry->d_name, len - 2);
        base[len - 2] = '\0';
        snprintf(dst, sizeof(dst), "ops/+x/%s.+x", base);

        const char *flags = NULL;
        if (strstr(entry->d_name, "dump_rgb_png"))
            flags = "-Iops/lib -lm";
        else
            flags = "-lm";

        fprintf(stderr, "[Orchestrator] Compiling %s → %s\n", src, dst);
        compile_single(src, dst, flags);
    }
    closedir(d);
}

static void compile_binaries(void) {
    fprintf(stderr, "[Orchestrator] Compiling...\n");
    compile_single("system/prisc+x.c", "system/prisc+x", NULL);
    compile_single("system/keyboard_input.c", "system/keyboard_input", NULL);
    compile_single("system/renderer.c", "system/renderer", NULL);
    compile_single("system/chtpm_parser_pal.c", "system/chtpm_parser_pal",
#ifdef _WIN32
                   NULL
#else
                   "-Wno-unused-result -Wno-stringop-truncation"
#endif
                   );
    compile_single("system/chtpm_rgb_render.c", "system/chtpm_rgb_render", NULL);

    /* Best effort: gl_mirror needs OpenGL libs, skip if gcc fails */
    fprintf(stderr, "[Orchestrator] Compiling gl_mirror (best effort)...\n");
#ifdef _WIN32
    compile_single("system/gl_mirror.c", "system/gl_mirror",
                   "-LC:/msys64/mingw64/lib -lopengl32 -lglu32 -lfreeglut -lwinmm -lgdi32 -luser32");
#else
    compile_single("system/gl_mirror.c", "system/gl_mirror", "-lglut -lGL -lGLU");
#endif

    compile_ops();
}

/* === DIRECTORY SETUP === */

static void ensure_directories(void) {
#ifdef _WIN32
    _mkdir("pieces");
    _mkdir("pieces/system");
    _mkdir("pieces/display");
    _mkdir("pieces/apps");
    _mkdir("pieces/apps/player_app");
    _mkdir("pieces/keyboard");
    _mkdir("pieces/os");
    _mkdir("ops");
    _mkdir("ops/+x");
#else
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/mkdir", "mkdir", "-p",
              "pieces/system", "pieces/display",
              "pieces/apps/player_app", "pieces/keyboard",
              "pieces/os", "ops/+x", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
#endif
}

/* === ENSURE ENTITIES === */

static void run_ensure_entities(void) {
#ifdef _WIN32
    system("powershell -NoProfile -ExecutionPolicy Bypass -File scripts\\ensure_entities.ps1");
#else
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "scripts/ensure_entities.sh", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
#endif
}

/* === MAIN === */

int main(void) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "[Orchestrator] Starting WSR-PAL from %s\n", cwd);

    /* Register signal handlers for 3-layer cleanup */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Skip full recompile when SKIP_ORCH_COMPILE=1 (Windows button.ps1 sets
     * this after a successful .\button.ps1 compile — recompiling on every
     * run stalls launch 30-60s and looks like "not running"). */
    if (getenv("SKIP_ORCH_COMPILE") && getenv("SKIP_ORCH_COMPILE")[0] == '1') {
        fprintf(stderr, "[Orchestrator] Skipping compile (SKIP_ORCH_COMPILE=1)\n");
    } else {
        compile_binaries();
    }

    fprintf(stderr, "[Orchestrator] Initializing...\n");
    ensure_directories();
    run_ensure_entities();

    /* Clear proc_list.txt */
    FILE *pl = fopen("pieces/os/proc_list.txt", "w");
    if (pl) fclose(pl);
    log_pid(getpid(), "orchestrator");

    /* Clear state files */
    FILE *fc = fopen("pieces/display/frame_changed.txt", "w");
    if (fc) fclose(fc);
    FILE *rp = fopen("pieces/display/renderer_pulse.txt", "w");
    if (rp) fclose(rp);
    FILE *hr = fopen("pieces/apps/player_app/history.txt", "w");
    if (hr) fclose(hr);
    FILE *kr = fopen("pieces/keyboard/history.txt", "w");
    if (kr) fclose(kr);
    FILE *relay = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (relay) fclose(relay);
    FILE *qf = fopen("pieces/system/quit_flag.txt", "w");
    if (qf) fclose(qf);
    FILE *wsc = fopen("pieces/display/wsr_screen_changed.txt", "w");
    if (wsc) fclose(wsc);

#ifdef _WIN32
    /* Relative root: absolute paths through emoji/Unicode house folders
     * break ANSI fopen/opendir on MinGW+OneDrive. Children inherit CWD. */
    SETENV("PRISC_PROJECT_ROOT", ".");
#else
    SETENV("PRISC_PROJECT_ROOT", cwd);
#endif
    SETENV("PRISC_PROJECT_ID", "wsr-pal");

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, NULL);
    launch("./system/keyboard_input", NULL, NULL);

    /* PAL layout: use PAL_LAYOUT env or default */
    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        fprintf(stderr, "[Orchestrator] Using PAL layout: %s\n", pal_layout);
        launch("./system/chtpm_parser_pal", pal_layout, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Using default: pieces/chtpm/layouts/wsr_main_menu.chtpm\n");
        launch("./system/chtpm_parser_pal", "pieces/chtpm/layouts/wsr_main_menu.chtpm", NULL);
    }

    /* Best effort: chtpm_rgb_render */
    fprintf(stderr, "[Orchestrator] Launching chtpm_rgb_render (best effort)...\n");
    launch("./system/chtpm_rgb_render", NULL, NULL);

    /* Best effort: gl_mirror (skip if NO_GL env set) */
    if (!getenv("NO_GL")) {
        fprintf(stderr, "[Orchestrator] Launching gl_mirror (best effort)...\n");
        launch("./system/gl_mirror", NULL, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Skipping gl_mirror (NO_GL set)\n");
    }

    fprintf(stderr, "[Orchestrator] Ready. Press Ctrl+C to exit.\n");

    while (!should_exit) {
#ifdef _WIN32
        /* Detached/async children return immediately; poll loop required
         * (TPMOS bible §9 Main Loop Stability). */
        Sleep(100);
#else
        sleep(1);
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            fprintf(stderr, "[Orchestrator] Child %d exited\n", dead);
        }
#endif
    }

    fprintf(stderr, "[Orchestrator] Exit.\n");
    return 0;
}
