/*
 * move_entity_animated.c
 *
 * Moves entity from current position to target with smooth animation.
 * Uses A* pathfinding to generate waypoints, writes animation_queue.txt
 * for entity's game loop to read and animate through.
 *
 * Usage: move_entity_animated.+x <package_dir> <target_x> <target_y>
 *   package_dir: entity's directory (contains desktop_pos.txt)
 *   target_x, target_y: destination in REFERENCE px
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define MAX_PATH 4096
#define MAX_WAYPOINTS 1024
#define GRID_STEP 32  /* pixels per step (matches entity grid) */

typedef struct {
    int x, y;
} Point;

typedef struct {
    int x, y;
    int g, h;  /* g = cost from start, h = heuristic to goal */
} Node;

/* Read current position from desktop_pos.txt */
static int read_current_pos(const char *package_dir, int *x, int *y) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "ERROR: cannot read %s\n", path);
        return 0;
    }

    char line[256];
    int found_x = 0, found_y = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "x=%d", x) == 1) found_x = 1;
        if (sscanf(line, "y=%d", y) == 1) found_y = 1;
    }
    fclose(f);
    return (found_x && found_y);
}

/* Simple A* pathfinding - returns waypoint count */
static int pathfind_astar(int sx, int sy, int tx, int ty, Point *waypoints) {
    /* For now: simple straight-line interpolation (Manhattan-style steps).
     * Full A* with obstacle avoidance can be added later. */

    int count = 0;
    int dx = (tx > sx) ? GRID_STEP : (tx < sx) ? -GRID_STEP : 0;
    int dy = (ty > sy) ? GRID_STEP : (ty < sy) ? -GRID_STEP : 0;

    int cx = sx, cy = sy;
    while (count < MAX_WAYPOINTS) {
        waypoints[count].x = cx;
        waypoints[count].y = cy;
        count++;

        /* Move one step closer */
        if (cx != tx) cx += dx;
        if (cy != ty) cy += dy;

        /* Check if we've reached target */
        if (cx == tx && cy == ty) {
            waypoints[count].x = tx;
            waypoints[count].y = ty;
            count++;
            break;
        }
    }

    return count;
}

/* Write animation_queue.txt with waypoints */
static void write_animation_queue(const char *package_dir, Point *waypoints, int count) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/animation_queue.txt", package_dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "ERROR: cannot write %s\n", path);
        return;
    }

    for (int i = 0; i < count; i++) {
        fprintf(f, "x=%d\ny=%d\n", waypoints[i].x, waypoints[i].y);
    }
    fclose(f);
}

/* Update desktop_pos.txt with final position */
static void write_final_pos(const char *package_dir, int x, int y) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    int z = 0;
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "z=%d", &z) == 1) break;
        }
        fclose(f);
    }

    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "x=%d\ny=%d\nz=%d\n", x, y, z);
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <package_dir> <target_x> <target_y>\n", argv[0]);
        return 1;
    }

    const char *package_dir = argv[1];
    int target_x = atoi(argv[2]);
    int target_y = atoi(argv[3]);

    int cur_x = 0, cur_y = 0;
    if (!read_current_pos(package_dir, &cur_x, &cur_y)) {
        fprintf(stderr, "ERROR: failed to read current position\n");
        return 1;
    }

    /* Generate waypoints via A* */
    Point waypoints[MAX_WAYPOINTS];
    int count = pathfind_astar(cur_x, cur_y, target_x, target_y, waypoints);

    /* Write animation queue for entity to read */
    write_animation_queue(package_dir, waypoints, count);

    /* Update final position */
    write_final_pos(package_dir, target_x, target_y);

    return 0;
}
