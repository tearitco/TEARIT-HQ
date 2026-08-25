#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MODULE_NAME "wrai-text-editor"
#define PROJECT_DIR "projects/wraith-alpha/wraith-projects/wrai-text-editor"

/* 2026-07-11: session/state.txt (active_page, browser_mode, etc.) and
   session/state_changed.txt (queued PROJECT_PAGE: transitions) are
   per-PROJECT files, not per-window-instance -- they persist across
   window opens. Without resetting them here, reopening the editor
   silently resumed whatever page a PREVIOUS window instance/session was
   last left on (e.g. file_menu), instead of starting on the Editor page
   as expected -- confirmed live. trigger_initial_render() only runs
   ONCE, exactly when a genuinely new window is opened (wraith-alpha_manager.c's
   launch path calls this manager once per window creation, unlike the
   ops binary which runs on every keypress) -- the correct, one-time
   place to reset per-open state before the first real render, without
   adding any per-keypress overhead. */
static void reset_session_state(void) {
    FILE *f;
    f = fopen(PROJECT_DIR "/session/state.txt", "w");
    if (f) fclose(f);
    f = fopen(PROJECT_DIR "/session/state_changed.txt", "w");
    if (f) fclose(f);
}

static void trigger_initial_render(void) {
    pid_t pid = fork();
    if (pid == 0) {
        execl(PROJECT_DIR "/ops/+x/wraith_project_input.+x",
              PROJECT_DIR "/ops/+x/wraith_project_input.+x", PROJECT_DIR, NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    reset_session_state();
    trigger_initial_render();

    return 0;
}
