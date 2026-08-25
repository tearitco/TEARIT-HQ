/*
 * Op: console_print
 * Usage: ./+x/console_print.+x <message>
 * Args: message - Text to display (can be quoted for spaces)
 * 
 * Behavior:
 *   1. Writes message to last_response.txt
 *   2. Hits frame_changed.txt to trigger re-render
 *   3. Logs to master_ledger.txt
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "X\n"); fclose(f); }
        free(path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        return 1;
    }
    
    resolve_root();
    
    /* Build message from all arguments */
    char message[MAX_PATH] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(message, " ");
        strcat(message, argv[i]);
    }
    
    /* Write to last_response.txt (APPEND MODE) */
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/fuzzpet_app/fuzzpet/last_response.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "%s ", message);
            fclose(f);
        }
        free(path);
    }
    
    /* Log event */
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t rawtime; struct tm *timeinfo; char timestamp[100];
            time(&rawtime); timeinfo = localtime(&rawtime);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
            fprintf(f, "[%s] ConsolePrint: %s\n", timestamp, message);
            fclose(f);
        }
        free(path);
    }
    
    return 0;
}
