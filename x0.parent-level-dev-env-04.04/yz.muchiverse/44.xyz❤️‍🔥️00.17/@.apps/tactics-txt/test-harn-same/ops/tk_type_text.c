/* tk_type_text - test-harness op. Types an arbitrary string into a
 * session's currently-ACTIVE cli_io field by injecting one KEY_PRESSED
 * line per character (matches exactly how a real keystroke-by-keystroke
 * human types - see !.local-ux-testing-ai.txt Part 1, step 3). Caller
 * is responsible for having already activated the target field (Enter
 * on the focused item) before calling this.
 *
 * Self-contained, no shared headers.
 * Usage: tk_type_text.+x <session_dir> <text> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tk_type_text.+x <session_dir> <text>\n");
        return 1;
    }
    const char *session_dir = argv[1];
    const char *text = argv[2];

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/keyboard/history.txt", session_dir);

    for (const char *p = text; *p; p++) {
        FILE *f = fopen(path, "a");
        if (!f) {
            fprintf(stderr, "tk_type_text: cannot open %s\n", path);
            return 1;
        }
        fprintf(f, "[2026-07-26 00:00:00] KEY_PRESSED: %d\n", (int)(unsigned char)*p);
        fclose(f);
        usleep(80000); /* ~one real keystroke's worth of spacing */
    }
    return 0;
}
