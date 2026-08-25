/* ee_resolve_desktop - print absolute house desktop root
 * Usage: ee_resolve_desktop.+x [hint_project_or_widget_dir]
 * Env XYZ_DESKTOP_ROOT wins. Else walk up from hint (or cwd) looking for #.desktop
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define PATH_BUF 4352

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int has_desktop(const char *parent, char *out, size_t out_sz) {
    char cand[PATH_BUF];
    snprintf(cand, sizeof(cand), "%s/#.desktop", parent);
    if (is_dir(cand)) {
        snprintf(out, out_sz, "%s", cand);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *env = getenv("XYZ_DESKTOP_ROOT");
    if (env && env[0] && is_dir(env)) {
        printf("%s\n", env);
        return 0;
    }

    char start[PATH_BUF];
    if (argc >= 2 && argv[1][0]) {
        snprintf(start, sizeof(start), "%s", argv[1]);
    } else {
        if (!getcwd(start, sizeof(start))) {
            fprintf(stderr, "ee_resolve_desktop: getcwd failed\n");
            return 1;
        }
    }

    char cur[PATH_BUF];
    snprintf(cur, sizeof(cur), "%s", start);
    char out[PATH_BUF];
    for (int i = 0; i < 12; i++) {
        if (has_desktop(cur, out, sizeof(out))) {
            printf("%s\n", out);
            return 0;
        }
        /* parent */
        char *slash = strrchr(cur, '/');
        if (!slash || slash == cur) break;
        *slash = '\0';
        if (cur[0] == '\0') break;
    }
    fprintf(stderr, "ee_resolve_desktop: #.desktop not found from %s\n", start);
    return 1;
}
