#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define PROJECT_DIR "projects/wraith-alpha/wraith-projects/context-menu"

/*
 * context-menu_manager.c -- init-only manager for the "context-menu"
 * project (see context-st8.txt at repo root for the full design). Matches
 * the CURRENT correct manager/ops pattern (settings_manager.c, rewritten
 * 2026-07-11 -- see that file's own header comment for why the OLDER
 * persistent-polling-loop pattern, still present in window-geom_manager.c,
 * is deprecated): this file only forks the ops binary once at startup so
 * a window isn't blank before the first keypress; ops/src/wraith_project_input.c
 * does all the real work, invoked SYNCHRONOUSLY by
 * wraith-alpha_manager.c's run_active_project_input_op() on every
 * KEY:/PROJECT_ACTION: dispatched to this project. No timer, no polling.
 */

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

    trigger_initial_render();

    return 0;
}
