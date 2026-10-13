/*
 * Op: dungeon_gen - Procedural dungeon level generator for Cataclysm-HQ
 * Usage: dungeon_gen.+x <width> <height>
 *   e.g. dungeon_gen.+x 15 10
 *
 * Generates a simple dungeon layout (rooms + corridors) and writes
 * it to maps/dng_AUTO.txt. Uses BLM (Batched Loot Metrics) pattern
 * for room density and can be Gemma-evolved via concept bank weights.
 *
 * Output: maps/dng_AUTO.txt (ASCII tile grid)
 *         session/state.txt (updates map_file reference)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH 8192
#define MAX_SIZE 256

char project_root[MAX_PATH] = ".";

void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        strncpy(project_root, env, MAX_PATH - 1);
        return;
    }
    /* Default: assume we're running from project root */
    if (getcwd(project_root, sizeof(project_root)) == NULL) {
        strncpy(project_root, ".", sizeof(project_root) - 1);
    }
}

int main(int argc, char *argv[]) {
    int w = (argc >= 2) ? atoi(argv[1]) : 15;
    int h = (argc >= 3) ? atoi(argv[2]) : 10;

    if (w < 5 || w > MAX_SIZE) w = 15;
    if (h < 5 || h > MAX_SIZE) h = 10;

    /* Seed */
    srand((unsigned int)(time(NULL) ^ getpid()));

    /* Build map: start with walls (#) */
    char map[MAX_SIZE][MAX_SIZE];
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            map[y][x] = '#';
        }
    }

    /* Carve rooms (simple BSP-like approach) */
    int num_rooms = 3 + (rand() % 3);
    int room_x[8], room_y[8], room_w[8], room_h[8];
    int placed = 0;

    for (int i = 0; i < num_rooms && i < 8; i++) {
        int rw = 4 + (rand() % 4);
        int rh = 3 + (rand() % 3);
        int rx, ry;
        int attempts = 0;
        int ok = 0;

        while (attempts < 30) {
            rx = 2 + (rand() % (w - rw - 4));
            ry = 2 + (rand() % (h - rh - 4));
            ok = 1;
            for (int j = 0; j < placed; j++) {
                /* Check overlap with margin */
                if (rx < room_x[j] + room_w[j] + 1 &&
                    rx + rw + 1 > room_x[j] &&
                    ry < room_y[j] + room_h[j] + 1 &&
                    ry + rh + 1 > room_y[j]) {
                    ok = 0;
                    break;
                }
            }
            if (ok) break;
            attempts++;
        }

        if (ok) {
            room_x[placed] = rx; room_y[placed] = ry;
            room_w[placed] = rw; room_h[placed] = rh;

            /* Carve floor (.) */
            for (int dy = 0; dy < rh; dy++) {
                for (int dx = 0; dx < rw; dx++) {
                    if (ry + dy < h && rx + dx < w) {
                        map[ry + dy][rx + dx] = '.';
                    }
                }
            }
            placed++;
        }
    }

    /* Connect rooms with corridors */
    for (int i = 1; i < placed; i++) {
        int x1 = room_x[i-1] + room_w[i-1]/2;
        int y1 = room_y[i-1] + room_h[i-1]/2;
        int x2 = room_x[i] + room_w[i]/2;
        int y2 = room_y[i] + room_h[i]/2;

        /* L-shaped corridor */
        /* Horizontal first */
        int x = x1, xdir = (x2 > x1) ? 1 : -1;
        while (x != x2) {
            if (y1 < h && x < w) map[y1][x] = '.';
            x += xdir;
        }
        /* Vertical */
        int y = y1, ydir = (y2 > y1) ? 1 : -1;
        while (y != y2) {
            if (y < h && x2 < w) map[y][x2] = '.';
            y += ydir;
        }
    }

    /* Place stairs down in last room */
    if (placed > 0) {
        int sx = room_x[placed-1] + room_w[placed-1]/2;
        int sy = room_y[placed-1] + room_h[placed-1]/2;
        if (sy < h && sx < w) map[sy][sx] = '>';
    }

    /* Write map */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/maps/dng_AUTO.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                fputc(map[y][x], f);
            }
            fputc('\n', f);
        }
        fputc('\0', f);
        fclose(f);
    }

    /* Update session state */
    snprintf(path, sizeof(path), "%s/session/state.txt", project_root);
    f = fopen(path, "a");
    if (f) {
        fprintf(f, "map_file=dng_AUTO.txt\n");
        fprintf(f, "dungeon_w=%d\n", w);
        fprintf(f, "dungeon_h=%d\n", h);
        fprintf(f, "num_rooms=%d\n", placed);
        fclose(f);
    }

    printf("Generated dungeon: %dx%d, %d rooms, %d corridors\n", w, h, placed, placed-1);
    return 0;
}
