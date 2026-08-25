#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

/* Helper to get the directory part of a path */
void get_base_dir(const char* full_path, char* base_dir) {
    char* last_slash = strrchr(full_path, '/');
    if (last_slash) {
        // We are in ops/+x/ - need to go up 2 levels to get the pet root
        // Find the slash before ops/
        char temp[1024];
        strncpy(temp, full_path, last_slash - full_path);
        temp[last_slash - full_path] = '\0';
        
        char* ops_slash = strrchr(temp, '/'); // This is the slash before +x/
        if (ops_slash) {
            *ops_slash = '\0';
            char* pet_slash = strrchr(temp, '/'); // This is the slash before ops/
            if (pet_slash) {
                strncpy(base_dir, temp, pet_slash - temp + 1);
                base_dir[pet_slash - temp + 1] = '\0';
            } else {
                strcpy(base_dir, "./");
            }
        }
    } else {
        strcpy(base_dir, "./");
    }
}

/* More robust helper: find the pet root by looking for piece.pdl or just stripping /ops/+x/bin */
void resolve_pet_root(const char* bin_path, char* pet_root) {
    char temp[1024];
    strncpy(temp, bin_path, sizeof(temp)-1);
    temp[sizeof(temp)-1] = '\0';
    
    // Pattern: pieces/.../PET_NAME/ops/+x/BIN
    // We want: pieces/.../PET_NAME/
    
    char* p = strstr(temp, "/ops/+x/");
    if (p) {
        *p = '\0';
        // Now temp is pieces/.../PET_NAME
        snprintf(pet_root, 1024, "%s/", temp);
    } else {
        strcpy(pet_root, "./");
    }
}

int main(int argc, char* argv[]) {
    char pet_root[1024];
    resolve_pet_root(argv[0], pet_root);
    
    // Enclosure is the parent of the pet_root
    char enclosure_path[1024];
    strncpy(enclosure_path, pet_root, sizeof(enclosure_path)-1);
    char* last_slash = strrchr(enclosure_path, '/');
    if (last_slash) {
        *last_slash = '\0'; // Remove trailing slash
        char* parent_slash = strrchr(enclosure_path, '/');
        if (parent_slash) {
            *(parent_slash + 1) = '\0';
        } else {
            strcpy(enclosure_path, "./");
        }
    } else {
        strcpy(enclosure_path, "./");
    }

    char obs_path[1024];
    snprintf(obs_path, sizeof(obs_path), "%smemory/observations.txt", pet_root);

    printf("[Scan] Pet Root: %s\n", pet_root);
    printf("[Scan] Enclosure: %s\n", enclosure_path);

    // Ensure memory dir exists
    char mem_dir[1024];
    snprintf(mem_dir, sizeof(mem_dir), "%smemory", pet_root);
    struct stat st = {0};
    if (stat(mem_dir, &st) == -1) {
        mkdir(mem_dir, 0777);
    }

    FILE *obs_file = fopen(obs_path, "w");
    if (!obs_file) {
        perror("Could not open observations.txt");
        return 1;
    }

    DIR *dir = opendir(enclosure_path);
    if (!dir) {
        perror("Could not open enclosure");
        fclose(obs_file);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char state_path[1024];
        snprintf(state_path, sizeof(state_path), "%s%s/state.txt", enclosure_path, entry->d_name);

        FILE *state_file = fopen(state_path, "r");
        if (state_file) {
            char line[256];
            while (fgets(line, sizeof(line), state_file)) {
                if (strncmp(line, "type | ", 7) == 0) {
                    char type[64];
                    strncpy(type, line + 7, 63);
                    type[strcspn(type, "\n\r")] = 0;
                    fprintf(obs_file, "%s | %s\n", entry->d_name, type);
                    break;
                }
            }
            fclose(state_file);
        }
    }

    closedir(dir);
    fclose(obs_file);
    printf("[Scan] Complete. Target: %s\n", obs_path);
    return 0;
}
