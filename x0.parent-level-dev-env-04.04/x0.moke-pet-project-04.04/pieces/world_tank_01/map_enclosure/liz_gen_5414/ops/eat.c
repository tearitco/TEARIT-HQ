#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void resolve_pet_root(const char* bin_path, char* pet_root) {
    char temp[1024];
    strncpy(temp, bin_path, sizeof(temp)-1);
    temp[sizeof(temp)-1] = '\0';
    char* p = strstr(temp, "/ops/+x/");
    if (p) {
        *p = '\0';
        snprintf(pet_root, 1024, "%s/", temp);
    } else {
        strcpy(pet_root, "./");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: eat <target_piece_id>\n");
        return 1;
    }

    const char* target = argv[1];
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

    char old_path[1024], new_path[1024];
    snprintf(old_path, sizeof(old_path), "%s%s", enclosure_path, target);
    snprintf(new_path, sizeof(new_path), "%sstomach/%s", pet_root, target);
    
    printf("[Eat] Target: %s\n", old_path);
    printf("[Eat] Destination: %s\n", new_path);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mv %s %s", old_path, new_path);
    
    if (system(cmd) == 0) {
        printf("SUCCESS: Ate %s\n", target);
    } else {
        printf("FAILURE: Could not eat %s\n", target);
        return 1;
    }

    return 0;
}
