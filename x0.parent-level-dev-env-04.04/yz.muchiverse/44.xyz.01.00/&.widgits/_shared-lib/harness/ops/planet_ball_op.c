/* Shared ball formula for earth, moon, mars, and the solar page.
 * One column is one solid run. The Minecraft stack (pc_generate_chunk)
 * is the surface tile, three dirt, then stone. The floor slot is the
 * surface and the wall slot is stone. Radii are a scale of the earth
 * ball already on page earth (92), not kilometres.
 *
 * The radius authority is @.apps/earth-moth/bodies.pdl (BODY | name | radius).
 * It is read at run time, found from the HOUSE env var or by walking up
 * from argv[0]. A body not listed there is refused, so a page cannot
 * invent its own size. Surface and ocean dip stay here (not radii).
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BODIES_REL "@.apps/earth-moth/bodies.pdl"
#define MAX_BALLS 16

struct ball {
    char name[32];
    int radius;
};

static struct ball BALLS[MAX_BALLS];
static int NBALLS = 0;

struct body {
    const char *name;
    int ocean_dip;
    const char *surface;
};

static const struct body BODIES[] = {
    { "earth", 5, "grass" },
    { "moon",  0, "regolith" },
    { "mars",  0, "dust" },
};

static const struct body *find_body(const char *name) {
    size_t i;
    for (i = 0; i < sizeof BODIES / sizeof BODIES[0]; i++)
        if (!strcmp(BODIES[i].name, name)) return &BODIES[i];
    return 0;
}

/* Returns the radius from the table, or -1 when the body has no BODY row. */
static int radius_of(const char *name) {
    int i;
    for (i = 0; i < NBALLS; i++)
        if (!strcmp(BALLS[i].name, name)) return BALLS[i].radius;
    return -1;
}

/* Finds bodies.pdl: $HOUSE first, else the nearest parent of argv[0] that has it. */
static int locate_bodies(const char *argv0, char *out, size_t n) {
    const char *house = getenv("HOUSE");
    char real[PATH_MAX], *cut;
    if (house && *house) {
        snprintf(out, n, "%s/%s", house, BODIES_REL);
        if (access(out, R_OK) == 0) return 0;
    }
    if (!argv0 || !realpath(argv0, real)) return -1;
    for (;;) {
        cut = strrchr(real, '/');
        if (!cut) return -1;
        *cut = '\0';
        if (strlen(real) + sizeof BODIES_REL + 1 > n) return -1;
        snprintf(out, n, "%s/%s", real, BODIES_REL);
        if (access(out, R_OK) == 0) return 0;
        if (real[0] == '\0') return -1;
    }
}

/* Parses BODY rows; '#' lines and blanks are skipped. Returns 0 or -1. */
static int load_bodies(const char *argv0) {
    char path[PATH_MAX], line[256], name[32];
    FILE *f;
    int radius;
    if (locate_bodies(argv0, path, sizeof path) != 0) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, " BODY | %31[^|] | %d", name, &radius) == 2 && NBALLS < MAX_BALLS) {
            char *p = name + strlen(name);
            while (p > name && (p[-1] == ' ' || p[-1] == '\t')) *--p = '\0';
            strcpy(BALLS[NBALLS].name, name);
            BALLS[NBALLS].radius = radius;
            NBALLS++;
        }
    }
    fclose(f);
    return NBALLS > 0 ? 0 : -1;
}

static void print_body(const struct body *b, int radius) {
    printf("FORMULA body=%s R=%d center up=%d dn=%d\n", b->name, radius, radius, radius);
    printf("FORMULA outside dist>%d empty\n", radius);
    printf("FORMULA ocean dip=%d\n", b->ocean_dip);
    printf("FORMULA surface=%s\n", b->surface);
    printf("FORMULA mc z==surface tile z>=surface-3 dirt else stone\n");
    printf("FORMULA slots floor=surface wall=stone\n");
}

static int formula_solar(void) {
    int er = radius_of("earth"), mr = radius_of("moon"), ar = radius_of("mars");
    if (er < 0 || mr < 0 || ar < 0) { fprintf(stderr, "fail body\n"); return 2; }
    printf("FORMULA solar pages earth moon mars solar\n");
    printf("FORMULA solar bodies earth R=%d moon R=%d mars R=%d\n", er, mr, ar);
    /* Centres stay at least the sum of the two radii apart, so the balls do not merge. */
    printf("FORMULA solar gap earth-moon %d\n", er + mr);
    printf("FORMULA solar gap earth-mars %d\n", er + ar);
    printf("FORMULA mc z==surface tile z>=surface-3 dirt else stone\n");
    return 0;
}

int main(int argc, char **argv) {
    const struct body *b;
    int radius;
    if (load_bodies(argc > 0 ? argv[0] : 0) != 0) {
        fprintf(stderr, "fail bodies\n");
        return 3;
    }
    if (argc == 3 && !strcmp(argv[1], "formula") && !strcmp(argv[2], "solar"))
        return formula_solar();
    if (argc != 3 || strcmp(argv[1], "formula")) {
        fprintf(stderr, "usage: planet_ball_op formula earth|moon|mars|solar\n");
        return 2;
    }
    b = find_body(argv[2]);
    radius = radius_of(argv[2]);
    if (!b || radius < 0) {
        fprintf(stderr, "fail body\n");
        return 2;
    }
    print_body(b, radius);
    return 0;
}
