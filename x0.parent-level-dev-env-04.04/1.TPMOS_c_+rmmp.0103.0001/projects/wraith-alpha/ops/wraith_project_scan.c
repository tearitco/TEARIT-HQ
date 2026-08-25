#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define MAX_LINE 2048
#define MAX_PROJECTS 64

typedef struct {
    char dir_name[128];
    char project_id[256];
    char title[128];
    char entry_layout[MAX_PATH];
    char command[256];
    char id_prefix[128];
} WraithProject;

static char g_project_root[MAX_PATH] = ".";

static char *trim_ws(char *str) {
    char *end;

    if (!str) {
        return str;
    }
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    return str;
}

static int root_has_anchors(const char *root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];

    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(void) {
    FILE *kvp = NULL;
    char line[MAX_LINE];

    if (getcwd(g_project_root, sizeof(g_project_root)) && root_has_anchors(g_project_root)) {
        return;
    }

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) {
        return;
    }

    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *value = line + 13;
            value[strcspn(value, "\r\n")] = '\0';
            if (value[0] != '\0' && root_has_anchors(value)) {
                strncpy(g_project_root, value, sizeof(g_project_root) - 1);
                g_project_root[sizeof(g_project_root) - 1] = '\0';
            }
            break;
        }
    }

    fclose(kvp);
}

static void sanitize_token(const char *src, char *dst, size_t dst_sz) {
    size_t out = 0;

    if (!src || !dst || dst_sz == 0) {
        return;
    }

    while (*src && out + 1 < dst_sz) {
        unsigned char ch = (unsigned char)*src++;
        if (isalnum(ch)) {
            dst[out++] = (char)tolower(ch);
        } else if (ch == '-' || ch == '_' || ch == ' ' || ch == '/') {
            dst[out++] = '_';
        }
    }
    dst[out] = '\0';
}

static void fallback_title(const char *dir_name, char *dst, size_t dst_sz) {
    size_t out = 0;
    int new_word = 1;

    if (!dir_name || !dst || dst_sz == 0) {
        return;
    }

    while (*dir_name && out + 1 < dst_sz) {
        unsigned char ch = (unsigned char)*dir_name++;
        if (ch == '-' || ch == '_' || ch == '/') {
            if (out > 0 && dst[out - 1] != ' ' && out + 1 < dst_sz) {
                dst[out++] = ' ';
            }
            new_word = 1;
            continue;
        }

        if (new_word) {
            dst[out++] = (char)toupper(ch);
            new_word = 0;
        } else {
            dst[out++] = (char)tolower(ch);
        }
    }

    dst[out] = '\0';
}

static void read_pdl_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!dst || dst_sz == 0) {
        return;
    }
    dst[0] = '\0';

    f = fopen(path, "r");
    if (!f) {
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        char *pipe1;
        char *pipe2;
        char field[MAX_LINE];
        size_t len;

        pipe1 = strchr(line, '|');
        if (!pipe1) {
            continue;
        }
        pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) {
            continue;
        }

        len = (size_t)(pipe2 - pipe1 - 1);
        if (len >= sizeof(field)) {
            len = sizeof(field) - 1;
        }
        memcpy(field, pipe1 + 1, len);
        field[len] = '\0';

        if (strcmp(trim_ws(field), key) != 0) {
            continue;
        }

        strncpy(dst, trim_ws(pipe2 + 1), dst_sz - 1);
        dst[dst_sz - 1] = '\0';
        dst[strcspn(dst, "\r\n")] = '\0';
        break;
    }

    fclose(f);
}

static int project_cmp(const void *a, const void *b) {
    const WraithProject *pa = (const WraithProject *)a;
    const WraithProject *pb = (const WraithProject *)b;
    return strcmp(pa->dir_name, pb->dir_name);
}

static unsigned long manifest_checksum(const WraithProject *projects, int count) {
    unsigned long hash = 2166136261u;
    int i;
    int j;
    const char *parts[6];

    for (i = 0; i < count; i++) {
        parts[0] = projects[i].dir_name;
        parts[1] = projects[i].project_id;
        parts[2] = projects[i].title;
        parts[3] = projects[i].entry_layout;
        parts[4] = projects[i].command;
        parts[5] = projects[i].id_prefix;

        for (j = 0; j < 6; j++) {
            const unsigned char *p = (const unsigned char *)parts[j];
            while (*p) {
                hash ^= (unsigned long)(*p++);
                hash *= 16777619u;
            }
            hash ^= (unsigned long)'|';
            hash *= 16777619u;
        }
    }

    return hash;
}

