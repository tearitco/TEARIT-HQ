#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

#define MAX_PATH 4096

static volatile int g_shutdown = 0;
static char g_project_root[MAX_PATH] = ".";
static char g_session_dir[MAX_PATH] = ".";

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

void log_debug(const char* fmt, ...) {
    char log_path[MAX_PATH];
    snprintf(log_path, sizeof(log_path), "%s/debug_log.txt", g_session_dir);
    FILE *f = fopen(log_path, "a");
    if (f) {
        fprintf(f, "[%ld] ", time(NULL));
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

void write_gui_state(int counter) {
    char gui_path[MAX_PATH];
    snprintf(gui_path, sizeof(gui_path), "%s/manager/gui_state.txt", g_session_dir);
    FILE *f = fopen(gui_path, "w");
    if (f) {
        fprintf(f, "module_path=projects/wraith-alpha/wraith-projects/wraith-man-test/manager/+x/wraith-man-test_manager.+x\n");
        fprintf(f, "project_id=wraith-alpha/wraith-projects/wraith-man-test\n");
        fprintf(f, "test_output=MANAGER ACTIVE - Counter: %d\n", counter);
        fprintf(f, "status=Running\n");
        fclose(f);
    }
}

void write_view() {
    char view_path[MAX_PATH];
    snprintf(view_path, sizeof(view_path), "%s/wraith_body.txt", g_session_dir);
    FILE *f = fopen(view_path, "w");
    if (f) {
        fprintf(f, "╔════════════════════════════╗\n");
        fprintf(f, "║   WRAITH MANAGER TEST      ║\n");
        fprintf(f, "║   (Test Project)           ║\n");
        fprintf(f, "╚════════════════════════════╝\n");
        fprintf(f, "\n");
        fprintf(f, "✓ Manager is RUNNING\n");
        fprintf(f, "✓ This text is from wraith-man-test_manager\n");
        fprintf(f, "✓ Session: wraith-man-test\n");
        fprintf(f, "\n");
        fprintf(f, "Status: If you see this, manager launched!\n");
        fclose(f);
    }
}

void resolve_paths() {
    if (!getcwd(g_project_root, sizeof(g_project_root))) {
        strncpy(g_project_root, ".", sizeof(g_project_root) - 1);
    }

    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *val = line + 13;
                while (*val && (*val == ' ' || *val == '\t')) val++;
                size_t len = strlen(val);
                while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r')) len--;
                strncpy(g_project_root, val, len);
                g_project_root[len] = '\0';
                break;
            }
        }
        fclose(kvp);
    }

    snprintf(g_session_dir, sizeof(g_session_dir),
             "%s/projects/wraith-alpha/wraith-projects/wraith-man-test/session", g_project_root);
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    resolve_paths();

    write_view();
    write_gui_state(0);

    log_debug("wraith-man-test Manager Started");
    log_debug("Project root: %s", g_project_root);
    log_debug("Session dir: %s", g_session_dir);
    log_debug("Manager running");

    int counter = 0;
    while (!g_shutdown) {
        sleep(1);
        counter++;
        if (counter % 10 == 0) {
            write_gui_state(counter);
            log_debug("Heartbeat: %d seconds", counter);
        }
    }

    log_debug("Manager shutdown");
    return 0;
}
