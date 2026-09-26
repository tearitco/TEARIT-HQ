/* actors_hq_manager.c — db-hq Actors tab MANAGER (TPMOS).
 * Canonical data is actors.pdl (SECTION | KEY | VALUE). Never JSON.
 * Publishes a copy to #.desktop/db_hq_actors.state.txt for the renderer.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PATH_BUF 4096

static char g_house_root[PATH_BUF];
static char g_package_dir[PATH_BUF];
static char g_src_pdl[PATH_BUF];
static char g_state_path[PATH_BUF];
static time_t g_src_mtime;

static const char *k_seed =
"SECTION      | KEY                | VALUE\n"
"----------------------------------------------------------------------\n"
"ACTOR        | id                 | 1\n"
"ACTOR        | name               | Harold\n"
"ACTOR        | nickname           | The Brave\n"
"ACTOR        | class              | Hero\n"
"ACTOR        | init_lv            | 1\n"
"ACTOR        | max_lv             | 99\n"
"ACTOR        | profile            | A young warrior seeking adventure.\n"
"ACTOR        | mhp                | 450\n"
"ACTOR        | mmp                | 80\n"
"ACTOR        | atk                | 32\n"
"ACTOR        | def                | 28\n"
"ACTOR        | mat                | 18\n"
"ACTOR        | mdf                | 20\n"
"ACTOR        | agi                | 30\n"
"ACTOR        | luk                | 24\n"
"ACTOR        | id                 | 2\n"
"ACTOR        | name               | Therese\n"
"ACTOR        | id                 | 3\n"
"ACTOR        | name               | Marsha\n"
"ACTOR        | id                 | 4\n"
"ACTOR        | name               | Lucius\n"
"ACTOR        | id                 | 5\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 6\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 7\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 8\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 9\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 10\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 11\n"
"ACTOR        | name               |\n"
"ACTOR        | id                 | 12\n"
"ACTOR        | name               |\n";

static void seed_if_missing(void) {
    if (access(g_src_pdl, F_OK) == 0) return;
    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s", g_src_pdl);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    FILE *f = fopen(g_src_pdl, "w");
    if (!f) return;
    fputs(k_seed, f);
    fclose(f);
}

static int copy_pdl(void) {
    struct stat st;
    if (stat(g_src_pdl, &st) != 0) return 0;
    if (st.st_mtime == g_src_mtime && access(g_state_path, F_OK) == 0) return 0;
    FILE *in = fopen(g_src_pdl, "r");
    if (!in) return 0;
    FILE *out = fopen(g_state_path, "w");
    if (!out) { fclose(in); return 0; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    g_src_mtime = st.st_mtime;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "actors_hq_manager: usage: <house_root> [package_dir]\n");
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s",
             argc >= 3 ? argv[2] : "#.desktop");
    snprintf(g_src_pdl, sizeof(g_src_pdl),
             "%s/&.widgits/db-hq/data/actors.pdl", g_house_root);
    if (g_package_dir[0] == '/')
        snprintf(g_state_path, sizeof(g_state_path),
                 "%s/db_hq_actors.state.txt", g_package_dir);
    else
        snprintf(g_state_path, sizeof(g_state_path),
                 "%s/%s/db_hq_actors.state.txt", g_house_root, g_package_dir);

    seed_if_missing();
    for (;;) {
        copy_pdl();
        usleep(400000);
    }
    return 0;
}
