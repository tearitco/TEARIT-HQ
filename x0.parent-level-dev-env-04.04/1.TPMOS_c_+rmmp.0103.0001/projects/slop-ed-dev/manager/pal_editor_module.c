/*
 * pal_editor_module.c - PAL Script Editor for slop-ed-dev
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_INSTRUCTIONS 64
#define MAX_OPS 32

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "slop-ed-dev";
char active_piece[MAX_LINE] = "xlector";
char active_event[MAX_LINE] = "on_interact";

/* UI State */
int gui_focus_index = 1;
int selected_op_idx = 0;

typedef enum {
    MODE_BROWSE = 0,
    MODE_SELECT_OP,
    MODE_PARAM_INPUT
} EditorMode;

EditorMode editor_mode = MODE_BROWSE;

/* Parameter Buffers */
char param_1_val[256] = "";
char param_2_val[256] = "";

/* PAL instruction storage */
typedef struct {
    char mnemonic[32];
    char args[256];
    char high_level_name[64];
} Instruction;

Instruction instructions[MAX_INSTRUCTIONS];
int instruction_count = 0;
int cursor_line = 0;

/* Available Ops */
typedef struct {
    char name[32];
    char pal_template[128];
    char param_1_label[32];
    char param_2_label[32];
    char desc[128];
} OpDef;

OpDef available_ops[MAX_OPS] = {
    {"Show Text", "SET_RESPONSE \"%s\"", "Message", "", "RPG: Display message"},
    {"Move Entity", "CALL_OP \"move_entity\" \"%s\" \"%s\" \"${project_id}\"", "Piece ID", "Dir (w/a/s/d)", "Physics: Move entity"},
    {"Change Map", "TRANSITION \"projects/slop-ed-dev/games/${project_id}/maps/%s.txt\"", "Map Name", "", "World: Teleport"},
    {"Wait", "SLEEP %s", "Millis", "", "Logic: Pause execution"},
    {"Give Item", "CALL_OP \"inventory_op\" \"%s\" \"add\" \"%s\"", "Piece ID", "Item ID", "RPG: Add to inventory"},
    {"Set Variable", "SET_STATE \"${pal_editor_piece}\" \"%s\" \"%s\"", "Key", "Value", "Logic: Set piece state"},
    {"Halt", "HALT", "", "", "System: Stop script"},
};
int op_count = 7;

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void transition_to_layout(const char *layout_path) {
    char *lp = NULL;
    if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
        FILE *f = fopen(lp, "a");
        if (f) { fprintf(f, "%s\n", layout_path); fclose(f); }
        free(lp);
    }
}

void sync_focus() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) { 
            int active_idx = 0; 
            if (fscanf(f, "%d", &active_idx) == 1) { 
                if (active_idx > 0) gui_focus_index = active_idx; 
            } 
            fclose(f); 
        }
        free(path);
    }
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void load_script() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/events/%s.asm",
                 project_root, current_project, active_piece, active_event) == -1) return;
    
    FILE *f = fopen(path, "r");
    instruction_count = 0;
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && instruction_count < MAX_INSTRUCTIONS) {
            char *trimmed = trim_str(line);
            if (trimmed[0] == '\0' || trimmed[0] == '#') continue;
            
            char *space = strchr(trimmed, ' ');
            if (space) {
                int len = space - trimmed;
                if (len >= 32) len = 31;
                strncpy(instructions[instruction_count].mnemonic, trimmed, len);
                instructions[instruction_count].mnemonic[len] = '\0';
                strncpy(instructions[instruction_count].args, trim_str(space + 1), 255);
                snprintf(instructions[instruction_count].high_level_name, 63, "%s %s", instructions[instruction_count].mnemonic, instructions[instruction_count].args);
            } else {
                strncpy(instructions[instruction_count].mnemonic, trimmed, 31);
                instructions[instruction_count].mnemonic[31] = '\0';
                instructions[instruction_count].args[0] = '\0';
                strncpy(instructions[instruction_count].high_level_name, instructions[instruction_count].mnemonic, 63);
            }
            instruction_count++;
        }
        fclose(f);
    }
    free(path);
}

