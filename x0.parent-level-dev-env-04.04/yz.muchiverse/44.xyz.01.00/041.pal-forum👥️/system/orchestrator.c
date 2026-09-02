/* orchestrator.c - background service launcher + 3-layer cascading kill
 * for pal-forum (mass-refactor 2026-07-26, ported from 041.pal-chain's
 * own orchestrator.c - see that file's own header comment for the full
 * adaptation rationale: background-launcher mode, not the foreground
 * process, so button.sh's own keyboard_input-stays-foreground exit UX
 * is preserved; no compile-on-launch, since scripts/build.sh already
 * owns real per-platform build logic; session-scoped kill via
 * pieces/os/kill_all.sh's own cwd-matching, since pal-forum runs
 * multiple concurrent sessions launching identical relative argv from
 * different session dirs.
 *
 * Unlike pal-chain, forum_inbox_watcher is launched unconditionally
 * here at startup (not gated behind a later "start watching" action) -
 * matches this project's own pre-orchestrator button.sh behavior (see
 * that file's own header comment: "a chat/social app is even less
 * usable than a wallet if incoming messages just don't show up by
 * default"), preserved rather than silently changed by this
 * conversion. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>

static volatile int should_exit = 0;
static volatile int shutdown_done = 0;

static void kill_process_group(void) {
    kill(0, SIGTERM);
    usleep(100000);
}

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
        if (sscanf(line, "%d %127s", &pid, name) == 2) kill(pid, SIGTERM);
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

static void run_final_kill_sweep(void) {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "pieces/os/kill_all.sh", cwd, NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
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

static pid_t launch(const char *path, const char **argv, int argc) {
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
}

static void ensure_directories(void) {
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
}

static int quit_requested(void) {
    struct stat st;
    if (stat("pieces/system/quit_flag.txt", &st) != 0) return 0;
    return st.st_size > 0;
}

int main(void) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "[Orchestrator] Starting pal-forum session from %s\n", cwd);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    ensure_directories();

    FILE *pl = fopen("pieces/os/proc_list.txt", "w");
    if (pl) fclose(pl);
    log_pid(getpid(), "orchestrator");

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, 0);

    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        const char *args[] = { pal_layout };
        launch("./system/chtpm_parser_pal", args, 1);
    }

    struct stat st;
    if (stat("./system/chtpm_rgb_render", &st) == 0) {
        launch("./system/chtpm_rgb_render", NULL, 0);
    }

    if (!getenv("NO_NET") && stat("./ops/+x/palnet_peer.+x", &st) == 0) {
        const char *args[] = { "forum_node", "pal-forum", "-", "net/outbox.txt", "net/inbox.txt", "forum_node" };
        launch("./ops/+x/palnet_peer.+x", args, 6);
    }

    if (!getenv("NO_NET") && stat("./ops/+x/forum_inbox_watcher.+x", &st) == 0) {
        launch("./ops/+x/forum_inbox_watcher.+x", NULL, 0);
    }

    fprintf(stderr, "[Orchestrator] Ready. Waiting for quit_flag.txt or signal.\n");

    while (!should_exit) {
        if (quit_requested()) {
            should_exit = 1;
            do_shutdown();
            break;
        }
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            fprintf(stderr, "[Orchestrator] Child %d exited\n", dead);
        }
        usleep(200000);
    }

    fprintf(stderr, "[Orchestrator] Exit.\n");
    return 0;
}
