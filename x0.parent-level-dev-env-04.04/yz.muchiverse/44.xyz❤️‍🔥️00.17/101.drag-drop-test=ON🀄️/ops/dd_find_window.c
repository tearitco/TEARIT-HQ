/* dd_find_window.c - Find X11 window ID by name substring
 *
 * Usage: dd_find_window <window_name>
 *
 * Uses xdotool to find windows matching the name.
 * Prints window ID to stdout, exits 1 if not found.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <window_name>\n", argv[0]);
        return 1;
    }

    const char *name = argv[1];
    char cmd[1024];

    /* Use xdotool search to find window by name */
    snprintf(cmd, sizeof(cmd), "xdotool search --name \"%s\" 2>/dev/null | head -1", name);

    FILE *p = popen(cmd, "r");
    if (!p) {
        fprintf(stderr, "ERROR: popen failed\n");
        return 1;
    }

    char buf[64];
    if (fgets(buf, sizeof(buf), p)) {
        /* Strip newline */
        buf[strcspn(buf, "\n")] = 0;
        printf("%s", buf);
        pclose(p);
        return 0;
    }

    pclose(p);
    fprintf(stderr, "ERROR: no window found matching '%s'\n", name);
    return 1;
}
