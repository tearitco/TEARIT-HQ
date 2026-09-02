// orchestrator.c - TPMOS launcher for pal-chat-irc
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

/* === LAUNCH WITH OUTPUT REDIRECT, ARBITRARY ARGV (XYZOS-PITFALLS #20) ===
 * launch_redirect() below only supports a FIXED 2-slot execl() - fine for
 * chat_inbox_watcher (no args) but silently wrong for palnet_peer, which
 * needs 5-6 real argv slots (own_kind, project_id, piece_id, outbox_file,
 * inbox_file, [seek_kind]). Passing all of them as one crammed string in
 * arg2 does NOT get shell-split by execl() - palnet_peer sees argc=3 and
 * exits on its own usage message, silently, into the log file nobody
 * checks by default. Use THIS launcher (real argv[] + execv, mirrors
 * 041.pal-chain's own orchestrator.c launch()) for any child needing more
 * than 2 arguments. */
static pid_t launch_argv_redirect(const char *path, const char **argv, int argc,
                                   const char *logfile) {
    pid_t pid = fork();
    if (pid == 0) {
        FILE *lf = fopen(logfile, "w");
        if (lf) {
            int fd = fileno(lf);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            fclose(lf);
        } else {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        }
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
}

/* === LAUNCH WITH OUTPUT REDIRECT (best effort, bible §3) === */

static pid_t launch_redirect(const char *path, const char *arg1,
                             const char *arg2, const char *logfile) {
    pid_t pid = fork();
    if (pid == 0) {
        FILE *lf = fopen(logfile, "w");
        if (lf) {
            int fd = fileno(lf);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            fclose(lf);
        } else {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        }
        if (arg2)
            execl(path, path, arg1, arg2, NULL);
        else if (arg1)
            execl(path, path, arg1, NULL);
        else
            execl(path, path, NULL);
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
    compile_single("ops/palnet_peer.c", "ops/+x/palnet_peer.+x", NULL);
    compile_single("ops/chat_create_user.c", "ops/+x/chat_create_user.+x", NULL);
    compile_single("ops/chat_switch_user.c", "ops/+x/chat_switch_user.+x", NULL);
    compile_single("ops/chat_post_message.c", "ops/+x/chat_post_message.+x", NULL);
    compile_single("ops/chat_inbox_watcher.c", "ops/+x/chat_inbox_watcher.+x", NULL);
    compile_single("ops/chat_menu_input.c", "ops/+x/chat_menu_input.+x", NULL);
    compile_single("ops/chat_compose_frame.c", "ops/+x/chat_compose_frame.+x", NULL);
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
              "pieces/os", "ops/+x",
              "users", "rooms", "net", NULL);
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
    fprintf(stderr, "[Orchestrator] Starting pal-chat-irc from %s\n", cwd);

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
    if (qf) fclose(qf);
    FILE *ob = fopen("net/outbox.txt", "w");
    if (ob) fclose(ob);
    FILE *ib = fopen("net/inbox.txt", "w");
    if (ib) fclose(ib);

    setenv("PRISC_PROJECT_ROOT", cwd, 1);
    setenv("PRISC_PROJECT_ID", "pal-chat-irc", 1);

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, NULL);
    launch("./system/keyboard_input", NULL, NULL);

    /* Check for PAL layout override (button.sh --pal sets PAL_LAYOUT) */
    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        fprintf(stderr, "[Orchestrator] Using PAL layout: %s\n", pal_layout);
        launch("./system/chtpm_parser_pal", pal_layout, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Using default layout: pieces/chtpm/layouts/login.chtpm\n");
        launch("./system/chtpm_parser_pal", "pieces/chtpm/layouts/login.chtpm", NULL);
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

    /* Best effort: launch IRC peer (skip if NO_NET or binary missing) */
    {
        const char *no_net = getenv("NO_NET");
        if (!no_net) {
            if (access("ops/+x/palnet_peer.+x", X_OK) == 0) {
                const char *pn_args[] = { "irc_node", "pal-chat-irc", "-",
                                           "net/outbox.txt", "net/inbox.txt", "irc_node" };
                launch_argv_redirect("ops/+x/palnet_peer.+x", pn_args, 6,
                                      "/tmp/pal_chat_irc_palnet_peer.log");
            } else {
                fprintf(stderr, "[Orchestrator] palnet_peer binary not found, skipping.\n");
            }
        } else {
            fprintf(stderr, "[Orchestrator] NO_NET set, skipping palnet_peer.\n");
        }
    }

    /* Best effort: launch inbox watcher (skip if NO_NET or binary missing) */
    {
        const char *no_net = getenv("NO_NET");
        if (!no_net) {
            if (access("ops/+x/chat_inbox_watcher.+x", X_OK) == 0) {
                launch_redirect("ops/+x/chat_inbox_watcher.+x",
                                NULL, NULL,
                                "/tmp/pal_chat_irc_inbox_watcher.log");
            } else {
                fprintf(stderr, "[Orchestrator] chat_inbox_watcher binary not found, skipping.\n");
            }
        } else {
            fprintf(stderr, "[Orchestrator] NO_NET set, skipping chat_inbox_watcher.\n");
        }
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
