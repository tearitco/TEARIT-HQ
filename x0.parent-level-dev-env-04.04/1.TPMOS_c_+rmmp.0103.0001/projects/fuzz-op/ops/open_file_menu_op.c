#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
}

int main() {
    resolve_root();
    
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/layout_changed.txt", project_root);
    
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "projects/user/layouts/file_menu.chtpm\n");
        fclose(f);
    }
    
    printf("File menu requested via layout_changed pulse.\n");
    return 0;
}
