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

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_MSG_LEN 256

/* --- Static Variables --- */
static char project_root[MAX_PATH] = ".";
static char current_project[MAX_LINE] = "chat-op";
static char active_target_id[64] = "selector";
static char last_key_str[32] = "None";
static char gui_state_path[MAX_PATH] = "";

static char node_hash[64] = "";
static char host_ip[64] = "127.0.0.1";
static char network_mode[32] = "CLIENT"; // or HOST
static int daemon_pid = -1;

static volatile sig_atomic_t g_shutdown = 0;

/* --- Forward Declarations --- */
static void handle_sigint(int sig);
static int run_command(const char* cmd);
static char* trim_str(char *str);
static void resolve_paths(void);
static void start_socket_daemon(void);
static void stop_socket_daemon(void);
static void send_to_outbox(const char *msg);
static int is_active_layout(void);
static void trigger_render(void);
static void save_manager_state(void);
static void process_key(int key);
static void process_command(const char *cmd);

/* --- Function Implementations --- */

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
    }
    return pid;
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
    snprintf(gui_state_path, sizeof(gui_state_path), "%s/projects/chat-op/manager/gui_state.txt", project_root);
}

static void start_socket_daemon(void) {
    stop_socket_daemon();
    char *cmd;
    asprintf(&cmd, "%s/projects/chat-op/ops/+x/p2p_socket_daemon.+x %s %s AUTO %s", 
             project_root, project_root, node_hash, network_mode);
    daemon_pid = run_command(cmd);
    free(cmd);
}

static void stop_socket_daemon(void) {
    if (daemon_pid > 0) {
        kill(daemon_pid, SIGTERM);
        waitpid(daemon_pid, NULL, WNOHANG);
        daemon_pid = -1;
    }
}

static void send_to_outbox(const char *msg) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/pieces/network/outbox.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

static void append_to_chat_log(const char* msg) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/projects/chat-op/chat_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s", msg);
        if (msg[strlen(msg)-1] != '\n') fprintf(f, "\n");
        fclose(f);
    }
}

static void cleanup_old_memory(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/projects/chat-op/chat_log.txt", project_root);
    unlink(path);
    
    snprintf(path, sizeof(path), "%s/projects/chat-op/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "messages=\ninput_text=\npeer_count=0\nnetwork_status=INIT\n");
        fclose(f);
    }
}

static void process_command(const char *cmd) {
    if (strncmp(cmd, "SET_MODE:", 9) == 0) {
        strncpy(network_mode, cmd + 9, sizeof(network_mode) - 1);
        cleanup_old_memory();
        start_socket_daemon();
    }
}

static void process_key(int key) {
    if (key == 10 || key == 13) { /* Enter key */
        char input_text[MAX_MSG_LEN] = "";
        char line[MAX_LINE];
        
        FILE *f = fopen(gui_state_path, "r");
        if (f) {
            FILE *debug = fopen("projects/chat-op/manager-debug.txt", "a");
            if (debug) fprintf(debug, "Reading gui_state.txt...\n");
            while (fgets(line, sizeof(line), f)) {
                if (debug) fprintf(debug, "LINE: %s", line);
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "input_text") == 0) {
                        strncpy(input_text, v, MAX_MSG_LEN - 1);
                    } else if (strcmp(k, "host_ip_input") == 0) {
                        strncpy(host_ip, v, 63);
                    }
                }
            }
            if (debug) fclose(debug);
            fclose(f);
        }

        if (strlen(input_text) > 0) {
            // Echo locally immediately
            char msg_echo[MAX_MSG_LEN + 32];
            snprintf(msg_echo, sizeof(msg_echo), "[%s] %s", node_hash, input_text);
            append_to_chat_log(msg_echo);

            send_to_outbox(input_text);
            
            // Re-write gui state (clear input)
            FILE *gf = fopen(gui_state_path, "a");
            if (gf) { fprintf(gf, "input_text=\n"); fclose(gf); }
            trigger_render();
        }
    }
    snprintf(last_key_str, sizeof(last_key_str), "KEY:%d", key);
}

static int is_active_layout(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int res = 0;
    if (fgets(line, sizeof(line), f)) res = (strstr(line, "chat-op") != NULL);
    fclose(f);
    return res;
}

static void trigger_render(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

static void save_manager_state(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=%s\nactive_target_id=%s\nlast_key=%s\n", 
                current_project, active_target_id, last_key_str);
        fclose(f);
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    snprintf(node_hash, sizeof(node_hash), "%x%x", (unsigned int)time(NULL), (unsigned int)getpid());

    cleanup_old_memory();
    
    // Launch daemon
    start_socket_daemon();

    char hist_path[1024];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/apps/player_app/history.txt", project_root);

    long last_pos = 0;
    long last_gui_size = 0;
    struct stat st, gui_st;

    while (!g_shutdown) {
        if (!is_active_layout()) {
            stop_socket_daemon();
            usleep(100000);
            continue;
        }

        // Poll gui_state.txt for changes
        if (stat(gui_state_path, &gui_st) == 0) {
            if (gui_st.st_size != last_gui_size) {
                trigger_render();
                last_gui_size = gui_st.st_size;
            }
        }

        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), hf)) {
                        char *t = trim_str(line);
                        if (isdigit(t[0])) process_key(atoi(t));
                        else if (strncmp(t, "SET_", 4) == 0) process_command(t);
                        save_manager_state();
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    stop_socket_daemon();
    return 0;
}
