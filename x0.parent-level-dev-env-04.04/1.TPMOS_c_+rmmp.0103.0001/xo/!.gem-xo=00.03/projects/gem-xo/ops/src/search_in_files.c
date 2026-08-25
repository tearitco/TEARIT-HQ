// tools/search_in_files.c - Self-contained recursive grep
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void search_in_file(const char* path, const char* query) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    char* line = NULL;
    size_t len = 0;
    int line_num = 1;
    while (getline(&line, &len, f) != -1) {
        if (strstr(line, query)) {
            printf("%s [Line %d]: %s", path, line_num, line);
        }
        line_num++;
    }
    free(line);
    fclose(f);
}

void walk_dir(const char* path, const char* query) {
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char* full_path = NULL;
        if (asprintf(&full_path, "%s/%s", path, entry->d_name) == -1) {
            continue;
        }
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) walk_dir(full_path, query);
            else if (S_ISREG(st.st_mode)) search_in_file(full_path, query);
        }
        free(full_path);
    }
    closedir(d);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: search_in_files <query> [path]\n"); return 1; }
    const char* query = argv[1];
    const char* path = (argc > 2) ? argv[2] : ".";
    walk_dir(path, query);
    return 0;
}
