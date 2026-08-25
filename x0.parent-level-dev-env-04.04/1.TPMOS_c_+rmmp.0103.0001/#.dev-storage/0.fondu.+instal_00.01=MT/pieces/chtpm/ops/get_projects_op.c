#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

/*
 * Op: get_projects_op
 * Purpose: Scans projects/ directory and outputs CHTPM markup for a list of projects.
 * Output: XML markup string for the Parser.
 */

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
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

int main() {
    resolve_paths();
    
    char *projects_path = NULL;
    if (asprintf(&projects_path, "%s/projects", project_root) == -1) return 1;
    
    DIR *dir = opendir(projects_path);
    if (!dir) {
        printf("| [Error: No Projects Dir] |");
        free(projects_path);
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char *path = NULL;
        if (asprintf(&path, "%s/projects/%s", project_root, entry->d_name) != -1) {
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                /* Padding: 60 total - 3 (prefix) - 7 (index) - 2 (brackets) - 1 (space) - len - 2 (suffix) */
                int label_len = strlen(entry->d_name);
                int padding = 45 - label_len;
                if (padding < 0) padding = 0;
                
                printf("<text label=\"║  \" /><button label=\"%s\" onClick=\"LOAD_PROJECT:%s\" />", entry->d_name, entry->d_name);
                if (padding > 0) {
                    printf("<text label=\"");
                    for (int i = 0; i < padding; i++) printf(" ");
                    printf("\" />");
                }
                printf("<text label=\" ║\" /><br/>\\n"); /* ESCAPED NEWLINE */
                count++;
            }
            free(path);
        }
    }
    closedir(dir);
    free(projects_path);

    if (count == 0) {
        printf("| [No Projects Found] |");
    }

    return 0;
}
