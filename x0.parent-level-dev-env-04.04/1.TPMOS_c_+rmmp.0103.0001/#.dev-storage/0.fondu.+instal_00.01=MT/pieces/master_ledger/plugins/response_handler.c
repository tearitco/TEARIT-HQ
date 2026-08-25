#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <direct.h>
#endif

#define TPM_PIECE_MANAGER_REL_PATH "pieces/master_ledger/plugins/+x/piece_manager.+x"
static char* pm_path_for(const char *project_root) {
    char *result = NULL;
    if (asprintf(&result, "%s/%s", project_root, TPM_PIECE_MANAGER_REL_PATH) == -1)
        return NULL;
    return result;
}

// Response Handler - Passive Detection via stat() on master_ledger.txt
// Optimized: Uses stat() for file size monitoring instead of tight polling loop

#define MAX_LINE 500
#define MAX_PATH 512

char project_root[MAX_PATH] = ".";

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                if (strlen(v) > 0) snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
}

void log_response(const char *event_type, const char *start_piece, const char *response) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    char *log_path = NULL;
    asprintf(&log_path, "%s/pieces/master_ledger/master_ledger.txt", project_root);
    FILE *log_fp = fopen(log_path, "a");
    if (log_fp) {
        fprintf(log_fp, "[%s] ResponseTriggered: %s on %s | Message: %s | Source: response_handler\n", 
               timestamp, event_type, start_piece, response);
        fclose(log_fp);
    }
    free(log_path);
}

void process_response_request(const char *line) {
    // Parse: ResponseRequest: event_type|start_piece
    char *req_start = strstr(line, "ResponseRequest: ");
    if (!req_start) return;
    
    char *data = req_start + 17;
    char event_type[128] = "";
    char start_piece[128] = "";
    
    char *pipe = strchr(data, '|');
    if (pipe) {
        int len = pipe - data;
        if (len > 0 && len < 128) {
            strncpy(event_type, data, len);
            event_type[len] = '\0';
        }
        strncpy(start_piece, pipe + 1, 127);
        start_piece[127] = '\0';
        char *nl = strchr(start_piece, '\n');
        if (nl) *nl = '\0';
    }
    
    if (strlen(event_type) == 0 || strlen(start_piece) == 0) return;
    
    // Call piece_manager to get response
    char *pm_path = pm_path_for(project_root);
    if (!pm_path) return;
    
    char *piece_name = NULL;
    asprintf(&piece_name, "response_%s", start_piece);
    
    char *cmd = NULL;
    asprintf(&cmd, "'%s' '%s' get_response '%s' 2>/dev/null", pm_path, piece_name, event_type);
    
    FILE *rsp_fp = popen(cmd, "r");
    if (rsp_fp) {
        char response[512] = "";
        if (fgets(response, sizeof(response), rsp_fp)) {
            char *nl = strchr(response, '\n');
            if (nl) *nl = '\0';
            if (strlen(response) > 0) {
                log_response(event_type, start_piece, response);
            }
        }
        pclose(rsp_fp);
    }
    
    free(cmd);
    free(piece_name);
    free(pm_path);
}

int main() {
    resolve_root();

    printf("Response Handler Service Started\n");
    printf("Monitoring master_ledger.txt for response requests (Passive Detection)...\n");
    printf("Press Ctrl+C to stop.\n\n");

#ifdef _WIN32
    _mkdir("pieces\\master_ledger");
#else
    mkdir("pieces/master_ledger", 0755);
#endif

    FILE *test_file = fopen("pieces/master_ledger/master_ledger.txt", "a");
    if (test_file) fclose(test_file);
    
    const char *ledger_path = "pieces/master_ledger/master_ledger.txt";
    struct stat st;
    long last_size = 0;
    long last_position = 0;
    
    if (stat(ledger_path, &st) == 0) {
        last_size = st.st_size;
        last_position = last_size;
    }

    printf("Monitoring started (stat-driven, low CPU)...\n");
    
    while (1) {
        if (stat(ledger_path, &st) == 0) {
            if (st.st_size > last_size) {
                FILE *fp = fopen(ledger_path, "r");
                if (fp) {
                    fseek(fp, last_position, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), fp)) {
                        last_position = ftell(fp);
                        if (strstr(line, "ResponseRequest: ")) {
                            process_response_request(line);
                        }
                    }
                    fclose(fp);
                    last_size = st.st_size;
                }
            } else if (st.st_size < last_size) {
                // File truncated (new session)
                last_size = st.st_size;
                last_position = st.st_size;
            }
        }
        
        usleep(100000); // 100ms - stat() driven, minimal CPU
    }
    
    return 0;
}