// orchestrator.c - TPMOS launcher for muchi-pal-agent
// Uses fork/exec for all child processes (Bible §3)
// Process management: x0.moke 3-layer cascading kill pattern
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>

#define MAX_CHILDREN 8

static volatile int should_exit = 0;

/* === LAYER 1: Process Group Kill (fast path) === */

static void kill_process_group(void) {
    /* Send SIGTERM to entire process group (all forked children) */
    kill(0, SIGTERM);
    usleep(100000); /* 100ms grace period */
}

/* === LAYER 2: File-Backed PID Tracking (x0.moke HOLY Pattern) === */

static void log_pid(int pid, const char* name) {
    FILE* f = fopen("pieces/os/proc_list.txt", "a");
    if (!f) return;
    flock(fileno(f), LOCK_EX);
    fprintf(f, "%d %s\n", pid, name);
    fflush(f);
    fsync(fileno(f));
    flock(fileno(f), LOCK_UN);
    fclose(f);
}

static void kill_all_tracked(void) {
    FILE* f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    char line[256];
    /* Phase 1: SIGTERM */
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
            kill(pid, SIGTERM);
        }
    }
    fclose(f);
    usleep(200000);
    /* Phase 2: SIGKILL survivors */
    f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, WNOHANG);
        }
    }
    fclose(f);
    /* Clear for next run */
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

/* === LAYER 3: kill_all.sh Final Sweep (nuclear option) === */

static void run_final_kill_sweep(void) {
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
}

/* === SIGNAL HANDLER === */

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
    
    fprintf(stderr, "\n[Orchestrator] Shutting down (signal %d)...\n", sig);
    
    /* 3-layer cascading kill (x0.moke pattern) */
    kill_process_group();    /* Layer 1: process group SIGTERM */
    kill_all_tracked();      /* Layer 2: file-backed PID sweep */
    run_final_kill_sweep();  /* Layer 3: kill_all.sh nuclear option */
    
    fprintf(stderr, "[Orchestrator] Cleanup complete.\n");
    _exit(0);
}

/* === PROCESS LAUNCH (fork/exec, Bible §3 compliant) === */

static pid_t launch(const char *path, const char *arg1, const char *arg2) {
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
}

/* === BINARY COMPILATION (fork/exec, Bible §3 compliant) === */

static void compile_single(const char *src, const char *dst, const char *extra_flags) {
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
}

static void compile_binaries(void) {
    fprintf(stderr, "[Orchestrator] Compiling...\n");
    compile_single("system/prisc+x.c", "system/prisc+x", NULL);
    compile_single("system/keyboard_input.c", "system/keyboard_input", NULL);
    compile_single("system/renderer.c", "system/renderer", NULL);
    compile_single("system/chtpm_parser_pal.c", "system/chtpm_parser_pal",
                   "-Wno-unused-result -Wno-stringop-truncation");
    compile_single("system/chtpm_rgb_render.c", "system/chtpm_rgb_render",
                   "-Wno-format-truncation");
    compile_single("manager/path_nav_manager.c", "manager/+x/path_nav_manager.+x", NULL);

    /* Best effort: gl_mirror (GL/GLUT reader - skip if libs unavailable) */
    {
        pid_t gpid = fork();
        if (gpid == 0) {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            execl("/usr/bin/gcc", "gcc", "-Wall", "-Wextra", "-O2",
                  "-o", "system/gl_mirror", "system/gl_mirror.c",
                  "-lglut", "-lGL", "-lGLU", NULL);
            _exit(1);
        }
        if (gpid > 0) {
            int status;
            waitpid(gpid, &status, 0);
        }
    }

    /* Compile all ops: ops dir .c files -> ops/+x binaries (emoji ops
     * compiled separately below - they need freetype headers/libs). */
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "-c",
              "for f in ops/*.c; do "
              "  base=$(basename \"$f\" .c); "
              "  [ \"$base\" = emoji_gen_atlas ] && continue; "
              "  [ \"$base\" = emoji_xtract ] && continue; "
              "  /usr/bin/gcc -o \"ops/+x/$base.+x\" \"$f\"; "
              "done",
              NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }

    /* On-demand emoji generator + extractor (chtpm_rgb_render system()s
     * these two - see that file's own 2026-07-30 generic path). */
    compile_single("ops/emoji_gen_atlas.c", "ops/+x/emoji_gen_atlas.+x",
                   "-I/usr/include/freetype2 -lfreetype -lm");
    compile_single("ops/emoji_xtract.c", "ops/+x/emoji_xtract.+x", "-lm");
}

