/*
 * fondu.c - TPMOS Project Lifecycle Manager
 * 
 * Commands:
 *   --install <project>    - Compile + deploy + register ops
 *   --uninstall <app>      - Remove + unregister ops
 *   --archive <project>    - Move to trunk (source only)
 *   --restore <project>    - Restore from trunk + recompile
 *   --list                 - Show all projects and states
 *   --list-ops             - Show all available ops
 *
 * KISS Principle: "Automate what humans do manually, keep everything swappable"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#define PROJECTS_DIR "projects/"
#define TRUNK_DIR "projects/trunk/"
#define INSTALLED_DIR "pieces/apps/installed/"
#define OPS_REGISTRY_DIR "pieces/os/ops_registry/"
#define COMPILED_LIST "pieces/os/compiled_projects.txt"
#define OPS_CATALOG "pieces/os/ops_catalog.txt"

/* Function prototypes */
int cmd_install(const char *project_id);
int cmd_uninstall(const char *app_id);
int cmd_archive(const char *project_id);
int cmd_restore(const char *project_id);
int cmd_list(void);
int cmd_list_ops(void);
int copy_directory(const char *src, const char *dst);
int remove_directory(const char *path);
int register_ops(const char *project_id, const char *project_path);
int unregister_ops(const char *project_id);
int update_compiled_list(const char *project_id, int add);
int generate_ops_catalog(void);
int file_exists(const char *path);
int dir_exists(const char *path);
void print_usage(void);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --install requires project name\n");
            return 1;
        }
        return cmd_install(argv[2]);
    }
    else if (strcmp(argv[1], "--uninstall") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --uninstall requires app name\n");
            return 1;
        }
        return cmd_uninstall(argv[2]);
    }
    else if (strcmp(argv[1], "--archive") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --archive requires project name\n");
            return 1;
        }
        return cmd_archive(argv[2]);
    }
    else if (strcmp(argv[1], "--restore") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --restore requires project name\n");
            return 1;
        }
        return cmd_restore(argv[2]);
    }
    else if (strcmp(argv[1], "--list") == 0) {
        return cmd_list();
    }
    else if (strcmp(argv[1], "--list-ops") == 0) {
        return cmd_list_ops();
    }
    else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 0;
}

void print_usage(void) {
    printf("Fondu - TPMOS Project Lifecycle Manager\n\n");
    printf("Usage: ./fondu <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  --install <project>    Compile, deploy to installed/, register ops\n");
    printf("  --uninstall <app>      Remove from installed/, unregister ops\n");
    printf("  --archive <project>    Move to trunk/ (source only, not compiled)\n");
    printf("  --restore <project>    Move from trunk/ back to projects/, recompile\n");
    printf("  --list                 Show all projects and their states\n");
    printf("  --list-ops             Show all available ops (from ops catalog)\n");
    printf("  --help, -h             Show this help message\n");
}

int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

int dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int cmd_install(const char *project_id) {
    char src_path[512];
    char dst_path[512];
    char ops_manifest[512];
    
    printf("=== Fondu: Installing %s ===\n", project_id);
    
    /* 1. Verify source exists */
    snprintf(src_path, sizeof(src_path), "%s%s", PROJECTS_DIR, project_id);
    if (!dir_exists(src_path)) {
        fprintf(stderr, "Error: Project '%s' not found in %s\n", project_id, src_path);
        return 1;
    }
    
    /* 2. Verify project.pdl exists */
    char pdl_path[512];
    snprintf(pdl_path, sizeof(pdl_path), "%s/project.pdl", src_path);
    if (!file_exists(pdl_path)) {
        fprintf(stderr, "Error: %s/project.pdl not found\n", src_path);
        return 1;
    }
    
    /* 3. Create destination directory */
    snprintf(dst_path, sizeof(dst_path), "%s%s", INSTALLED_DIR, project_id);
    printf("Creating: %s\n", dst_path);
    
    /* Remove existing installation if present */
    if (dir_exists(dst_path)) {
        printf("Removing existing installation...\n");
        remove_directory(dst_path);
    }
    
    mkdir(dst_path, 0755);
    
    /* 4. Copy project to installed directory */
    printf("Copying source to installed...\n");
    if (copy_directory(src_path, dst_path) != 0) {
        fprintf(stderr, "Error: Failed to copy project\n");
        return 1;
    }
    
    /* 5. Set read-only permissions on installed files */
    printf("Setting read-only permissions...\n");
    chmod(dst_path, 0555);
    
    /* 6. Register ops if ops_manifest.txt exists */
    snprintf(ops_manifest, sizeof(ops_manifest), "%s/ops/ops_manifest.txt", src_path);
    if (file_exists(ops_manifest)) {
        printf("Found ops_manifest.txt, registering ops...\n");
        if (register_ops(project_id, src_path) != 0) {
            fprintf(stderr, "Warning: Failed to register ops\n");
        }
    } else {
        printf("No ops_manifest.txt found (project has no exposed ops)\n");
    }
    
    /* 7. Update compiled_projects.txt */
    update_compiled_list(project_id, 1);
    
    /* 8. Generate ops catalog */
    generate_ops_catalog();
    
    printf("\n✓ Installed: %s\n", dst_path);
    printf("✓ Registered ops in %s%s.txt\n", OPS_REGISTRY_DIR, project_id);
    printf("✓ Updated %s\n", COMPILED_LIST);
    
    return 0;
}

