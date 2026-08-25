#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#define MAX_BOOKS 20
#define MAX_PATH 4096
#define MAX_CHOICES 10
#define UI_WIDTH 57

typedef struct {
    char name[128];
    char path[MAX_PATH];
} Book;

typedef struct {
    int page_num;
    char text[512];
} Choice;

Book books[MAX_BOOKS];
int book_count = 0;
int current_book_idx = -1;
int current_page = 1;
Choice choices[MAX_CHOICES];
int choice_count = 0;
int status = 0; // 0: select_book, 1: playing

char project_root[MAX_PATH] = ".";
char data_xl_root[MAX_PATH] = "";
char g_page_content[8192] = "";
char g_choices_content[1024] = "";

static volatile sig_atomic_t g_shutdown = 0;

void handle_sigint(int sig) { (void)sig; g_shutdown = 1; }

void resolve_paths() {
    FILE *df = fopen("debug.txt", "a");
    if (df) { fprintf(df, "MANAGER: resolve_paths called\n"); fclose(df); }

    // 1. Try local location.txt first for project-specific overrides
    FILE *loc = fopen("projects/cyoa-engine/location.txt", "r");
    if (loc) {
        char line[1024];
        while (fgets(line, sizeof(line), loc)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line;
                char *v = eq + 1;
                v[strcspn(v, "\n\r")] = 0;
                if (strcmp(k, "data_xl") == 0) strncpy(data_xl_root, v, MAX_PATH - 1);
            }
        }
        fclose(loc);
        df = fopen("debug.txt", "a");
        if (df) { fprintf(df, "MANAGER: Read data_xl from location.txt: %s\n", data_xl_root); fclose(df); }
    }

    // 2. Read global location_kvp
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line;
                char *v = eq + 1;
                v[strcspn(v, "\n\r")] = 0;
                if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH - 1);
                // Only use data_xl from kvp if not already set by location.txt
                if (strcmp(k, "data_xl") == 0 && data_xl_root[0] == '\0') {
                    strncpy(data_xl_root, v, MAX_PATH - 1);
                    df = fopen("debug.txt", "a");
                    if (df) { fprintf(df, "MANAGER: Read data_xl from kvp: %s\n", data_xl_root); fclose(df); }
                }
            }
        }
        fclose(kvp);
    }

    if (data_xl_root[0] == '\0') {
        snprintf(data_xl_root, MAX_PATH, "%s/projects/cyoa-engine/books", project_root);
        df = fopen("debug.txt", "a");
        if (df) { fprintf(df, "MANAGER: data_xl defaulted to: %s\n", data_xl_root); fclose(df); }
    } else {
        // If it's a relative path starting with ../, it's relative to CWD
        // If it's an absolute path, we might still need to ensure /books is appended if it's pointing to the data root
        if (data_xl_root[0] != '/' && strncmp(data_xl_root, "..", 2) != 0) {
            // Probably relative to project_root if not starting with / or ..
            char tmp[MAX_PATH];
            snprintf(tmp, MAX_PATH, "%s/%s", project_root, data_xl_root);
            strncpy(data_xl_root, tmp, MAX_PATH - 1);
        }
        
        if (strstr(data_xl_root, "/books") == NULL) {
            char tmp[MAX_PATH];
            snprintf(tmp, MAX_PATH, "%s/books", data_xl_root);
            strncpy(data_xl_root, tmp, MAX_PATH - 1);
        }
        df = fopen("debug.txt", "a");
        if (df) { fprintf(df, "MANAGER: final data_xl_root: %s\n", data_xl_root); fclose(df); }
    }
}

void hit_frame_marker() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

