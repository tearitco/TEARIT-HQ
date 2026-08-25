/* agy_scan_dir - copied VERBATIM from &.widgits/file-menu/ops/
 * fm_scan_dir.c (2026-07-30, explicit reuse preference - this file is
 * fully generic, zero project-specific paths, confirmed real and
 * working there already). Usage: agy_scan_dir <dir> [filter], prints
 * one line per entry: DIR|<name>|<size>|1 or FIL|<name>|<size>|0,
 * sorted dirs-first then case-insensitive name. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    char name[512];
    long size;
    int is_dir;
} Entry;

static int entry_cmp(const void *a, const void *b) {
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;
    if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    return strcasecmp(ea->name, eb->name);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: agy_scan_dir <dir> [filter]\n"); return 1; }
    const char *dir_path = argv[1];
    const char *filter = (argc >= 3) ? argv[2] : "";

    DIR *d = opendir(dir_path);
    if (!d) { fprintf(stderr, "agy_scan_dir: cannot open %s\n", dir_path); return 1; }

    Entry entries[4096];
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) && count < 4096) {
        if (de->d_name[0] == '.') continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);
        struct stat st;
        int is_dir = 0;
        long size = 0;
        if (stat(full, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
            size = is_dir ? 0 : (long)st.st_size;
        }
        snprintf(entries[count].name, sizeof(entries[count].name), "%s", de->d_name);
        entries[count].is_dir = is_dir;
        entries[count].size = size;
        count++;
    }
    closedir(d);

    qsort(entries, count, sizeof(Entry), entry_cmp);

    for (int i = 0; i < count; i++) {
        if (filter[0] && !strstr(entries[i].name, filter)) continue;
        printf("%s|%s|%ld|%d\n",
               entries[i].is_dir ? "DIR" : "FIL",
               entries[i].name,
               entries[i].size,
               entries[i].is_dir);
    }
    return 0;
}
