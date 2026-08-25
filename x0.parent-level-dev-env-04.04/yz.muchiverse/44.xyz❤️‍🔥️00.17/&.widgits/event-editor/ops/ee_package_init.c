/* ee_package_init - create empty event package under desktop or path
 * Usage:
 *   ee_package_init.+x <parent_dir> <package_name> [map_id] [x] [y]
 * Creates parent/name/{state.txt,event.pal,event.ir.pdl,piece.pdl}
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define PATH_BUF 4352

static int mkdir_p(const char *path) {
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    return system(cmd) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: ee_package_init.+x <parent_dir> <name> [map_id] [x] [y]\n");
        return 1;
    }
    const char *parent = argv[1];
    const char *name = argv[2];
    const char *map_id = (argc >= 4 && argv[3][0]) ? argv[3] : "map_start";
    int x = (argc >= 5) ? atoi(argv[4]) : 0;
    int y = (argc >= 6) ? atoi(argv[5]) : 0;

    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/%s", parent, name);
    if (mkdir_p(dir) != 0) {
        fprintf(stderr, "ee_package_init: mkdir failed %s\n", dir);
        return 1;
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/state.txt", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "name=%s\n", name);
    fprintf(f, "kind=map_event\n");
    fprintf(f, "map_id=%s\n", map_id);
    fprintf(f, "pos_x=%d\n", x);
    fprintf(f, "pos_y=%d\n", y);
    fprintf(f, "trigger=action\n");
    fprintf(f, "sprite=\n");
    fprintf(f, "self_A=0\n");
    fprintf(f, "self_B=0\n");
    fprintf(f, "self_C=0\n");
    fprintf(f, "self_D=0\n");
    fclose(f);

    snprintf(path, sizeof(path), "%s/piece.pdl", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "META         | version            | 1.0\n");
        fprintf(f, "STATE        | name                 | %s\n", name);
        fprintf(f, "STATE        | kind                 | map_event\n");
        fprintf(f, "METHOD       | on_interact          | ops/+x/event_run.+x\n");
        fprintf(f, "RESPONSE     | default              | *event idle*\n");
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/event.pal", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "# event.pal — lowered IR / hand-editable\n");
        fprintf(f, "# created by ee_package_init\n");
        fprintf(f, "show_text \"(empty event)\"\n");
        fprintf(f, "ret\n");
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/event.ir.pdl", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "STATE        | source               | blocks\n");
        fprintf(f, "NODE         | id=1 type=show_text    | text=(empty event)\n");
        fprintf(f, "NODE         | id=2 type=ret          | \n");
        fclose(f);
    }

    printf("PACKAGE %s\n", dir);
    return 0;
}
