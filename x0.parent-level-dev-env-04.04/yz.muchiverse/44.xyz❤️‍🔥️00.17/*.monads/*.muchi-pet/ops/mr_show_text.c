/* mr_show_text - real "Show Text" event command op, 2026-08-24.
 * Second event command through the compiled pipeline (AU24 handoff §1,
 * direct continuation of mr_change_gold's own proven end-to-end shape):
 * event-ez authors a page -> compiles to event.pal -> play_event.sh /
 * a real Play click runs prisc+x -> blocking `exec cmd_N.sh` -> THIS op.
 *
 * Usage: mr_show_text.+x <package_dir> <text_file>
 *   text_file: real, already-line-wrapped plain text (each line renders
 *   as its own popup row - caller wraps, op does not reflow; same rule
 *   &.widgits/tile-picker/ops/khtpm_show_text.c documents).
 *
 * Effect: queues the display by writing ONE relay command -
 *   SHOW_TEXT_FILE:<text_file>
 * - into <package_dir>/interact_relay.txt (truncate-write, same
 * fire-and-forget convention as khtpm_show_text.c: RPG Maker would block
 * the event until dismiss; every current caller uses Show Text as the
 * LAST command in its branch so blocking changes nothing observable yet).
 * The live entity window's own relay poll picks the line up on its next
 * ~300ms tick and renders the text popup.
 * Also appends a real audit line to <package_dir>/history.txt, same
 * ledger habit as mr_change_gold.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mr_show_text.+x <package_dir> <text_file>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *text_file = argv[2];

    /* resolve relative text paths NOW (op's cwd is the muta system dir
     * when run under prisc+x via play_event.sh, not the caller's) */
    char abs_text[PATH_BUF];
    if (text_file[0] == '/') {
        snprintf(abs_text, sizeof(abs_text), "%s", text_file);
    } else if (realpath(text_file, abs_text) == NULL) {
        fprintf(stderr, "mr_show_text: cannot resolve %s\n", text_file);
        return 1;
    }

    FILE *tf = fopen(abs_text, "r");
    if (!tf) {
        fprintf(stderr, "mr_show_text: no such text file: %s\n", abs_text);
        return 1;
    }
    fclose(tf);

    char relay_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/interact_relay.txt", package_dir);
    FILE *rf = fopen(relay_path, "w");
    if (!rf) {
        fprintf(stderr, "mr_show_text: cannot open %s\n", relay_path);
        return 1;
    }
    fprintf(rf, "SHOW_TEXT_FILE:%s\n", abs_text);
    fclose(rf);

    char hist_path[PATH_BUF];
    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", package_dir);
    FILE *hf = fopen(hist_path, "a");
    if (hf) {
        fprintf(hf, "SHOW_TEXT queued file=%s\n", abs_text);
        fclose(hf);
    }

    printf("SHOW_TEXT queued file=%s\n", abs_text);
    return 0;
}
