/* runtime_register - register a house process in RUNTIME_ROOT
 * Usage:
 *   runtime_register.+x <runtime_root> <pid> <project_id> <session_path> \
 *       <kind> <gl_window 0|1> <display_name>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define PATH_BUF 4352

static void slugify(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '-' || c == '_') out[j++] = (char)c;
        else if (c == ' ' || c == '.' || c == '/') out[j++] = '_';
    }
    out[j] = '\0';
    if (!out[0]) snprintf(out, out_sz, "proc");
}

int main(int argc, char **argv) {
    if (argc < 8) {
        fprintf(stderr,
                "Usage: runtime_register.+x <runtime_root> <pid> <project_id> "
                "<session_path> <kind> <gl 0|1> <display_name>\n");
        return 1;
    }
    const char *root = argv[1];
    const char *pid = argv[2];
    const char *project_id = argv[3];
    const char *session = argv[4];
    const char *kind = argv[5];
    const char *gl = argv[6];
    const char *display = argv[7];

    char slug[128], dir[PATH_BUF], cmd[PATH_BUF];
    slugify(project_id, slug, sizeof(slug));
    snprintf(dir, sizeof(dir), "%s/processes/%s-%s", root, pid, slug);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    if (system(cmd) != 0) return 1;

    char meta[PATH_BUF];
    snprintf(meta, sizeof(meta), "%s/meta.pdl", dir);
    FILE *f = fopen(meta, "w");
    if (!f) return 1;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "STATE        | pid                  | %s\n", pid);
    fprintf(f, "STATE        | project_id           | %s\n", project_id);
    fprintf(f, "STATE        | session_path         | %s\n", session);
    fprintf(f, "STATE        | kind                 | %s\n", kind);
    fprintf(f, "STATE        | gl_window            | %s\n", gl);
    fprintf(f, "STATE        | display_name         | %s\n", display);
    fprintf(f, "STATE        | registered_at        | %ld\n", (long)time(NULL));
    fclose(f);

    /* heart */
    char heart[PATH_BUF];
    snprintf(heart, sizeof(heart), "%s/heart.beat", dir);
    f = fopen(heart, "w");
    if (f) { fprintf(f, "%ld\n", (long)time(NULL)); fclose(f); }

    printf("REGISTER %s-%s\n", pid, slug);
    return 0;
}
