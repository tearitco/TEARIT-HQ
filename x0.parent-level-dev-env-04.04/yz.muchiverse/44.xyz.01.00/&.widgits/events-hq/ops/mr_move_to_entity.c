/* mr_move_to_entity - real "Move" event command (pathfind-to-entity
 * half), PLAY-MODE-ENTITY-HARNESS-DESIGN.md §3's "move" Common Event -
 * direct instruction: "we will use move / pathfinding event program to
 * make cursword move to castle."
 *
 * Usage: mr_move_to_entity.+x <mover_pal_dir> <target_pal_dir> [house_root]
 *   mover_pal_dir: the entity that actually moves (cursword, today's
 *     only real mover - see PLAY-MODE-ENTITY-HARNESS-DESIGN.md §5, "the
 *     entity harness" - this op is real precedent for that harness's
 *     own movement primitive, not the harness itself).
 *   target_pal_dir: the entity to walk toward (its own real, current
 *     desktop_pos.txt x/y - re-read every step, so a MOVING target
 *     would still be chased, though nothing moves targets yet).
 *
 * Mechanism: cursword's own real position is only ever changed by ITS
 * OWN live process (a real click-to-place event moves the actual X11
 * window, not just a file another process could blindly overwrite) -
 * this op drives that exact same real code path via the house-standard
 * "AI-injection power" relay channel (khtpm_core_render.c's own
 * interact_relay.txt poll, already used for RAISE/RUN_METHOD/etc), a
 * NEW `MOVE_TO:<x>,<y>` command added for this feature (real click-to-
 * place grid-snap + XMoveWindow + write_pos(), same code, relay-driven
 * instead of a real click). One real grid-cell step per relay write,
 * a real pause between steps so movement is honestly visible (a
 * genuine walk, not a teleport), re-confirmed via a fresh read of the
 * mover's own desktop_pos.txt before committing the next step (so a
 * step that didn't land - process not yet caught the relay - doesn't
 * silently get skipped).
 *
 * Blocking, like every other real event command op (mr_show_text.c
 * etc) - the real "smallest provable proof" bar
 * (EVENT-TRIGGER-LAYER-PLAN.md §3 Step 3's own framing): walk into it,
 * something happens, checkable, no manual step. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4352
#define MAX_STEPS 64
#define STEP_DELAY_USEC 400000 /* 400ms/step - real, visible, not instant */
#define POLL_DELAY_USEC 50000
#define POLL_TRIES 20 /* up to 1s waiting for a step to actually land */

static int read_pos(const char *pal_dir, int *x, int *y) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", pal_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int gx = 0, gy = 0, found_x = 0, found_y = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) { gx = atoi(line + 2); found_x = 1; }
        else if (strncmp(line, "y=", 2) == 0) { gy = atoi(line + 2); found_y = 1; }
    }
    fclose(f);
    if (!found_x || !found_y) return 0;
    *x = gx; *y = gy;
    return 1;
}

static int read_grid_cell_px(const char *house_root) {
    if (!house_root || !house_root[0]) return 80;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 80;
    char line[256];
    int result = 80;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        char *bar2 = strchr(p, '|');
        if (!bar2) continue;
        int v = atoi(bar2 + 1);
        if (v > 0) result = v;
    }
    fclose(f);
    return result;
}

static void write_relay(const char *mover_pal_dir, const char *cmd) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/interact_relay.txt", mover_pal_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%s\n", cmd);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mr_move_to_entity.+x <mover_pal_dir> <target_pal_dir> [house_root]\n");
        return 1;
    }
    const char *mover_dir = argv[1];
    const char *target_dir = argv[2];
    const char *house_root = argc >= 4 ? argv[3] : "";

    int cell = read_grid_cell_px(house_root);
    int steps_taken = 0;

    for (int step = 0; step < MAX_STEPS; step++) {
        int mx, my, tx, ty;
        if (!read_pos(mover_dir, &mx, &my)) {
            fprintf(stderr, "mr_move_to_entity: cannot read mover position\n");
            return 1;
        }
        /* Real target re-read every step, not just once - a moving
         * target would still be chased (nothing moves targets yet,
         * but the op is honestly built to handle it, not just the
         * static case it's tested against). */
        if (!read_pos(target_dir, &tx, &ty)) {
            fprintf(stderr, "mr_move_to_entity: cannot read target position\n");
            return 1;
        }

        int dx = tx - mx, dy = ty - my;
        /* Arrived: the EXACT same cell, not merely adjacent - matches
         * how the real trigger check works (EVENT-TRIGGER-LAYER-PLAN.
         * md's own check_player_touch_trigger(): exact x,y equality
         * against the registered tile, not "close enough"). An
         * adjacency-only arrival would report "arrived" without ever
         * actually stepping onto the target's own real tile -
         * confirmed live: cursword started exactly one cell from
         * castle, and a `<= cell` check called that "arrived" with
         * zero real steps taken, which is not what walking INTO
         * something means. */
        if (dx == 0 && dy == 0) {
            printf("mr_move_to_entity: arrived at target (mover %d,%d target %d,%d) after %d step(s)\n",
                   mx, my, tx, ty, steps_taken);
            return 0;
        }

        int step_x = mx, step_y = my;
        if (dx > 0) step_x = mx + cell; else if (dx < 0) step_x = mx - cell;
        if (dy > 0) step_y = my + cell; else if (dy < 0) step_y = my - cell;

        char cmd[64];
        snprintf(cmd, sizeof(cmd), "MOVE_TO:%d,%d", step_x, step_y);
        write_relay(mover_dir, cmd);
        steps_taken++;

        /* Poll for real confirmation the step actually landed (the
         * mover's own live process must poll and apply the relay -
         * same real tolerance every other relay-injection consumer in
         * this house already has for "wrote it, but is it applied
         * yet") before pacing the next step. */
        int landed = 0;
        for (int p = 0; p < POLL_TRIES; p++) {
            usleep(POLL_DELAY_USEC);
            int nx, ny;
            if (read_pos(mover_dir, &nx, &ny) && nx == step_x && ny == step_y) { landed = 1; break; }
        }
        if (!landed) {
            fprintf(stderr, "mr_move_to_entity: step to %d,%d did not land - mover process not running?\n", step_x, step_y);
            return 1;
        }
        usleep(STEP_DELAY_USEC);
    }

    fprintf(stderr, "mr_move_to_entity: gave up after %d steps, never reached target\n", MAX_STEPS);
    return 1;
}