void write_app_context() {
    char path1[MAX_PATH], path2[MAX_PATH];
    snprintf(path1, sizeof(path1), "%s/pieces/apps/player_app/state.txt", project_root);
    snprintf(path2, sizeof(path2), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    
    FILE *f1 = fopen(path1, "w");
    if (f1) {
        fprintf(f1, "project_id=cyoa-engine\n");
        fprintf(f1, "app_title=RED WHITE AND U: CYOA PLAYER\n");
        fprintf(f1, "active_target_id=engine\n");
        if (status == 0) {
            fprintf(f1, "book_name=Select a Book\n");
            fprintf(f1, "page_num=0\n");
        } else {
            fprintf(f1, "book_name=%s\n", books[current_book_idx].name);
            fprintf(f1, "page_num=%d\n", current_page);
        }
        fprintf(f1, "page_content=%s\n", g_page_content);
        fprintf(f1, "choices_content=%s\n", g_choices_content);
        fclose(f1);
    }
    FILE *f2 = fopen(path2, "w");
    if (f2) {
        fprintf(f2, "project_id=cyoa-engine\n");
        fprintf(f2, "app_title=RED WHITE AND U: CYOA PLAYER\n");
        fprintf(f2, "active_target_id=engine\n");
        fclose(f2);
    }
}

void wrap_text_to_buf(char *out, const char *text, const char *prefix) {
    char buf[8192];
    strncpy(buf, text, sizeof(buf)-1);
    char *token = strtok(buf, " \n");
    int current_len = 0;
    out[0] = '\0';
    strcat(out, prefix);
    while (token != NULL) {
        int token_len = strlen(token);
        if (current_len + token_len + 1 > UI_WIDTH) {
            for (int i = 0; i < (UI_WIDTH - current_len); i++) strcat(out, " ");
            strcat(out, " ║\\n");
            strcat(out, prefix);
            current_len = 0;
        }
        strcat(out, token);
        strcat(out, " ");
        current_len += token_len + 1;
        token = strtok(NULL, " \n");
    }
    for (int i = 0; i < (UI_WIDTH - current_len); i++) strcat(out, " ");
    strcat(out, " ║");
}

void write_pdl() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/cyoa-engine/pieces/engine/piece.pdl", project_root);
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s/projects/cyoa-engine/pieces/engine", project_root);
    mkdir(dir_path, 0755);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | engine\n");
    fprintf(f, "STATE        | name               | CYOA Engine\n\n");
    if (status == 0) {
        for (int i = 0; i < book_count; i++) {
            fprintf(f, "METHOD       | %s | CYOA:SELECT:%d\n", books[i].name, i);
        }
    } else {
        for (int i = 0; i < choice_count; i++) {
            char label[512];
            snprintf(label, sizeof(label), "%s (Page %d)", choices[i].text, choices[i].page_num);
            fprintf(f, "METHOD       | %s | CYOA:SELECT:%d\n", label, i);
        }
    }
    fprintf(f, "METHOD       | Audio: PLAY | CYOA:PLAY\n");
    fprintf(f, "METHOD       | Audio: STOP | CYOA:STOP\n");
    fclose(f);
}

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/cyoa-engine/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/cyoa-engine/manager/+x/cyoa-engine_manager.+x\n");
        fprintf(f, "active_layout_id=projects/cyoa-engine/layouts/cyoa.chtpm\n");
        fprintf(f, "app_title=RED WHITE AND U: CYOA PLAYER\n");
        if (status == 0) {
            fprintf(f, "book_name=Select a Book\n");
            fprintf(f, "page_num=0\n");
        } else {
            fprintf(f, "book_name=%s\n", books[current_book_idx].name);
            fprintf(f, "page_num=%d\n", current_page);
        }
        fprintf(f, "page_content=%s\n", g_page_content);
        fprintf(f, "choices_content=%s\n", g_choices_content);
        fclose(f);
    }
}

void write_mirror() {
    if (status == 0) {
        strcpy(g_page_content, "║  Please select a book from the menu below:      ║");
        strcpy(g_choices_content, "║  Use Arrows to navigate, ENTER to select        ║");
    } else {
        char page_path[MAX_PATH];
        snprintf(page_path, sizeof(page_path), "%s/%s/page_%02d.txt", data_xl_root, books[current_book_idx].name, current_page);
        FILE *pf = fopen(page_path, "r");
        if (pf) {
            char content[8192] = ""; char line[1024];
            while (fgets(line, sizeof(line), pf)) { if (line[0] == '*') break; strcat(content, line); }
            wrap_text_to_buf(g_page_content, content, "║  ");
            fclose(pf);
        } else {
            strcpy(g_page_content, "║  Error loading page.                            ║");
        }
        strcpy(g_choices_content, "║  See dynamic menu below for choices             ║");
    }

    write_app_context(); 
    write_gui_state();
    write_pdl();
    hit_frame_marker();
}

void resolve_books() {
    book_count = 0;
    DIR *dir = opendir(data_xl_root);
    FILE *df = fopen("debug.txt", "a");
    if (df) { fprintf(df, "MANAGER: resolve_books opendir(%s) -> %p\n", data_xl_root, dir); fclose(df); }
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (book_count < MAX_BOOKS) {
            strncpy(books[book_count].name, entry->d_name, 127);
            snprintf(books[book_count].path, MAX_PATH, "%s/%s", data_xl_root, entry->d_name);
            df = fopen("debug.txt", "a");
            if (df) { fprintf(df, "MANAGER: Found book: %s\n", entry->d_name); fclose(df); }
            book_count++;
        }
    }
    closedir(dir);
    for (int i = 0; i < book_count - 1; i++) {
        for (int j = 0; j < book_count - i - 1; j++) {
            if (strcmp(books[j].name, books[j+1].name) > 0) { Book temp = books[j]; books[j] = books[j+1]; books[j+1] = temp; }
        }
    }
}

