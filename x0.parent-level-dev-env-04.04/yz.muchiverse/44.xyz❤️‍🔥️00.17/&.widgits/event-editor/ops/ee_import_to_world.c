/* ee_import_to_world - drop a desktop event package into mutaclysm world
 * Usage:
 *   ee_import_to_world.+x <package_dir> <live_world_01> [map_id] [x] [y]
 * Copies package to:
 *   <live_world_01>/<map_id>/events/<basename>/
 * Updates state pos if x/y given.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

static int run_cmd(const char *cmd) {
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "ee_import_to_world: cmd failed: %s\n", cmd);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "Usage: ee_import_to_world.+x <package_dir> <world_01> [map_id] [x] [y]\n");
        return 1;
    }
    const char *pkg = argv[1];
    const char *world = argv[2];
    char map_id[128];
    snprintf(map_id, sizeof(map_id), "%s", (argc >= 4 && argv[3][0]) ? argv[3] : "map_start");
    int have_xy = (argc >= 6);
    int x = have_xy ? atoi(argv[4]) : -1;
    int y = have_xy ? atoi(argv[5]) : -1;

    char pkg_copy[PATH_BUF];
    snprintf(pkg_copy, sizeof(pkg_copy), "%s", pkg);
    char *base = basename(pkg_copy);
    if (!base || !base[0]) return 1;

    char dest_parent[PATH_BUF], dest[PATH_BUF], cmd[PATH_BUF * 2];
    snprintf(dest_parent, sizeof(dest_parent), "%s/%s/events", world, map_id);
    snprintf(dest, sizeof(dest), "%s/%s", dest_parent, base);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -rf '%s' && cp -a '%s' '%s'",
             dest_parent, dest, pkg, dest);
    if (run_cmd(cmd) != 0) return 1;

    if (have_xy) {
        char st[PATH_BUF];
        snprintf(st, sizeof(st), "%s/state.txt", dest);
        /* rewrite pos lines simply by appending override file merge */
        FILE *rf = fopen(st, "r");
        char lines[64][MAX_LINE];
        int n = 0;
        if (rf) {
            while (n < 64 && fgets(lines[n], MAX_LINE, rf)) n++;
            fclose(rf);
        }
        FILE *wf = fopen(st, "w");
        if (wf) {
            int px = 0, py = 0, mid = 0;
            for (int i = 0; i < n; i++) {
                if (strncmp(lines[i], "pos_x=", 6) == 0) {
                    fprintf(wf, "pos_x=%d\n", x); px = 1;
                } else if (strncmp(lines[i], "pos_y=", 6) == 0) {
                    fprintf(wf, "pos_y=%d\n", y); py = 1;
                } else if (strncmp(lines[i], "map_id=", 7) == 0) {
                    fprintf(wf, "map_id=%s\n", map_id); mid = 1;
                } else fputs(lines[i], wf);
            }
            if (!px) fprintf(wf, "pos_x=%d\n", x);
            if (!py) fprintf(wf, "pos_y=%d\n", y);
            if (!mid) fprintf(wf, "map_id=%s\n", map_id);
            fclose(wf);
        }
    }

    printf("IMPORT %s\n", dest);
    return 0;
}