int cmd_uninstall(const char *app_id) {
    char app_path[512];
    
    printf("=== Fondu: Uninstalling %s ===\n", app_id);
    
    /* 1. Verify installed app exists */
    snprintf(app_path, sizeof(app_path), "%s%s", INSTALLED_DIR, app_id);
    if (!dir_exists(app_path)) {
        fprintf(stderr, "Error: App '%s' not found in %s\n", app_id, app_path);
        return 1;
    }
    
    /* 2. Remove from installed directory */
    printf("Removing: %s\n", app_path);
    if (remove_directory(app_path) != 0) {
        fprintf(stderr, "Error: Failed to remove %s\n", app_path);
        return 1;
    }
    
    /* 3. Unregister ops */
    printf("Unregistering ops...\n");
    unregister_ops(app_id);
    
    /* 4. Update compiled_projects.txt */
    update_compiled_list(app_id, 0);
    
    /* 5. Generate updated ops catalog */
    generate_ops_catalog();
    
    printf("\n✓ Uninstalled: %s\n", app_id);
    printf("✓ Removed ops from registry\n");
    printf("✓ Updated %s\n", COMPILED_LIST);
    
    return 0;
}

int cmd_archive(const char *project_id) {
    char src_path[512];
    char dst_path[512];
    
    printf("=== Fondu: Archiving %s ===\n", project_id);
    
    /* 1. Verify source exists */
    snprintf(src_path, sizeof(src_path), "%s%s", PROJECTS_DIR, project_id);
    if (!dir_exists(src_path)) {
        fprintf(stderr, "Error: Project '%s' not found in %s\n", project_id, src_path);
        return 1;
    }
    
    /* 2. Create destination in trunk */
    snprintf(dst_path, sizeof(dst_path), "%s%s", TRUNK_DIR, project_id);
    
    /* Remove existing trunk copy if present */
    if (dir_exists(dst_path)) {
        remove_directory(dst_path);
    }
    
    /* 3. Move project to trunk */
    printf("Moving %s → %s\n", src_path, dst_path);
    if (rename(src_path, dst_path) != 0) {
        fprintf(stderr, "Error: Failed to move project to trunk\n");
        return 1;
    }
    
    /* 4. Update compiled_projects.txt (remove) */
    update_compiled_list(project_id, 0);
    
    /* 5. Unregister ops */
    unregister_ops(project_id);
    
    /* 6. Generate updated ops catalog */
    generate_ops_catalog();
    
    printf("\n✓ Archived: %s → %s\n", project_id, dst_path);
    printf("✓ Removed from %s\n", COMPILED_LIST);
    
    return 0;
}

