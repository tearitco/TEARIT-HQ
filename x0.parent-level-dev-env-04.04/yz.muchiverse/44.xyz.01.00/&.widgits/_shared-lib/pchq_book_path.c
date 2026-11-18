/* pchq_book_path.c - ONE place that finds a book's folder (owner 2026-11-18: "we dont use symlinks for anything in code").
 *
 * Text-included canonical helper (same family as khtpm_locations.c: pure, no environment policy). It needs <stdio.h> <string.h> <stdlib.h> <unistd.h>
 * <dirent.h> <sys/stat.h> and khtpm_locations.c is NOT required: the caller passes the house root (loc_house_root() of the project root gives it).
 *
 * WHY: an editor instance (the desktop editor, a phone's own editor) keeps its OWN data root (<root>/pieces/system/maps/<book>/...) but must open
 * every book of the house, which lives in the app that owns it. Until 2026-11-18 a launcher made one symlink per book inside the instance, and
 * a book outside the "@.apps/<app>/<app>-pchq" layout (tsots, doom, mineclonia_sample in piececraft-hq) never appeared. Links are gone: every
 * reader asks this resolver for the book folder, and the books stay where they are (one authority per fact, no copy, no link).
 *
 * bp_book_dir(root, house, book, out, n)  ->  returns 1 when the book was found, 0 otherwise; out is ALWAYS a path:
 *   1. <root>/pieces/system/maps/<book>              a real folder of the instance itself (the editor's own book, a book made in the editor)
 *   2. <house>/@.apps/<book>/<book>-pchq/pieces/system/maps/<book>     the app named like the book (the old launcher's rule)
 *   3. <house>/@.apps/<app>/<app>-pchq/pieces/system/maps/<book>      any other app that carries that book (needs game.pdl)
 *   4. <house>/@.apps/piececraft-hq/pieces/system/maps/<book>         the generic pc-hq book store (tsots, doom, mineclonia_sample, ...), needs game.pdl
 *   5. every `ROOT | <folder relative to the house>` row of <house>/#.desktop/book_roots.pdl   (optional, one row per extra store), needs game.pdl
 *   not found: out = the step-1 path, so the caller's fopen fails exactly as it did before this helper existed.
 * A book name with '/', a leading '.', an empty name or 64+ characters is refused (returns 0, out = step-1 path): no path trick can leave the maps folder.
 * Results are cached per (root, book) and re-checked with one stat() of the cached folder, so a per-frame caller costs one stat.
 *
 * bp_list_books(root, house, names, max)  ->  count; names[i] are the book ids found by steps 1-5 (a folder with game.pdl, except "default"),
 * de-duplicated with the earlier step winning, unsorted (the caller sorts, as the projector already does).
 *
 * Why not symlinks / a copy: a link is an invisible second path to the same fact and breaks on a move; a copy is a second authority.
 * Design-choice entry: DESIGN-CHOICE-TEST-AND-DECISION-LOG.md (D89). Harness: case book_roots. */
#ifndef PCHQ_BOOK_PATH_C
#define PCHQ_BOOK_PATH_C
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BP_UNUSED __attribute__((unused))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"   /* a path cut by a long name simply does not exist: the caller gets "not found" */
#define BP_PATH 4352
#define BP_NAME 64
#define BP_MAX_ROOTS 16

static BP_UNUSED int bp_name_ok(const char *b) {
    size_t n = b ? strlen(b) : 0;
    return n > 0 && n < BP_NAME && b[0] != '.' && !strchr(b, '/');
}
static BP_UNUSED int bp_isdir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static BP_UNUSED int bp_game(const char *dir) { char g[BP_PATH]; snprintf(g, sizeof g, "%s/game.pdl", dir); return access(g, R_OK) == 0; }

/* the extra store folders of book_roots.pdl, absolute, in file order; returns how many */
static BP_UNUSED int bp_extra_roots(const char *house, char roots[][BP_PATH]) {
    char f[BP_PATH], ln[BP_PATH]; int n = 0;
    snprintf(f, sizeof f, "%s/#.desktop/book_roots.pdl", house);
    FILE *fp = fopen(f, "r"); if (!fp) return 0;
    while (n < BP_MAX_ROOTS && fgets(ln, sizeof ln, fp)) {
        char *p = ln; while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "ROOT", 4)) continue;
        p += 4; while (*p == ' ' || *p == '\t') p++;
        if (*p != '|') continue;
        p++; while (*p == ' ' || *p == '\t') p++;
        p[strcspn(p, "\r\n")] = 0;
        size_t l = strlen(p); while (l > 0 && (p[l - 1] == ' ' || p[l - 1] == '\t')) p[--l] = 0;
        if (!*p || strstr(p, "..") || p[0] == '/') continue;     /* house-relative only, no climbing out */
        snprintf(roots[n++], BP_PATH, "%s/%s", house, p);
    }
    fclose(fp); return n;
}

