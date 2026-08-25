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
    char pet_root[1024];
    resolve_pet_root(argv[0], pet_root);

    char stat_path[1024];
    snprintf(stat_path, sizeof(stat_path), "%smemory/stats.txt", pet_root);

    int hp = 10;
    FILE* f = fopen(stat_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "hp=", 3) == 0) {
                hp = atoi(line + 3);
                break;
            }
        }
        fclose(f);
    }

    if (hp <= 0) {
        printf("[Death] Entity has died.\n");
        
        char state_path[1024];
        snprintf(state_path, sizeof(state_path), "%sstate.txt", pet_root);
        
        FILE* sf = fopen(state_path, "w");
        if (sf) {
            fprintf(sf, "type | food\nname | Lizard Corpse\n");
            fclose(sf);
            printf("[Death] Type changed to food.\n");
        }
        
        // Final audit note
        return 0;
    }

    return 0;
}
