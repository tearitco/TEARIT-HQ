// tools/search_in_files.c - Recursive regex grep
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>

void search_in_file(const char* path, regex_t* regex) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    char* line = NULL;
    size_t len = 0;
    int line_num = 1;
    while (getline(&line, &len, f) != -1) {
        if (regexec(regex, line, 0, NULL, 0) == 0) {
            // Path in dim, line number in white, match in green
            printf("\033[90m%s\033[0m [\033[37mLine %d\033[0m]: \033[32m%s\033[0m", path, line_num, line);
        }
        line_num++;
    }
    free(line);
    fclose(f);
}

void walk_dir(const char* path, regex_t* regex) {
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char* full_path;
        if (asprintf(&full_path, "%s/%s", path, entry->d_name) == -1) continue;
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) walk_dir(full_path, regex);
            else if (S_ISREG(st.st_mode)) search_in_file(full_path, regex);
        }
        free(full_path);
    }
    closedir(d);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { 
        fprintf(stderr, "Usage: search_in_files <regex> [path]");
        fputc(10, stderr); 
        return 1; 
    }
    const char* query = argv[1];
    const char* path = (argc > 2) ? argv[2] : ".";
    
    regex_t regex;
    if (regcomp(&regex, query, REG_EXTENDED | REG_NOSUB) != 0) {
        fprintf(stderr, "Error: Invalid regex: %s", query);
        fputc(10, stderr);
        return 1;
    }
    
    walk_dir(path, &regex);
    regfree(&regex);
    return 0;
}
