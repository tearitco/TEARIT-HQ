#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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
    char pet_root[1024];
    resolve_pet_root(argv[0], pet_root);

    char stat_path[1024];
    snprintf(stat_path, sizeof(stat_path), "%smemory/stats.txt", pet_root);

    int hp = 10, hunger = 0;
    FILE* f = fopen(stat_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "hp=", 3) == 0) hp = atoi(line + 3);
            else if (strncmp(line, "hunger=", 7) == 0) hunger = atoi(line + 7);
        }
        fclose(f);
    }

    // Metabolism Logic
    hunger += 1;
    if (hunger > 20) {
        hp -= 1;
        printf("[Breathe] Starving! HP: %d, Hunger: %d\n", hp, hunger);
    } else {
        printf("[Breathe] Alive. HP: %d, Hunger: %d\n", hp, hunger);
    }

    f = fopen(stat_path, "w");
    if (f) {
        fprintf(f, "hp=%d\nhunger=%d\n", hp, hunger);
        fclose(f);
    } else {
        perror("Could not update stats");
        return 1;
    }

    return 0;
}
