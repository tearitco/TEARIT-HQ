/* ee_open_request_read - print package_dir from pending open request
 * Usage: ee_open_request_read.+x <desktop_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ee_open_request_read.+x <desktop_root>\n");
        return 1;
    }
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/inbox/event_editor_open.request", argv[1]);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "no pending request at %s\n", path);
        return 1;
    }
    char line[MAX_LINE];
    char pkg[PATH_BUF] = "";
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "package_dir=", 12) == 0) {
            snprintf(pkg, sizeof(pkg), "%s", line + 12);
            pkg[strcspn(pkg, "\n")] = '\0';
        }
    }
    fclose(f);
    if (!pkg[0]) return 1;
    printf("%s\n", pkg);
    return 0;
}
