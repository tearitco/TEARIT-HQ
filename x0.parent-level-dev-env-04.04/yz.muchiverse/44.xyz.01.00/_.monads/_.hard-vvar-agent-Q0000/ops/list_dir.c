// tools/list_dir.c - Self-contained directory lister
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

int main(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : ".";
    DIR* d = opendir(path);
    if (!d) { perror("opendir"); return 1; }
    
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char* full_path = NULL;
        if (asprintf(&full_path, "%s/%s", path, entry->d_name) == -1) {
            continue;
        }
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) printf("%s/\n", entry->d_name);
            else printf("%s\n", entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
        free(full_path);
    }
    closedir(d);
    return 0;
}
