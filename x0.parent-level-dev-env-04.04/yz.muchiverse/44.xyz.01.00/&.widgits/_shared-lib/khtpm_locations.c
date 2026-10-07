/* khtpm_locations.c - resolve a SAVED path against the LIVE house root.
 *
 * Text-included canonical helper (same family as khtpm_inventory.c / khtpm_page_rows.c): pure, no environment policy.
 *
 * WHY (owner 2026-10-06, "why can't it use relative/dynamic dir path?"): a few pc-hq state files persist a path across
 * launches (open_book_page.txt pdl= is the desk page file the board follows). Written as an absolute path it goes stale
 * the moment the checkout is moved, renamed or copied (4ORK, another machine), and the reader then silently falls back to
 * a different map. TPMOS solved the same bug by deleting its checked-in location_kvp and generating it at launch from the
 * script's own folder (1.TPMOS/CHANGES.md "Hardcoded Path Removal"); every manager reads the root at startup and builds
 * the rest relative to it. Here: the launcher writes pieces/system/locations.pdl (open_pchq_board.sh) and a saved pointer
 * is stored house-RELATIVE and resolved on read with this function, which also heals an old absolute value.
 *
 * loc_resolve_saved(house, saved, out, n):
 *   - empty saved                -> out = "" , returns 0
 *   - relative                   -> house/saved
 *   - absolute and it exists     -> as is
 *   - absolute but missing (the checkout moved) -> house + the part from "/xyzfs/" on, if that exists
 *   - otherwise                  -> out = saved unchanged (the caller's fopen fails exactly as before)
 * Returns 1 if the resolved path is readable, else 0.
 *
 * loc_make_relative(house, abs, out, n): strips "<house>/" when abs starts with it (what writers store).
 *
 * loc_house_root(project_root, out, n): THE way to find the house root from a pc-hq / board-viewer project folder (replaces
 * the ten private copies of "read pieces/system/house_root.txt"). Order: (1) <project_root>/pieces/system/locations.pdl
 * "house_root" row, (2) <project_root>/pieces/system/house_root.txt (both generated at every launch), each accepted only if it
 * really is a house root (has _.monads/_.livedesk-taskbar), then (3) walk UP from project_root to the first such folder, which
 * needs no file at all. Returns 1 if found.
 *
 * loc_real_root(project_root, out, n): a session folder's real pc-hq root: real_project_root.txt may be absolute, relative to
 * the house, or stale after a move (rebased); falls back to project_root itself, exactly as every private copy did.
 * Prefix: loc_. */
#ifndef KHTPM_LOCATIONS_C
#define KHTPM_LOCATIONS_C

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LOC_UNUSED __attribute__((unused))

/* a house root has the taskbar monad in it */
static LOC_UNUSED int loc_is_house(const char *dir) {
    char p[4096];
    if (!dir || !dir[0]) return 0;
    snprintf(p, sizeof(p), "%s/_.monads/_.livedesk-taskbar", dir);
    return access(p, F_OK) == 0;
}

/* a stale absolute path -> house + the part from the first known house-level folder on, if that exists */
static LOC_UNUSED int loc_rebase(const char *house, const char *abs, char *out, size_t n) {
    static const char *marks[] = { "/xyzfs/", "/@.apps/", "/&.widgits/", "/#.desktop/", "/_.monads/", NULL };
    if (!house || !house[0]) return 0;
    for (int i = 0; marks[i]; i++) {
        const char *x = strstr(abs, marks[i]);
        if (x) {
            char rebased[4096];
            snprintf(rebased, sizeof(rebased), "%s%s", house, x);
            if (access(rebased, F_OK) == 0) { snprintf(out, n, "%s", rebased); return 1; }
        }
    }
    return 0;
}

/* first line of a one-line file into out (BOM and newline stripped); 0 if unreadable/empty */
static LOC_UNUSED int loc_read_line(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "r");
    out[0] = '\0';
    if (!f) return 0;
    if (fgets(out, (int)n, f)) {
        char *b = out;
        if ((unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) memmove(out, out + 3, strlen(out + 3) + 1);
        out[strcspn(out, "\r\n")] = '\0';
    }
    fclose(f);
    return out[0] != '\0';
}

static LOC_UNUSED int loc_house_root(const char *project_root, char *out, size_t n) {
    char p[4096], v[4096];
    out[0] = '\0';
    snprintf(p, sizeof(p), "%s/pieces/system/locations.pdl", project_root);
    {   /* "LOCATION | house_root | <path>" */
        FILE *f = fopen(p, "r"); char line[4608];
        while (f && fgets(line, sizeof(line), f)) {
            char *a = strstr(line, "house_root");
            char *bar = a ? strchr(a, '|') : NULL;
            if (!bar) continue;
            bar++; while (*bar == ' ' || *bar == '\t') bar++;
            bar[strcspn(bar, "\r\n")] = '\0';
            if (loc_is_house(bar)) { snprintf(out, n, "%s", bar); fclose(f); return 1; }
        }
        if (f) fclose(f);
    }
    snprintf(p, sizeof(p), "%s/pieces/system/house_root.txt", project_root);
    if (loc_read_line(p, v, sizeof(v)) && loc_is_house(v)) { snprintf(out, n, "%s", v); return 1; }
    snprintf(v, sizeof(v), "%s", project_root);   /* walk up */
    for (;;) {
        char *sl;
        if (loc_is_house(v)) { snprintf(out, n, "%s", v); return 1; }
        sl = strrchr(v, '/');
        if (!sl || sl == v) break;
        *sl = '\0';
    }
    return 0;
}

static LOC_UNUSED void loc_real_root(const char *project_root, char *out, size_t n) {
    char p[4096], v[4096], house[4096], r[4096];
    snprintf(out, n, "%s", project_root);
    snprintf(p, sizeof(p), "%s/pieces/system/real_project_root.txt", project_root);
    if (!loc_read_line(p, v, sizeof(v))) return;
    if (v[0] != '/' && !(v[0] && v[1] == ':')) {            /* relative: against the house */
        if (loc_house_root(project_root, house, sizeof(house))) snprintf(out, n, "%s/%s", house, v);
        else snprintf(out, n, "%s", v);
        return;
    }
    if (access(v, F_OK) == 0) { snprintf(out, n, "%s", v); return; }
    if (loc_house_root(project_root, house, sizeof(house)) && loc_rebase(house, v, r, sizeof(r))) { snprintf(out, n, "%s", r); return; }
    snprintf(out, n, "%s", v);
}

static LOC_UNUSED int loc_resolve_saved(const char *house, const char *saved, char *out, size_t n) {
    if (!saved || !saved[0]) { if (n) out[0] = '\0'; return 0; }
    if (saved[0] != '/') {
        snprintf(out, n, "%s/%s", house ? house : ".", saved);
        return access(out, R_OK) == 0;
    }
    if (access(saved, R_OK) == 0) { snprintf(out, n, "%s", saved); return 1; }
    {
        char rebased[4096];
        if (loc_rebase(house, saved, rebased, sizeof(rebased)) && access(rebased, R_OK) == 0) { snprintf(out, n, "%s", rebased); return 1; }
    }
    snprintf(out, n, "%s", saved);
    return 0;
}

static LOC_UNUSED void loc_make_relative(const char *house, const char *abs, char *out, size_t n) {
    size_t hl = house ? strlen(house) : 0;
    if (hl && !strncmp(abs, house, hl) && abs[hl] == '/') snprintf(out, n, "%s", abs + hl + 1);
    else snprintf(out, n, "%s", abs);
}

#endif
