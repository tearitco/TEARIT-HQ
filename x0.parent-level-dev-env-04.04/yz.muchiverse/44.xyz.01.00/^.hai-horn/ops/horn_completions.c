/* horn_completions - '@' path completion for HORN_CHAT.
 *
 * Mirrors gem-dev's behaviour (ops/src/complete_path.c there): typing
 * '@' in the composer opens a completion list of paths under the
 * directory being typed, and '@' does NOT travel to the model.
 *
 * Dispatch is at the harness level, not in the chtpm parser, which is
 * the same conclusion the HORN_CHAT handoff reached: chtpm renders a
 * layout and knows nothing about what text is destined for a model, so
 * prompt-level parsing belongs here in the .pal/.c layer.
 *
 * Usage: horn_completions.+x "<partial-path-after-@>"
 *   Writes matching paths, one per line, to pieces/horn/completions.txt.
 *   Prints the same list to stdout.
 * Self-contained: own root resolution, no shared headers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF  (MAX_PATH + 256)
#define MAX_MATCH 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int main(int argc, char *argv[]) {
    resolve_root();

    const char *input = (argc > 1) ? argv[1] : "";
    if (input[0] == '@') input++;

    char dir_path[MAX_PATH];
    const char *prefix = "";

    const char *slash = strrchr(input, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - input);
        if (dlen == 0) { snprintf(dir_path, sizeof(dir_path), "/"); }
        else {
            snprintf(dir_path, sizeof(dir_path), "%.*s", (int)dlen, input);
        }
        prefix = slash + 1;
    } else {
        /* No slash: complete against the project root, not the process
         * cwd, so what the list offers matches what the harness thinks
         * the project is regardless of who launched it. The whole input
         * is the filename prefix here - leaving prefix empty would match
         * every entry and turn completion into a directory listing. */
        snprintf(dir_path, sizeof(dir_path), "%s", project_root);
        prefix = input;
    }

    DIR *d = opendir(dir_path);
    char *matches[MAX_MATCH];
    int count = 0;

    /* The directory to PRINT, and how to join a name onto it. Empty
     * dir_prefix means "print the bare name" (we are listing the project
     * root itself); otherwise dir_prefix is a real directory that needs a
     * trailing separator. */
    char dir_prefix[PATH_BUF];
    size_t rootlen = strlen(project_root);
    int bare = 0;

    if (strncmp(dir_path, project_root, rootlen) == 0 &&
        (dir_path[rootlen] == '\0' || (dir_path[rootlen] == '/' && dir_path[rootlen + 1] == '\0'))) {
        /* Completing at or directly under the project root: show
         * repo-relative paths, or every line carries the same long
         * absolute prefix and the list is unreadable. */
        bare = 1;
    } else if (strncmp(dir_path, project_root, rootlen) == 0 && dir_path[rootlen] == '/') {
        const char *rel = dir_path + rootlen + 1;
        snprintf(dir_prefix, sizeof(dir_prefix), "%s", rel);
    } else {
        snprintf(dir_prefix, sizeof(dir_prefix), "%s", dir_path);
    }

    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL && count < MAX_MATCH) {
            if (e->d_name[0] == '.' && prefix[0] != '.') continue;
            if (strncmp(e->d_name, prefix, strlen(prefix)) != 0) continue;

            char full[PATH_BUF];
            snprintf(full, sizeof(full), "%s/%s", dir_path, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;

            char *m = NULL;
            if (bare) {
                if (asprintf(&m, "%s%s", e->d_name, S_ISDIR(st.st_mode) ? "/" : "") < 0)
                    continue;
            } else {
                if (asprintf(&m, "%s%s%s%s", dir_prefix,
                             dir_prefix[strlen(dir_prefix) - 1] == '/' ? "" : "/",
                             e->d_name,
                             S_ISDIR(st.st_mode) ? "/" : "") < 0)
                    continue;
            }
            matches[count++] = m;
        }
        closedir(d);
    }

    qsort(matches, (size_t)count, sizeof(char *), cmp_str);

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/horn/completions.txt", project_root);
    FILE *f = fopen(out_path, "wb");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", matches[i]);
        printf("%s\n", matches[i]);
        free(matches[i]);
    }
    if (f) fclose(f);

    return 0;
}