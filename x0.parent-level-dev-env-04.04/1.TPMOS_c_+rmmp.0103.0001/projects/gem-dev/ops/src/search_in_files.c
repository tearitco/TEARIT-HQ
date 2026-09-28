// tools/search_in_files.c - Self-contained recursive grep
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
/* MinGW has no POSIX getline(). Identical signature and semantics -- read one
   line, growing *lineptr as needed, return length or -1 at EOF -- so the loop
   below is unchanged. Growth matters here rather than being incidental: this
   is a grep over source files, and a fixed buffer would silently stop
   matching on any line longer than the buffer, which for a search tool means
   quietly wrong results. */
static long win_getline(char **lineptr, size_t *n, FILE *stream) {
    size_t used = 0;
    int c;
    if (!lineptr || !n || !stream) return -1;
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = (char *)malloc(*n);
        if (!*lineptr) { *n = 0; return -1; }
    }
    while ((c = fgetc(stream)) != EOF) {
        if (used + 2 > *n) {
            size_t newn = *n * 2;
            char *tmp = (char *)realloc(*lineptr, newn);
            if (!tmp) return -1;
            *lineptr = tmp; *n = newn;
        }
        (*lineptr)[used++] = (char)c;
        if (c == '\n') break;
    }
    if (used == 0) return -1;
    (*lineptr)[used] = '\0';
    return (long)used;
}
#define getline win_getline
#endif

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
        char* full_path;
        asprintf(&full_path, "%s/%s", path, entry->d_name);
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
