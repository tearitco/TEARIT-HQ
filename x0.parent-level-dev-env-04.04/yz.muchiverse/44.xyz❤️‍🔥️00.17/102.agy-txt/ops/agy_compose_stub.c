/* agy_compose_stub - Phase T2 STUB compose op (PLAN.md §4). Only job:
 * write which layout is currently active into view.txt, so href
 * navigation between editor/file_menu/file_browser is provable before
 * any real editing/save/load logic exists. Deleted/replaced once T3
 * wires the real editor_compose_frame-derived op.
 *
 * Reads the currently active layout name the same way the reference
 * project's own manager did (pieces/display/current_layout.txt, written
 * by chtpm_parser_pal itself on every layout switch - confirmed present
 * in this house's own chtpm_parser_pal.c, same mechanism, not invented
 * here) - proves this house's own real layout-tracking file, not a
 * custom one.
 *
 * Writes ONLY view.txt (ONE VISIBLE FRAME WRITER, matching every other
 * project in this family).
 * Usage: agy_compose_stub.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();

    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *lf = fopen(layout_path, "r");
    char layout[256] = "(unknown)";
    if (lf) {
        if (fgets(layout, sizeof(layout), lf)) {
            layout[strcspn(layout, "\r\n")] = '\0';
        }
        fclose(lf);
    }

    char view_path[PATH_BUF];
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    FILE *vf = fopen(view_path, "w");
    if (!vf) return 1;
    fprintf(vf, "=== agy-txt Phase T2 stub ===\\nACTIVE LAYOUT: %s\\nPID: %d\\n", layout, (int)getpid());
    fclose(vf);

    char pulse_path[PATH_BUF];
    snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *pf = fopen(pulse_path, "a");
    if (pf) { fputc('1', pf); fclose(pf); }

    return 0;
}
