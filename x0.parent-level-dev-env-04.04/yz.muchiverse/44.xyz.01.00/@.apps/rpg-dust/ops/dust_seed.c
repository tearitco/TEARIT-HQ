/* dust_seed - R1 of rpg-dust (BOOK 16.game/RPG-DUST-DESIGN.md sections 4 and 9): the seed rule and the first deterministic page. Pure: prints to stdout, writes nothing.
 *   dust_seed mix   <u64>                         one splitmix64 step (known-answer anchor: mix(0) = e220a8397b1dcdaf, the published first output of splitmix64 seeded 0)
 *   dust_seed child <u64 seed> <key>              child seed = hash(parent seed, key); prints it as decimal
 *   dust_seed path  <u64 seed> <a/b/c>            folds child() over the '/'-separated keys, e.g. globe:3,4/region:0,1
 *   dust_seed page  <u64 seed> <path> [n]         the page at that address: an n x n (default 16, max 64) grid of heights 0..99 from value noise on the page seed, one row per line
 * The rule: the world is a pure function of (seed, address). No clock, no pid, no rand(), no hash of a pointer. Same inputs => byte-identical output on any machine (unsigned 64-bit arithmetic only).
 * Build: sh ops/build_dust_check.sh builds both ops. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
static uint64_t mix(uint64_t x) { uint64_t z = x + 0x9E3779B97F4A7C15ULL; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static uint64_t child(uint64_t parent, const char *key) { uint64_t h = mix(parent); for (const unsigned char *p = (const unsigned char *)key; *p; p++) h = mix(h ^ *p); return mix(h ^ (uint64_t)strlen(key)); }
static int parse_u64(const char *s, uint64_t *v) { char *e; if (!*s || *s == '-' || *s == '+') return 0; errno = 0; unsigned long long x = strtoull(s, &e, 10); if (*e || errno) return 0; *v = x; return 1; }
static uint64_t fold(uint64_t seed, const char *path) { char b[512]; snprintf(b, sizeof b, "%s", path); for (char *t = strtok(b, "/"); t; t = strtok(NULL, "/")) seed = child(seed, t); return seed; }
int main(int argc, char **argv) {
    uint64_t s;
    if (argc >= 3 && !strcmp(argv[1], "mix") && argc == 3 && parse_u64(argv[2], &s)) { printf("%016llx\n", (unsigned long long)mix(s)); return 0; }
    if (argc == 4 && !strcmp(argv[1], "child") && parse_u64(argv[2], &s)) { printf("%llu\n", (unsigned long long)child(s, argv[3])); return 0; }
    if (argc == 4 && !strcmp(argv[1], "path") && parse_u64(argv[2], &s)) { printf("%llu\n", (unsigned long long)fold(s, argv[3])); return 0; }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "page") && parse_u64(argv[2], &s)) {
        int n = argc == 5 ? atoi(argv[4]) : 16; if (n < 1 || n > 64) { fprintf(stderr, "dust_seed: n must be 1..64\n"); return 2; }
        uint64_t ps = fold(s, argv[3]); printf("PAGE | %llu | %s | %d\n", (unsigned long long)ps, argv[3], n);
        for (int y = 0; y < n; y++) { for (int x = 0; x < n; x++) printf("%s%02u", x ? " " : "", (unsigned)(mix(ps ^ mix((uint64_t)(unsigned)x << 32 | (unsigned)y)) % 100)); printf("\n"); }
        return 0;
    }
    fprintf(stderr, "usage: dust_seed mix <u64> | child <u64> <key> | path <u64> <a/b> | page <u64> <path> [n]\n"); return 2;
}
