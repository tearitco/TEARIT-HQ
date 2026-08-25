/*
 * Op: get_ops_list
 * Usage: ./+x/get_ops_list.+x
 * 
 * Behavior:
 *   1. Scans pieces/apps/playrm/ops/+x/
 *   2. Outputs a comma-separated list of op names to stdout.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define TPM_OPS_ROOT "pieces/apps/playrm/ops/+x"
static char* ops_root_for(const char *project_root) {
    char *r = NULL;
    if (asprintf(&r, "%s/%s", project_root, TPM_OPS_ROOT) == -1) return NULL;
    return r;
}

#define MAX_PATH 4096

char project_root[MAX_PATH] = ".";

void resolve_root() {
    const char* attempts[] = {"pieces/locations/location_kvp", "../pieces/locations/location_kvp", "../../pieces/locations/location_kvp", "../../../pieces/locations/location_kvp"};
    for (int i = 0; i < 4; i++) {
        FILE *kvp = fopen(attempts[i], "r");
        if (kvp) {
            char line[256];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(line, "project_root") == 0) {
                        strncpy(project_root, eq + 1, MAX_PATH - 1);
                        project_root[strcspn(project_root, "\n\r")] = 0;
                        fclose(kvp);
                        return;
                    }
                }
            }
            fclose(kvp);
        }
    }
}

int main() {
    resolve_root();
    
    char *path = ops_root_for(project_root);
    
    DIR *dir = opendir(path);
    if (!dir) { free(path); return 1; }
    
    struct dirent *entry;
    int first = 1;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strstr(entry->d_name, ".+x")) {
            char name[256];
            strncpy(name, entry->d_name, 255);
            char *ext = strstr(name, ".+x");
            if (ext) *ext = '\0';
            
            if (!first) printf(",");
            printf("%s", name);
            first = 0;
        }
    }
    closedir(dir);
    printf("\n");
    
    free(path);
    return 0;
}
