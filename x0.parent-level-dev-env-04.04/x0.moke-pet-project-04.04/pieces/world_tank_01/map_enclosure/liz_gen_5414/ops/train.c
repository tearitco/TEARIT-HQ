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
    if (argc < 3) {
        printf("Usage: train <log_path> <entity_id>\n");
        return 1;
    }

    const char* log_path = argv[1];
    const char* entity_id = argv[2];
    char pet_root[1024];
    resolve_pet_root(argv[0], pet_root);

    char weight_path[1024];
    snprintf(weight_path, sizeof(weight_path), "%smemory/weights.txt", pet_root);

    FILE* lf = fopen(log_path, "r");
    if (!lf) {
        perror("Could not open log file");
        return 1;
    }

    int reward = 0;
    char line[1024];
    while (fgets(line, sizeof(line), lf)) {
        if (strstr(line, entity_id)) {
            if (strstr(line, "eat")) reward += 10;
            else if (strstr(line, "attack")) reward += 5;
            else if (strstr(line, "rest")) reward -= 1;
            else if (strstr(line, "nothing")) reward -= 2;
        }
    }
    fclose(lf);

    int current_weight = 0;
    FILE* wf = fopen(weight_path, "r");
    if (wf) {
        fscanf(wf, "weight=%d", &current_weight);
        fclose(wf);
    }

    wf = fopen(weight_path, "w");
    if (wf) {
        fprintf(wf, "weight=%d\n", current_weight + reward);
        fclose(wf);
        printf("[Train] %s updated. Reward: %d, New Weight: %d\n", entity_id, reward, current_weight + reward);
    } else {
        perror("Could not open weight file for writing");
        return 1;
    }

    return 0;
}
