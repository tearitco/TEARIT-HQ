/* mr_change_gold - real "Change Gold" event command, 2026-08-05.
 * The first real op invoked FROM an actual compiled event.pal (via
 * prisc+x's own real, BLOCKING `exec` opcode - confirmed via direct
 * source read of prisc+x.c's OP_EXEC, system(cmd), not backgrounded),
 * proving the visual-compiler pipeline end to end: event-ez authors a
 * page -> compiles to event.pal -> a real context-menu click runs it ->
 * this op does the actual state change.
 *
 * Usage: mr_change_gold.+x <package_dir> <delta>
 *   delta: a signed integer, e.g. "+10" or "-5"
 *
 * Real state file: <package_dir>/inventory.txt (direct instruction:
 * "in monsters inventory is fine for now" - a real, simple key=value
 * file, expandable later to hold real items, not just qolq). Missing
 * file or missing qolq= key defaults to 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

static int read_qolq(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int val = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "qolq=", 5) == 0) { val = atoi(line + 5); break; }
    }
    fclose(f);
    return val;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mr_change_gold.+x <package_dir> <delta>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    int delta = atoi(argv[2]);

    char inv_path[PATH_BUF];
    snprintf(inv_path, sizeof(inv_path), "%s/inventory.txt", package_dir);

    int cur = read_qolq(inv_path);
    int next = cur + delta;
    if (next < 0) next = 0;

    /* Keep every other real line (future items), rewrite only qolq=. */
    char keep[64][256];
    int n_keep = 0;
    FILE *rf = fopen(inv_path, "r");
    if (rf) {
        char line[256];
        while (n_keep < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "qolq=", 5) == 0) continue;
            snprintf(keep[n_keep], sizeof(keep[0]), "%s", line);
            n_keep++;
        }
        fclose(rf);
    }
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", inv_path);
    FILE *wf = fopen(tmp_path, "w");
    if (!wf) return 1;
    fprintf(wf, "qolq=%d\n", next);
    for (int i = 0; i < n_keep; i++) fputs(keep[i], wf);
    fclose(wf);
    rename(tmp_path, inv_path);

    char hist_path[PATH_BUF];
    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", package_dir);
    FILE *hf = fopen(hist_path, "a");
    if (hf) {
        fprintf(hf, "CHANGE_GOLD delta=%+d qolq: %d -> %d\n", delta, cur, next);
        fclose(hf);
    }

    printf("CHANGE_GOLD %+d qolq: %d -> %d\n", delta, cur, next);
    return 0;
}
