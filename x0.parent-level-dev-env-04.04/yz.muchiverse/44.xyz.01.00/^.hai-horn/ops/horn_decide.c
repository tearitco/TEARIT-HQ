/* horn_decide - record the human's approve/deny answer.
 *
 * The counterpart to horn_turn's approval gate. Kept as its own op so the
 * pal loop can call it from an injected button key without needing to
 * write a file itself, and so there is exactly one place that decides
 * what an answer means.
 *
 * gem-dev reads its y/n straight off stdin, because its exec is a
 * foreground call. HORN's terminal belongs to the chtpm window, so the
 * answer arrives as a button keycode (KEY:1 / KEY:0, injected by the
 * layout's onClick) and is turned into a file here instead.
 *
 * Usage: horn_decide.+x y|n
 * Self-contained.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

int main(int argc, char *argv[]) {
    if (argc < 2 || (argv[1][0] != 'y' && argv[1][0] != 'Y' &&
                     argv[1][0] != 'n' && argv[1][0] != 'N')) {
        fprintf(stderr, "usage: horn_decide.+x y|n\n");
        return 2;
    }

    char root[MAX_PATH];
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(root, sizeof(root), "%s", env);
    else if (!getcwd(root, sizeof(root))) snprintf(root, sizeof(root), ".");

    char path[MAX_PATH + 64];
    snprintf(path, sizeof(path), "%s/pieces/horn/decision.txt", root);

    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "horn_decide: cannot write %s\n", path); return 1; }
    fprintf(f, "%c\n", (argv[1][0] == 'y' || argv[1][0] == 'Y') ? 'y' : 'n');
    fclose(f);
    return 0;
}
