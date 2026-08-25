#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 4096
#define GRID_SIZE 25
#define TILE_SIZE 8
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char asset_base[MAX_PATH];

typedef struct {
    int r, g, b;
} Pixel;

typedef struct {
    Pixel grid[TILE_SIZE][TILE_SIZE];
} Tile;

void resolve_paths() {
    if (getcwd(project_root, sizeof(project_root))) {
        // Continue to set asset_base but return or continue
    } else {
        FILE *kvp = fopen("pieces/locations/location_kvp", "r");
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(line, "project_root") == 0) {
                        char *val = eq + 1;
                        val[strcspn(val, "\n\r")] = 0;
                        strncpy(project_root, val, MAX_PATH - 1);
                    }
                }
            }
            fclose(kvp);
        }
    }
    snprintf(asset_base, sizeof(asset_base), "%s/#.ref/^.fin_mc_8&16=99_c2+]FIXD/mc_extracted_csvs_8x8", project_root);
}

void load_tile_csv(const char* rel_path, Tile* out_tile) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", asset_base, rel_path);
    FILE *f = fopen(path, "r");
    if (!f) {
        // Fallback: fill with magenta for error
        for(int y=0; y<TILE_SIZE; y++) for(int x=0; x<TILE_SIZE; x++) { out_tile->grid[y][x] = (Pixel){255, 0, 255}; }
        return;
    }
    char line[MAX_LINE];
    for (int y = 0; y < TILE_SIZE && fgets(line, sizeof(line), f); y++) {
        char *ptr = line;
        for (int x = 0; x < TILE_SIZE; x++) {
            // CSV format: "r,g,b","r,g,b",...
            while (*ptr && *ptr != '"') ptr++;
            if (*ptr == '"') ptr++;
            sscanf(ptr, "%d,%d,%d", &out_tile->grid[y][x].r, &out_tile->grid[y][x].g, &out_tile->grid[y][x].b);
            while (*ptr && *ptr != '"') ptr++;
            if (*ptr == '"') ptr++;
            while (*ptr && *ptr != ',') ptr++;
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    resolve_paths();
    
    int current_z = 0;
    if (argc > 1) current_z = atoi(argv[1]);

    char map_path[MAX_PATH];
    snprintf(map_path, sizeof(map_path), "%s/projects/aow-2d/pieces/world/map_01/map_01_z%d.txt", project_root, current_z);
    
    FILE *mf = fopen(map_path, "r");
    if (!mf) {
        printf("Error: Could not open map at %s\n", map_path);
        return 1;
    }

    char grid[GRID_SIZE][GRID_SIZE];
    for (int y = 0; y < GRID_SIZE; y++) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), mf)) {
            for (int x = 0; x < GRID_SIZE; x++) {
                grid[y][x] = line[x];
            }
        }
    }
    fclose(mf);

    // Pre-load common tiles
    Tile water, grass, bricks, unknown;
    load_tile_csv("water/water.csv", &water);
    load_tile_csv("grass/grass.csv", &grass);
    load_tile_csv("bricks/bricks.csv", &bricks);
    load_tile_csv("unknown/unknown.csv", &unknown);

    char output_path[MAX_PATH];
    snprintf(output_path, sizeof(output_path), "%s/projects/aow-2d/manager/map_view.txt", project_root);
    FILE *out = fopen(output_path, "w");
    if (!out) return 1;

    // Render 25x25 tiles, each 8x8 pixels
    for (int ty = 0; ty < GRID_SIZE; ty++) {
        for (int py = 0; py < TILE_SIZE; py++) {
            for (int tx = 0; tx < GRID_SIZE; tx++) {
                Tile *current;
                char c = grid[ty][tx];
                if (c == '.') current = &water;
                else if (c == '#') current = &grass;
                else if (c >= 'A' && c <= 'D') current = &bricks;
                else current = &unknown;

                for (int px = 0; px < TILE_SIZE; px++) {
                    Pixel p = current->grid[py][px];
                    // Using ANSI 24-bit color: \x1b[48;2;R;G;Bm  \x1b[0m
                    // Use two spaces for a "pixel" to keep aspect ratio
                    fprintf(out, "\x1b[48;2;%d;%d;%dm  ", p.r, p.g, p.b);
                }
            }
            fprintf(out, "\x1b[0m\n");
        }
    }
    fclose(out);

    return 0;
}
