/* dd_drag_drop.c - Simulate X11 drag-drop via xdotool
 *
 * Usage: dd_drag_drop <start_x> <start_y> <end_x> <end_y> [steps] [delay_ms]
 *
 * Simulates: mousedown at start, move to end in steps, mouseup at end.
 * Default: 20 steps, 50ms delay between steps.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <start_x> <start_y> <end_x> <end_y> [steps] [delay_ms]\n", argv[0]);
        return 1;
    }

    int start_x = atoi(argv[1]);
    int start_y = atoi(argv[2]);
    int end_x = atoi(argv[3]);
    int end_y = atoi(argv[4]);
    int steps = (argc > 5) ? atoi(argv[5]) : 20;
    int delay_ms = (argc > 6) ? atoi(argv[6]) : 50;

    char cmd[256];

    /* Move to start position */
    snprintf(cmd, sizeof(cmd), "xdotool mousemove %d %d", start_x, start_y);
    system(cmd);
    usleep(100000); /* 100ms settle */

    /* Press mouse button */
    snprintf(cmd, sizeof(cmd), "xdotool mousedown 1");
    system(cmd);
    usleep(50000); /* 50ms settle */

    /* Move in steps */
    for (int i = 1; i <= steps; i++) {
        int x = start_x + (end_x - start_x) * i / steps;
        int y = start_y + (end_y - start_y) * i / steps;
        snprintf(cmd, sizeof(cmd), "xdotool mousemove %d %d", x, y);
        system(cmd);
        usleep(delay_ms * 1000);
    }

    /* Release mouse button */
    snprintf(cmd, sizeof(cmd), "xdotool mouseup 1");
    system(cmd);

    printf("OK: drag-drop from (%d,%d) to (%d,%d) in %d steps\n",
           start_x, start_y, end_x, end_y, steps);
    return 0;
}
