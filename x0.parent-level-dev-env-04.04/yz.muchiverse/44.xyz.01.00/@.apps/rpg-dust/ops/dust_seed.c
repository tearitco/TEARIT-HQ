/* dust_seed - R1 of rpg-dust (BOOK 16.game/RPG-DUST-DESIGN.md sections 4 and 9): the seed rule and the first deterministic page. Pure: prints to stdout, writes nothing.
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
#include "dust_pdl.c"
static uint64_t mix(uint64_t x) { uint64_t z = x + 0x9E3779B97F4A7C15ULL; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static uint64_t child(uint64_t parent, const char *key) { uint64_t h = mix(parent); for (const unsigned char *p = (const unsigned char *)key; *p; p++) h = mix(h ^ *p); return mix(h ^ (uint64_t)strlen(key)); }
static int parse_u64(const char *s, uint64_t *v) { char *e; if (!*s || *s == '-' || *s == '+') return 0; errno = 0; unsigned long long x = strtoull(s, &e, 10); if (*e || errno) return 0; *v = x; return 1; }
static uint64_t fold(uint64_t seed, const char *path) { char b[512]; snprintf(b, sizeof b, "%s", path); for (char *t = strtok(b, "/"); t; t = strtok(NULL, "/")) seed = child(seed, t); return seed; }

#define MAXLV 64
typedef struct { char id[48], parent[48]; } Lv;
typedef struct { char key[112], lvl[48]; } Pm;
/* reads SEED / SCALE / PARAM rows (validity is dust_check's job; here only what the seed chain needs). Returns 0 and a message on an unreadable file, unknown level or missing seed. */
static int level_seed(const char *pdl, const char *level, uint64_t *out) {
    FILE *f = fopen(pdl, "r"); if (!f) { fprintf(stderr, "dust_seed: cannot read %s\n", pdl); return 0; }
    Lv lv[MAXLV]; Pm pm[MAXLV]; int nl = 0, np = 0, have = 0; uint64_t u = 0; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *t = ln; while (*t == ' ') t++; if (*t == '#') continue; char *fl[10]; int n = split_bar(t, fl, 10);
        if (!strcmp(fl[0], "SEED") && n >= 3 && parse_u64(fl[2], &u)) have = 1;
        else if (!strcmp(fl[0], "SCALE") && n >= 5 && nl < MAXLV) { cp(lv[nl].id, sizeof lv[0].id, fl[1]); cp(lv[nl].parent, sizeof lv[0].parent, fl[4]); nl++; }
        else if (!strcmp(fl[0], "PARAM") && n >= 3 && np < MAXLV) { snprintf(pm[np].key, sizeof pm[0].key, "%s=%s", fl[1], fl[2]); cp(pm[np].lvl, sizeof pm[0].lvl, n > 3 ? fl[3] : ""); np++; }
    }
    fclose(f); if (!have) { fprintf(stderr, "dust_seed: no valid SEED row in %s\n", pdl); return 0; }
    char chain[MAXLV][48]; int depth = 0, at = -1;
    for (int i = 0; i < nl; i++) if (!strcmp(lv[i].id, level)) at = i;
    while (at >= 0 && depth < MAXLV) { cp(chain[depth++], 48, lv[at].id); int up = -1; for (int i = 0; i < nl; i++) if (strcmp(lv[at].parent, "-") && !strcmp(lv[i].id, lv[at].parent)) up = i; at = up; }
    if (!depth) { fprintf(stderr, "dust_seed: no level '%s' in %s\n", level, pdl); return 0; }
    char root[48]; cp(root, sizeof root, chain[depth - 1]); uint64_t s = u;
    for (int d = depth - 1; d >= 0; d--) {
        s = child(s, chain[d]);
        int idx[MAXLV], k = 0; for (int i = 0; i < np; i++) if (!strcmp(pm[i].lvl[0] ? pm[i].lvl : root, chain[d])) idx[k++] = i;
        for (int a = 1; a < k; a++) for (int b = a; b > 0 && strcmp(pm[idx[b]].key, pm[idx[b - 1]].key) < 0; b--) { int x = idx[b]; idx[b] = idx[b - 1]; idx[b - 1] = x; }
        for (int a = 0; a < k; a++) s = child(s, pm[idx[a]].key);
    }
    *out = s; return 1;
}
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
