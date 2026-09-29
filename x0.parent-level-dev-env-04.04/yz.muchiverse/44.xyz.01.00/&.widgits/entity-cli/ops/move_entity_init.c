/*
 * move_entity_init.c
 *
 * House prisc+ops standard op (PRISC-OPS-ARCHITECTURE.md, 2026-09-27).
 * Replaces the shell-orchestrated move_entity_with_animation.sh /
 * pathfind_linear.sh / move_entity_animated.c chain from the prior
 * session, which put the animation game loop, frame timing, and
 * kill+relaunch process management directly in shell - a direct
 * violation of that doc's "no shell scripts driving the game loop"
 * rule, and the likely real cause of the entity-disappears-on-Move
 * bug (blind `kill` + fixed `sleep 0.2` racing X11's own async window
 * teardown, not a confirmed-dead check before relaunch).
 *
 * This op owns ALL of the one-shot Move logic: grid-snap, screen-
 * bounds clamp, waypoint pathfinding. It writes the waypoint ledger
 * and launches move_entity.pal (via prisc+x), then exits. Per-frame
 * timing and the actual relaunch-with-confirmed-death happen in
 * move_entity_tick.+x, ticked by that .pal loop - never in this op,
 * never in shell.
 *
 * Usage: move_entity_init.+x <house_root> <entity_dir> <target_x> <target_y>
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_WAYPOINTS 4096
#define ANIM_STEP 8
#define SCREEN_WIDTH 2496
#define SCREEN_HEIGHT 1664

typedef struct { int x, y; } Point;

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int read_pos(const char *entity_dir, int *x, int *y) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", entity_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int fx = 0, fy = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "x=%d", x) == 1) fx = 1;
        if (sscanf(line, "y=%d", y) == 1) fy = 1;
    }
    fclose(f);
    return fx && fy;
}

/* Same pipe-delimited desk_grid.pdl convention fe_place_on_desk.sh /
 * the old move_entity_on_desk.sh read via awk - ported to C so this
 * logic lives in an op, not shell. */
static int read_grid_cell_px(const char *house_root) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 80;
    char line[512];
    int g = 80;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "cell_px")) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        int val = atoi(p2 + 1);
        if (val > 0) g = val;
        break;
    }
    fclose(f);
    return g;
}

static int pathfind_linear(int sx, int sy, int tx, int ty, Point *waypoints) {
    int count = 0;
    int dx = (tx > sx) ? ANIM_STEP : (tx < sx) ? -ANIM_STEP : 0;
    int dy = (ty > sy) ? ANIM_STEP : (ty < sy) ? -ANIM_STEP : 0;
    int cx = sx, cy = sy;
    if (dx == 0 && dy == 0) return 0; /* already there */
    while (count < MAX_WAYPOINTS) {
        int reached_x = (dx == 0) ? (cx == tx) : ((dx > 0) ? (cx >= tx) : (cx <= tx));
        int reached_y = (dy == 0) ? (cy == ty) : ((dy > 0) ? (cy >= ty) : (cy <= ty));
        if (reached_x && reached_y) {
            waypoints[count].x = tx;
            waypoints[count].y = ty;
            count++;
            break;
        }
        if (cx != tx) cx += dx;
        if (cy != ty) cy += dy;
        waypoints[count].x = cx;
        waypoints[count].y = cy;
        count++;
    }
    return count;
}

static void ensure_prisc_built(const char *entity_cli_dir, const char *house_root) {
    char prisc_bin[MAX_PATH];
    snprintf(prisc_bin, sizeof(prisc_bin), "%s/system/prisc+x", entity_cli_dir);
    if (access(prisc_bin, X_OK) == 0) return;

    char sys_dir[MAX_PATH];
    snprintf(sys_dir, sizeof(sys_dir), "%s/system", entity_cli_dir);
    mkdir(sys_dir, 0755);

    char src[MAX_PATH];
    snprintf(src, sizeof(src), "%s/&.widgits/_shared-lib/system/prisc+x.c", house_root);
    if (access(src, F_OK) != 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        execlp("gcc", "gcc", "-O0", "-std=c11", "-w", "-o", prisc_bin, src, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <house_root> <entity_dir> <target_x> <target_y>\n", argv[0]);
        return 1;
    }
    const char *house_root = argv[1];
    const char *entity_dir = argv[2];
    int target_x = atoi(argv[3]);
    int target_y = atoi(argv[4]);

    int g = read_grid_cell_px(house_root);
    if (g <= 0) g = 80;
    target_x = (target_x / g) * g;
    target_y = (target_y / g) * g;
    target_x = clamp_i(target_x, 0, SCREEN_WIDTH);
    target_y = clamp_i(target_y, 0, SCREEN_HEIGHT);

    int cx, cy;
    if (!read_pos(entity_dir, &cx, &cy)) { cx = 0; cy = 0; }

    Point waypoints[MAX_WAYPOINTS];
    int count = pathfind_linear(cx, cy, target_x, target_y, waypoints);

    char queue_path[MAX_PATH];
    snprintf(queue_path, sizeof(queue_path), "%s/animation_queue.txt", entity_dir);
    char cursor_path[MAX_PATH];
    snprintf(cursor_path, sizeof(cursor_path), "%s/animation_queue.cursor", entity_dir);
    char done_path[MAX_PATH];
    snprintf(done_path, sizeof(done_path), "%s/animation_done", entity_dir);
    unlink(done_path);

    if (count == 0) {
        /* Already at (snapped) target - just write it, no pal loop. */
        char pos_path[MAX_PATH];
        snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", entity_dir);
        FILE *pf = fopen(pos_path, "w");
        if (pf) { fprintf(pf, "x=%d\ny=%d\nz=0\n", target_x, target_y); fclose(pf); }
        unlink(queue_path);
        unlink(cursor_path);
        printf("already at target\n");
        return 0;
    }

    FILE *qf = fopen(queue_path, "w");
    if (!qf) { fprintf(stderr, "ERROR: cannot write %s\n", queue_path); return 1; }
    for (int i = 0; i < count; i++) fprintf(qf, "x=%d|y=%d\n", waypoints[i].x, waypoints[i].y);
    fclose(qf);

    FILE *cf = fopen(cursor_path, "w");
    if (cf) { fprintf(cf, "0\n"); fclose(cf); }

    /* Derive entity-cli dir from this binary's own real path:
     * .../entity-cli/ops/move_entity_init.+x -> .../entity-cli */
    char self_path[MAX_PATH];
    if (!realpath(argv[0], self_path)) { fprintf(stderr, "ERROR: realpath(argv[0]) failed\n"); return 1; }
    char self_copy[MAX_PATH];
    strcpy(self_copy, self_path);
    char *ops_dir = dirname(self_copy);
    char ops_copy[MAX_PATH];
    strcpy(ops_copy, ops_dir);
    char *entity_cli_dir = dirname(ops_copy);

    ensure_prisc_built(entity_cli_dir, house_root);

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2); }
        if (chdir(entity_cli_dir) != 0) _exit(1);
        setenv("ENTITY_DIR", entity_dir, 1);
        setenv("HOUSE_ROOT", house_root, 1);
        execl("./system/prisc+x", "./system/prisc+x", "./move_entity.pal", (char *)NULL);
        _exit(1);
    }

    printf("move started: %d waypoints to (%d,%d)\n", count, target_x, target_y);
    return 0;
}
