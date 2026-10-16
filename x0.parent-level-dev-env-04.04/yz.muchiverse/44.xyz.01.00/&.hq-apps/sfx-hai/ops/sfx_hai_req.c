/* sfx_hai_req - the click side of sfx-hai: appends one request line to <package_dir>/state/request.txt.
 * Invoked by the renderer as: sfx_hai_req <CMD> <package_dir> <house_root> [typed value]  (action= gets package_dir and house_root appended).
 * The manager reads the file by size growth (marker), never by mtime. Pure append; no logging. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
int main(int argc, char **argv) {
    if (argc < 3) return 1;
    char dir[4096], p[4200]; snprintf(dir, sizeof dir, "%s/state", argv[2]); mkdir(dir, 0755);
    snprintf(p, sizeof p, "%s/request.txt", dir);
    FILE *f = fopen(p, "a"); if (!f) return 1;
    fprintf(f, "%s%s%s\n", argv[1], argc > 4 ? ":" : "", argc > 4 ? argv[4] : "");
    fclose(f); return 0;
}
