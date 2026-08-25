#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

/*
 * dir_browse.+x -- all-purpose, reusable directory-listing Op (Bible
 * section 11's REUSE RULE). Extracted from agy-text-editor's inline
 * directory-scanning code in update_gui_state(). Generic: any project
 * that needs a file browser (not just text editors) can use this.
 *
 * Usage: dir_browse.+x <directory_path> <search_query> <output_path>
 *   directory_path: the directory to scan (caller resolves project_root
 *     + relative path first -- this Op does no path resolution of its own).
 *   search_query: case-insensitive substring filter against entry names;
 *     pass "" for no filtering.
 *   output_path: where the listing is written, one entry per line:
 *     "DIR|<name>" for subdirectories, "FILE|<name>|<size_bytes>" for
 *     files. Subdirectories are listed first (sorted), then files
 *     (sorted) -- matches agy-text-editor's original ordering. Entries
 *     starting with '.' are skipped.
 *
 * Exit code: 0 on success (including an empty/non-existent directory,
 * which just produces an empty output file), 1 if output_path couldn't
 * be opened for writing.
 */

#define MAX_ENTRIES 512
#define MAX_NAME 256

static char subdirs[MAX_ENTRIES][MAX_NAME];
static char files[MAX_ENTRIES][MAX_NAME];

static int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int main(int argc, char *argv[]) {
    const char *dir_path, *search_query, *output_path;
    DIR *d;
    struct dirent *entry;
    FILE *out;
    int subdir_count = 0, file_count = 0;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <directory_path> <search_query> <output_path>\n", argv[0]);
        return 2;
    }
    dir_path = argv[1];
    search_query = argv[2];
    output_path = argv[3];

    d = opendir(dir_path);
    if (d) {
        while ((entry = readdir(d)) != NULL) {
            char entry_path[4096];
            struct stat st;

            if (entry->d_name[0] == '.') continue;
            if (search_query[0] != '\0' && strcasestr(entry->d_name, search_query) == NULL) continue;

            snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);
            if (stat(entry_path, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (subdir_count < MAX_ENTRIES) {
                    strncpy(subdirs[subdir_count], entry->d_name, MAX_NAME - 1);
                    subdirs[subdir_count][MAX_NAME - 1] = '\0';
                    subdir_count++;
                }
            } else {
                if (file_count < MAX_ENTRIES) {
                    strncpy(files[file_count], entry->d_name, MAX_NAME - 1);
                    files[file_count][MAX_NAME - 1] = '\0';
                    file_count++;
                }
            }
        }
        closedir(d);
    }

    qsort(subdirs, subdir_count, MAX_NAME, compare_names);
    qsort(files, file_count, MAX_NAME, compare_names);

    out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "dir_browse: cannot write %s\n", output_path);
        return 1;
    }

    for (int i = 0; i < subdir_count; i++) {
        fprintf(out, "DIR|%s\n", subdirs[i]);
    }
    for (int i = 0; i < file_count; i++) {
        char full_path[4096];
        struct stat st;
        long size = 0;
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, files[i]);
        if (stat(full_path, &st) == 0) size = (long)st.st_size;
        fprintf(out, "FILE|%s|%ld\n", files[i], size);
    }

    fclose(out);
    return 0;
}