void play_audio(int page) {
    char book_path[MAX_PATH];
    snprintf(book_path, sizeof(book_path), "%s/%s", data_xl_root, books[current_book_idx].name);
    DIR *dir = opendir(book_path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char page_str[32]; snprintf(page_str, sizeof(page_str), "page%02d", page);
        if (strstr(entry->d_name, page_str) && strstr(entry->d_name, ".mp3")) {
            char full_audio_path[MAX_PATH]; snprintf(full_audio_path, sizeof(full_audio_path), "%s/%s", book_path, entry->d_name);
            char op_path[MAX_PATH];
            snprintf(op_path, sizeof(op_path), "%s/projects/cyoa-engine/ops/+x/play_audio.+x", project_root);
            
            pid_t pid = fork();
            if (pid == 0) {
                execl(op_path, op_path, full_audio_path, NULL);
                exit(1);
            } else if (pid > 0) {
                waitpid(pid, NULL, 0);
            }
            break;
        }
    }
    closedir(dir);
}

void stop_audio() {
    char op_path[MAX_PATH];
    snprintf(op_path, sizeof(op_path), "%s/projects/cyoa-engine/ops/+x/play_audio.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, "--stop", NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void load_page(int page) {
    current_page = page;
    char page_path[MAX_PATH];
    snprintf(page_path, sizeof(page_path), "%s/%s/page_%02d.txt", data_xl_root, books[current_book_idx].name, page);
    choice_count = 0;
    
    char op_path[MAX_PATH];
    snprintf(op_path, sizeof(op_path), "%s/projects/cyoa-engine/ops/+x/get_choices.+x", project_root);
    
    int pipefd[2];
    if (pipe(pipefd) == -1) return;
    
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        execl(op_path, op_path, page_path, NULL);
        exit(1);
    } else if (pid > 0) {
        close(pipefd[1]);
        FILE *choice_fp = fdopen(pipefd[0], "r");
        if (choice_fp) {
            char line[1024];
            while (fgets(line, sizeof(line), choice_fp) && choice_count < MAX_CHOICES) {
                char *sep = strchr(line, '|');
                if (sep) {
                    *sep = '\0'; choices[choice_count].page_num = atoi(line);
                    char *choice_text = sep + 1; int len = strlen(choice_text);
                    if (len > 0 && choice_text[len-1] == '\n') choice_text[len-1] = '\0';
                    strncpy(choices[choice_count].text, choice_text, 511);
                    choice_count++;
                }
            }
            fclose(choice_fp);
        }
        waitpid(pid, NULL, 0);
    }
    play_audio(page);
    write_mirror();
}

static char* trim_pmo(char *s) {
    char *p = s;
    int l = strlen(p);
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
    return s;
}

void process_command(const char *cmd) {
    if (strcmp(cmd, "CYOA:PLAY") == 0) {
        if (status == 1) play_audio(current_page);
    } else if (strcmp(cmd, "CYOA:STOP") == 0) {
        stop_audio();
    } else if (strncmp(cmd, "CYOA:SELECT:", 12) == 0) {
        int idx = atoi(cmd + 12);
        if (status == 0) {
            if (idx >= 0 && idx < book_count) {
                current_book_idx = idx;
                status = 1;
                load_page(1);
            }
        } else {
            if (idx >= 0 && idx < choice_count) {
                load_page(choices[idx].page_num);
            }
        }
    }
}

void process_input(int key) {
    if (key == 27) { status = 0; stop_audio(); write_mirror(); return; }
    
    // Convert ASCII '1'-'9' to 1-9
    int num = -1;
    if (key >= '1' && key <= '9') num = key - '0';
    else if (key >= 1 && key <= 9) num = key;
    
    // Adjust for method_idx offset (parser sends KEY:2 for first method)
    int idx = num - 2;
    
    if (status == 0) {
        if (idx >= 0 && idx < book_count) { current_book_idx = idx; status = 1; load_page(1); }
    } else {
        if (idx >= 0 && idx < choice_count) { load_page(choices[idx].page_num); }
    }
}

int is_active_layout() {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[1024]; if (fgets(line, sizeof(line), f)) { fclose(f); return (strstr(line, "cyoa.chtpm") != NULL); }
    fclose(f); return 0;
}

int main() {
    signal(SIGINT, handle_sigint); signal(SIGTERM, handle_sigint);
    setpgid(0, 0); resolve_paths(); resolve_books();
    write_app_context();
    write_mirror();
    long last_pos = 0; struct stat st; char history_path[MAX_PATH];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    if (stat(history_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        if (!is_active_layout()) { usleep(100000); continue; }
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET); char line[512];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd = strstr(line, "COMMAND: ");
                        if (cmd) { process_command(trim_pmo(cmd + 9)); }
                        else {
                            char *kp = strstr(line, "KEY_PRESSED: ");
                            if (kp) { int key = atoi(kp + 13); if (key > 0) process_input(key); }
                            else { int key = atoi(line); if (key > 0 || line[0] == '0') process_input(key); }
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    stop_audio(); return 0;
}
