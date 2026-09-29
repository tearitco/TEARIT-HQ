#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // REAL FIX 2026-09-28 (bug_bounty.md, direct instruction: "we
    // rather use malloc, and sizeof, but remember, linux automatically
    // frees memory, so double free could occur"): realpath(path, NULL)
    // is a glibc extension that MALLOCs exactly the resolved length
    // itself - no fixed destination buffer to size, ever (a fixed
    // buffer under PATH_MAX tripping FORTIFY_SOURCE was the earlier fix
    // here; this is the permanent version of that fix). Each resolved
    // pointer is read exactly once (the strcpy/snprintf immediately
    // below it) and freed right after, same scope, no branch - matches
    // TPMOS house precedent (`1.TPMOS.../#.docs/^.pmo.ld-faq+8/
    // PITFALLS_ACTIVE_2026-03-18.txt` #20: never free before the read
    // completes, and never on more than one path).
    char script_path[4096];
    char *ops_dir_ptr;
    char page_root[4096];

    // Get the absolute path of this binary
    char *resolved_script = realpath(argv[0], NULL);
    if (argc > 0 && resolved_script) {
        snprintf(script_path, sizeof(script_path), "%s", resolved_script);
        free(resolved_script);
        // script_path is now: ...&.hq-apps/world-manager/ops/world_manager_init
        // We need: ...&.hq-apps/world-manager/

        // Find last two slashes and extract parent directory (ops -> world-manager)
        char *last_slash = strrchr(script_path, '/');
        if (last_slash) {
            *last_slash = '\0';  // Remove /world_manager_init
            char *second_last = strrchr(script_path, '/');
            if (second_last) {
                *second_last = '\0';  // Remove /ops
                strcpy(page_root, script_path);  // page_root is now .../world-manager
            }
        }
    } else {
        if (resolved_script) free(resolved_script);
        strcpy(page_root, ".");
    }

    // Normalize
    {
        char *resolved_root = realpath(page_root, NULL);
        if (resolved_root) {
            snprintf(page_root, sizeof(page_root), "%s", resolved_root);
            free(resolved_root);
        }
    }
    
    char state_dir[4096];
    snprintf(state_dir, sizeof(state_dir), "%s/state", page_root);
    
    char log_file[4096];
    snprintf(log_file, sizeof(log_file), "%s/page_manager.log", state_dir);
    
    char cursor_file[4096];
    snprintf(cursor_file, sizeof(cursor_file), "%s/page_manager.cursor", state_dir);
    
    // Create directories
    pid_t mkdir_pid = fork();
    if (mkdir_pid == 0) {
        // Child process: create state directory
        execvp("mkdir", (char *[]) { "mkdir", "-p", state_dir, NULL });
        exit(1);
    } else if (mkdir_pid > 0) {
        int mkdir_status;
        waitpid(mkdir_pid, &mkdir_status, 0);
    }

    // Create event_pkg directory
    mkdir_pid = fork();
    if (mkdir_pid == 0) {
        char event_pkg_dir[4096];
        snprintf(event_pkg_dir, sizeof(event_pkg_dir), "%s/event_pkg", page_root);
        execvp("mkdir", (char *[]) { "mkdir", "-p", event_pkg_dir, NULL });
        exit(1);
    } else if (mkdir_pid > 0) {
        int mkdir_status;
        waitpid(mkdir_pid, &mkdir_status, 0);
    }
    
    // Create empty ledger files
    const char *ledgers[] = {"entities_live", "world_events", "animation_queue"};
    for (int i = 0; i < 3; i++) {
        char ledger_path[4096];
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