int main(void) {
    char projects_dir[MAX_PATH];
    char manifest_path[MAX_PATH];
    char receipt_path[MAX_PATH];
    DIR *dir = NULL;
    struct dirent *entry;
    WraithProject projects[MAX_PROJECTS];
    int count = 0;
    FILE *manifest = NULL;
    FILE *receipt = NULL;
    time_t now;
    unsigned long checksum;
    int i;

    resolve_root();

    snprintf(projects_dir, sizeof(projects_dir), "%s/projects/wraith-alpha/wraith-projects", g_project_root);
    snprintf(manifest_path, sizeof(manifest_path), "%s/projects/wraith-alpha/session/wraith_project_manifest.txt", g_project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/projects/wraith-alpha/session/wraith_project_scan.receipt.pdl", g_project_root);

    dir = opendir(projects_dir);
    if (!dir) {
        fprintf(stderr, "wraith_project_scan: cannot open %s\n", projects_dir);
        return 1;
    }

    while ((entry = readdir(dir)) != NULL && count < MAX_PROJECTS) {
        char project_dir[MAX_PATH];
        char pdl_path[MAX_PATH];
        struct stat st;
        WraithProject *project = &projects[count];

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(project_dir, sizeof(project_dir), "%s/%s", projects_dir, entry->d_name);
        if (stat(project_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }

        snprintf(pdl_path, sizeof(pdl_path), "%s/project.pdl", project_dir);
        if (stat(pdl_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        memset(project, 0, sizeof(*project));
        strncpy(project->dir_name, entry->d_name, sizeof(project->dir_name) - 1);
        sanitize_token(entry->d_name, project->id_prefix, sizeof(project->id_prefix));
        snprintf(project->command, sizeof(project->command), "DESKTOP_ACTION:launch_%s", project->id_prefix);

        read_pdl_value(pdl_path, "project_id", project->project_id, sizeof(project->project_id));
        read_pdl_value(pdl_path, "entry_layout", project->entry_layout, sizeof(project->entry_layout));
        read_pdl_value(pdl_path, "title", project->title, sizeof(project->title));

        if (project->project_id[0] == '\0') {
            snprintf(project->project_id, sizeof(project->project_id), "wraith-alpha/wraith-projects/%s", entry->d_name);
        }
        if (project->title[0] == '\0') {
            fallback_title(entry->d_name, project->title, sizeof(project->title));
        }

        count++;
    }

    closedir(dir);
    qsort(projects, count, sizeof(projects[0]), project_cmp);

    manifest = fopen(manifest_path, "w");
    if (!manifest) {
        fprintf(stderr, "wraith_project_scan: cannot write %s\n", manifest_path);
        return 1;
    }

    fprintf(manifest, "# wraith_project_manifest\n");
    fprintf(manifest, "# root=%s\n", projects_dir);
    fprintf(manifest, "# format=dir_name|project_id|title|entry_layout|command|id_prefix\n");
    for (i = 0; i < count; i++) {
        fprintf(manifest, "%s|%s|%s|%s|%s|%s\n",
            projects[i].dir_name,
            projects[i].project_id,
            projects[i].title,
            projects[i].entry_layout,
            projects[i].command,
            projects[i].id_prefix);
    }
    fclose(manifest);

    checksum = manifest_checksum(projects, count);
    now = time(NULL);
    receipt = fopen(receipt_path, "w");
    if (!receipt) {
        fprintf(stderr, "wraith_project_scan: cannot write %s\n", receipt_path);
        return 1;
    }

    fprintf(receipt, "receipt_type=wraith_project_scan\n");
    fprintf(receipt, "host_project=wraith-alpha\n");
    fprintf(receipt, "scan_root=projects/wraith-alpha/wraith-projects\n");
    fprintf(receipt, "manifest_path=projects/wraith-alpha/session/wraith_project_manifest.txt\n");
    fprintf(receipt, "project_count=%d\n", count);
    fprintf(receipt, "manifest_checksum=%lu\n", checksum);
    fprintf(receipt, "generated_epoch=%ld\n", (long)now);
    fprintf(receipt, "status=ok\n");
    fclose(receipt);

    printf("wraith_project_scan: wrote %d projects\n", count);
    printf("manifest=%s\n", manifest_path);
    printf("receipt=%s\n", receipt_path);
    return 0;
}
