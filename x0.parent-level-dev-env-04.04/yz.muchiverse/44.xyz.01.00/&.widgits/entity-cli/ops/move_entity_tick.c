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
#include <time.h>

#include "khtpm_move_range.c"
#define MAX_PATH 4096

int main(void) {
    const char *entity_dir = getenv("ENTITY_DIR");
    if (!entity_dir) {
        fprintf(stderr, "ERROR: ENTITY_DIR not set\n");
        return 1;
    }

    char cursor_path[MAX_PATH], queue_path[MAX_PATH];  /* cursor_path: kept for the paths helper */
    mvr_queue_paths(entity_dir, queue_path, sizeof(queue_path), cursor_path, sizeof(cursor_path));

    /* Queue read = the SHARED khtpm_move_range.c ledger+cursor helpers. */
    if (access(queue_path, F_OK) != 0) {
        kill(getppid(), SIGTERM);
        return 0;
    }
    int target_x = -1, target_y = -1, target_z = -1, line_no = 0;
    if (!mvr_queue_next(entity_dir, &target_x, &target_y, &target_z, &line_no)
        || target_x < 0 || target_y < 0) {
        /* Queue drained: clean up ephemeral state, stop the loop. */
        mvr_queue_clear(entity_dir);
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

    /* REAL FIX 2026-09-28 (bug_bounty.md "world_manager sustained CPU
     * throttling" entry, direct instruction: "we need some sort of
     * master ledger trunking strategy early" for scaling to more
     * entities). PRODUCER-owns-write half of the push model: append
     * this entity's own move directly to a house-wide, append-only
     * entities_live.ledger, so world_manager_tick.c can pick it up by
     * reading forward from a cursor (O(moves since last tick)) instead
     * of forking sync_entity_positions.+x to recursively `find` every
     * entity's desktop_pos.txt house-wide every tick (O(all entities),
     * gets slower as more entities are added regardless of how many
     * actually moved - the real scaling problem, separate from the
     * crash-loop bug fixed the same day). house_root is derived the
     * same way sync_entity_positions.c already does (no HOUSE_ROOT env
     * var is actually exported despite this file's own header comment
     * claiming one is - confirmed by grep, not assumed) - entity_dir's
     * own path always contains "/xyzfs/" between house_root and the
     * per-user tree, same structural assumption already relied on
     * house-wide. Best-effort (no fprintf return check) - a missed
     * ledger append just means world_manager's next periodic full
     * resync (still present as a self-healing fallback) catches it
     * late, never a correctness or crash risk. */
    {
        char entity_dir_copy[MAX_PATH];
        snprintf(entity_dir_copy, sizeof(entity_dir_copy), "%s", entity_dir);
        char *xyzfs_ptr = strstr(entity_dir_copy, "/xyzfs/");
        char *pals_ptr = strstr(entity_dir_copy, "/pals/");
        if (xyzfs_ptr && pals_ptr) {
            *xyzfs_ptr = '\0';
            char entity_id[256];
            snprintf(entity_id, sizeof(entity_id), "%s", pals_ptr + strlen("/pals/"));
            char *next_slash = strchr(entity_id, '/');
            if (next_slash) *next_slash = '\0';
            char ledger_path[MAX_PATH];
            snprintf(ledger_path, sizeof(ledger_path),
                     "%s/&.hq-apps/world-manager/state/entities_live.ledger",
                     entity_dir_copy);
            FILE *lf = fopen(ledger_path, "a");
            if (lf) {
                fprintf(lf, "%s | x=%d | y=%d | ts=%ld\n", entity_id, target_x, target_y, (long)time(NULL));
                fclose(lf);
            }
        }
    }

    /* Advance cursor. */
    mvr_queue_advance(entity_dir, line_no);

    return 0;
}