int cmd_restore(const char *project_id) {
    char src_path[512];
    char dst_path[512];
    
    printf("=== Fondu: Restoring %s ===\n", project_id);
    
    /* 1. Verify source exists in trunk */
    snprintf(src_path, sizeof(src_path), "%s%s", TRUNK_DIR, project_id);
    if (!dir_exists(src_path)) {
        fprintf(stderr, "Error: Project '%s' not found in %s\n", project_id, src_path);
        return 1;
    }
    
    /* 2. Create destination in projects */
    snprintf(dst_path, sizeof(dst_path), "%s%s", PROJECTS_DIR, project_id);
    
    /* Remove existing if present */
    if (dir_exists(dst_path)) {
        remove_directory(dst_path);
    }
    
    /* 3. Move project from trunk to projects */
    printf("Moving %s → %s\n", src_path, dst_path);
    if (rename(src_path, dst_path) != 0) {
        fprintf(stderr, "Error: Failed to restore project\n");
        return 1;
    }
    
    /* 4. Update compiled_projects.txt (add) */
    update_compiled_list(project_id, 1);
    
    /* 5. Register ops if available */
    char ops_manifest[512];
    snprintf(ops_manifest, sizeof(ops_manifest), "%s/ops/ops_manifest.txt", dst_path);
    if (file_exists(ops_manifest)) {
        register_ops(project_id, dst_path);
    }
    
    /* 6. Generate updated ops catalog */
    generate_ops_catalog();
    
    printf("\n✓ Restored: %s → %s\n", project_id, dst_path);
    printf("✓ Added to %s\n", COMPILED_LIST);
    
    return 0;
}

int cmd_list(void) {
    printf("=== Fondu: Project List ===\n\n");
    
    /* List active projects */
    printf("ACTIVE (projects/):\n");
    DIR *dir = opendir(PROJECTS_DIR);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "trunk") == 0) continue;
            
            char pdl_path[512];
            snprintf(pdl_path, sizeof(pdl_path), "%s%s/project.pdl", PROJECTS_DIR, entry->d_name);
            if (file_exists(pdl_path)) {
                printf("  - %s\n", entry->d_name);
            }
        }
        closedir(dir);
    }
    
    /* List archived projects */
    printf("\nARCHIVED (projects/trunk/):\n");
    dir = opendir(TRUNK_DIR);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "legacy_archive") == 0) continue;
            
            char pdl_path[512];
            snprintf(pdl_path, sizeof(pdl_path), "%s%s/project.pdl", TRUNK_DIR, entry->d_name);
            if (file_exists(pdl_path)) {
                printf("  - %s\n", entry->d_name);
            }
        }
        closedir(dir);
    }
    
    /* List installed apps */
    printf("\nINSTALLED (pieces/apps/installed/):\n");
    dir = opendir(INSTALLED_DIR);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            
            char pdl_path[512];
            snprintf(pdl_path, sizeof(pdl_path), "%s%s/project.pdl", INSTALLED_DIR, entry->d_name);
            if (file_exists(pdl_path)) {
                printf("  - %s (production)\n", entry->d_name);
            }
        }
        closedir(dir);
    }
    
    printf("\n");
    return 0;
}

int cmd_list_ops(void) {
    printf("=== Fondu: Available Ops ===\n\n");
    
    if (!file_exists(OPS_CATALOG)) {
        printf("(No ops registered yet)\n");
        return 0;
    }
    
    FILE *fp = fopen(OPS_CATALOG, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot read %s\n", OPS_CATALOG);
        return 1;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;
        fputs(line, stdout);
    }
    
    fclose(fp);
    return 0;
}

/* Helper: Copy directory recursively */
int copy_directory(const char *src, const char *dst) {
    char src_path[512];
    char dst_path[512];
    
    /* Create destination directory */
    mkdir(dst, 0755);
    
    DIR *dir = opendir(src);
    if (!dir) return -1;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        
        struct stat st;
        if (stat(src_path, &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            /* Recursively copy subdirectory */
            copy_directory(src_path, dst_path);
        } else {
            /* Copy file */
            FILE *in = fopen(src_path, "rb");
            FILE *out = fopen(dst_path, "wb");
            if (in && out) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
                    fwrite(buf, 1, n, out);
                }
                fclose(in);
                fclose(out);
            }
        }
    }
    
    closedir(dir);
    return 0;
}

/* Helper: Remove directory recursively */
int remove_directory(const char *path) {
    /* First, make the directory writable so we can modify its contents */
    chmod(path, 0755);
    
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char entry_path[512];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(entry_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Recursively remove subdirectory */
            remove_directory(entry_path);
        } else {
            /* Make writable before deleting */
            chmod(entry_path, 0644);
            remove(entry_path);
        }
    }

    closedir(dir);
    rmdir(path);
    return 0;
}

