#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH 4096

char project_root[MAX_PATH] = ".";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *key = line;
                char *val = eq + 1;
                val[strcspn(val, "\n")] = 0;
                if (strcmp(key, "project_root") == 0) {
                    strncpy(project_root, val, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

float get_state_float(const char* user_hash, const char* key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, user_hash);
    FILE *f = fopen(path, "r");
    if (!f) return 0.0f;
    char line[1024];
    float val = 0.0f;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq && strncmp(line, key, strlen(key)) == 0) {
            val = atof(eq + 1);
            break;
        }
    }
    fclose(f);
    return val;
}

void set_state_float(const char* user_hash, const char* key, float val) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, user_hash);
    
    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    
    FILE *f = fopen(path, "r");
    FILE *tf = fopen(temp_path, "w");
    if (!tf) return;
    
    int found = 0;
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq && strncmp(line, key, strlen(key)) == 0) {
                fprintf(tf, "%s=%.2f\n", key, val);
                found = 1;
            } else {
                fputs(line, tf);
            }
        }
        fclose(f);
    }
    if (!found) {
        fprintf(tf, "%s=%.2f\n", key, val);
    }
    fclose(tf);
    rename(temp_path, path);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <user_hash> <amount>\n", argv[0]);
        return 1;
    }
    resolve_paths();
    char *hash = argv[1];
    float amount = atof(argv[2]);
    
    float balance = get_state_float(hash, "balance");
    balance += amount;
    set_state_float(hash, "balance", balance);
    
    printf("Added $%.2f. New balance: $%.2f\n", amount, balance);
    return 0;
}
