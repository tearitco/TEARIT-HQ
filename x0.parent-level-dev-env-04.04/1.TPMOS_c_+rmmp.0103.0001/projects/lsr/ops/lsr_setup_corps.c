#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_CORPS 100
#define MAX_LINE_LEN 512

typedef struct {
    char name[100];
    char ticker[20];
    char industry[100];
} Corporation;

void create_directory(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

void trim_trailing_whitespace(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && isspace((unsigned char)str[i])) {
        str[i] = '\0';
        i--;
    }
}

int read_corporations(const char* file_path, Corporation corporations[]) {
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        perror("Error opening starting_corporations.txt");
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL && count < MAX_CORPS) {
        line[strcspn(line, "\r\n")] = 0;
        char *ticker_start = strrchr(line, '(');
        char *ticker_end = strrchr(line, ')');
        char *industry_start = strstr(line, "Ind:");
        if (ticker_start && ticker_end && industry_start && ticker_start < ticker_end && ticker_end < industry_start) {
            *ticker_start = '\0';
            *ticker_end = '\0';
            char *industry_info = industry_start + 4;
            while (isspace((unsigned char)*industry_info)) industry_info++;
            trim_trailing_whitespace(line);

            strncpy(corporations[count].name, line, sizeof(corporations[count].name) - 1);
            strncpy(corporations[count].ticker, ticker_start + 1, sizeof(corporations[count].ticker) - 1);
            strncpy(corporations[count].industry, industry_info, sizeof(corporations[count].industry) - 1);
            count++;
        }
    }
    fclose(fp);
    return count;
}

float rand_float(float min, float max) {
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

void setup_piece(Corporation corp, const char* base_path) {
    char piece_dir[1024];
    snprintf(piece_dir, sizeof(piece_dir), "%s/%s", base_path, corp.ticker);
    create_directory(piece_dir);

    // Create state.txt
    char state_path[1024];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", piece_dir);
    FILE *f = fopen(state_path, "w");
    if (f) {
        float cash = rand_float(100.0, 1000.0);
        float assets = cash + rand_float(500.0, 2000.0);
        float stock_price = rand_float(5.0, 150.0);
        
        fprintf(f, "name=%s\n", corp.name);
        fprintf(f, "ticker=%s\n", corp.ticker);
        fprintf(f, "industry=%s\n", corp.industry);
        fprintf(f, "cash=%.2f\n", cash);
        fprintf(f, "total_assets=%.2f\n", assets);
        fprintf(f, "debt=0.00\n");
        fprintf(f, "net_worth=%.2f\n", assets);
        fprintf(f, "stock_price=%.2f\n", stock_price);
        fprintf(f, "shares_outstanding=%.2f\n", rand_float(10.0, 100.0));
        fprintf(f, "controlled_by=public\n");
        fclose(f);
    }

    // Create piece.pdl
    char pdl_path[1024];
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", piece_dir);
    f = fopen(pdl_path, "w");
    if (f) {
        fprintf(f, "<piece_id>%s</piece_id>\n", corp.ticker);
        fprintf(f, "<traits>\n  <trait>corporation</trait>\n  <trait>auditable</trait>\n</traits>\n");
        fprintf(f, "<methods>\n  <method label=\"LSR\" href=\"projects/lsr/layouts/lsr_main.chtpm\" />\n</methods>\n");
        fclose(f);
    }
}

int main(int argc, char** argv) {
    srand(time(NULL));
    const char* asset_path = "projects/lsr/assets/corporations/starting_corporations.txt";
    const char* pieces_path = "projects/lsr/pieces/world/lunar";
    
    if (argc > 1) asset_path = argv[1];
    if (argc > 2) pieces_path = argv[2];

    create_directory(pieces_path);

    Corporation corporations[MAX_CORPS] = {0};
    int num_corps = read_corporations(asset_path, corporations);

    if (num_corps > 0) {
        for (int i = 0; i < num_corps; i++) {
            setup_piece(corporations[i], pieces_path);
        }
        printf("Successfully initialized %d corporation pieces in %s\n", num_corps, pieces_path);
    } else {
        printf("Failed to read corporations from %s\n", asset_path);
    }

    return 0;
}
