/* pal_side - the editor's left side bar switch: pets <-> palettes (BOOK RPG-EDIT-HQ-PALETTES-SIDEBAR-RESEARCH-2026-11-02.md).
 * Usage: pal_side <house> <verb> [arg]
 *   open        show the palettes bar (hides the pets bar: they share the left slot), list view
 *   pets        show the pets bar (hides the palettes bar)
 *   none        hide both (the bars' "_" buttons)
 *   tiles <key> palettes bar, tile view of one category (key = a CATEGORY row of &.widgits/palettes/pallets.pdl)
 *   back        palettes bar, back to the category list
 *   list        only rewrite pal.txt (category list for the window)
 * Files (board/state/): sides.txt (side_left / side_pal: "1" = shown, EMPTY = hidden, other keys kept; same convention as layout-studio side_toggle) and pal.txt
 * (what the window binds: n_pcat, pcat_<i>_key/_label, pal_is_list, pal_is_tiles, pal_key, pal_title). Both written atomically. A state-key switch, not an href stack (owner OK 2026-11-02). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define MAXC 40
static char KEY[MAXC][64], LAB[MAXC][96]; static int nc;
static void trim(char *s) { size_t n = strlen(s); while (n && (s[n-1]==' '||s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]=='\t')) s[--n]=0; char *p=s; while (*p==' '||*p=='\t') p++; if (p!=s) memmove(s,p,strlen(p)+1); }
static void load_cats(const char *house) {
    char p[4200]; snprintf(p, sizeof p, "%s/&.widgits/palettes/pallets.pdl", house);
    FILE *f = fopen(p, "r"); if (!f) return; char ln[512];
    while (nc < MAXC && fgets(ln, sizeof ln, f)) {
        if (strncmp(ln, "CATEGORY", 8)) continue;
        char *fld[4]; int k = 0; char *c = ln;
        while (k < 4) { fld[k++] = c; char *b = strchr(c, '|'); if (!b) break; *b = 0; c = b + 1; }
        if (k < 3) continue;
        trim(fld[1]); trim(fld[2]);
        if (!fld[1][0] || !fld[2][0]) continue;
        snprintf(KEY[nc], sizeof KEY[0], "%s", fld[1]); snprintf(LAB[nc], sizeof LAB[0], "%s", fld[2]); nc++;
    }
    fclose(f);
}
/* set key=value ("" = hidden) in a kv file, keeping every other line */
static void kv_set(const char *path, const char *key, const char *val) {
    static char L[64][256]; int n = 0, at = -1; size_t kl = strlen(key); FILE *f = fopen(path, "r");
    if (f) { while (n < 64 && fgets(L[n], sizeof L[0], f)) { if (!strncmp(L[n], key, kl) && L[n][kl] == '=') at = n; n++; } fclose(f); }
    char row[256]; snprintf(row, sizeof row, "%s=%s\n", key, val);
    if (at >= 0) memcpy(L[at], row, strlen(row) + 1); else if (n < 64) { memcpy(L[n], row, strlen(row) + 1); n++; }
    char tmp[4300]; snprintf(tmp, sizeof tmp, "%s.tmp", path); f = fopen(tmp, "w"); if (!f) return;
    for (int i = 0; i < n; i++) { fputs(L[i], f); }
    fclose(f); rename(tmp, path);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: pal_side <house> open|pets|none|tiles <key>|back|list\n"); return 2; }
    const char *house = argv[1], *verb = argv[2], *arg = argc > 3 ? argv[3] : "";
    char sides[4200], pal[4200]; snprintf(sides, sizeof sides, "%s/@.apps/rpg-edit-hq/board/state/sides.txt", house); snprintf(pal, sizeof pal, "%s/@.apps/rpg-edit-hq/board/state/pal.txt", house);
    load_cats(house);
    int tiles = 0; char key[64] = "", title[96] = "";
    if (!strcmp(verb, "open")) { kv_set(sides, "side_pal", "1"); kv_set(sides, "side_left", ""); }
    else if (!strcmp(verb, "pets")) { kv_set(sides, "side_left", "1"); kv_set(sides, "side_pal", ""); }
    else if (!strcmp(verb, "none")) { kv_set(sides, "side_left", ""); kv_set(sides, "side_pal", ""); }
    else if (!strcmp(verb, "tiles")) {
        int at = -1; for (int i = 0; i < nc; i++) if (!strcmp(KEY[i], arg)) at = i;
        if (at < 0) { fprintf(stderr, "pal_side: no such category %s\n", arg); return 1; }
        tiles = 1; snprintf(key, sizeof key, "%s", KEY[at]); snprintf(title, sizeof title, "%s", LAB[at]);
        kv_set(sides, "side_pal", "1"); kv_set(sides, "side_left", "");
    } else if (strcmp(verb, "back") && strcmp(verb, "list")) { fprintf(stderr, "pal_side: unknown verb %s\n", verb); return 2; }
    char tmp[4300]; snprintf(tmp, sizeof tmp, "%s.tmp", pal); FILE *o = fopen(tmp, "w"); if (!o) return 1;
    fprintf(o, "n_pcat=%d\npal_is_list=%s\npal_is_tiles=%s\npal_key=%s\npal_title=%s\n", nc, tiles ? "" : "1", tiles ? "1" : "", key, title[0] ? title : "palettes");
    for (int i = 0; i < nc; i++) fprintf(o, "pcat_%d_key=%s\npcat_%d_label=%s\n", i, KEY[i], i, LAB[i]);
    fclose(o); rename(tmp, pal); return 0;
}
