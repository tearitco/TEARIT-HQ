#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

int main(int argc, char *argv[]) {
    resolve_paths();

    char *target_dir = (argc > 1) ? argv[1] : ".";
    
    // Safety check: ensure target is within project_root or relative
    // For now, I'll just list the directory.
    
    DIR *dir = opendir(target_dir);
    if (!dir) {
        fprintf(stderr, "Error: Could not open directory %s\n", target_dir);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        struct stat st;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", target_dir, entry->d_name);
        
        char type = 'F';
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) type = 'D';
        
        printf("[%c] %s\n", type, entry->d_name);
    }
    closedir(dir);

    return 0;
}