/* === DIRECTORY SETUP (fork/exec mkdir, Bible §3 compliant) === */

static void ensure_directories(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/mkdir", "mkdir", "-p",
              "pieces/system", "pieces/display",
              "pieces/apps/player_app", "pieces/keyboard",
              "pieces/os", "ops/+x", "manager/+x",
              "pieces/world_01/session_01/chat", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* === MAIN === */

int main(void) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "[Orchestrator] Starting muchi-pal-agent from %s\n", cwd);

    /* Register signal handlers for 3-layer cleanup */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    compile_binaries();

    fprintf(stderr, "[Orchestrator] Initializing...\n");
    ensure_directories();

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
    if (qf) { fclose(qf); }

    setenv("PRISC_PROJECT_ROOT", cwd, 1);
    setenv("PRISC_PROJECT_ID", "muchi-pal-agent", 1);

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, NULL);
    launch("./system/keyboard_input", NULL, NULL);

    /* Check for PAL layout override (button.sh --pal sets PAL_LAYOUT) */
    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        fprintf(stderr, "[Orchestrator] Using PAL layout: %s\n", pal_layout);
        launch("./system/chtpm_parser_pal", pal_layout, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Using default layout: pieces/chtpm/layouts/chat.chtpm\n");
        launch("./system/chtpm_parser_pal", "pieces/chtpm/layouts/chat.chtpm", NULL);
    }

    /* RGB mirror pair (chtpm_rgb_render + gl_mirror) - only when a real
     * X display is available and NO_GL is not set, and only AFTER a real
     * first frame exists (PITFALL 54 race parity with 102.agy-txt's own
     * button.sh - opening GL before the first compose shows a black/
     * empty window). Bounded wait (~2s), non-blocking otherwise. */
    if (!getenv("NO_GL") && getenv("DISPLAY") && getenv("DISPLAY")[0]) {
        struct stat st;
        int waited = 0;
        while (waited < 20) {
            if (stat("pieces/display/current_frame.txt", &st) == 0 && st.st_size > 0)
                break;
            usleep(100000);
            waited++;
        }
        fprintf(stderr, "[Orchestrator] RGB mirror: first frame %s after %.1fs\n",
                (waited < 20) ? "seen" : "not seen", waited * 0.1);

        if (stat("./system/chtpm_rgb_render", &st) == 0)
            launch("./system/chtpm_rgb_render", NULL, NULL);
        if (stat("./system/gl_mirror", &st) == 0)
            launch("./system/gl_mirror", NULL, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] RGB mirror skipped (NO_GL set or no DISPLAY)\n");
    }

    /* Best-effort path_nav_manager launch (skip if binary missing) */
    if (access("./manager/+x/path_nav_manager.+x", X_OK) == 0) {
        pid_t nav_pid = fork();
        if (nav_pid == 0) {
            freopen("/tmp/muchi_pal_agent_path_nav.log", "w", stdout);
            freopen("/tmp/muchi_pal_agent_path_nav.log", "a", stderr);
            execl("./manager/+x/path_nav_manager.+x",
                  "./manager/+x/path_nav_manager.+x", NULL);
            _exit(1);
        }
        if (nav_pid > 0) {
            log_pid(nav_pid, "path_nav_manager");
            fprintf(stderr, "[Orchestrator] Launched path_nav_manager (PID %d)\n", nav_pid);
        }
    } else {
        fprintf(stderr, "[Orchestrator] Skipping path_nav_manager (binary not found)\n");
    }

    fprintf(stderr, "[Orchestrator] Ready. Press Ctrl+C to exit.\n");

    while (!should_exit) {
        sleep(1);
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            fprintf(stderr, "[Orchestrator] Child %d exited\n", dead);
        }
    }

    fprintf(stderr, "[Orchestrator] Exit.\n");
    return 0;
}