/* Helper: Register ops from ops_manifest.txt */
int register_ops(const char *project_id, const char *project_path) {
    char manifest_path[512];
    char registry_path[512];
    
    snprintf(manifest_path, sizeof(manifest_path), "%s/ops/ops_manifest.txt", project_path);
    snprintf(registry_path, sizeof(registry_path), "%s%s.txt", OPS_REGISTRY_DIR, project_id);
    
    FILE *in = fopen(manifest_path, "r");
    if (!in) return -1;
    
    FILE *out = fopen(registry_path, "w");
    if (!out) {
        fclose(in);
        return -1;
    }
    
    fprintf(out, "# Ops Registry: %s\n", project_id);
    fprintf(out, "# Auto-generated by fondu --install\n");
    fprintf(out, "# Format: op_name|description|args_format\n\n");
    
    char line[512];
    while (fgets(line, sizeof(line), in)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        fputs(line, out);
    }
    
    fclose(in);
    fclose(out);
    
    return 0;
}

/* Helper: Unregister ops */
int unregister_ops(const char *project_id) {
    char registry_path[512];
    snprintf(registry_path, sizeof(registry_path), "%s%s.txt", OPS_REGISTRY_DIR, project_id);
    
    if (file_exists(registry_path)) {
        remove(registry_path);
    }
    
    return 0;
}

/* Helper: Update compiled_projects.txt */
int update_compiled_list(const char *project_id, int add) {
    /* Read existing entries */
    char entries[100][256];
    int count = 0;
    
    FILE *fp = fopen(COMPILED_LIST, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            /* Skip lines mentioning this project */
            if (strstr(line, project_id) != NULL) continue;
            strncpy(entries[count++], line, sizeof(entries[0]) - 1);
            entries[count-1][sizeof(entries[0]) - 1] = '\0';
        }
        fclose(fp);
    }
    
    /* Write back */
    fp = fopen(COMPILED_LIST, "w");
    if (!fp) return -1;
    
    fprintf(fp, "# Compiled Projects List\n");
    fprintf(fp, "# Format: One project_id per line\n");
    fprintf(fp, "# Lines starting with # are comments\n\n");
    fprintf(fp, "# Active projects (compiled on every build)\n");
    
    for (int i = 0; i < count; i++) {
        fputs(entries[i], fp);
    }
    
    if (add) {
        fprintf(fp, "%s\n", project_id);
    }
    
    fclose(fp);
    return 0;
}

/* Helper: Generate ops catalog */
int generate_ops_catalog(void) {
    FILE *out = fopen(OPS_CATALOG, "w");
    if (!out) return -1;
    
    fprintf(out, "# Ops Catalog (Human-Readable)\n");
    fprintf(out, "# Auto-generated by fondu\n");
    fprintf(out, "# Use: fondu --list-ops to view\n\n");
    
    DIR *dir = opendir(OPS_REGISTRY_DIR);
    if (!dir) {
        fclose(out);
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "README.txt") == 0) continue;
        
        /* Get project name (remove .txt) */
        char project_id[256];
        strncpy(project_id, entry->d_name, sizeof(project_id) - 1);
        char *dot = strrchr(project_id, '.');
        if (dot) *dot = '\0';
        
        /* Read registry file */
        char registry_path[512];
        snprintf(registry_path, sizeof(registry_path), "%s%s", OPS_REGISTRY_DIR, entry->d_name);
        
        FILE *in = fopen(registry_path, "r");
        if (!in) continue;
        
        fprintf(out, "=== %s ===\n", project_id);
        
        char line[512];
        while (fgets(line, sizeof(line), in)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            
            /* Parse: op_name|description|args */
            char *pipe1 = strchr(line, '|');
            if (pipe1) {
                *pipe1 = '\0';
                char *desc = pipe1 + 1;
                char *pipe2 = strchr(desc, '|');
                if (pipe2) *pipe2 = '\0';
                
                fprintf(out, "  %-20s - %s\n", line, desc);
            }
        }
        
        fprintf(out, "\n");
        fclose(in);
    }
    
    closedir(dir);
    fclose(out);
    
    return 0;
}
