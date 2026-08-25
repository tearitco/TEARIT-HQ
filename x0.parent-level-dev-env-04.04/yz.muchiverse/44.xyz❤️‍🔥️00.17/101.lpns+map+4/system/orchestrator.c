// orchestrator.c - TPMOS launcher for LPNS+MAP+4
// Uses fork/exec for all child processes (Bible §3)
// Process management: x0.moke 3-layer cascading kill pattern
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
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
        if (extra_flags)
            execl("/usr/bin/gcc", "gcc", extra_flags, "-o", dst, src, NULL);
        else
            execl("/usr/bin/gcc", "gcc", "-o", dst, src, NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

static void compile_binaries(void) {
    fprintf(stderr, "[Orchestrator] Compiling...\n");
    compile_single("system/game_manager.c", "system/game_manager", "-pthread");
    compile_single("system/keyboard_input.c", "system/keyboard_input", NULL);
    compile_single("system/renderer.c", "system/renderer", NULL);
    compile_single("system/chtpm_parser_pal.c", "system/chtpm_parser_pal", "-Wno-unused-result -Wno-stringop-truncation");
    compile_single("ops/game_compose_frame.c", "ops/game_compose_frame", NULL);
    compile_single("ops/game_turn_input.c", "ops/game_turn_input", NULL);
}

/* === CONFIG/LEDGER WRITERS === */

static void write_config(void) {
    FILE *f = fopen("config.txt", "w");
    if (f) {
        fprintf(f,
            "current_epoch=1\n"
            "current_turn=0\n"
            "num_players=4\n"
            "num_npcs=3\n"
            "epoch_length=5\n"
            "player_1_type=human\n"
            "player_2_type=computer\n"
            "player_3_type=computer\n"
            "player_4_type=computer\n"
            "player_1_name=alice\n"
            "player_2_name=bot1\n"
            "player_3_name=bot2\n"
            "player_4_name=bot3\n"
            "player_1_start_x=0\n"
            "player_1_start_y=0\n"
            "player_2_start_x=7\n"
            "player_2_start_y=0\n"
            "player_3_start_x=0\n"
            "player_3_start_y=7\n"
            "player_4_start_x=7\n"
            "player_4_start_y=7\n");
        fclose(f);
    }
}

static void write_ledger(void) {
    FILE *f = fopen("data/master_ledger.txt", "w");
    if (f) {
        fprintf(f, "timestamp|epoch|player|turn|action_data|action_type\n");
        fclose(f);
    }
}

/* === DIRECTORY SETUP (fork/exec mkdir, Bible §3 compliant) === */

static void ensure_directories(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/mkdir", "mkdir", "-p",
              "data", "pieces/display", "pieces/system",
              "pieces/apps/player_app", "pieces/keyboard",
              "pieces/os", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* === INITIAL COMPOSE (fork/exec, Bible §3 compliant) === */

static void initial_compose(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("./ops/game_compose_frame", "./ops/game_compose_frame", NULL);
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
    fprintf(stderr, "[Orchestrator] Starting LPNS+MAP+4 from %s\n", cwd);

    /* Register signal handlers for 3-layer cleanup */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    compile_binaries();

    fprintf(stderr, "[Orchestrator] Initializing...\n");
    ensure_directories();
    write_config();
    write_ledger();

    /* Clear proc_list.txt */
    FILE *pl = fopen("pieces/os/proc_list.txt", "w");
    if (pl) fclose(pl);
    log_pid(getpid(), "orchestrator");

    /* Clear state files */
    FILE *fc = fopen("pieces/display/frame_changed.txt", "w");
    if (fc) fclose(fc);
    FILE *hr = fopen("pieces/apps/player_app/history.txt", "w");
    if (hr) fclose(hr);
    FILE *kr = fopen("pieces/keyboard/history.txt", "w");
    if (kr) fclose(kr);
    FILE *relay = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (relay) fclose(relay);
    FILE *qf = fopen("pieces/system/quit_flag.txt", "w");
    if (qf) { fclose(qf); }

    initial_compose();

    setenv("PRISC_PROJECT_ROOT", cwd, 1);
    setenv("PRISC_PROJECT_ID", "lpns-map", 1);

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, NULL);
    launch("./system/keyboard_input", NULL, NULL);
    
    /* Check for PAL layout override (button.sh --pal sets PAL_LAYOUT) */
    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        fprintf(stderr, "[Orchestrator] Using PAL layout: %s\n", pal_layout);
        launch("./system/chtpm_parser_pal", pal_layout, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Using C layout: pieces/chtpm/layouts/lpns_main_menu.chtpm\n");
        launch("./system/chtpm_parser_pal", "pieces/chtpm/layouts/lpns_main_menu.chtpm", NULL);
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
