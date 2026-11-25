/* Shared ball formula for earth, moon, mars, and the solar page.
 * One column is one solid run. The Minecraft stack (pc_generate_chunk)
 * is the surface tile, three dirt, then stone. The floor slot is the
 * surface and the wall slot is stone. Radii are a scale of the earth
 * ball already on page earth (92), not kilometres. A body not in this
 * table is refused, so a page cannot invent its own size.
 */
#include <stdio.h>
#include <string.h>

struct body {
    const char *name;
    int radius;
    int ocean_dip;
    const char *surface;
};

static const struct body BODIES[] = {
    { "earth", 92, 5, "grass" },
    { "moon",  25, 0, "regolith" },
    { "mars",  49, 0, "dust" },
};

static const struct body *find_body(const char *name) {
    size_t i;
    for (i = 0; i < sizeof BODIES / sizeof BODIES[0]; i++)
        if (!strcmp(BODIES[i].name, name)) return &BODIES[i];
    return 0;
}

static void print_body(const struct body *b) {
    printf("FORMULA body=%s R=%d center up=%d dn=%d\n", b->name, b->radius, b->radius, b->radius);
    printf("FORMULA outside dist>%d empty\n", b->radius);
    printf("FORMULA ocean dip=%d\n", b->ocean_dip);
    printf("FORMULA surface=%s\n", b->surface);
    printf("FORMULA mc z==surface tile z>=surface-3 dirt else stone\n");
    printf("FORMULA slots floor=surface wall=stone\n");
}

static int formula_solar(void) {
    const struct body *e = find_body("earth");
    const struct body *m = find_body("moon");
    const struct body *a = find_body("mars");
    printf("FORMULA solar pages earth moon mars solar\n");
    printf("FORMULA solar bodies earth R=%d moon R=%d mars R=%d\n", e->radius, m->radius, a->radius);
    /* Centres stay at least the sum of the two radii apart, so the balls do not merge. */
    printf("FORMULA solar gap earth-moon %d\n", e->radius + m->radius);
    printf("FORMULA solar gap earth-mars %d\n", e->radius + a->radius);
    printf("FORMULA mc z==surface tile z>=surface-3 dirt else stone\n");
    return 0;
}

int main(int argc, char **argv) {
    const struct body *b;
    if (argc == 3 && !strcmp(argv[1], "formula") && !strcmp(argv[2], "solar"))
        return formula_solar();
    if (argc != 3 || strcmp(argv[1], "formula")) {
        fprintf(stderr, "usage: planet_ball_op formula earth|moon|mars|solar\n");
        return 2;
    }
    b = find_body(argv[2]);
    if (!b) {
        fprintf(stderr, "fail body\n");
        return 2;
    }
    print_body(b);
    return 0;
}
