/* orchestrator.c - background service launcher + 3-layer cascading kill
 * for pal-chain (mass-refactor 2026-07-26, ported from
 * 01.muchi-pals-🥚️-13.01's session-aware orchestrator.c - see that
 * file's own header comment for the full adaptation rationale:
 * background-launcher mode, not the foreground process, so button.sh's
 * own keyboard_input-stays-foreground exit UX is preserved; no
 * compile-on-launch, since scripts/build.sh already owns real per-
 * platform build logic; session-scoped kill via pieces/os/kill_all.sh's
 * own cwd-matching, since pal-chain runs multiple concurrent sessions
 * launching identical relative argv from different session dirs. */
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

/* CATEGORY B FIX 2026-08-20 (see SIMLINK_PITFALL.md / sim-smell-fix.md).
 * Step 1 of a staged, one-at-a-time migration - button.sh is NOT yet
 * changed (still symlinks, PRISC_PROJECT_ROOT still $SESSION_DIR), so
 * project_root_path == session_root_path == CWD for now and this is a
 * behavior-inert change - only takes effect once button.sh's own step
 * (later, separately tested) actually diverges the two. */
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
    if (stat("pieces/system/quit_flag.txt", &st) == 0 && st.st_size > 0) return 1;
    char qpath[2048];
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
