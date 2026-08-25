// tools/complete_path.c - Self-contained path completion tool
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 0;
    const char* input = argv[1];
    
    char* dir_path = NULL;
    char* prefix = "";
    
    char* last_slash = strrchr(input, '/');
    if (last_slash) {
        size_t dir_len = last_slash - input;
        dir_path = malloc(dir_len + 1);
        memcpy(dir_path, input, dir_len);
        dir_path[dir_len] = '\0';
        prefix = last_slash + 1;
        if (strlen(dir_path) == 0) { free(dir_path); dir_path = strdup("/"); }
    } else {
        prefix = (char*)input;
        dir_path = strdup(".");
    }

    DIR* d = opendir(dir_path);
    if (!d) { free(dir_path); return 0; }
    
    struct dirent* entry;
    char* matches[1024];
    int count = 0;

    while ((entry = readdir(d)) != NULL) {
        // Skip hidden files unless prefix starts with .
        if (entry->d_name[0] == '.' && prefix[0] != '.') continue;
        
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            char* match;
            struct stat st;
            char* full_entry_path;
            asprintf(&full_entry_path, "%s/%s", dir_path, entry->d_name);
            stat(full_entry_path, &st);
            free(full_entry_path);

            asprintf(&match, "%s%s%s%s", 
                (strcmp(dir_path, ".") == 0) ? "" : dir_path,
                (strcmp(dir_path, ".") == 0 || strcmp(dir_path, "/") == 0) ? "" : "/",
                entry->d_name,
                S_ISDIR(st.st_mode) ? "/" : "");
            matches[count++] = match;
            if (count >= 1024) break;
        }
    }
    
    qsort(matches, count, sizeof(char*), compare_strings);

    for (int i = 0; i < count; i++) {
        printf("%s%s", matches[i], (i == count - 1) ? "" : "  ");
        free(matches[i]);
    }

    closedir(d);
    free(dir_path);
    return 0;
}
