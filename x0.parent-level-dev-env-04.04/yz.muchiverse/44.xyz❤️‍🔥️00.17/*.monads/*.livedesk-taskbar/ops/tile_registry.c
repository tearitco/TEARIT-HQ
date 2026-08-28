/* tile_registry.c - real loader for the multi-tileset registry
 * (TILE-SYSTEM-DESIGN.md sec.4b.2 item 3, direct requirement from the
 * user's own real tile-picker mockup: a dropdown/list of NAMED,
 * complete tileset bundles - "001: Outside," "002: Inside," etc. - not
 * one fixed tileset). Reads
 * &.widgits/palettes/tilesets/tileset_registry.pdl, same real
 * SECTION|KEY|VALUE convention every other flat PDL in this house
 * uses. Standalone, testable - not yet wired into the real picker UI
 * (that's TILE-SYSTEM-DESIGN.md sec.6 step 5).
 *
 * Real registry row shape, one row per (tileset, field):
 *   TILESET | <key>.name | <display name>
 *   TILESET | <key>.<category> | <path, relative to tilesets/ dir>
 * where <category> is one of: a1 a2 a3 a4 a5 b c d e (lowercase,
 * matching RPG-CODE-INDEX-REF.md's real TILE_ID_A1..A5/B/C/D/E
 * categories). A tileset need not have every category present - only
 * A2 is populated today (the two real assets sourced this session),
 * others are legitimately absent until sourced. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 512
#define MAX_TILESETS 32
#define N_CATEGORIES 9 /* a1 a2 a3 a4 a5 b c d e */

static const char *CATEGORY_NAMES[N_CATEGORIES] = {"a1","a2","a3","a4","a5","b","c","d","e"};

typedef struct {
    char key[64];
    char name[128];
    char category_path[N_CATEGORIES][PATH_BUF]; /* empty string = not present */
} TilesetEntry;

static FILE *pdl_open(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }
    return f;
}

static TilesetEntry *find_or_add(TilesetEntry *arr, int *n, const char *key) {
    for (int i = 0; i < *n; i++) if (strcmp(arr[i].key, key) == 0) return &arr[i];
    if (*n >= MAX_TILESETS) return NULL;
    TilesetEntry *e = &arr[*n];
    memset(e, 0, sizeof(*e));
    snprintf(e->key, sizeof(e->key), "%s", key);
    (*n)++;
    return e;
}

/* Real, minimal SECTION|KEY|VALUE row parser - same shape as every
 * other real PDL reader in this codebase (read_footprint_tiles(),
 * read_grid_cell_px(), etc.), generalized to return the raw
 * key/value pair instead of matching one fixed key. */
static int parse_row(const char *line, char *out_key, size_t klen, char *out_val, size_t vlen) {
    if (strncmp(line, "TILESET", 7) != 0) return 0;
    const char *p = strchr(line, '|');
    if (!p) return 0;
    p++;
    while (*p == ' ') p++;
    const char *end = strchr(p, '|');
    if (!end) return 0;
    const char *label_end = end;
    while (label_end > p && label_end[-1] == ' ') label_end--;
    size_t n = (size_t)(label_end - p);
    if (n >= klen) n = klen - 1;
    memcpy(out_key, p, n); out_key[n] = '\0';

    const char *v = end + 1;
    while (*v == ' ') v++;
    size_t vn = strlen(v);
    while (vn > 0 && (v[vn-1] == '\n' || v[vn-1] == '\r' || v[vn-1] == ' ')) vn--;
    if (vn >= vlen) vn = vlen - 1;
    memcpy(out_val, v, vn); out_val[vn] = '\0';
    return 1;
}

static int category_index(const char *suffix) {
    for (int i = 0; i < N_CATEGORIES; i++) if (strcmp(suffix, CATEGORY_NAMES[i]) == 0) return i;
    return -1;
}

/* Loads the real registry into `out` (caller-provided array of
 * MAX_TILESETS entries), returns the real count found. */
static int load_tileset_registry(const char *registry_path, TilesetEntry *out) {
    FILE *f = pdl_open(registry_path);
    if (!f) return 0;
    char line[PATH_BUF];
    TilesetEntry entries[MAX_TILESETS];
    int n = 0;
    char rawkey[128], rawval[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (!parse_row(line, rawkey, sizeof(rawkey), rawval, sizeof(rawval))) continue;
        char *dot = strchr(rawkey, '.');
        if (!dot) continue;
        *dot = '\0';
        const char *tileset_key = rawkey;
        const char *field = dot + 1;
        TilesetEntry *e = find_or_add(entries, &n, tileset_key);
        if (!e) continue;
        if (strcmp(field, "name") == 0) {
            snprintf(e->name, sizeof(e->name), "%s", rawval);
        } else {
            int ci = category_index(field);
            if (ci >= 0) snprintf(e->category_path[ci], PATH_BUF, "%s", rawval);
        }
    }
    fclose(f);
    memcpy(out, entries, sizeof(TilesetEntry) * (size_t)n);
    return n;
}

/* ---------------- standalone structural test ---------------- */
static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail = 1; } } while (0)

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "tileset_registry.pdl";
    TilesetEntry entries[MAX_TILESETS];
    int n = load_tileset_registry(path, entries);
    printf("loaded %d tileset(s) from %s\n", n, path);
    for (int i = 0; i < n; i++) {
        printf("  [%d] key=%s name=%s\n", i, entries[i].key, entries[i].name);
        for (int c = 0; c < N_CATEGORIES; c++)
            if (entries[i].category_path[c][0])
                printf("      %s -> %s\n", CATEGORY_NAMES[c], entries[i].category_path[c]);
    }

    CHECK(n == 2, "real registry has exactly 2 real tileset entries today");
    int found_outside = 0, found_inside = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(entries[i].key, "outside") == 0) {
            found_outside = 1;
            CHECK(strcmp(entries[i].name, "World Map") == 0, "outside.name must be 'World Map'");
            CHECK(strcmp(entries[i].category_path[category_index("a2")], "rmmv/World_A2.png") == 0,
                  "outside.a2 path must match the real registry row");
            CHECK(entries[i].category_path[category_index("a1")][0] == '\0',
                  "outside.a1 must be empty (not sourced yet) - no fabricated entries");
        }
        if (strcmp(entries[i].key, "inside") == 0) {
            found_inside = 1;
            CHECK(strcmp(entries[i].name, "Inside") == 0, "inside.name must be 'Inside'");
        }
    }
    CHECK(found_outside, "outside tileset must be present");
    CHECK(found_inside, "inside tileset must be present");

    if (g_fail) { fprintf(stderr, "\nSOME CHECKS FAILED\n"); return 1; }
    printf("All structural checks PASS.\n");
    return 0;
}
