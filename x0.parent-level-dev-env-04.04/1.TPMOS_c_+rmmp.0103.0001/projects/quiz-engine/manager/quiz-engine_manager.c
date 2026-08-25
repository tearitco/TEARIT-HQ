#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#define MODULE_NAME "quiz-engine"
#define MAX_PATH 4096
#define MAX_LINE 1024

static char project_root[MAX_PATH] = ".";
static char current_project[MAX_LINE] = "quiz-engine";
static char active_target_id[64] = "xlector";
static char last_key_str[32] = "None";

static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
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

static void resolve_paths(void) {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }
}

static int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    int result = 0;
    if (fgets(line, sizeof(line), f)) {
        result = (strstr(line, "quiz.chtpm") != NULL);
    }
    fclose(f);
    free(path);
    return result;
}

static void process_key(int key) {
    char cmd[MAX_PATH];
    if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");

    if (key == '1') {
        /* Submit Answer */
        char answer[MAX_LINE] = "";
        char buf_path[MAX_PATH];
        snprintf(buf_path, sizeof(buf_path), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
        FILE *bf = fopen(buf_path, "r");
        if (bf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), bf)) {
                if (line[0] == 'A') {
                    char *val = line + 1;
                    val[strcspn(val, "\n")] = 0;
                    strcpy(answer, val);
                }
            }
            fclose(bf);
        }
        
        char q_id[64] = "";
        char state_path[MAX_PATH];
        snprintf(state_path, sizeof(state_path), "%s/projects/quiz-engine/manager/state.txt", project_root);
        FILE *sf = fopen(state_path, "r");
        if (sf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), sf)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "q_id") == 0) strcpy(q_id, trim_str(eq+1));
                }
            }
            fclose(sf);
        }

        if (strlen(q_id) > 0) {
            snprintf(cmd, sizeof(cmd), "%s/projects/quiz-engine/ops/+x/check_answer.+x %s 1 \"%s\" > /dev/null 2>&1", 
                     project_root, q_id, answer);
            run_command(cmd);
        }
    } else if (key == '2') {
        /* Next Question */
        snprintf(cmd, sizeof(cmd), "%s/projects/quiz-engine/ops/+x/load_question.+x > /dev/null 2>&1", project_root);
        run_command(cmd);
        
        /* Clear answer buffer */
        char buf_path[MAX_PATH];
        snprintf(buf_path, sizeof(buf_path), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
        FILE *bf = fopen(buf_path, "a");
        if (bf) { fprintf(bf, "A\n"); fclose(bf); }
    }
}

static void update_state(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *rf = fopen(path, "r");
        char lines[200][MAX_LINE];
        int lc = 0, found_proj = 0, found_key = 0;
        if (rf) {
            while (fgets(lines[lc], MAX_LINE, rf) && lc < 190) {
                if (strncmp(lines[lc], "project_id=", 11) == 0) {
                    snprintf(lines[lc], MAX_LINE, "project_id=%s\n", current_project);
                    found_proj = 1;
                } else if (strncmp(lines[lc], "last_key=", 9) == 0) {
                    snprintf(lines[lc], MAX_LINE, "last_key=%s\n", last_key_str);
                    found_key = 1;
                }
                lc++;
            }
            fclose(rf);
        }
        if (!found_proj && lc < 190) snprintf(lines[lc++], MAX_LINE, "project_id=%s\n", current_project);
        if (!found_key && lc < 190) snprintf(lines[lc++], MAX_LINE, "last_key=%s\n", last_key_str);
        
        FILE *wf = fopen(path, "w");
        if (wf) {
            for (int i = 0; i < lc; i++) fputs(lines[i], wf);
            fclose(wf);
        }
        free(path);
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    /* Initial question load */
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "%s/projects/quiz-engine/ops/+x/load_question.+x > /dev/null 2>&1", project_root);
    run_command(cmd);
    
    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;
    
    long last_pos = 0;
    struct stat st;
    
    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        process_key(key);
                        update_state();
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    free(hist_path);
    return 0;
}
