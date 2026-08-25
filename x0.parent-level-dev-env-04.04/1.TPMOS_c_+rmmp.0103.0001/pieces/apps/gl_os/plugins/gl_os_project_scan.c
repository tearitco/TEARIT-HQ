#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define MAX_LINE 1024
#define MAX_PROJECTS 50

typedef struct {
    char id[64];
    char name[128];
} GLProject;

static char project_root[MAX_PATH] = ".";

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

static int root_has_anchors(const char* root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];

    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(void) {
    FILE *kvp = NULL;

    if (getcwd(project_root, sizeof(project_root)) && root_has_anchors(project_root)) {
        return;
    }

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;

    char line[2048];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *v = line + 13;
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0 && root_has_anchors(v)) {
                strncpy(project_root, v, MAX_PATH - 1);
            }
            break;
        }
    }
    fclose(kvp);
}

static void get_pdl_value(const char* project, const char* section, const char* key, char* out_val, size_t out_sz) {
    out_val[0] = '\0';
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/project.pdl", project_root, project);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[MAX_LINE];
    int in_section = 0;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        if (strncmp(trimmed, section, strlen(section)) == 0 && (trimmed[strlen(section)] == ' ' || trimmed[strlen(section)] == '\t' || trimmed[strlen(section)] == '\0')) {
            in_section = 1;
            continue;
        }
        if (strncmp(trimmed, "SECTION", 7) == 0 || (line[0] != ' ' && line[0] != '\t' && line[0] != '\n' && line[0] != '-' && strstr(line, "|") == NULL && strlen(trimmed) > 0)) {
            if (in_section) break;
        }
        if (in_section) {
            char *pipe1 = strchr(line, '|');
            if (pipe1) {
                char k_buf[MAX_LINE];
                char *pipe2 = strchr(pipe1 + 1, '|');
                if (pipe2) {
                    int k_len = pipe2 - pipe1 - 1;
                    if (k_len >= MAX_LINE) k_len = MAX_LINE - 1;
                    strncpy(k_buf, trim_str(pipe1 + 1), k_len);
                    k_buf[k_len] = '\0';
                    if (strcmp(trim_str(k_buf), key) == 0) {
                        strncpy(out_val, trim_str(pipe2 + 1), out_sz - 1);
                        out_val[out_sz - 1] = '\0';
                        fclose(f);
                        return;
                    }
                }
            }
        }
    }
    fclose(f);
}

static int project_exists(GLProject *projects, int count, const char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(projects[i].id, id) == 0) return 1;
    }
    return 0;
}

int main(void) {
    resolve_root();

    GLProject projects[MAX_PROJECTS];
    int project_count = 0;

    char kvp_path[MAX_PATH];
    snprintf(kvp_path, sizeof(kvp_path), "%s/pieces/apps/gl_os/gl_os_projects.kvp", project_root);
    FILE *out = NULL;
    char cache_path[MAX_PATH];
    snprintf(cache_path, sizeof(cache_path), "%s/pieces/apps/gl_os/manager/projects_cache.txt", project_root);
    out = fopen(cache_path, "w");
    if (!out) return 1;

    FILE *kf = fopen(kvp_path, "r");
    if (kf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kf) && project_count < MAX_PROJECTS) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *id = trim_str(line);
            if (project_exists(projects, project_count, id)) continue;

            strncpy(projects[project_count].id, id, sizeof(projects[project_count].id) - 1);
            char nice_name[128] = "";
            get_pdl_value(id, "STATE", "app_title", nice_name, sizeof(nice_name));
            if (nice_name[0] == '\0') get_pdl_value(id, "STATE", "title", nice_name, sizeof(nice_name));
            if (nice_name[0] != '\0') strncpy(projects[project_count].name, nice_name, sizeof(projects[project_count].name) - 1);
            else strncpy(projects[project_count].name, trim_str(eq + 1), sizeof(projects[project_count].name) - 1);

            fprintf(out, "%s|%s\n", projects[project_count].id, projects[project_count].name);
            project_count++;
        }
        fclose(kf);
    }

    char projects_dir[MAX_PATH];
    snprintf(projects_dir, sizeof(projects_dir), "%s/projects", project_root);
    DIR *dir = opendir(projects_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && project_count < MAX_PROJECTS) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, "trunk") == 0) continue;
            if (project_exists(projects, project_count, entry->d_name)) continue;

            char pdl_path[MAX_PATH];
            snprintf(pdl_path, sizeof(pdl_path), "%s/%s/project.pdl", projects_dir, entry->d_name);
            if (access(pdl_path, F_OK) == 0) {
                strncpy(projects[project_count].id, entry->d_name, sizeof(projects[project_count].id) - 1);
                char nice_name[128] = "";
                get_pdl_value(entry->d_name, "STATE", "app_title", nice_name, sizeof(nice_name));
                if (nice_name[0] == '\0') get_pdl_value(entry->d_name, "STATE", "title", nice_name, sizeof(nice_name));

                if (nice_name[0] != '\0') strncpy(projects[project_count].name, nice_name, sizeof(projects[project_count].name) - 1);
                else strncpy(projects[project_count].name, entry->d_name, sizeof(projects[project_count].name) - 1);

                fprintf(out, "%s|%s\n", projects[project_count].id, projects[project_count].name);
                project_count++;
            }
        }
        closedir(dir);
    }

    fclose(out);
    return 0;
}
