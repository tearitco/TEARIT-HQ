#define _DEFAULT_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>


#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_shutdown = 0;

void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    // TPMOS standard: try relative paths first, then absolute if provided
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) {
        // Fallback: search up the tree for standard TPMOS root markers
        kvp = fopen("../../../pieces/locations/location_kvp", "r");
    }
    
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    strncpy(project_root, v, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

int run_op(const char* op_name, const char* arg1, const char* arg2, const char* arg3) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/ops/+x/%s.+x", project_root, op_name);
    pid_t pid = fork();
    if (pid == 0) {
        // In Op: redirect output, execute
        if (arg3) execl(path, path, arg1, arg2, arg3, NULL);
        else if (arg2) execl(path, path, arg1, arg2, NULL);
        else if (arg1) execl(path, path, arg1, NULL);
        else execl(path, path, NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
    return -1;
}

void trigger_render() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "P\n");
        fclose(f);
    }
}

void update_state(const char* user_hash) {
    char user_state_path[MAX_PATH];
    snprintf(user_state_path, sizeof(user_state_path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, user_hash);
    
    char manager_state_path[MAX_PATH];
    snprintf(manager_state_path, sizeof(manager_state_path), "%s/projects/yahoo/manager/state.txt", project_root);
    
    FILE *uf = fopen(user_state_path, "r");
    FILE *mf = fopen(manager_state_path, "w");
    if (!mf) return;

    fprintf(mf, "user_id=%s\n", user_hash);
    if (uf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), uf)) {
            fputs(line, mf);
        }
        fclose(uf);
    }
    fclose(mf);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    char *user_hash = "CA33D1";
    if (argc > 1) user_hash = argv[1];

    // Ensure user piece exists
    char user_dir[MAX_PATH];
    snprintf(user_dir, sizeof(user_dir), "%s/projects/yahoo/pieces/user_%s", project_root, user_hash);
    mkdir(user_dir, 0777);
    
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", user_dir);
    if (access(state_path, F_OK) == -1) {
        char cmd[MAX_PATH * 2];
        snprintf(cmd, sizeof(cmd), "cp %s/projects/yahoo/pieces/user_template/state.txt %s", project_root, state_path);
        system(cmd);
    }

    update_state(user_hash);
    trigger_render();

    FILE *dbg = fopen("manager_debug.txt", "w");
    if (dbg) {
        fprintf(dbg, "Yahoo Manager started. Root: %s, User: %s\n", project_root, user_hash);
        fclose(dbg);
    }

    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    long last_pos = 0;
    struct stat st;

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(hist_path, "r");
                if (f) {
                    fseek(f, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(f, "%d", &key) == 1) {
                        if (key == '1') run_op("lookup_stock", user_hash, "NVDA", NULL);
                        else if (key == '2') run_op("add_credit", user_hash, "1000", NULL);
                        else if (key == '3') run_op("portfolio_new", user_hash, NULL, NULL);
                        update_state(user_hash);
                        trigger_render();
                    }
                    last_pos = ftell(f);
                    fclose(f);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        usleep(16667); // 60 FPS
    }

    return 0;
}
