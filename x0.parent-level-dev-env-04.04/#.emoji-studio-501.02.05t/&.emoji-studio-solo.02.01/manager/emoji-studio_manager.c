#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

/*
 * emoji-studio_manager.c - Standalone Manager
 * Orchestrates emoji extraction and project state.
 */

#define MAX_LINE 1024
#define MAX_EMOJIS 4000

typedef struct {
    char name[256];
    int index;
} EmojiEntry;

EmojiEntry emoji_list[MAX_EMOJIS];
int emoji_count = 0;
int selected_index = -1;
int resolution = 8;
char active_piece[256] = "";

void load_emoji_list() {
    FILE *f = fopen("../parsed_emojis.txt", "r");
    if (!f) return;
    char line[MAX_LINE];
    int idx = 0;
    while (fgets(line, sizeof(line), f) && emoji_count < MAX_EMOJIS) {
        if (line[0] == '#') continue;
        char *token = strtok(line, "|"); // hex
        token = strtok(NULL, "|"); // string
        token = strtok(NULL, "|"); // name
        if (token) {
            char *trimmed = token;
            trimmed[strcspn(trimmed, "\n\r")] = 0;
            strncpy(emoji_list[emoji_count].name, trimmed, 255);
            emoji_list[emoji_count].index = idx++;
            emoji_count++;
        }
    }
    fclose(f);
}

void extract_emoji(int idx, int res) {
    if (idx < 0 || idx >= emoji_count) return;
    
    char output_dir[1024];
    snprintf(output_dir, sizeof(output_dir), "pieces/%s", emoji_list[idx].name);
    
    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
    if (system(mkdir_cmd) != 0) {
        fprintf(stderr, "Error creating directory: %s\n", output_dir);
    }
    
    char csv_path[2048];
    snprintf(csv_path, sizeof(csv_path), "%s/voxels.csv", output_dir);
    
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "./emoji-xtract \"../#.emoji.xtract.stb]c4/emoji_atlas.png\" %d %d \"%s\"", 
             emoji_list[idx].index, res, csv_path);
    
    if (system(cmd) != 0) {
        fprintf(stderr, "Error executing extraction: %s\n", cmd);
    }
    
    char state_path[2048];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", output_dir);
    FILE *sf = fopen(state_path, "w");
    if (sf) {
        fprintf(sf, "name=%s\n", emoji_list[idx].name);
        fprintf(sf, "index=%d\n", emoji_list[idx].index);
        fprintf(sf, "resolution=%d\n", res);
        fprintf(sf, "pos=0,0,0\n");
        fprintf(sf, "scale=1.0\n");
        fclose(sf);
    }
    
    strncpy(active_piece, emoji_list[idx].name, 255);
}

void write_gui_state() {
    FILE *f = fopen("manager/gui_state.txt.tmp", "w");
    if (!f) return;
    fprintf(f, "app_title=Emoji Studio\n");
    fprintf(f, "active_piece=%s\n", active_piece);
    fprintf(f, "resolution=%d\n", resolution);
    fprintf(f, "emoji_count=%d\n", emoji_count);
    
    /* Publish a condensed list for the GL Grid */
    fprintf(f, "emoji_list_start\n");
    for (int i = 0; i < emoji_count && i < 100; i++) { // Limit for now
        fprintf(f, "%d:%s\n", emoji_list[i].index, emoji_list[i].name);
    }
    fprintf(f, "emoji_list_end\n");
    fclose(f);
    rename("manager/gui_state.txt.tmp", "manager/gui_state.txt");
}

int main() {
    setpgid(0, 0);
    load_emoji_list();
    
    while (1) {
        /* Monitor history for commands */
        FILE *hf = fopen("session/history.txt", "r");
        if (hf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), hf)) {
                if (strstr(line, "COMMAND: SELECT ")) {
                    int idx = atoi(line + 16);
                    selected_index = idx;
                    extract_emoji(idx, resolution);
                } else if (strstr(line, "COMMAND: SET_RES ")) {
                    resolution = atoi(line + 17);
                    if (selected_index != -1) extract_emoji(selected_index, resolution);
                }
            }
            fclose(hf);
            /* Clear history after processing to avoid re-run */
            FILE *hfw = fopen("session/history.txt", "w");
            if (hfw) fclose(hfw);
        }
        
        write_gui_state();
        
        /* Pulse */
        FILE *p = fopen("session/frame_changed.txt", "a");
        if (p) { fprintf(p, "P\n"); fclose(p); }
        
        usleep(100000); // 10 FPS
    }
    return 0;
}
