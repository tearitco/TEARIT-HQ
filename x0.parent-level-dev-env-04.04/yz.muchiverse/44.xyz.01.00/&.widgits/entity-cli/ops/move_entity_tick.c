/*
 * move_entity_tick.c
 *
 * House prisc+ops standard op, ticked by move_entity.pal (see
 * PRISC-OPS-ARCHITECTURE.md, world_manager_tick.c's own cursor-
 * polling convention). Consumes ONE waypoint from animation_queue.txt
 * per invocation (append-only ledger, cursor-based - never re-reads
 * earlier lines) and writes it to desktop_pos.txt.
 *
 * REAL FIX 2026-09-27 (second pass, direct live report - "the
 * animation loop doesn't draw the entity... its entry in bottom tb
 * keeps flickering... why cant we just move its position w/o doing
 * that?"): this op used to kill the entity's live process and
 * relaunch it fresh at every waypoint. That fixed the earlier
 * disappearance bug (a confirmed-dead check replaced a blind kill +
 * fixed sleep) but was the wrong mechanism for ANIMATION - tearing
 * down and recreating the whole X11 window + taskbar registration on
 * every ~90ms tick is exactly what caused the purple-flash/flicker:
 * the window never got to actually paint before the next kill, and
 * the taskbar entry re-registered on every relaunch.
 *
 * The entity's own process is already running with a live X11
 * connection and window - moving it just needs XMoveWindow(), no
 * relaunch, no re-registration. This op now only writes the new
 * position and bumps an append-only marker file
 * (desktop_pos_changed.txt - size growth, never mtime, per house
 * convention) that khtpm_entity.c's own event loop polls each idle
 * tick to self-move via XMoveWindow, keeping its cached win_x/win_y
 * in the same branch as the move so they never disagree (see that
 * file's own 2026-09-27 header comment at the top of its event loop).
 *
 * When the queue is drained, this op removes the ephemeral queue/
 * cursor files and sends SIGTERM to its own parent (the running
 * prisc+x process) to end the .pal loop - prisc+x's OP_EXEC does not
 * consult an exec'd op's exit status for branching (see prisc+x.c),
 * so self-termination is the reliable way to stop a per-move pal
 * instance instead of leaking it as a stray background process.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_PATH 4096

int main(void) {
    const char *entity_dir = getenv("ENTITY_DIR");
    if (!entity_dir) {
        fprintf(stderr, "ERROR: ENTITY_DIR not set\n");
        return 1;
    }

    char cursor_path[MAX_PATH], queue_path[MAX_PATH];
    snprintf(cursor_path, sizeof(cursor_path), "%s/animation_queue.cursor", entity_dir);
    snprintf(queue_path, sizeof(queue_path), "%s/animation_queue.txt", entity_dir);

    int cursor = 0;
    FILE *cf = fopen(cursor_path, "r");
    if (cf) {
        if (fscanf(cf, "%d", &cursor) != 1) cursor = 0;
        fclose(cf);
    }

    FILE *qf = fopen(queue_path, "r");
    if (!qf) {
        kill(getppid(), SIGTERM);
        return 0;
    }

    char line[128];
    int line_no = 0;
    int target_x = -1, target_y = -1;
    while (fgets(line, sizeof(line), qf)) {
        line_no++;
        if (line_no <= cursor) continue;
        if (sscanf(line, "x=%d|y=%d", &target_x, &target_y) == 2) break;
        target_x = -1;
        target_y = -1;
    }
    fclose(qf);

    if (target_x < 0 || target_y < 0) {
        /* Queue drained: clean up ephemeral state, stop the loop. */
        unlink(queue_path);
        unlink(cursor_path);
        kill(getppid(), SIGTERM);
        return 0;
    }

    /* Write the new waypoint - preserve z. */
    char pos_path[MAX_PATH];
    snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", entity_dir);
    int z = 0;
    FILE *pf = fopen(pos_path, "r");
    if (pf) {
        char pl[128];
        while (fgets(pl, sizeof(pl), pf)) { if (sscanf(pl, "z=%d", &z) == 1) break; }
        fclose(pf);
    }
    pf = fopen(pos_path, "w");
    if (pf) { fprintf(pf, "x=%d\ny=%d\nz=%d\n", target_x, target_y, z); fclose(pf); }

    /* Bump the append-only marker - khtpm_entity.c's own event loop
     * polls this file's SIZE (never mtime) to know a new position is
     * ready to read and self-move to. */
    char marker_path[MAX_PATH];
    snprintf(marker_path, sizeof(marker_path), "%s/desktop_pos_changed.txt", entity_dir);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fprintf(mf, "x\n"); fclose(mf); }

    /* Advance cursor. */
    FILE *wc = fopen(cursor_path, "w");
    if (wc) { fprintf(wc, "%d\n", line_no); fclose(wc); }

    return 0;
}
