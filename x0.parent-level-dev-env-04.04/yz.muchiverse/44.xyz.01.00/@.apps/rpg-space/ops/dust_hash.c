/* dust_hash.c - pure text-include shared by dust_seed and dust_gen (two consumers, no state): the seed rule. mix = splitmix64; child(parent,key); fold over a/b/c; level_seed(scale.pdl, level) = universe seed down the ladder with each PARAM folded in at its level (see dust_seed.c header). Needs stdio/stdlib/string/stdint/errno and dust_pdl.c. */
#ifndef DUST_HASH_C
#define DUST_HASH_C
#include <stdint.h>
#include <errno.h>
#include "dust_pdl.c"
static __attribute__((unused)) uint64_t mix(uint64_t x) { uint64_t z = x + 0x9E3779B97F4A7C15ULL; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static uint64_t child(uint64_t parent, const char *key) { uint64_t h = mix(parent); for (const unsigned char *p = (const unsigned char *)key; *p; p++) h = mix(h ^ *p); return mix(h ^ (uint64_t)strlen(key)); }
static int parse_u64(const char *s, uint64_t *v) { char *e; if (!*s || *s == '-' || *s == '+') return 0; errno = 0; unsigned long long x = strtoull(s, &e, 10); if (*e || errno) return 0; *v = x; return 1; }
static __attribute__((unused)) uint64_t fold(uint64_t seed, const char *path) { char b[512]; snprintf(b, sizeof b, "%s", path); for (char *t = strtok(b, "/"); t; t = strtok(NULL, "/")) seed = child(seed, t); return seed; }
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
#endif
