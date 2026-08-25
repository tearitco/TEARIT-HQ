/* generate_egg - one verb, one binary, no shared headers.
 * The faucet's "generate" verb: picks a random species (weighted toward
 * common) from pieces/registry/emoji_pool/common_emoji.txt, mints a new
 * egg piece under pieces/world_01/map_lobby/, assigns it the next serial
 * number, and adds it to the calling owner piece's inventory.
 *
 * Usage: generate_egg.+x <owner_piece_id>
 *
 * This is the template for future verb-ops in this project (feed, hatch,
 * speak, ...): self-contained, own root resolution, own constants, plain
 * pipe-delimited/key=value text in and out, no shared header. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_SPECIES 128

typedef struct {
    char emoji[32];
    char id[64];
    char name[64];
    int rarity;
    char movement[16]; /* ground/flying/digging/aquatic - sets tick_pets.c's z-band for this species */
} Species;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static unsigned int random_seed(void) {
    unsigned int seed;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(&seed, sizeof(seed), 1, f);
        fclose(f);
        if (n == 1) return seed;
    }
    return (unsigned int)(time(NULL) ^ getpid());
}

static int load_species(Species *out, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/emoji_pool/common_emoji.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    int count = 0;
    while (count < max && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0'; /* CRLF-safe - a Windows-touched registry file can have \r\n endings */

        char *emoji = line;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *id = p1 + 1;
        char *p2 = strchr(id, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *name = p2 + 1;
        char *p3 = strchr(name, '|');
        if (!p3) continue;
        *p3 = '\0';
        char *rarity_str = p3 + 1;
        char *p4 = strchr(rarity_str, '|');
        const char *movement = "ground"; /* default for rows without the movement column (back-compat) */
        if (p4) { *p4 = '\0'; movement = p4 + 1; }
        int rarity = atoi(rarity_str);

        /* Registry fields are genuinely short (an emoji is a few UTF-8
         * bytes, ids/names are short words) despite gcc only being able
         * to statically prove the source no shorter than MAX_LINE; sizing
         * these destination fields to MAX_LINE would waste memory for no
         * real benefit, so the truncation warning is suppressed narrowly
         * here rather than widened away (same approach used in
         * mutaclsym/system/prisc+x.c for the same class of warning). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(out[count].emoji, sizeof(out[count].emoji), "%s", emoji);
        snprintf(out[count].id, sizeof(out[count].id), "%s", id);
        snprintf(out[count].name, sizeof(out[count].name), "%s", name);
        snprintf(out[count].movement, sizeof(out[count].movement), "%s", movement);
#pragma GCC diagnostic pop
        out[count].rarity = rarity;
        count++;
    }
    fclose(f);
    return count;
}

/* Weighted pick: rarity 1 = weight 10, rarity 2 = weight 3, rarity 3 = weight 1. */
static int weight_for_rarity(int rarity) {
    if (rarity <= 1) return 10;
    if (rarity == 2) return 3;
    return 1;
}

static const Species *pick_weighted(const Species *species, int count) {
    int total_weight = 0;
    for (int i = 0; i < count; i++) total_weight += weight_for_rarity(species[i].rarity);
    if (total_weight <= 0) return &species[0];

    int roll = rand() % total_weight;
    for (int i = 0; i < count; i++) {
        int w = weight_for_rarity(species[i].rarity);
        if (roll < w) return &species[i];
        roll -= w;
    }
    return &species[count - 1];
}

static int next_serial(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/serial_counter.txt", project_root);

    int serial = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d", &serial) != 1) serial = 0;
        fclose(f);
    }
    serial++;

    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", serial);
        fclose(f);
    }
    return serial;
}

static void append_to_inventory(const char *owner_id, const char *egg_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/inventory.txt", project_root, owner_id);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", egg_id);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <owner_piece_id>\n", argv[0]);
        return 1;
    }
    const char *owner_id = argv[1];
    resolve_root();
    srand(random_seed());

    Species species[MAX_SPECIES];
    int count = load_species(species, MAX_SPECIES);
    if (count == 0) {
        fprintf(stderr, "No species found in emoji_pool registry\n");
        return 1;
    }

    const Species *picked = pick_weighted(species, count);
    int serial = next_serial();

    char egg_id[64];
    snprintf(egg_id, sizeof(egg_id), "egg_%d", serial);

    char egg_dir[PATH_BUF];
    snprintf(egg_dir, sizeof(egg_dir), "%s/pieces/world_01/map_lobby/%s", project_root, egg_id);
#ifdef _WIN32
    mkdir(egg_dir);
#else
    mkdir(egg_dir, 0755);
#endif

    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", egg_dir);
    FILE *sf = fopen(state_path, "w");
    if (!sf) {
        fprintf(stderr, "Could not write %s\n", state_path);
        return 1;
    }
    fprintf(sf, "name=%s\n", egg_id);
    fprintf(sf, "type=egg\n");
    fprintf(sf, "species_id=%s\n", picked->id);
    fprintf(sf, "species_emoji=%s\n", picked->emoji);
    fprintf(sf, "species_name=%s\n", picked->name);
    fprintf(sf, "rarity=%d\n", picked->rarity);
    fprintf(sf, "movement=%s\n", picked->movement);
    fprintf(sf, "serial=%d\n", serial);
    fprintf(sf, "owner=%s\n", owner_id);
    fprintf(sf, "hatched=0\n");
    fclose(sf);

    char pdl_path[PATH_BUF + 32];
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", egg_dir);
    FILE *pf = fopen(pdl_path, "w");
    if (pf) {
        fprintf(pf, "SECTION      | KEY                | VALUE\n");
        fprintf(pf, "----------------------------------------\n\n");
        fprintf(pf, "META         | piece_id           | %s\n", egg_id);
        fprintf(pf, "META         | version            | 1.0\n\n");
        fprintf(pf, "STATE        | species_id           | %s\n", picked->id);
        fprintf(pf, "STATE        | serial               | %d\n\n", serial);
        fprintf(pf, "METHOD       | hatch                | ops/+x/hatch_egg.+x\n\n");
        fprintf(pf, "# Add one more METHOD line per new op as it's built\n");
        fprintf(pf, "# (feed, speak, ...), same pattern as mutaclsym's\n");
        fprintf(pf, "# hero/piece.pdl.\n");
        fclose(pf);
    }

    append_to_inventory(owner_id, egg_id);

    printf("%s\n", egg_id);
    return 0;
}
