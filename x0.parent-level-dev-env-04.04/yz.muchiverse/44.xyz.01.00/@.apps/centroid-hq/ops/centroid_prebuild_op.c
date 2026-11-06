/* centroid_prebuild_op — check the centroid-test campus file.
 * check <court.pdl>   exit 0 and one PREBUILD line, or exit 2
 * The main building is larger than a 2x2 island stamp, has two floor
 * levels, and a place to stand (a floor with two empty cells above it)
 * beside the door. A bridge column has air under it.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXC 512
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

static int occ(int x, int y, int z) {
    for (int i = 0; i < n; i++)
        if (c[i].x == x && c[i].y == y && c[i].z == z) return 1;
    return 0;
}

static int below(int x, int y, int z) {
    for (int i = 0; i < n; i++)
        if (c[i].x == x && c[i].y == y && c[i].z < z) return 1;
    return 0;
}

static int is_job(const char *j) {
    return strcmp(j, "stone") == 0 || strcmp(j, "floor") == 0 || strcmp(j, "roof") == 0
        || strcmp(j, "door") == 0 || strcmp(j, "bridge") == 0 || strcmp(j, "farm") == 0;
}

static int check(void) {
    int roof = 0, doors = 0, bridge = 0, farm = 0, side = 0, main = 0, stands = 0;
    int bridge_x = 1000;
    int minx = 1000, miny = 1000, maxx = -1, maxy = -1;
    int floor_z[8], nf = 0;
    for (int i = 0; i < n; i++) if (!is_job(c[i].job)) { fprintf(stderr, "unknown job\n"); return 2; }
    for (int i = 0; i < n; i++)
        if (strcmp(c[i].job, "bridge") == 0 && c[i].x < bridge_x) bridge_x = c[i].x;
    if (bridge_x == 1000) { fprintf(stderr, "no bridge\n"); return 2; }
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
        else if (strcmp(c[i].job, "floor") == 0) {
            int seen = 0;
            if (!occ(c[i].x, c[i].y, c[i].z + 1) && !occ(c[i].x, c[i].y, c[i].z + 2)) stands++;
            for (int k = 0; k < nf; k++) if (floor_z[k] == c[i].z) seen = 1;
            if (!seen && nf < 8) floor_z[nf++] = c[i].z;
            if (c[i].x < bridge_x) main = 1;
        } else if (strcmp(c[i].job, "stone") == 0) {
            if (c[i].x >= bridge_x) side = 1;
            else main = 1;
        }
        if (c[i].x < bridge_x && strcmp(c[i].job, "bridge") != 0 && strcmp(c[i].job, "farm") != 0) {
            if (c[i].x < minx) minx = c[i].x;
            if (c[i].y < miny) miny = c[i].y;
            if (c[i].x > maxx) maxx = c[i].x;
            if (c[i].y > maxy) maxy = c[i].y;
        }
    }
    if (farm) side = 1;
    {
        int sx = maxx - minx + 1, sy = maxy - miny + 1, door_ok = 0;
        if (sx < 4 || sy < 4) { fprintf(stderr, "building is a stamp %dx%d\n", sx, sy); return 2; }
        if (nf < 2) { fprintf(stderr, "need two floors\n"); return 2; }
        if (stands < 4) { fprintf(stderr, "room has nowhere to stand\n"); return 2; }
        for (int i = 0; i < n && !door_ok; i++) if (strcmp(c[i].job, "door") == 0) {
            for (int j = 0; j < n; j++) if (strcmp(c[j].job, "floor") == 0
                && !occ(c[j].x, c[j].y, c[j].z + 1) && !occ(c[j].x, c[j].y, c[j].z + 2)
                && abs(c[j].x - c[i].x) + abs(c[j].y - c[i].y) == 1) door_ok = 1;
        }
        if (!door_ok) { fprintf(stderr, "door does not open onto a room\n"); return 2; }
        if (roof < 1 || doors != 2 || bridge < 1 || farm != 1 || !side || !main) {
            fprintf(stderr, "shape roof=%d doors=%d bridge=%d farm=%d\n", roof, doors, bridge, farm);
            return 2;
        }
        printf("PREBUILD cells=%d floors=%d span=%dx%d stands=%d doors=%d bridge=%d farm=%d islands=2\n",
               n, nf, sx, sy, stands, doors, bridge, farm);
    }
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
