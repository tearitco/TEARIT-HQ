/* examine - one verb, one binary, no shared headers.
 * Pressing examine on the outer action bar doesn't run this directly -
 * ops/choice.c special-cases the method name "examine" the same way it
 * special-cases "craft", opening the inventory overlay panel
 * (active_panel="inventory") instead of exec'ing a handler. This binary
 * exists anyway (same precedent as ops/craft.c's own no-argument
 * fallback) so hero/piece.pdl's METHOD table always resolves to a real,
 * buildable handler - pdl_reader.c's contract never has to special-case
 * "this entry has no real binary". Not reachable from normal play. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "You look over your gear.\n"); fclose(f); }
    return 0;
}
