/* video_player_op.c - the window's tiny command line: appends one command row to <pkg>/video_player_action.txt.
 *   video_player_op <package_dir> <VERB> [arg]      VERB: drop add play pause resume stop back fwd next prev clear
 * `drop` takes the path from $DROP_PATH (set by the renderer's XDND drop_action, first dropped path). Append only; the manager reads by cursor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <package_dir> <VERB> [arg]\n", argv[0]); return 2; }
    const char *arg = argc > 3 ? argv[3] : "";
    if (!strcmp(argv[2], "drop")) { const char *d = getenv("DROP_PATH"); if (!d || !*d) return 1; arg = d; argv[2] = "add"; }
    if (strchr(arg, '\n') || strchr(argv[2], '\n')) return 2;
    char p[1536]; snprintf(p, sizeof p, "%s/video_player_action.txt", argv[1]);
    FILE *f = fopen(p, "a"); if (!f) return 1;
    fprintf(f, "%s %s\n", argv[2], arg); fclose(f); return 0;
}
