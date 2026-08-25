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
#include <stdbool.h>

/*
 * Op: sync_project_routes_op
 * Usage: sync_project_routes_op
 * Purpose: Rebuilds pieces/os/project_routes.kvp from project PDL files.
 */

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_PROJECTS 512

char project_root[MAX_PATH] = ".";

typedef struct {
    char name[MAX_LINE];
    char layout[MAX_PATH];
} RouteEntry;

static RouteEntry routes[MAX_PROJECTS];
static int route_count = 0;

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths(void) {
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
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }

    if (access(project_root, F_OK) != 0) {
        if (!getcwd(project_root, sizeof(project_root))) {
            strcpy(project_root, ".");
        }
    }
}

void get_pdl_value(const char* project, const char* section, const char* key, char* out_val, size_t out_sz) {
    out_val[0] = '\0';
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/project.pdl", project_root, project) == -1) return;

    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        int in_section = 0;
        char sect_header[64];
        snprintf(sect_header, sizeof(sect_header), "[%s]", section);

        while (fgets(line, sizeof(line), f)) {
            char *tl = trim_str(line);
            if (strlen(tl) == 0 || strncmp(tl, "SECTION", 7) == 0) continue;

            if (tl[0] == '[' && strcasecmp(tl, sect_header) == 0) {
                in_section = 1;
                continue;
            }
            if (tl[0] == '[' && tl[strlen(tl) - 1] == ']') {
                if (in_section) break;
            }

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

                    char *sep2 = strchr(v, '|');
                    if (sep2) {
                        *sep2 = '\0';
                        char *v2 = trim_str(sep2 + 1);
                        if (strcasecmp(k, section) == 0) {
                            k = trim_str(v);
                            v = v2;
                        }
                    }

                    if (strcasecmp(k, key) == 0) {
                        strncpy(out_val, v, out_sz - 1);
                        out_val[out_sz - 1] = '\0';
                        fclose(f);
                        free(path);
                        return;
                    }
                }
            }
        }
        fclose(f);
    }
    free(path);
}

static int route_cmp(const void *a, const void *b) {
    const RouteEntry *ra = (const RouteEntry *)a;
    const RouteEntry *rb = (const RouteEntry *)b;
    return strcmp(ra->name, rb->name);
}

static bool project_has_pdl(const char *project_name, char *entry_layout, size_t layout_sz) {
    struct stat st;
    char pdl_path[MAX_PATH];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root, project_name);
    if (stat(pdl_path, &st) != 0 || !S_ISREG(st.st_mode)) return false;

    get_pdl_value(project_name, "META", "entry_layout", entry_layout, layout_sz);
    return entry_layout[0] != '\0';
}

int main(void) {
    resolve_paths();

    char projects_dir[MAX_PATH];
    snprintf(projects_dir, sizeof(projects_dir), "%s/projects", project_root);

    DIR *dir = opendir(projects_dir);
    if (!dir) return 1;

    struct dirent *entry;
    route_count = 0;

    while ((entry = readdir(dir)) != NULL && route_count < MAX_PROJECTS) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "trunk") == 0) continue;

        char project_path[MAX_PATH];
        struct stat st;
        snprintf(project_path, sizeof(project_path), "%s/%s", projects_dir, entry->d_name);
        if (stat(project_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        char entry_layout[MAX_PATH] = "";
        if (project_has_pdl(entry->d_name, entry_layout, sizeof(entry_layout))) {
            strncpy(routes[route_count].name, entry->d_name, sizeof(routes[route_count].name) - 1);
            routes[route_count].name[sizeof(routes[route_count].name) - 1] = '\0';
            strncpy(routes[route_count].layout, entry_layout, sizeof(routes[route_count].layout) - 1);
            routes[route_count].layout[sizeof(routes[route_count].layout) - 1] = '\0';
            route_count++;
        }
    }
    closedir(dir);

    qsort(routes, route_count, sizeof(RouteEntry), route_cmp);

    char route_path[MAX_PATH];
    char tmp_path[MAX_PATH];
    snprintf(route_path, sizeof(route_path), "%s/pieces/os/project_routes.kvp", project_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", route_path);

    FILE *out = fopen(tmp_path, "w");
    if (!out) return 1;

    for (int i = 0; i < route_count; i++) {
        fprintf(out, "%s=%s\n", routes[i].name, routes[i].layout);
    }

    fclose(out);
    rename(tmp_path, route_path);
    return 0;
}
