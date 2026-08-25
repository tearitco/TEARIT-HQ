/*
 * user_manager.c - User Profile Manager (v3.0)
 * Standardized CHTPM interaction.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <windows.h>
#define WEXITSTATUS(status) (status)
#define usleep(us) Sleep((us)/1000)
#endif
#include <sys/stat.h>

#define STATE_FILE "projects/user/manager/state.txt"
#define OPS_PATH "projects/user/ops/+x"

typedef struct {
    char username_input[256];
    char password_input[256];
    char auth_response[512];
    char session_status[50];
    char current_user[256];
    char last_key[10];
    char active_target_id[64];
    char input_mode[50];
} AppState;

void trim_whitespace(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = 0;
}

int read_state(AppState *state) {
    FILE *fp = fopen(STATE_FILE, "r");
    if (!fp) return 0;
    char line[1024], key[100], value[512];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%99[^=]=%511[^\n]", key, value) == 2) {
            trim_whitespace(value);
            if (strcmp(key, "username_input") == 0) strcpy(state->username_input, value);
            else if (strcmp(key, "password_input") == 0) strcpy(state->password_input, value);
            else if (strcmp(key, "auth_response") == 0) strcpy(state->auth_response, value);
            else if (strcmp(key, "session_status") == 0) strcpy(state->session_status, value);
            else if (strcmp(key, "current_user") == 0) strcpy(state->current_user, value);
            else if (strcmp(key, "last_key") == 0) strcpy(state->last_key, value);
            else if (strcmp(key, "active_target_id") == 0) strcpy(state->active_target_id, value);
            else if (strcmp(key, "input_mode") == 0) strcpy(state->input_mode, value);
        } else if (sscanf(line, "%99[^=]=", key) == 1) { // Handle empty values
            if (strcmp(key, "username_input") == 0) state->username_input[0] = '\0';
            else if (strcmp(key, "password_input") == 0) state->password_input[0] = '\0';
            else if (strcmp(key, "auth_response") == 0) state->auth_response[0] = '\0';
            else if (strcmp(key, "last_key") == 0) state->last_key[0] = '\0';
        }
    }
    fclose(fp);
    return 1;
}

int write_state(const AppState *state) {
    FILE *fp = fopen(STATE_FILE, "w");
    if (!fp) return -1;
    fprintf(fp, "username_input=%s\n", state->username_input);
    fprintf(fp, "password_input=%s\n", state->password_input);
    fprintf(fp, "auth_response=%s\n", state->auth_response);
    fprintf(fp, "session_status=%s\n", state->session_status);
    fprintf(fp, "current_user=%s\n", state->current_user);
    fprintf(fp, "last_key=%s\n", state->last_key);
    fprintf(fp, "active_target_id=%s\n", state->active_target_id);
    fprintf(fp, "input_mode=%s\n", state->input_mode);
    fclose(fp);

    /* Write gui_state.txt for CHTPM Parser */
    fp = fopen("projects/user/manager/gui_state.txt", "w");
    if (fp) {
        fprintf(fp, "module_path=projects/user/manager/+x/user_manager.+x\n");
        fprintf(fp, "active_layout_id=user.chtpm\n");
        fprintf(fp, "app_title=USER PROFILE\n");
        fprintf(fp, "username_input=%s\n", state->username_input);
        fprintf(fp, "password_input=%s\n", state->password_input);
        fprintf(fp, "auth_response=%s\n", state->auth_response);
        fprintf(fp, "session_status=%s\n", state->session_status);
        fprintf(fp, "current_user=%s\n", state->current_user);
        fclose(fp);
    }

    return 0;
}

int main(void) {
    AppState state;
    memset(&state, 0, sizeof(state));
    strcpy(state.session_status, "IDLE");
    strcpy(state.auth_response, "Ready.");

    while (1) {
        if (read_state(&state)) {
            if (strlen(state.last_key) > 0) {
                int key = atoi(state.last_key);
                memset(state.last_key, 0, sizeof(state.last_key));
                
                if (key == 27) break; // ESC
                if (key == 5) { // Global Clear
                    memset(state.username_input, 0, sizeof(state.username_input));
                    memset(state.password_input, 0, sizeof(state.password_input));
                    strcpy(state.auth_response, "Cleared.");
                } else if (key == 3) { // Login Action
                    char cmd[1024];
                    snprintf(cmd, sizeof(cmd), "'%s/auth_user.+x' '.' '%s'", OPS_PATH, state.username_input);
                    int res = system(cmd);
                    if (WEXITSTATUS(res) == 0) {
                        strcpy(state.current_user, state.username_input);
                        strcpy(state.session_status, "AUTHENTICATED");
                        strcpy(state.auth_response, "Login Successful.");
                        // Force redirect to profile via manager pulse? 
                        // Actually, CHTPM doesn't support server-side href easily yet.
                        // We'll rely on the user to click Back or use the profile.
                    } else {
                        strcpy(state.auth_response, "Login Failed.");
                    }
                } else if (key == 4) { // Signup/Logout
                    if (strlen(state.current_user) > 0) { // Logout
                        memset(state.current_user, 0, sizeof(state.current_user));
                        strcpy(state.session_status, "IDLE");
                        strcpy(state.auth_response, "Logged out.");
                    } else { // Signup
                        char cmd[1024];
                        snprintf(cmd, sizeof(cmd), "'%s/create_profile.+x' '.' '%s'", OPS_PATH, state.username_input);
                        int res = system(cmd);
                        if (WEXITSTATUS(res) == 0) {
                            strcpy(state.auth_response, "Profile created.");
                        } else {
                            strcpy(state.auth_response, "Signup Failed.");
                        }
                    }
                }
                write_state(&state);
            }
        }
        usleep(16667); // 60 FPS
    }
    return 0;
}
