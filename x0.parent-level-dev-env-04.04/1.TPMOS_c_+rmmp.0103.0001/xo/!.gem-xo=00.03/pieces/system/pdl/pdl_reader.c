/*
 * pdl_reader.c - PDL file reading utility (standalone binary)
 * 
 * Usage:
 *   ./pdl_reader.+x <piece_id> get_method <event>
 *   ./pdl_reader.+x <piece_id> list_methods
 *   ./pdl_reader.+x <piece_id> has_method <event>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

static char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static int root_has_anchors(const char* root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_paths(char* project_root, char* current_project) {
    if (!getcwd(project_root, MAX_PATH - 1)) strncpy(project_root, ".", MAX_PATH - 1);
    project_root[MAX_PATH - 1] = '\0';

    /* Read project_root from location_kvp */
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0 && *v) {
                    char candidate[MAX_PATH];
                    strncpy(candidate, v, sizeof(candidate) - 1);
                    candidate[sizeof(candidate) - 1] = '\0';
                    if (root_has_anchors(candidate)) {
                        strncpy(project_root, candidate, MAX_PATH - 1);
                        project_root[MAX_PATH - 1] = '\0';
                    }
                }
            }
        }
        fclose(kvp);
    }
    
    /* Read current_project from manager state */
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *sf = fopen(state_path, "r");
    if (sf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), sf)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_id") == 0) {
                    strncpy(current_project, trim_str(eq + 1), MAX_LINE - 1);
                }
            }
        }
        fclose(sf);
    }
}

static int resolve_piece_pdl_path(const char* piece_id, char* out_path, size_t out_sz,
                                  const char* project_root, const char* current_project) {
    snprintf(out_path, out_sz, "%s/projects/%s/pieces/%s/piece.pdl",
             project_root, current_project, piece_id);
    if (access(out_path, F_OK) == 0) return 0;

    snprintf(out_path, out_sz, "%s/projects/%s/pieces/%s/%s.pdl",
             project_root, current_project, piece_id, piece_id);
    if (access(out_path, F_OK) == 0) return 0;

    snprintf(out_path, out_sz, "%s/pieces/world/map_01/%s/%s.pdl",
             project_root, piece_id, piece_id);
    if (access(out_path, F_OK) == 0) return 0;

    snprintf(out_path, out_sz, "%s/pieces/%s/%s.pdl",
             project_root, piece_id, piece_id);
    if (access(out_path, F_OK) == 0) return 0;

    return -1;
}

static int get_method(const char* piece_id, const char* event, char* out_handler, 
                      const char* project_root, const char* current_project) {
    char pdl_path[MAX_PATH];
    if (resolve_piece_pdl_path(piece_id, pdl_path, sizeof(pdl_path), project_root, current_project) != 0) {
        return -1;
    }
    
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) == 0) {
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;
            
            char parsed_event[64];
            size_t event_len = pipe2 - pipe1 - 1;
            if (event_len >= sizeof(parsed_event)) event_len = sizeof(parsed_event) - 1;
            strncpy(parsed_event, pipe1 + 1, event_len);
            parsed_event[event_len] = '\0';
            char* trimmed_event = trim_str(parsed_event);
            
            char parsed_handler[MAX_PATH];
            strncpy(parsed_handler, pipe2 + 1, sizeof(parsed_handler) - 1);
            parsed_handler[sizeof(parsed_handler) - 1] = '\0';
            char* trimmed_handler = trim_str(parsed_handler);
            
            if (strcmp(trimmed_event, event) == 0) {
                strcpy(out_handler, trimmed_handler);
                fclose(f);
                return 0;
            }
        }
    }
    
    fclose(f);
    return -1;
}

static int list_methods(const char* piece_id, char* out_list, size_t out_size,
                        const char* project_root, const char* current_project) {
    char pdl_path[MAX_PATH];
    if (resolve_piece_pdl_path(piece_id, pdl_path, sizeof(pdl_path), project_root, current_project) != 0) {
        return -1;
    }
    
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;
    
    out_list[0] = '\0';
    int count = 0;
    char line[MAX_LINE];
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) == 0) {
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;
            
            char parsed_event[64];
            size_t event_len = pipe2 - pipe1 - 1;
            if (event_len >= sizeof(parsed_event)) event_len = sizeof(parsed_event) - 1;
            strncpy(parsed_event, pipe1 + 1, event_len);
            parsed_event[event_len] = '\0';
            char *trimmed_event = trim_str(parsed_event);
            
            /* Skip built-in methods */
            if (strcmp(trimmed_event, "move") == 0 || strcmp(trimmed_event, "select") == 0) {
                continue;
            }
            
            if (count > 0) {
                strncat(out_list, "\n", out_size - strlen(out_list) - 1);
            }
            strncat(out_list, trimmed_event, out_size - strlen(out_list) - 1);
            count++;
        }
    }
    
    fclose(f);
    return count;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <piece_id> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  get_method <event>  - Get handler for event\n");
        fprintf(stderr, "  list_methods        - List all methods\n");
        fprintf(stderr, "  has_method <event>  - Check if method exists (returns 0/1)\n");
        return 1;
    }
    
    char project_root[MAX_PATH] = ".";
    char current_project[MAX_LINE] = "template";
    resolve_paths(project_root, current_project);
    
    const char* piece_id = argv[1];
    const char* command = argv[2];
    
    if (strcmp(command, "get_method") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s get_method <event>\n", argv[0], piece_id);
            return 1;
        }
        const char* event = argv[3];
        char handler[MAX_PATH] = "";
        if (get_method(piece_id, event, handler, project_root, current_project) == 0) {
            printf("%s\n", handler);
            return 0;
        } else {
            printf("NOT_FOUND\n");
            return 1;
        }
    }
    else if (strcmp(command, "list_methods") == 0) {
        char methods[MAX_PATH * 2] = "";
        int count = list_methods(piece_id, methods, sizeof(methods), project_root, current_project);
        if (count > 0) {
            printf("%s\n", methods);
            return 0;
        } else {
            printf("NONE\n");
            return 1;
        }
    }
    else if (strcmp(command, "has_method") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s has_method <event>\n", argv[0], piece_id);
            return 1;
        }
        const char* event = argv[3];
        char handler[MAX_PATH] = "";
        if (get_method(piece_id, event, handler, project_root, current_project) == 0) {
            printf("1\n");
            return 0;
        } else {
            printf("0\n");
            return 0;
        }
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return 1;
    }
    
    return 0;
}
