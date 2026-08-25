#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

#define MAX_LINE 1024
#define BASE_PATH "projects/lsr/pieces/world/lunar"

void create_directory(const char *path) {
    char cmd[MAX_LINE];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

char* trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void setup_entity(const char* type, const char* name, const char* ticker, const char* extra) {
    char piece_dir[MAX_LINE];
    snprintf(piece_dir, sizeof(piece_dir), "%s/%s", BASE_PATH, ticker);
    create_directory(piece_dir);

    // state.txt
    char state_path[MAX_LINE];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", piece_dir);
    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "name=%s\n", name);
        fprintf(f, "ticker=%s\n", ticker);
        if (strcmp(type, "GOV") == 0) {
            fprintf(f, "type=government\n");
            fprintf(f, "cash=1000000.00\n");
            // Parse extra for tax_rate
            char *tax = strstr(extra, "tax_rate=");
            if (tax) fprintf(f, "%s\n", tax);
            else fprintf(f, "tax_rate=0.15\n");
        } else {
            fprintf(f, "type=corporation\n");
            fprintf(f, "cash=5000.00\n");
            fprintf(f, "stock_price=10.00\n");
            // Parse extra for industry
            char *ind = strstr(extra, "industry=");
            if (ind) fprintf(f, "%s\n", ind);
            else fprintf(f, "industry=GENERAL\n");
        }
        fprintf(f, "on_map=1\n");
        fprintf(f, "pos_x=%d\n", rand() % 20);
        fprintf(f, "pos_y=%d\n", rand() % 10);
        fprintf(f, "pos_z=0\n");
        fclose(f);
    }

    // piece.pdl
    char pdl_path[MAX_LINE];
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", piece_dir);
    f = fopen(pdl_path, "w");
    if (f) {
        fprintf(f, "<piece_id>%s</piece_id>\n", ticker);
        fprintf(f, "<traits>\n  <trait>%s</trait>\n  <trait>auditable</trait>\n</traits>\n", 
                (strcmp(type, "GOV") == 0 ? "government" : "corporation"));
        fprintf(f, "<methods>\n  <method label=\"LSR\" href=\"projects/lsr/layouts/lsr_main.chtpm\" />\n</methods>\n");
        fclose(f);
    }
}

int main() {
    srand(time(NULL));
    FILE *fp = fopen("projects/lsr/config/setup_config.txt", "r");
    if (!fp) {
        perror("Error opening setup_config.txt");
        return 1;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *type = strtok(line, "|");
        char *name = strtok(NULL, "|");
        char *ticker = strtok(NULL, "|");
        char *extra = strtok(NULL, "|");

        if (type && name && ticker) {
            setup_entity(trim(type), trim(name), trim(ticker), (extra ? trim(extra) : ""));
        }
    }

    fclose(fp);
    printf("LSR Initialization Complete. Pieces created in %s\n", BASE_PATH);
    return 0;
}
