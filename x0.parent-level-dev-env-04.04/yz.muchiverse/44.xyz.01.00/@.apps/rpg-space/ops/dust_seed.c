/* dust_seed - R1 of rpg-space (BOOK 16.game/RPG-DUST-DESIGN.md sections 4 and 9): the seed rule and the first deterministic page. Pure: prints to stdout, writes nothing.
 *   dust_seed mix   <u64>                         one splitmix64 step (known-answer anchor: mix(0) = e220a8397b1dcdaf, the published first output of splitmix64 seeded 0)
 *   dust_seed child <u64 seed> <key>              child seed = hash(parent seed, key); prints it as decimal
 *   dust_seed path  <u64 seed> <a/b/c>            folds child() over the '/'-separated keys, e.g. globe:3,4/region:0,1
 *   dust_seed page  <u64 seed> <path> [n]         the page at that address: an n x n (default 16, max 64) grid of heights 0..99 from value noise on the page seed, one row per line
 *   dust_seed lseed <scale.pdl> <level>           the seed of a LEVEL: universe seed, then down the ladder from the root: s = child(s, level id), then for that level's PARAM rows (a PARAM belongs to the level in its 4th field, default the root), sorted by name: s = child(s, "name=value") with the value text as written. A parameter therefore moves its level and everything below it, never a level above.
 *   dust_seed lpage <scale.pdl> <level> <path> [n]   like page, from the level seed
 * The rule: the world is a pure function of (seed, address). No clock, no pid, no rand(), no hash of a pointer. Same inputs => byte-identical output on any machine (unsigned 64-bit arithmetic only).
 * Build: sh ops/build_dust_check.sh builds both ops. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include "dust_hash.c"

static void print_page(uint64_t ps, const char *label, int n) {
    printf("PAGE | %llu | %s | %d\n", (unsigned long long)ps, label, n);
    for (int y = 0; y < n; y++) { for (int x = 0; x < n; x++) printf("%s%02u", x ? " " : "", (unsigned)(mix(ps ^ mix((uint64_t)(unsigned)x << 32 | (unsigned)y)) % 100)); printf("\n"); }
}
int main(int argc, char **argv) {
    uint64_t s;
    if (argc >= 3 && !strcmp(argv[1], "mix") && argc == 3 && parse_u64(argv[2], &s)) { printf("%016llx\n", (unsigned long long)mix(s)); return 0; }
    if (argc == 4 && !strcmp(argv[1], "child") && parse_u64(argv[2], &s)) { printf("%llu\n", (unsigned long long)child(s, argv[3])); return 0; }
    if (argc == 4 && !strcmp(argv[1], "path") && parse_u64(argv[2], &s)) { printf("%llu\n", (unsigned long long)fold(s, argv[3])); return 0; }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "page") && parse_u64(argv[2], &s)) {
        int n = argc == 5 ? atoi(argv[4]) : 16; if (n < 1 || n > 64) { fprintf(stderr, "dust_seed: n must be 1..64\n"); return 2; }
        print_page(fold(s, argv[3]), argv[3], n);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "lseed")) { if (!level_seed(argv[2], argv[3], &s)) return 2; printf("%llu\n", (unsigned long long)s); return 0; }
    if ((argc == 5 || argc == 6) && !strcmp(argv[1], "lpage")) {
        int n = argc == 6 ? atoi(argv[5]) : 16; if (n < 1 || n > 64) { fprintf(stderr, "dust_seed: n must be 1..64\n"); return 2; }
        if (!level_seed(argv[2], argv[3], &s)) { return 2; }
        char lab[300]; snprintf(lab, sizeof lab, "%s@%s", argv[4], argv[3]); print_page(fold(s, argv[4]), lab, n); return 0;
    }
    fprintf(stderr, "usage: dust_seed mix <u64> | child <u64> <key> | path <u64> <a/b> | page <u64> <path> [n] | lseed <scale.pdl> <level> | lpage <scale.pdl> <level> <path> [n]\n"); return 2;
}
