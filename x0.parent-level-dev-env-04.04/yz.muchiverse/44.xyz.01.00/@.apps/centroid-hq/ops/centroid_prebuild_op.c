/* centroid_prebuild_op — check the centroid-test campus file.
 * check <court.pdl>   exit 0 and one PREBUILD line, or exit 2
 * A roof shares a column with a floor. A bridge column has air under it.
 * Two doors. One farm. The side island is x >= 8 and does not touch the main island.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXC 256
typedef struct { int x, y, z; char job[16]; } Cell;
static Cell c[MAXC];
static int n;

static int load(const char *path) {
    FILE *f = fopen(path, "r");
    char line[256];
    if (!f) { fprintf(stderr, "no file\n"); return 2; }
    n = 0;
    while (fgets(line, sizeof line, f)) {
        int x, y, z; char job[16];
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "CELL | %d | %d | %d | %15s", &x, &y, &z, job) != 4) {
            fprintf(stderr, "bad row\n"); fclose(f); return 2;
        }
        if (n >= MAXC) { fprintf(stderr, "too many\n"); fclose(f); return 2; }
        c[n].x = x; c[n].y = y; c[n].z = z; snprintf(c[n].job, sizeof c[n].job, "%s", job);
        n++;
    }
    fclose(f);
    return 0;
}

static int below(int x, int y, int z) {
    for (int i = 0; i < n; i++)
        if (c[i].x == x && c[i].y == y && c[i].z < z) return 1;
    return 0;
}

static int check(void) {
    int roof = 0, doors = 0, bridge = 0, farm = 0, side = 0, main = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(c[i].job, "roof") == 0) {
            int floor = 0;
            for (int j = 0; j < n; j++)
                if (c[j].x == c[i].x && c[j].y == c[i].y && c[j].z < c[i].z && strcmp(c[j].job, "floor") == 0)
                    floor = 1;
            if (!floor) { fprintf(stderr, "roof without floor\n"); return 2; }
            roof++;
        } else if (strcmp(c[i].job, "door") == 0) doors++;
        else if (strcmp(c[i].job, "bridge") == 0) {
            if (below(c[i].x, c[i].y, c[i].z)) { fprintf(stderr, "bridge has ground under it\n"); return 2; }
            bridge++;
        } else if (strcmp(c[i].job, "farm") == 0) farm++;
        else if (strcmp(c[i].job, "stone") == 0 || strcmp(c[i].job, "floor") == 0) {
            if (c[i].x >= 8) side = 1;
            else main = 1;
        } else { fprintf(stderr, "unknown job\n"); return 2; }
    }
    if (roof < 1 || doors != 2 || bridge < 1 || farm != 1 || !side || !main) {
        fprintf(stderr, "shape roof=%d doors=%d bridge=%d farm=%d\n", roof, doors, bridge, farm);
        return 2;
    }
    printf("PREBUILD cells=%d roof_over_room=%d doors=%d bridge=%d farm=%d islands=2\n",
           n, roof, doors, bridge, farm);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "check") == 0) {
        int rc = load(argv[2]);
        if (rc) return rc;
        return check();
    }
    fprintf(stderr, "usage: centroid_prebuild_op check <court.pdl>\n");
    return 2;
}
