#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdbool.h>
#include <dirent.h>

/*
 * Op: get_piece_methods_op
 * Usage: get_piece_methods_op <active_id> [project_id]
 * Purpose: Reads Piece PDL and outputs CHTPM markup for its methods.
 */

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

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

    // Safety check: fallback to current working directory if path is invalid
    if (access(project_root, F_OK) != 0) {
        if (!getcwd(project_root, sizeof(project_root))) {
            strcpy(project_root, ".");
        }
    }
}

bool find_pdl_recursive(const char* dir_path, const char* active_id, char* out_path, size_t out_size, int depth) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_path || !active_id || !out_path || depth > 8) return false;

    dir = opendir(dir_path);
    if (!dir) return false;

    while ((entry = readdir(dir)) != NULL) {
        char child[MAX_PATH];
        struct stat st;

        if (entry->d_name[0] == '.') continue;
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if (stat(child, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (strcmp(entry->d_name, active_id) == 0) {
            char pdl_path[MAX_PATH];
            snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", child);
            if (access(pdl_path, F_OK) == 0) {
                strncpy(out_path, pdl_path, out_size - 1);
                out_path[out_size - 1] = '\0';
                closedir(dir);
                return true;
            }

            snprintf(pdl_path, sizeof(pdl_path), "%s/%s.pdl", child, active_id);
            if (access(pdl_path, F_OK) == 0) {
                strncpy(out_path, pdl_path, out_size - 1);
                out_path[out_size - 1] = '\0';
                closedir(dir);
                return true;
            }
        }

        if (find_pdl_recursive(child, active_id, out_path, out_size, depth + 1)) {
            closedir(dir);
            return true;
        }
    }

    closedir(dir);
    return false;
}

char* resolve_pdl_path(const char* active_id, const char* project_id) {
    char *path = NULL;

    /* 1) Project piece.pdl (canonical) */
    asprintf(&path, "%s/projects/%s/pieces/%s/piece.pdl", project_root, project_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    /* 2) Project <piece_id>.pdl (legacy variant) */
    asprintf(&path, "%s/projects/%s/pieces/%s/%s.pdl", project_root, project_id, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    /* 3) Project pieces may be nested under sovereign world/map folders */
    char root_path[MAX_PATH], found_path[MAX_PATH];
    snprintf(root_path, sizeof(root_path), "%s/projects/%s/pieces", project_root, project_id);
    if (find_pdl_recursive(root_path, active_id, found_path, sizeof(found_path), 0)) {
        return strdup(found_path);
    }

    /* 4) App pieces (User Auth pattern) */
    asprintf(&path, "%s/pieces/apps/%s/pieces/%s/piece.pdl", project_root, project_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    asprintf(&path, "%s/pieces/apps/%s/pieces/%s/%s.pdl", project_root, project_id, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    /* 5) App-specific (Playrm fallback) */
    asprintf(&path, "%s/pieces/apps/playrm/%s/%s.pdl", project_root, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    /* 6) World/global */
    asprintf(&path, "%s/pieces/world/map_01/%s/%s.pdl", project_root, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    asprintf(&path, "%s/pieces/%s/%s.pdl", project_root, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <active_id> [project_id]\n", argv[0]);
        return 1;
    }

    const char *active_id = argv[1];
    const char *project_id = (argc > 2) ? argv[2] : "";

    resolve_paths();
    
    char *path = resolve_pdl_path(active_id, project_id);
    if (!path) {
        printf("[No Methods]");
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("[No Methods]");
        free(path);
        return 0;
    }

    char line[MAX_LINE]; 
    int method_idx = (strcmp(active_id, "loader") == 0) ? 1 : 2;
    bool found = false;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) == 0) {
            char *key_start = strchr(line, '|'); if (!key_start) continue;
            key_start++; char *val_start = strchr(key_start, '|'); if (!val_start) continue;
            
            char *cmd_val = trim_str(val_start + 1);
            *val_start = '\0';
            char *trimmed_key = trim_str(key_start);
            
            if (strcmp(trimmed_key, "move") == 0 || strcmp(trimmed_key, "select") == 0 || 
                strcmp(trimmed_key, "interact") == 0 || strcmp(trimmed_key, "stat_decay") == 0 ||
                strcmp(trimmed_key, "on_turn_end") == 0 || trimmed_key[0] == '_') continue;
            
            bool is_parser_cmd = (strncmp(cmd_val, "LOAD_PROJECT:", 13) == 0 || 
                                  strncmp(cmd_val, "LAUNCH:", 7) == 0 ||
                                  strncmp(cmd_val, "MP3:", 4) == 0 ||
                                  strcmp(cmd_val, "BACK") == 0 ||
                                  strcmp(cmd_val, "RELEASE") == 0);

            if (strcmp(cmd_val, "void") == 0 || !is_parser_cmd) {
                printf("<button label=\"%s\" onClick=\"KEY:%d\" /><br/>", trimmed_key, method_idx++);
            } else {
                printf("<button label=\"%s\" onClick=\"%s\" /><br/>", trimmed_key, cmd_val);
            }
            found = true;
        }
    }
    fclose(f);
    free(path);

    if (!found) {
        printf("[No Methods]");
    }

    return 0;
}