/* the app store folders in lookup order: the piececraft-hq store, then each app's own pc-hq data (callers add the named app first) */
static BP_UNUSED int bp_book_dir(const char *root, const char *house, const char *book, char *out, size_t n) {
    static struct { char root[BP_PATH], book[BP_NAME], dir[BP_PATH]; } cache[8]; static int cnext;
    char own[BP_PATH], cand[BP_PATH];
    snprintf(own, sizeof own, "%s/pieces/system/maps/%s", root, bp_name_ok(book) ? book : "_");
    if (!bp_name_ok(book)) { snprintf(out, n, "%s", own); return 0; }
    if (bp_isdir(own)) { snprintf(out, n, "%s", own); return 1; }
    if (house && house[0]) {
        for (int ci = 0; ci < 8; ci++) if (cache[ci].dir[0] && !strcmp(cache[ci].root, root) && !strcmp(cache[ci].book, book) && bp_isdir(cache[ci].dir)) { snprintf(out, n, "%s", cache[ci].dir); return 1; }
        snprintf(cand, sizeof cand, "%s/@.apps/%s/%s-pchq/pieces/system/maps/%s", house, book, book, book);
        if (bp_isdir(cand)) goto hit;
        char apps[BP_PATH]; snprintf(apps, sizeof apps, "%s/@.apps", house);
        DIR *d = opendir(apps);
        if (d) { struct dirent *e;
            while ((e = readdir(d))) { if (e->d_name[0] == '.') continue;
                snprintf(cand, sizeof cand, "%s/%s/%s-pchq/pieces/system/maps/%s", apps, e->d_name, e->d_name, book);
                if (bp_isdir(cand) && bp_game(cand)) { closedir(d); goto hit; } }
            closedir(d); }
        snprintf(cand, sizeof cand, "%s/@.apps/piececraft-hq/pieces/system/maps/%s", house, book);
        if (bp_isdir(cand) && bp_game(cand)) goto hit;
        static char roots[BP_MAX_ROOTS][BP_PATH]; int nr = bp_extra_roots(house, roots);
        for (int i = 0; i < nr; i++) { snprintf(cand, sizeof cand, "%s/%s", roots[i], book); if (bp_isdir(cand) && bp_game(cand)) goto hit; }
    }
    snprintf(out, n, "%s", own); return 0;
hit:
    { int ci = cnext++ % 8; snprintf(cache[ci].root, BP_PATH, "%s", root); snprintf(cache[ci].book, BP_NAME, "%s", book); snprintf(cache[ci].dir, BP_PATH, "%s", cand); }
    snprintf(out, n, "%s", cand); return 1;
}

static BP_UNUSED int bp_add(char names[][BP_NAME], int cnt, int max, const char *nm) {
    if (cnt >= max || !bp_name_ok(nm) || !strcmp(nm, "default")) return cnt;
    for (int i = 0; i < cnt; i++) if (!strcmp(names[i], nm)) return cnt;
    snprintf(names[cnt], BP_NAME, "%s", nm); return cnt + 1;
}
static BP_UNUSED int bp_scan(const char *dir, char names[][BP_NAME], int cnt, int max) {
    DIR *d = opendir(dir); if (!d) return cnt; struct dirent *e;
    while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; char b[BP_PATH]; snprintf(b, sizeof b, "%s/%s", dir, e->d_name); if (bp_isdir(b) && bp_game(b)) cnt = bp_add(names, cnt, max, e->d_name); }
    closedir(d); return cnt;
}
static BP_UNUSED int bp_list_books(const char *root, const char *house, char names[][BP_NAME], int max) {
    int cnt = 0; char dir[BP_PATH];
    snprintf(dir, sizeof dir, "%s/pieces/system/maps", root); cnt = bp_scan(dir, names, cnt, max);
    if (!house || !house[0]) return cnt;
    char apps[BP_PATH]; snprintf(apps, sizeof apps, "%s/@.apps", house);
    DIR *d = opendir(apps);
    if (d) { struct dirent *e; while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; snprintf(dir, sizeof dir, "%s/%s/%s-pchq/pieces/system/maps", apps, e->d_name, e->d_name); cnt = bp_scan(dir, names, cnt, max); } closedir(d); }
    snprintf(dir, sizeof dir, "%s/@.apps/piececraft-hq/pieces/system/maps", house); cnt = bp_scan(dir, names, cnt, max);
    static char roots[BP_MAX_ROOTS][BP_PATH]; int nr = bp_extra_roots(house, roots);
    for (int i = 0; i < nr; i++) cnt = bp_scan(roots[i], names, cnt, max);
    return cnt;
}
#ifdef LOC_UNUSED
/* bp_book_dir_auto(root, book, out, n): the same, finding the house root itself (needs khtpm_locations.c included BEFORE this file). The house root is cached per root. */
static BP_UNUSED int bp_book_dir_auto(const char *root, const char *book, char *out, size_t n) {
    static char hr[BP_PATH], hr_for[BP_PATH];
    if (strcmp(hr_for, root) != 0) { hr[0] = 0; loc_house_root(root, hr, sizeof hr); snprintf(hr_for, sizeof hr_for, "%s", root); }
    return bp_book_dir(root, hr, book, out, n);
}
/* bp_dir(root, book): the same as a string expression (a ring of 4 buffers, so up to four calls may sit in one statement). Not thread safe. */
static BP_UNUSED const char *bp_dir(const char *root, const char *book) {
    static char ring[4][BP_PATH]; static int k; char *o = ring[k++ & 3];
    bp_book_dir_auto(root, book, o, BP_PATH); return o;
}
#endif
#pragma GCC diagnostic pop
#endif
