#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MODULE_NAME "settings"
#define PROJECT_DIR "projects/wraith-alpha/wraith-projects/settings"

/*
 * settings_manager.c -- init-only manager for the "settings" hub project.
 *
 * 2026-07-11: rewritten to match the standard manager/ops pattern every
 * other correctly-structured Wraith project uses (see
 * !.TPMOS_ONBORD_BIBLE_10.md section 13.2, piececraft-wraith's own
 * architecture history). This file previously owned a persistent
 * usleep(50000) polling loop that watched session/state_changed.txt for
 * growth and re-rendered on its own timer -- the exact "manager owns a
 * background thread that polls" pattern already identified, in this
 * same codebase's own history, as deprecated: it causes a real, provable
 * lag between an action and its visible effect (up to one poll interval),
 * whereas the correct shape is: manager runs once at startup to avoid a
 * blank window before the first keypress, and
 * ops/src/wraith_project_input.c is invoked SYNCHRONOUSLY by
 * wraith-alpha_manager.c's route_command() for every relevant action
 * (KEY:/PROJECT_ACTION:/SETTINGS_PAGE:), computing and publishing fresh
 * state in that same call -- no timer, no race, no marker-file-growth
 * polling of any kind. All of the actual page-computation logic that
 * used to live directly in this file (discover_entries(), write_state(),
 * write_wraith_body(), the persistent while(1) loop) has moved to
 * ops/src/wraith_project_input.c.
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
