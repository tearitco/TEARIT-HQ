#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
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

    if (argc < 4) {
        fprintf(stderr, "Usage: ai_evolve <curriculum_file> <target_token> <reward>\n");
        return 1;
    }

    char *curriculum_path = argv[1];
    char *target_token = argv[2];
    float reward = atof(argv[3]);
    float learning_rate = 0.05f;

    FILE *cf = fopen(curriculum_path, "r");
    if (!cf) {
        fprintf(stderr, "Error: Could not open curriculum %s\n", curriculum_path);
        return 1;
    }

    // Read all lines to memory to update
    char lines[1000][MAX_LINE];
    int lc = 0;
    while (fgets(lines[lc], MAX_LINE, cf) && lc < 999) lc++;
    fclose(cf);

    int updated = 0;
    for (int i = 1; i < lc; i++) { // Skip header
        char temp[MAX_LINE];
        strcpy(temp, lines[i]);
        int num;
        char word[MAX_LINE];
        float emb, pe, attn_bias;
        
        if (sscanf(temp, "%d %s %f %f %f", &num, word, &emb, &pe, &attn_bias) >= 5) {
            if (strcasecmp(word, target_token) == 0) {
                // Evolve: increase bias if reward is positive, decrease if negative
                float new_bias = attn_bias + (reward * learning_rate);
                
                // Reconstruct the line (keeping other fields if possible - simplified for now)
                // number word embedding pe attention_bias ...
                // Note: We only overwrite the first 5 fields for now
                snprintf(lines[i], MAX_LINE, "%d %s %.6f %.6f %.6f auto_evolved\n", 
                         num, word, emb, pe, new_bias);
                updated = 1;
                printf("Evolved token %s: Bias %.4f -> %.4f\n", word, attn_bias, new_bias);
            }
        }
    }

    if (updated) {
        FILE *wf = fopen(curriculum_path, "w");
        if (wf) {
            for (int i = 0; i < lc; i++) fputs(lines[i], wf);
            fclose(wf);
        }
    } else {
        printf("Token %s not found in curriculum. No evolution occurred.\n", target_token);
    }

    return 0;
}
