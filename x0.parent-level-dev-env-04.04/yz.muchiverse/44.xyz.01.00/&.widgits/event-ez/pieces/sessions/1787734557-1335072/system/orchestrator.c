// orchestrator.c - TPMOS launcher for MUTACLSYM
// Uses fork/exec for all child processes (Bible §3)
// Process management: x0.moke 3-layer cascading kill pattern
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/file.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_CHILDREN 8

static volatile int should_exit = 0;

/* === LAYER 1: Process Group Kill (fast path) === */

static void kill_process_group(void) {
    kill(0, SIGTERM);
    usleep(100000);
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
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
            kill(pid, SIGTERM);
        }
    }
    fclose(f);
    usleep(200000);
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
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

/* === LAYER 3: kill_all.sh Final Sweep (nuclear option) === */

static void run_final_kill_sweep(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "pieces/os/kill_all.sh", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, WNOHANG);
    }
}

/* === SIGNAL HANDLER === */

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;

    fprintf(stderr, "\n[Orchestrator] Shutting down (signal %d)...\n", sig);

    kill_process_group();
    kill_all_tracked();
    run_final_kill_sweep();

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

static void compile_ops(void) {
    DIR *d = opendir("ops");
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len < 3) continue;
        if (strcmp(ent->d_name + len - 2, ".c") != 0) continue;

        char src[512], dst[512];
        snprintf(src, sizeof(src), "ops/%s", ent->d_name);

        /* Build output path: ops/+x/basename (without .c, replace dots with +x) */
        char base[256];
        strncpy(base, ent->d_name, sizeof(base) - 1);
        base[len - 2] = '\0'; /* strip .c */

        snprintf(dst, sizeof(dst), "ops/+x/%s", base);

        const char *flags = NULL;
        if (strcmp(ent->d_name, "compose_rgb_frame.c") == 0)
            flags = "-lm";
        else if (strcmp(ent->d_name, "dump_rgb_png.c") == 0)
            flags = "-Iops/lib -lm";

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
                   "-Wno-unused-result -Wno-stringop-truncation");
    compile_single("system/chtpm_rgb_render.c", "system/chtpm_rgb_render", NULL);
    compile_single("system/gl_mirror.c", "system/gl_mirror", "-lglut -lGL -lGLU -lX11");
    compile_ops();
}

/* === CONFIG/LEDGER WRITERS (ledger-based architecture, Phase 1) === */

static void write_config_if_absent(void) {
    FILE *f = fopen("config.txt", "r");
    if (f) { fclose(f); return; }
    /* Seed hero_start_x/y from the actual state.txt position, NOT
     * hardcoded defaults - the hero may have moved before the ledger
     * was created, and compose_frame.c replays the ledger starting
     * from these values. Using stale coordinates causes ASCII and
     * RGB views to disagree on the hero's position. */
    int sx = 18, sy = 3;
    FILE *sf = fopen("pieces/world_01/map_start/hero/state.txt", "r");
    if (sf) {
        char line[512];
        while (fgets(line, sizeof(line), sf)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            if (strcmp(line, "pos_x") == 0) sx = atoi(eq + 1);
            else if (strcmp(line, "pos_y") == 0) sy = atoi(eq + 1);
        }
        fclose(sf);
    }
    f = fopen("config.txt", "w");
    if (f) {
        fprintf(f,
            "current_epoch=1\n"
            "current_turn=0\n"
            "hero_start_x=%d\n"
            "hero_start_y=%d\n", sx, sy);
        fclose(f);
        fprintf(stderr, "[Orchestrator] Wrote initial config.txt (hero at %d,%d)\n", sx, sy);
    }
}

static void write_ledger_if_absent(void) {
    FILE *f = fopen("data/master_ledger.txt", "r");
    if (f) { fclose(f); return; }
    f = fopen("data/master_ledger.txt", "w");
    if (f) {
        fprintf(f, "timestamp|epoch|actor|turn|action_data|action_type\n");
        fclose(f);
        fprintf(stderr, "[Orchestrator] Wrote initial ledger header\n");
    }
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
              "pieces/os", "data", "ops/+x",
              "pieces/world_01/map_start/hero/inventory", NULL);
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
    fprintf(stderr, "[Orchestrator] Starting MUTACLSYM from %s\n", cwd);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    compile_binaries();

    fprintf(stderr, "[Orchestrator] Initializing...\n");
    ensure_directories();
    write_config_if_absent();
    write_ledger_if_absent();

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

    /* Truncate debug/log files that grow unboundedly across reruns */
    FILE *dbg = fopen("debug.txt", "w");
    if (dbg) fclose(dbg);
    FILE *gkl = fopen("pieces/display/gl_key_debug.log", "w");
    if (gkl) fclose(gkl);

    setenv("PRISC_PROJECT_ROOT", cwd, 1);
    setenv("PRISC_PROJECT_ID", "mutaclsym", 1);

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, NULL);
    launch("./system/keyboard_input", NULL, NULL);

    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        fprintf(stderr, "[Orchestrator] Using PAL layout: %s\n", pal_layout);
        launch("./system/chtpm_parser_pal", pal_layout, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Using default layout: pieces/chtpm/layouts/game.chtpm\n");
        launch("./system/chtpm_parser_pal", "pieces/chtpm/layouts/game.chtpm", NULL);
    }

    /* Best effort: chtpm_rgb_render */
    {
        struct stat st;
        if (stat("./system/chtpm_rgb_render", &st) == 0)
            launch("./system/chtpm_rgb_render", NULL, NULL);
    }

    /* Best effort: gl_mirror (skip if NO_GL is set) */
    if (!getenv("NO_GL")) {
        struct stat st;
        if (stat("./system/gl_mirror", &st) == 0)
            launch("./system/gl_mirror", NULL, NULL);
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
