#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include <ctype.h>

// proc_manager.c - OS Process Monitor (v1.1 - ASPRINTF REFACTOR)
// Responsibility: Manage the global process list without warnings.

#define MAX_LINE 1024
#define MAX_PATH 16384

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void log_event(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *msg = NULL;
    if (vasprintf(&msg, fmt, args) != -1) {
        FILE *f = fopen("pieces/os/proc_list.txt", "a"); // Simplified for now
        if (f) {
            fprintf(f, "%s\n", msg);
            fclose(f);
        }
        free(msg);
    }
    va_end(args);
}

void register_proc(const char* name, int pid) {
    FILE *f = fopen("pieces/os/proc_list.txt", "a");
    if (f) {
        fprintf(f, "%d %s\n", pid, name);
        fclose(f);
    }
}

void list_procs() {
    FILE *f = fopen("pieces/os/proc_list.txt", "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            int pid;
            char name[128];
            if (sscanf(line, "%d %127s", &pid, name) == 2) {
                char *proc_path = NULL;
                asprintf(&proc_path, "/proc/%d/status", pid);
                if (access(proc_path, F_OK) == 0) {
                    printf("[RUNNING] %s (PID: %d)\n", name, pid);
                } else {
                    printf("[STOPPED] %s (PID: %d)\n", name, pid);
                }
                free(proc_path);
            }
        }
        fclose(f);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    if (strcmp(argv[1], "register") == 0 && argc >= 4) {
        register_proc(argv[2], atoi(argv[3]));
    } else if (strcmp(argv[1], "list") == 0) {
        list_procs();
    }
    return 0;
}
