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

/*
 * Op: resolve_project_op
 * Usage: resolve_project_op <project_name>
 * Purpose: Resolves project entry layout and updates state, signaling the parser to switch.
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

void get_pdl_value(const char* project, const char* section, const char* key, char* out_val, size_t out_sz) {
    out_val[0] = '\0';
    char *path = NULL;
    asprintf(&path, "%s/projects/%s/project.pdl", project_root, project);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        int in_section = 0;
        char sect_header[64]; snprintf(sect_header, sizeof(sect_header), "[%s]", section);

        while (fgets(line, sizeof(line), f)) {
            char *tl = trim_str(line);
            if (strlen(tl) == 0 || strncmp(tl, "SECTION", 7) == 0) continue;

            /* Check for [SECTION] style */
            if (tl[0] == '[' && strcasecmp(tl, sect_header) == 0) { in_section = 1; continue; }
            if (tl[0] == '[' && tl[strlen(tl)-1] == ']') { if (in_section) break; }

            /* Check for SECTION | format header or line prefix */
            bool is_section_match = (strncmp(tl, section, strlen(section)) == 0 && 
                                    (tl[strlen(section)] == ' ' || tl[strlen(section)] == '\t' || tl[strlen(section)] == '|'));
            
            if (is_section_match) in_section = 1;

            if (in_section) {
                char *sep = strchr(tl, '|');
                if (!sep) sep = strchr(tl, '=');
                
                if (sep) {
                    *sep = '\0';
                    char *k = trim_str(tl);
                    char *v = trim_str(sep + 1);
                    
                    /* Handle PIPE format second column if needed: SECTION | KEY | VALUE */
                    char *sep2 = strchr(v, '|');
                    if (sep2) {
                        *sep2 = '\0';
                        char *v2 = trim_str(sep2 + 1);
                        if (strcasecmp(k, section) == 0) {
                            /* We are in section-prefixed PIPE format: k=SECTION, v=KEY, v2=VALUE */
                            k = trim_str(v);
                            v = v2;
                        }
                    }

                    if (strcasecmp(k, key) == 0) {
                        strncpy(out_val, v, out_sz - 1); out_val[out_sz - 1] = '\0';
                        fclose(f); free(path); return;
                    }
                }
            }
        }
        fclose(f);
    }
    free(path);
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    const char *name = argv[1];
    resolve_paths();

    char entry_layout[MAX_PATH] = "";
    char starting_map[MAX_LINE] = "map_01.txt";
    char player_piece[MAX_LINE] = "selector";
    
    get_pdl_value(name, "META", "entry_layout", entry_layout, sizeof(entry_layout));
    get_pdl_value(name, "STATE", "starting_map", starting_map, sizeof(starting_map));
    get_pdl_value(name, "STATE", "player_piece", player_piece, sizeof(player_piece));

    if (entry_layout[0] == '\0') {
        char *route_path = NULL;
        asprintf(&route_path, "%s/pieces/os/project_routes.kvp", project_root);
        FILE *rf = route_path ? fopen(route_path, "r") : NULL;
        if (rf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), rf)) {
                char *eq;
                char *k;
                char *v;

                if (line[0] == '#') continue;
                eq = strchr(line, '=');
                if (!eq) continue;

                *eq = '\0';
                k = trim_str(line);
                v = trim_str(eq + 1);
                if (strcmp(k, name) == 0) {
                    strncpy(entry_layout, v, sizeof(entry_layout) - 1);
                    entry_layout[sizeof(entry_layout) - 1] = '\0';
                    break;
                }
            }
            fclose(rf);
        }
        free(route_path);
    }

    if (entry_layout[0] == '\0') {
        /* Probing Fallback */
        char probe[MAX_PATH];
        char *probes[] = {
            "projects/%s/layouts/%s.chtpm",
            "projects/%s/layouts/main.chtpm",
            "projects/%s/layouts/quiz.chtpm",
            "pieces/apps/%s/layouts/%s.chtpm"
        };
        for (int i = 0; i < 4; i++) {
            snprintf(probe, sizeof(probe), probes[i], name, name);
            char *abs_p = NULL;
            asprintf(&abs_p, "%s/%s", project_root, probe);
            if (abs_p && access(abs_p, F_OK) == 0) {
                strcpy(entry_layout, probe);
                free(abs_p);
                break;
            }
            if (abs_p) free(abs_p);
        }
    }
    
    if (entry_layout[0] == '\0') {
        snprintf(entry_layout, sizeof(entry_layout), "projects/%s/layouts/%s.chtpm", name, name);
    }

    // Update player_app manager state
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=%s\n", name);
        fprintf(f, "current_map=%s\n", starting_map);
        fprintf(f, "active_target_id=%s\n", player_piece);
        fprintf(f, "current_z=0\n");
        fprintf(f, "last_key=None\n");
        fclose(f);
    }

    // Write to layout_changed.txt
    snprintf(path, sizeof(path), "%s/pieces/display/layout_changed.txt", project_root);
    f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", entry_layout); fclose(f); }

    // Hit state changed marker
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    f = fopen(path, "a");
    if (f) { fprintf(f, "S\n"); fclose(f); }

    return 0;
}