void save_script() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/events/%s.asm",
                 project_root, current_project, active_piece, active_event) == -1) return;
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "# PAL Script: %s/%s\n", active_piece, active_event);
        fprintf(f, "# Generated by PAL Editor\n\n");
        for (int i = 0; i < instruction_count; i++) {
            if (instructions[i].args[0] != '\0') {
                fprintf(f, "%s %s\n", instructions[i].mnemonic, instructions[i].args);
            } else {
                fprintf(f, "%s\n", instructions[i].mnemonic);
            }
        }
        fclose(f);
    }
    free(path);
}

void set_response(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/slop-ed-dev/manager/response.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "[RESP]: %-49s", msg); fclose(f); }
        free(path);
    }
}

static void read_param_inputs(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '1') { // p1 -> prefix '1'
                char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
                strncpy(param_1_val, trim_str(line + 1), sizeof(param_1_val) - 1);
            }
            else if (line[0] == '2') { // p2 -> prefix '2'
                char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
                strncpy(param_2_val, trim_str(line + 1), sizeof(param_2_val) - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

static void clear_param_inputs(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "1\n2\n"); fclose(f); }
        free(path);
    }
    param_1_val[0] = '\0';
    param_2_val[0] = '\0';
}

static void add_instruction_from_op(OpDef *op, const char *p1, const char *p2) {
    if (instruction_count >= MAX_INSTRUCTIONS) return;
    Instruction *instr = &instructions[instruction_count];
    
    if (strlen(op->param_1_label) > 0 && strlen(op->param_2_label) > 0) {
        snprintf(instr->high_level_name, 63, "%s (%s, %s)", op->name, p1, p2);
    } else if (strlen(op->param_1_label) > 0) {
        snprintf(instr->high_level_name, 63, "%s (%s)", op->name, p1);
    } else {
        strncpy(instr->high_level_name, op->name, 63);
    }

    char pal_line[256];
    if (strstr(op->pal_template, "%s")) {
        if (strlen(op->param_2_label) > 0) snprintf(pal_line, 255, op->pal_template, p1, p2);
        else snprintf(pal_line, 255, op->pal_template, p1);
    } else {
        strncpy(pal_line, op->pal_template, 255);
    }
    
    char *space = strchr(pal_line, ' ');
    if (space) {
        int m_len = space - pal_line;
        if (m_len > 31) m_len = 31;
        strncpy(instr->mnemonic, pal_line, m_len);
        instr->mnemonic[m_len] = '\0';
        strncpy(instr->args, trim_str(space + 1), 255);
    } else {
        strncpy(instr->mnemonic, pal_line, 31);
        instr->mnemonic[31] = '\0';
        instr->args[0] = '\0';
    }
    
    instruction_count++;
    cursor_line = instruction_count - 1;
}

int process_key(int key) {
    if (key == 27) { // ESC
        if (editor_mode == MODE_BROWSE) {
            transition_to_layout("projects/slop-ed-dev/layouts/slop-ed-dev.chtpm");
            set_response("Returned to slop-ed-dev");
        } else {
            editor_mode = MODE_BROWSE;
            set_response("Cancelled");
        }
        return 1;
    }

    if (editor_mode == MODE_BROWSE) {
        if (key == 119 || key == 1002 || key == 'w') {
            if (cursor_line > 0) cursor_line--;
        } else if (key == 115 || key == 1003 || key == 's') {
            if (cursor_line < instruction_count - 1) cursor_line++;
        } else if (key == 100 || key == 'd') { // DEL
            if (cursor_line < instruction_count) {
                for (int i = cursor_line; i < instruction_count - 1; i++) instructions[i] = instructions[i+1];
                instruction_count--;
                if (cursor_line >= instruction_count) cursor_line = instruction_count - 1;
                if (cursor_line < 0) cursor_line = 0;
                set_response("Deleted line");
            }
        } else if (key == 97 || key == 'a') { // ADD
            editor_mode = MODE_SELECT_OP;
            selected_op_idx = 0;
            set_response("Select Op to Add");
        } else if (key == 115 || key == 'S') { // SAVE
            save_script();
            set_response("Script Saved");
        }
    } 
    else if (editor_mode == MODE_SELECT_OP) {
        if (key == 1002 || key == 'w') {
            if (selected_op_idx > 0) selected_op_idx--;
        } else if (key == 1003 || key == 's') {
            if (selected_op_idx < op_count - 1) selected_op_idx++;
        } else if (key == 10 || key == 13) { // ENTER
            OpDef *op = &available_ops[selected_op_idx];
            if (strlen(op->param_1_label) == 0 && strlen(op->param_2_label) == 0) {
                add_instruction_from_op(op, "", "");
                editor_mode = MODE_BROWSE;
                set_response("Block Added");
            } else {
                editor_mode = MODE_PARAM_INPUT;
                clear_param_inputs();
                set_response("Enter Parameters");
            }
        }
    }
    else if (editor_mode == MODE_PARAM_INPUT) {
        if (key == 10 || key == 13) { // ENTER: BUILD
            read_param_inputs();
            add_instruction_from_op(&available_ops[selected_op_idx], param_1_val, param_2_val);
            editor_mode = MODE_BROWSE;
            set_response("Block Added");
        }
    }
    return 1;
}

