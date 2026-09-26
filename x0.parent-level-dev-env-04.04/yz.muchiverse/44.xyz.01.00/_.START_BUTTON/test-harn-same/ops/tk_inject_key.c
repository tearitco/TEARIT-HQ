/* tk_inject_key - test-harness op. Appends ONE valid KEY_PRESSED line
 * to a session's pieces/keyboard/history.txt, in the exact strict
 * format chtpm_parser_pal.c requires (see
 * #.haiku+/!.local-ux-testing-ai.txt Part 2). A reusable primitive -
 * callable directly by an agent for one-off exploration, or by
 * test-harn-same/button.sh (or any other script/code) as a building
 * block for a full scenario.
 *
 * Self-contained, no shared headers.
 * Usage: tk_inject_key.+x <session_dir> <decimal_key_code> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tk_inject_key.+x <session_dir> <decimal_key_code>\n");
        return 1;
    }
    const char *session_dir = argv[1];
    int code = atoi(argv[2]);

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/keyboard/history.txt", session_dir);
    FILE *f = fopen(path, "a");
    if (!f) {
        fprintf(stderr, "tk_inject_key: cannot open %s\n", path);
        return 1;
    }
    /* Timestamp content is never checked against real time by the
     * parser - a fixed valid-shaped one is fine (see local-ux-testing
     * doc, Part 2). */
    fprintf(f, "[2026-07-26 00:00:00] KEY_PRESSED: %d\n", code);
    fclose(f);
    return 0;
}
