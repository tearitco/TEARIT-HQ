#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    char script_path[2048];
    char *ops_dir_ptr;
    char page_root[2048];
    char normalized[2048];
    
    // Get the absolute path of this binary
    if (argc > 0 && realpath(argv[0], script_path)) {
        // script_path is now: .../pages/test_page_001/manager/ops/page_manager_init
        // We need the parent: .../pages/test_page_001/manager
        // Then parent again: .../pages/test_page_001
        
        // Find last two slashes and extract parent of parent
        char *last_slash = strrchr(script_path, '/');
        if (last_slash) {
            *last_slash = '\0';  // Remove /page_manager_init
            char *second_last = strrchr(script_path, '/');
            if (second_last) {
                *second_last = '\0';  // Remove /ops
                strcpy(page_root, script_path);  // page_root is now .../pages/test_page_001/manager
                // We want parent of manager, so go up one more
                last_slash = strrchr(page_root, '/');
                if (last_slash) {
                    *last_slash = '\0';  // Remove /manager
                }
            }
        }
    } else {
        strcpy(page_root, ".");
    }
    
    // Normalize
    realpath(page_root, normalized);
    strcpy(page_root, normalized);
    
    char state_dir[2048];
    snprintf(state_dir, sizeof(state_dir), "%s/state", page_root);
    
    char log_file[2048];
    snprintf(log_file, sizeof(log_file), "%s/page_manager.log", state_dir);
    
    char cursor_file[2048];
    snprintf(cursor_file, sizeof(cursor_file), "%s/page_manager.cursor", state_dir);
    
    // Create directories
    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s' '%s/event_pkg'", state_dir, page_root);
    system(mkdir_cmd);
    
    // Create empty ledger files
    const char *ledgers[] = {"entities_live", "world_events", "animation_queue"};
    for (int i = 0; i < 3; i++) {
        char ledger_path[2048];
        snprintf(ledger_path, sizeof(ledger_path), "%s/%s.txt", state_dir, ledgers[i]);
        FILE *fp = fopen(ledger_path, "a");
        if (fp) fclose(fp);
    }
    
    // Create cursor marker file if it doesn't exist
    FILE *fp = fopen(cursor_file, "r");
    if (!fp) {
        fp = fopen(cursor_file, "w");
        if (fp) {
            fprintf(fp, "entities_live: 0\nworld_events: 0\nanimation_queue: 0\n");
            fclose(fp);
        }
    } else {
        fclose(fp);
    }
    
    // Initialize log
    fp = fopen(log_file, "w");
    if (fp) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(fp, "[%s] Page Manager initialized for: %s\n", timestamp, page_root);
        fclose(fp);
    }
    
    return 0;
}
