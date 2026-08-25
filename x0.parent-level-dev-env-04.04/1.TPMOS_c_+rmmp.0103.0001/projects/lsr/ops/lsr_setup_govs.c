#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_LINE_LEN 512

void create_directory(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

void trim_whitespace(char *str) {
    int start = 0, end = strlen(str) - 1;
    while (start <= end && isspace((unsigned char)str[start])) start++;
    while (end >= start && isspace((unsigned char)str[end])) end--;
    str[end + 1] = '\0';
    memmove(str, str + start, end - start + 2);
}

void setup_gov(const char* name, const char* id, const char* base_path) {
    char piece_dir[1024];
    snprintf(piece_dir, sizeof(piece_dir), "%s/%s", base_path, id);
    create_directory(piece_dir);

    char state_path[1024];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", piece_dir);
    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "name=%s\n", name);
        fprintf(f, "gov_id=%s\n", id);
        fprintf(f, "cash=1000000.00\n");
        fprintf(f, "tax_rate=0.15\n");
        fprintf(f, "status=active\n");
        fclose(f);
    }

    char pdl_path[1024];
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", piece_dir);
    f = fopen(pdl_path, "w");
    if (f) {
        fprintf(f, "<piece_id>%s</piece_id>\n", id);
        fprintf(f, "<traits>\n  <trait>government</trait>\n  <trait>auditable</trait>\n</traits>\n");
        fprintf(f, "<methods>\n  <method label=\"LSR\" href=\"projects/lsr/layouts/lsr_main.chtpm\" />\n</methods>\n");
        fclose(f);
    }
}

int main() {
    const char* asset_path = "projects/lsr/assets/governments/gov-list_earth.txt";
    const char* pieces_path = "projects/lsr/pieces/world/lunar";
    
    FILE *fp = fopen(asset_path, "r");
    if (!fp) {
        perror("Error opening gov-list_earth.txt");
        return 1;
    }

    char line[MAX_LINE_LEN];
    int count = 0;
    // Skip header
    fgets(line, sizeof(line), fp);
    
    while (fgets(line, sizeof(line), fp) && count < 50) {
        char *rank = strtok(line, "\t");
        char *name = strtok(NULL, "\t");
        if (rank && name) {
            trim_whitespace(name);
            char id[4];
            strncpy(id, name, 3);
            id[3] = '\0';
            for(int i=0; i<3; i++) id[i] = toupper(id[i]);
            
            setup_gov(name, id, pieces_path);
            count++;
        }
    }
    fclose(fp);
    printf("Successfully initialized %d government pieces in %s\n", count, pieces_path);
    return 0;
}
