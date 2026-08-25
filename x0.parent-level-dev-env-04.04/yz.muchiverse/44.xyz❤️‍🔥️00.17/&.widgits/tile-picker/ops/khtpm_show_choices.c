/* khtpm_show_choices - real, generic "Show Choices" event command,
 * 2026-08-05. Real RPG Maker command (confirmed,
 * #.ref/menu/event.commands.1.txt), first real proof-of-concept target:
 * MUCHI_RANCHER's own Change Gold. This is the SECOND, harder half -
 * Show Choices, real branching - built for a real book-stack reading
 * app, reusable by ANY entity's own event.pal.
 *
 * REAL FIX 2026-08-16, direct instruction ("its very old lets fix it to
 * use khtpm. it should still show books and random verse"): the
 * original design here (write a SHOW_PAGE relay command into the
 * entity's own interact_relay.txt, poll a result file) was real but
 * never actually got used - the deployed khtpm_show_choices.+x binary
 * was, in practice, a build of the old GLX-based tp_picker_window.c
 * (confirmed live: hung in a real X event loop with zero error but
 * never produced a mapped/visible window - a real, separate,
 * pre-existing GLX bug). Real fix: fork+exec the new khtpm_choice_
 * picker.+x directly (same real shared khtpm_render_core.c Elem model
 * every other khtpm app this session uses, real proven override_
 * redirect/phantom-click/focus fixes) instead of relaying through the
 * entity's own live main loop - simpler, lower risk (doesn't touch
 * tp_desktop_window_rgb.c's SHOW_PAGE relay handler at all), and this
 * binary's own real contract (poll a result file, print the picked
 * token to stdout) stays completely unchanged for dispatch.sh's own
 * $() capture - only what generates that result file changed.
 *
 * Usage: khtpm_show_choices.+x <entity_package_dir> <choices_objects_file>
 *   entity_package_dir: kept for real positional compatibility with
 *   every existing caller (dispatch.sh etc) - not read by the new
 *   picker itself, real future use would be entity-relative window
 *   placement.
 *   choices_objects_file: a real, flat "OBJECT | label=.. | action=.."
 *   list (no PAGE header needed - always exactly one page). The caller
 *   is responsible for generating this file (real content, not
 *   hardcoded here) before calling this op.
 *
 * Real result file: a fresh temp file under /tmp, cleaned up after.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352
#define POLL_TIMEOUT_SEC 120

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: khtpm_show_choices.+x <entity_package_dir> <choices_objects_file>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *choices_file = argv[2];

    /* REAL FIX 2026-08-16, direct instruction ("make sure teh new
     * choices show p near the actual entity"): read the entity's own
     * real on-screen position (package_dir/desktop_pos.txt - "x=.."/
     * "y=.." lines, the same file tp_desktop_window_rgb.c's own
     * write_pos() maintains) so the picker opens near the entity
     * instead of a fixed default spot. */
    char pos_path[PATH_BUF];
    snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", package_dir);
    int pos_x = -1, pos_y = -1;
    FILE *pf = fopen(pos_path, "r");
    if (pf) {
        char pline[128];
        while (fgets(pline, sizeof(pline), pf)) {
            if (strncmp(pline, "x=", 2) == 0) pos_x = atoi(pline + 2);
            else if (strncmp(pline, "y=", 2) == 0) pos_y = atoi(pline + 2);
        }
        fclose(pf);
    }
    char pos_x_str[16], pos_y_str[16];
    if (pos_x >= 0) snprintf(pos_x_str, sizeof(pos_x_str), "%d", pos_x);
    if (pos_y >= 0) snprintf(pos_y_str, sizeof(pos_y_str), "%d", pos_y);

    char result_path[PATH_BUF];
    snprintf(result_path, sizeof(result_path), "/tmp/khtpm_choice_result_%d.txt", (int)getpid());
    unlink(result_path);

    /* Real self-relative resolution (same real convention this house
     * uses everywhere - readlink /proc/self/exe, not a hardcoded
     * install path) to find the new picker binary next to this one. */
    char self_path[PATH_BUF];
    ssize_t slen = self_exe_readlink(self_path, sizeof(self_path));
    if (slen <= 0) { fprintf(stderr, "khtpm_show_choices: cannot resolve own path\n"); return 1; }
    self_path[slen] = '\0';
    char *last_slash = strrchr(self_path, '/');
    if (!last_slash) { fprintf(stderr, "khtpm_show_choices: bad own path\n"); return 1; }
    *last_slash = '\0';
    char picker_path[PATH_BUF];
    snprintf(picker_path, sizeof(picker_path), "%s/khtpm_choice_picker.+x", self_path);

    pid_t pid = fork();
    if (pid == 0) {
        /* macOS leg (2026-08-22): the picker must NOT inherit this
         * process's stdout. Callers capture our stdout with $(...) -
         * the pipe only EOFs when EVERY holder exits, so a long-lived
         * picker holding it left dispatch.sh (and every other caller)
         * blocked forever even after we printed the pick and exited.
         * Point the child's stdout at /dev/null; stderr stays for
         * diagnostics. */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); close(devnull); }
        if (pos_x >= 0 && pos_y >= 0)
            execl(picker_path, picker_path, choices_file, result_path, pos_x_str, pos_y_str, (char *)NULL);
        else
            execl(picker_path, picker_path, choices_file, result_path, (char *)NULL);
        _exit(1);
    } else if (pid < 0) {
        fprintf(stderr, "khtpm_show_choices: fork failed\n");
        return 1;
    }

    int waited = 0;
    char picked[256] = "";
    while (waited < POLL_TIMEOUT_SEC * 10) {
        FILE *f = fopen(result_path, "r");
        if (f) {
            if (fgets(picked, sizeof(picked), f)) {
                picked[strcspn(picked, "\r\n")] = '\0';
            }
            fclose(f);
            if (picked[0]) break;
        }
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            /* picker exited (Escape/close) without ever writing a
             * result - stop polling immediately instead of waiting out
             * the full timeout for a process that's already gone. */
            break;
        }
        usleep(100000);
        waited++;
    }
    waitpid(pid, NULL, WNOHANG);
    unlink(result_path);

    if (!picked[0]) {
        fprintf(stderr, "khtpm_show_choices: no pick made (cancelled or timed out)\n");
        return 2;
    }
    printf("%s\n", picked);
    return 0;
}
