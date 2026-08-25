#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define MAP_HEIGHT 10
#define MAP_WIDTH 20

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <x> <y> <z> <map_file_path>\n", argv[0]);
        return 1;
    }

    int xel_x = atoi(argv[1]);
    int xel_y = atoi(argv[2]);
    int xel_z = atoi(argv[3]);
    const char* map_path = argv[4];

    FILE* map_file = fopen(map_path, "r");
    if (!map_file) {
        fprintf(stderr, "Error: Cannot open map file: %s\n", map_path);
        return 1;
    }

    char line[256];
    int row = 0;
    while (fgets(line, sizeof(line), map_file) && row < MAP_HEIGHT) {
        if (row == xel_y) {
            for (int col = 0; col < (int)strlen(line); col++) {
                if (col == xel_x) {
                    printf(">");
                } else if (line[col] == '\n') {
                    printf("\n");
                } else {
                    printf("%c", line[col]);
                }
            }
        } else {
            fputs(line, stdout);
        }
        row++;
    }

    fclose(map_file);
    return 0;
}
