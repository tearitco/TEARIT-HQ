#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

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

void copy_dir(const char* src, const char* dst) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cp -r %s %s", src, dst);
    system(cmd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: mate <partner_id>\n");
        return 1;
    }

    const char* partner = argv[1];
    char pet_root[1024];
    resolve_pet_root(argv[0], pet_root);

    // Enclosure is the parent of the pet_root
    char enclosure_path[1024];
    strncpy(enclosure_path, pet_root, sizeof(enclosure_path)-1);
    char* last_slash = strrchr(enclosure_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        char* parent_slash = strrchr(enclosure_path, '/');
        if (parent_slash) {
            *(parent_slash + 1) = '\0';
        } else {
            strcpy(enclosure_path, "./");
        }
    } else {
        strcpy(enclosure_path, "./");
    }

    // Generate unique ID for offspring
    srand(time(NULL) + getpid());
    int offspring_id = rand() % 9000 + 1000;
    char offspring_name[128];
    snprintf(offspring_name, sizeof(offspring_name), "liz_gen_%d", offspring_id);

    char offspring_path[1024];
    snprintf(offspring_path, sizeof(offspring_path), "%s%s", enclosure_path, offspring_name);

    printf("[Mate] Parents: %s + %s\n", argv[0], partner);
    printf("[Mate] Offspring: %s\n", offspring_path);

    // 1. Create directory
    mkdir(offspring_path, 0777);

    // 2. Clone Parent Structure (ops/)
    char offspring_ops[1024], parent_ops[1024];
    snprintf(offspring_ops, sizeof(offspring_ops), "%s/ops", offspring_path);
    snprintf(parent_ops, sizeof(parent_ops), "%sops", pet_root);
    copy_dir(parent_ops, offspring_ops);

    // 3. Initialize fresh memory and stomach
    char path[1024];
    snprintf(path, sizeof(path), "%s/memory", offspring_path); mkdir(path, 0777);
    snprintf(path, sizeof(path), "%s/stomach", offspring_path); mkdir(path, 0777);

    // 4. Set state.txt
    snprintf(path, sizeof(path), "%s/state.txt", offspring_path);
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "type | lizard\nname | %s\n", offspring_name);
        fclose(f);
    }

    // 5. Initialize stats.txt
    snprintf(path, sizeof(path), "%s/memory/stats.txt", offspring_path);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "hp=10\nhunger=0\n");
        fclose(f);
    }

    // 6. Copy PDL
    char offspring_pdl[1024], parent_pdl[1024];
    snprintf(offspring_pdl, sizeof(offspring_pdl), "%s/piece.pdl", offspring_path);
    snprintf(parent_pdl, sizeof(parent_pdl), "%spiece.pdl", pet_root);
    
    // Update PDL paths for the new entity (this is a simplified copy)
    // The next agent or a build script will fix binary execution paths
    snprintf(path, sizeof(path), "cp %s %s", parent_pdl, offspring_pdl);
    system(path);

    printf("SUCCESS: Offspring %s born into the enclosure.\n", offspring_name);
    return 0;
}
