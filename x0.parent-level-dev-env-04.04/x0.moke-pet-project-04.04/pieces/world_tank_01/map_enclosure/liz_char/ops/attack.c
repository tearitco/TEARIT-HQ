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
        printf("Usage: attack <target_id>\n");
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

    char target_stats[1024];
    snprintf(target_stats, sizeof(target_stats), "%s%s/memory/stats.txt", enclosure_path, target);

    int hp = 10, hunger = 0;
    FILE* f = fopen(target_stats, "r");
    if (!f) {
        printf("FAILURE: Target %s stats not found or not a lizard.\n", target);
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "hp=", 3) == 0) hp = atoi(line + 3);
        else if (strncmp(line, "hunger=", 7) == 0) hunger = atoi(line + 7);
    }
    fclose(f);

    // Attack reduces HP by 3
    hp -= 3;
    if (hp < 0) hp = 0;

    f = fopen(target_stats, "w");
    if (f) {
        fprintf(f, "hp=%d\nhunger=%d\n", hp, hunger);
        fclose(f);
        printf("SUCCESS: Attacked %s. Target HP is now %d\n", target, hp);
    } else {
        perror("Could not update target stats");
        return 1;
    }

    return 0;
}
