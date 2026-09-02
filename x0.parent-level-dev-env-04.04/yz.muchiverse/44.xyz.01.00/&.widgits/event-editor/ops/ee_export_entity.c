/* ee_export_entity - copy an entity/event dir onto house desktop
 * Usage: ee_export_entity.+x <src_dir> <desktop_root> <subdir> <name>
 *   subdir = events | entities | tiles
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: ee_export_entity.+x <src_dir> <desktop_root> <subdir> <name>\n");
        return 1;
    }
    const char *src = argv[1];
    const char *desk = argv[2];
    const char *sub = argv[3];
    const char *name = argv[4];

    char dest[PATH_BUF], cmd[PATH_BUF * 2];
    snprintf(dest, sizeof(dest), "%s/%s/%s", desk, sub, name);
    snprintf(cmd, sizeof(cmd),
             "mkdir -p '%s/%s' && rm -rf '%s' && cp -a '%s' '%s'",
             desk, sub, dest, src, dest);
    if (system(cmd) != 0) {
        fprintf(stderr, "ee_export_entity: copy failed\n");
        return 1;
    }
    printf("EXPORT %s\n", dest);
    return 0;
}
