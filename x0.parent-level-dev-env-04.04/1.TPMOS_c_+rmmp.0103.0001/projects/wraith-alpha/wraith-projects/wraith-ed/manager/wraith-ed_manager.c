#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <sys/wait.h>

#define MAX_PATH 4096

char project_root[2048] = ".";
char project_session_dir[2048] = ".";
char debug_log_path[4096] = "";

/*
 * TPMOS compliance: every project's layout loads a manager, same as HTML
 * loads a <script> tag, regardless of whether the hot path already lives
 * in ops/wraith_project_input.c. Init-only — see
 * x0.piececrafts/wra-mana-checklist.txt for the full pattern/rationale.
 */

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int root_has_anchors(const char* root) {
    char pieces_path[4096], projects_path[4096];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_paths() {
    if (!getcwd(project_root, sizeof(project_root))) {
        strncpy(project_root, ".", sizeof(project_root) - 1);
    }
    project_root[sizeof(project_root) - 1] = '\0';

    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0 && *v) {
                    if (root_has_anchors(v)) {
                        snprintf(project_root, sizeof(project_root), "%s", v);
                    }
                }
            }
        }
        fclose(kvp);
    }

    snprintf(project_session_dir, sizeof(project_session_dir),
             "%s/projects/wraith-alpha/wraith-projects/wraith-ed/session", project_root);
    snprintf(debug_log_path, sizeof(debug_log_path), "%s/debug_log.txt", project_session_dir);
}

void log_debug(const char* fmt, ...) {
    FILE *f = fopen(debug_log_path, "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        fprintf(f, "[%ld] ", time(NULL));
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

void trigger_initial_render() {
    char op_path[4096];
    pid_t pid;

    snprintf(op_path, sizeof(op_path),
             "%s/projects/wraith-alpha/wraith-projects/wraith-ed/ops/+x/wraith_project_input.+x", project_root);
    if (access(op_path, X_OK) != 0) {
        log_debug("Initial render skipped: op not found at %s", op_path);
        return;
    }

    pid = fork();
    if (pid == 0) {
        char project_dir[4096];
        snprintf(project_dir, sizeof(project_dir),
                 "%s/projects/wraith-alpha/wraith-projects/wraith-ed", project_root);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(op_path, op_path, project_dir, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void trigger_render() {
    char frame_marker[4096];
    snprintf(frame_marker, sizeof(frame_marker), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(frame_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }
}

int main() {
    resolve_paths();

    log_debug("Wraith Ed Manager Started");
    log_debug("Project root: %s", project_root);
    log_debug("Session dir: %s", project_session_dir);

    trigger_initial_render();
    trigger_render();

    log_debug("Wraith Ed Manager init complete (hot path owned by ops/wraith_project_input.c)");

    return 0;
}
