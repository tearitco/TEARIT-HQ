#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static char project_root[1024];

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: import_pet <pet_id>\n"); return 1; }
    const char *pet_id = argv[1];
    resolve_root();

    char receipt_path[1024];
    snprintf(receipt_path, sizeof(receipt_path), "%s/imported_pets.txt", project_root);
    FILE *f = fopen(receipt_path, "a");
    if (f) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
        fprintf(f, "[%s] pet_id=%s\n", ts, pet_id);
        fclose(f);
    }

    char close_request[1024];
    snprintf(close_request, sizeof(close_request), "/tmp/egg_window_close_%s.txt", pet_id);
    FILE *cr = fopen(close_request, "w");
    if (cr) { fprintf(cr, "close=1\n"); fclose(cr); }

    fprintf(stdout, "imported: %s\n", pet_id);
    return 0;
}
