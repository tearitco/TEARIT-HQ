/* dbhq_pdl_publish_manager.c — copy a canonical .pdl to a db-hq state file.
 * usage: <house_root> <package_dir> <src_rel_under_house> <state_filename>
 * Never JSON. Seed copy only if dest missing and src exists.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define PATH_BUF 4096

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "dbhq_pdl_publish_manager: <house> <pkgdir> <src_rel> <state_name>\n");
        return 1;
    }
    char house[PATH_BUF], pkg[PATH_BUF], src[PATH_BUF], dst[PATH_BUF];
    snprintf(house, sizeof(house), "%s", argv[1]);
    snprintf(pkg, sizeof(pkg), "%s", argv[2]);
    snprintf(src, sizeof(src), "%s/%s", house, argv[3]);
    if (pkg[0] == '/')
        snprintf(dst, sizeof(dst), "%s/%s", pkg, argv[4]);
    else
        snprintf(dst, sizeof(dst), "%s/%s/%s", house, pkg, argv[4]);

    time_t src_mtime = 0;
    for (;;) {
        struct stat st;
        if (stat(src, &st) == 0 && (st.st_mtime != src_mtime || access(dst, F_OK) != 0)) {
            FILE *in = fopen(src, "r");
            FILE *out = fopen(dst, "w");
            if (in && out) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                    fwrite(buf, 1, n, out);
            }
            if (in) fclose(in);
            if (out) fclose(out);
            src_mtime = st.st_mtime;
        }
        usleep(400000);
    }
    return 0;
}