void handle_command(const char* cmd) {
    if (strncmp(cmd, "KEY:", 4) == 0) {
        process_key(atoi(cmd + 4));
        return;
    }
    if (strncmp(cmd, "SET_SELECT_OP:", 14) == 0) {
        selected_op_idx = atoi(cmd + 14);
        process_key(10); // Auto-confirm on click
    }
    else if (strncmp(cmd, "SET_SELECT_INSTR:", 17) == 0) {
        cursor_line = atoi(cmd + 17);
    }
}

void write_editor_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/slop-ed-dev/manager/gui_state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (!f) { free(path); return; }
        
        fprintf(f, "module_path=projects/slop-ed-dev/manager/+x/pal_editor_module.+x\n");
        fprintf(f, "active_layout_id=pal_editor.chtpm\n");
        fprintf(f, "project_id=%s\n", current_project);
        fprintf(f, "gui_focus=%d\n", gui_focus_index);
        fprintf(f, "active_target_id=%s\n", active_piece);
        fprintf(f, "pal_editor_piece=%s\n", active_piece);
        fprintf(f, "pal_editor_event=%s\n", active_event);
        
        char main_display[8192] = "";
        char action_markup[8192] = "";
        
        if (editor_mode == MODE_BROWSE) {
            strcat(main_display, "<text label=\"║  CURRENT SCRIPT:                            ║\" /><br/>");
            for (int i = 0; i < instruction_count && i < 8; i++) {
                char btn_buf[512];
                snprintf(btn_buf, sizeof(btn_buf), "<text label=\"║  %2d: \" /><button label=\"%-32.32s\" onClick=\"SET_SELECT_INSTR:%d\" /><text label=\" ║\" /><br/>", i, instructions[i].high_level_name, i);
                strcat(main_display, btn_buf);
            }
            if (instruction_count == 0) strcat(main_display, "<text label=\"║  (Empty Script)                             ║\" /><br/>");
            
            strcat(action_markup, "<button label=\"[A] ADD BLOCK\" onClick=\"KEY:97\" /> ");
            strcat(action_markup, "<button label=\"[D] DELETE\" onClick=\"KEY:100\" /> ");
            strcat(action_markup, "<button label=\"[S] SAVE\" onClick=\"KEY:115\" /> ");
        } 
        else if (editor_mode == MODE_SELECT_OP) {
            strcat(main_display, "<text label=\"║  SELECT BLOCK TO ADD:                       ║\" /><br/>");
            for (int i = 0; i < op_count; i++) {
                char btn_buf[512];
                snprintf(btn_buf, sizeof(btn_buf), "<text label=\"║  \" /><button label=\"%-36.32s\" onClick=\"SET_SELECT_OP:%d\" /><text label=\" ║\" /><br/>", available_ops[i].name, i);
                strcat(main_display, btn_buf);
            }
            strcat(action_markup, "<button label=\"[ESC] CANCEL\" onClick=\"KEY:27\" /> ");
        }
        else if (editor_mode == MODE_PARAM_INPUT) {
            OpDef *op = &available_ops[selected_op_idx];
            strcat(main_display, "<text label=\"║  CONFIGURE BLOCK:                           ║\" /><br/>");
            char line_buf[256];
            snprintf(line_buf, sizeof(line_buf), "<text label=\"║  OP: %-35.35s ║\" /><br/>", op->name);
            strcat(main_display, line_buf);
            strcat(main_display, "<text label=\"║                                               ║\" /><br/>");
            
            if (strlen(op->param_1_label) > 0) {
                read_param_inputs();
                snprintf(line_buf, sizeof(line_buf), "<text label=\"║  %-10.10s: \" /><cli_io id=\"p1\" label=\"${p1_val}\" /><text label=\" ║\" /><br/>", op->param_1_label);
                strcat(main_display, line_buf);
            }
            if (strlen(op->param_2_label) > 0) {
                snprintf(line_buf, sizeof(line_buf), "<text label=\"║  %-10.10s: \" /><cli_io id=\"p2\" label=\"${p2_val}\" /><text label=\" ║\" /><br/>", op->param_2_label);
                strcat(main_display, line_buf);
            }
            strcat(main_display, "<text label=\"║                                               ║\" /><br/>");
            strcat(action_markup, "<button label=\"[ENTER] ADD BLOCK\" onClick=\"KEY:10\" /> ");
            strcat(action_markup, "<button label=\"[ESC] BACK\" onClick=\"KEY:27\" /> ");
        }
        
        fprintf(f, "pal_editor_display=%s\n", main_display);
        fprintf(f, "pal_editor_actions=%s\n", action_markup);
        fprintf(f, "p1_val=%s\n", param_1_val);
        fprintf(f, "p2_val=%s\n", param_2_val);
        
        char resp[MAX_LINE] = "";
        char *resp_path = NULL;
        if (asprintf(&resp_path, "%s/projects/slop-ed-dev/manager/response.txt", project_root) != -1) {
            FILE *rf = fopen(resp_path, "r");
            if (rf) { fgets(resp, sizeof(resp), rf); fclose(rf); }
            free(resp_path);
        }
        fprintf(f, "last_response=%s\n", resp);
        
        fclose(f);
    }
    free(path);
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        fclose(f);
        int res = (strstr(line, "pal_editor.chtpm") != NULL);
        free(path);
        return res;
    }
    fclose(f);
    free(path);
    return 0;
}

