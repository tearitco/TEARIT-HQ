#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GRID_SIZE 25
#define MAX_PATH 4096

char project_root[MAX_PATH] = ".";

void resolve_paths() {
    if (getcwd(project_root, sizeof(project_root))) {
        return;
    }
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "project_root") == 0) {
                    char *val = eq + 1;
                    val[strcspn(val, "\n")] = 0;
                    strncpy(project_root, val, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    resolve_paths();

    char map_path[MAX_PATH];
    snprintf(map_path, sizeof(map_path), "%s/projects/aow-2d/pieces/world/map_01/map_01_z0.txt", project_root);

    FILE *f = fopen(map_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not create map at %s\n", map_path);
        return 1;
    }

    char grid[GRID_SIZE][GRID_SIZE];

    // Basic island generation
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (rand() % 100 < 30) grid[y][x] = '#'; // Land
            else grid[y][x] = '.'; // Water
        }
    }

    // Place Capitals
    grid[2][2] = 'A';
    grid[2][GRID_SIZE-3] = 'B';
    grid[GRID_SIZE-3][2] = 'C';
    grid[GRID_SIZE-3][GRID_SIZE-3] = 'D';
    
    // Ensure land under capitals
    if (grid[2][2] == 'A') { /* land check logic here if needed */ }

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            fputc(grid[y][x], f);
        }
        fputc('\n', f);
    }

    fclose(f);
    printf("Map generated at %s\n", map_path);
    return 0;
}
