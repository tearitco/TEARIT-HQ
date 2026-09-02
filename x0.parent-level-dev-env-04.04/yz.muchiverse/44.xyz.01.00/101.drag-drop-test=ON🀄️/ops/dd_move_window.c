/* dd_move_window.c - Move X11 window to position
 *
 * Usage: dd_move_window <window_id> <x> <y>
 *
 * Uses xdotool to move the window.
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <window_id> <x> <y>\n", argv[0]);
        return 1;
    }

    const char *wid = argv[1];
    int x = atoi(argv[2]);
    int y = atoi(argv[3]);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdotool windowmove %s %d %d", wid, x, y);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "ERROR: xdotool windowmove failed for window %s\n", wid);
        return 1;
    }

    printf("OK: moved window %s to (%d, %d)\n", wid, x, y);
    return 0;
}
