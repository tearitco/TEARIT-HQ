#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

// project_loader.c - Engine Operation (v1.0)
// Responsibility: Scan projects/ dir, generate a PDL menu, and return selection.
// Updated: Read entry_layout from project.pdl and trigger layout switch.

#define MAX_PATH 1024
#define MAX_LINE 1024

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Read entry_layout from project.pdl
int get_entry_layout(const char* project_id, char* layout_out, size_t layout_sz) {
    char pdl_path[MAX_PATH];
    snprintf(pdl_path, sizeof(pdl_path), "./projects/%s/project.pdl", project_id);
    
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "entry_layout")) {
            char *pipe = strrchr(line, '|');
            if (pipe) {
                strncpy(layout_out, trim_str(pipe + 1), layout_sz - 1);
                layout_out[layout_sz - 1] = '\0';
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}

int main() {
    DIR *dir = opendir("./projects");
    if (!dir) {
        printf("0\n");
        return 1;
    }

    FILE *f = fopen("./pieces/apps/player_app/PAL/tmp_projects.pdl", "w");
    if (!f) {
        closedir(dir);
        printf("0\n");
        return 1;
    }

    fprintf(f, "META | piece_id | tmp_projects\n");
    fprintf(f, "META | title    | Select Project\n\n");

    struct dirent *entry;
    char proj_list[16][256];
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < 16) {
        if (entry->d_name[0] == '.') continue;
        
        char path[MAX_PATH];
        snprintf(path, MAX_PATH, "./projects/%s", entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            strcpy(proj_list[count], entry->d_name);
            fprintf(f, "METHOD | %s | void\n", entry->d_name);
            count++;
        }
    }
    fclose(f);
    closedir(dir);

    if (count == 0) {
        printf("0\n");
        return 0;
    }

    // Now call menu_op on the generated PDL
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "./pieces/apps/player_app/world/plugins/+x/menu_op.+x ./pieces/apps/player_app/PAL/tmp_projects.pdl");
    
    FILE *mf = popen(cmd, "r");
    if (mf) {
        char buf[16];
        if (fgets(buf, sizeof(buf), mf)) {
            int sel = atoi(buf);
            if (sel > 0 && sel <= count) {
                // Success! Write selected project to engine state
                FILE *sf = fopen("./pieces/apps/player_app/manager/state.txt", "a");
                if (sf) {
                    fprintf(sf, "project_id=%s\n", proj_list[sel-1]);
                    fclose(sf);
                }
                
                // CRITICAL: Read entry_layout and trigger layout switch
                char entry_layout[MAX_PATH];
                if (get_entry_layout(proj_list[sel-1], entry_layout, sizeof(entry_layout)) == 0) {
                    FILE *lf = fopen("./pieces/display/layout_changed.txt", "w");
                    if (lf) {
                        fprintf(lf, "%s\n", entry_layout);
                        fclose(lf);
                    }
                }
                
                printf("%d\n", sel); // Return success index to VM
            } else {
                printf("0\n");
            }
        }
        pclose(mf);
    }

    return 0;
}