int main() {
    resolve_paths();
    
    char *mgr_state = NULL;
    if (asprintf(&mgr_state, "%s/projects/slop-ed-dev/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(mgr_state, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "project_id") == 0) strncpy(current_project, v, MAX_LINE - 1);
                    if (strcmp(k, "pal_editor_piece") == 0) strncpy(active_piece, v, MAX_LINE - 1);
                    else if (strcmp(k, "active_target_id") == 0 && strlen(active_piece) == 0) strncpy(active_piece, v, MAX_LINE - 1);
                    if (strcmp(k, "pal_editor_event") == 0) strncpy(active_event, v, MAX_LINE - 1);
                }
            }
            fclose(f);
        }
        free(mgr_state);
    }
    
    load_script();
    write_editor_state();
    
    char *pulse = NULL;
    if (asprintf(&pulse, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(pulse, "a");
        if (f) { fprintf(f, "P\n"); fclose(f); }
        free(pulse);
    }
    
    long last_pos = 0;
    struct stat st;
    char *history_path = NULL;
    if (asprintf(&history_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;
    if (stat(history_path, &st) == 0) last_pos = st.st_size;
    
    while (1) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        
        sync_focus();
        
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd = strstr(line, "COMMAND: ");
                        char *kpress = strstr(line, "KEY_PRESSED: ");
                        if (cmd) {
                            handle_command(trim_str(cmd + 9));
                        } else if (kpress) {
                            process_key(atoi(kpress + 13));
                        } else {
                            int k = atoi(line);
                            if (k != 0 || line[0] == '0') process_key(k);
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            }
        }
        
        write_editor_state();
        usleep(16667);
    }
    
    free(history_path);
    return 0;
}
