/* kh_proc_register_op - real, standalone shell-callable wrapper around
 * kh_proc_register_owned() (kh_proc_registry.h), for launcher SCRIPTS
 * (button.sh-family) that spawn a real house binary via bare shell `&`
 * and have no C of their own to call the registry API directly from.
 *
 * REAL FIX 2026-09-15, direct live report ("they should show up in
 * proc-mon, thats why its there") - root cause confirmed by direct
 * code read: board-viewer's own button.sh spawns `system/renderer`,
 * `system/chtpm_parser_pal`, and `system/chtpm_rgb_render` as bare `&`
 * background jobs, with NO kh_proc_register() call anywhere for any of
 * the three (chtpm_parser_pal.c DOES already register the prisc
 * MODULES it itself spawns, via kh_pal_register_module() - but nothing
 * registers these three top-level processes themselves). Orphaned
 * (deleted-cwd) copies of exactly these three binaries were found live
 * this session, running invisibly to proc-mon and burning CPU -
 * confirms this isn't hypothetical.
 *
 * Usage: kh_proc_register_op.+x <house_root> <pid> <name>
 *   pid registers with pgid=pid (setsid group-leader convention, same
 *   as kh_proc_register()'s own documented pgid<=0 default) and
 *   master_pid=0 ("owned by the orchestrator; reaped only by
 *   reap_all") - matches how button.sh's own teardown already treats
 *   these three (no single "master" process owns them individually,
 *   the whole session IS the owner).
 */
#define KH_PROC_REGISTRY_IMPL
#include "../kh_proc_registry.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <house_root> <pid> <name>\n", argv[0]);
        return 1;
    }
    const char *house_root = argv[1];
    long pid = strtol(argv[2], NULL, 10);
    const char *name = argv[3];
    if (pid <= 1) {
        fprintf(stderr, "kh_proc_register_op: refusing pid=%ld\n", pid);
        return 1;
    }
    int rc = kh_proc_register(house_root, pid, pid, name);
    return rc == 0 ? 0 : 1;
}
