/* ee_open_request_write - mutaclysm Event menu → pending open for widget
 * Usage:
 *   ee_open_request_write.+x <desktop_root> <package_dir> [map_id] [x] [y] [note]
 * Writes:
 *   <desktop_root>/inbox/event_editor_open.request
 *   and echoes path
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define PATH_BUF 4352

static int mkdir_p(const char *path) {
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    return system(cmd) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "Usage: ee_open_request_write.+x <desktop_root> <package_dir> [map] [x] [y] [note]\n");
        return 1;
    }
    const char *desk = argv[1];
    const char *pkg = argv[2];
    const char *map_id = (argc >= 4) ? argv[3] : "";
    const char *xs = (argc >= 5) ? argv[4] : "";
    const char *ys = (argc >= 6) ? argv[5] : "";
    const char *note = (argc >= 7) ? argv[6] : "xlector_event";

    char inbox[PATH_BUF];
    snprintf(inbox, sizeof(inbox), "%s/inbox", desk);
    if (mkdir_p(inbox) != 0) return 1;

    char req[PATH_BUF];
    snprintf(req, sizeof(req), "%s/event_editor_open.request", inbox);
    FILE *f = fopen(req, "w");
    if (!f) return 1;
    fprintf(f, "widget=event-editor\n");
    fprintf(f, "action=open\n");
    fprintf(f, "package_dir=%s\n", pkg);
    fprintf(f, "map_id=%s\n", map_id);
    fprintf(f, "pos_x=%s\n", xs);
    fprintf(f, "pos_y=%s\n", ys);
    fprintf(f, "note=%s\n", note);
    fprintf(f, "created_at=%ld\n", (long)time(NULL));
    fclose(f);

    /* also append a line log for harnesses */
    char logp[PATH_BUF];
    snprintf(logp, sizeof(logp), "%s/event_editor_open.log", inbox);
    FILE *lf = fopen(logp, "a");
    if (lf) {
        fprintf(lf, "open package_dir=%s map=%s x=%s y=%s\n", pkg, map_id, xs, ys);
        fclose(lf);
    }

    printf("REQUEST %s\n", req);
    return 0;
}
