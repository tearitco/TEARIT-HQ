#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define CACHE_DURATION 3600

char project_root[MAX_PATH] = ".";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "project_root") == 0) {
                    char *val = eq + 1;
                    val[strcspn(val, "\n")] = 0;
                    strncpy(project_root, val, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

void set_state_string(const char* user_hash, const char* key, const char* val) {
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
                fprintf(tf, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, tf);
            }
        }
        fclose(f);
    }
    if (!found) fprintf(tf, "%s=%s\n", key, val);
    fclose(tf);
    rename(temp_path, path);
}

void set_state_float(const char* user_hash, const char* key, float val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", val);
    set_state_string(user_hash, key, buf);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <user_hash> <symbol>\n", argv[0]);
        return 1;
    }
    resolve_paths();
    char *user_hash = argv[1];
    char *symbol = argv[2];
    for (char *p = symbol; *p; p++) *p = toupper(*p);

    char command[512];
    // Use the compiled fetch_stock.+x
    snprintf(command, sizeof(command), "'%s/projects/yahoo/ops/+x/fetch_stock.+x' '%s'", project_root, symbol);
    system(command);

    // Read price from symbol.txt (which fetch_stock creates in CWD)
    snprintf(command, sizeof(command), "'%s/projects/yahoo/ops/+x/read_price.+x' '%s'", project_root, symbol);
    FILE *pipe = popen(command, "r");
    if (!pipe) return 1;

    char output[1024];
    float price = 0.0;
    if (fgets(output, sizeof(output), pipe)) {
        char *p = strstr(output, "Current Price: ");
        if (p) price = atof(p + 15);
    }
    pclose(pipe);

    if (price > 0) {
        set_state_string(user_hash, "last_lookup_symbol", symbol);
        set_state_float(user_hash, "last_lookup_price", price);
        time_t now = time(NULL);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", localtime(&now));
        set_state_string(user_hash, "last_lookup_time", time_str);
        printf("%s Current Price: %.2f\n", symbol, price);
    } else {
        printf("Failed to fetch price for %s\n", symbol);
    }

    return 0;
}
