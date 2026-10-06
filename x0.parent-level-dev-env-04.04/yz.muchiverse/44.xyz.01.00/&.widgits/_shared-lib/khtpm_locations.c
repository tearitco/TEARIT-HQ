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
 * loc_make_relative(house, abs, out, n): strips "<house>/" when abs starts with it (what writers store). Prefix: loc_. */
#ifndef KHTPM_LOCATIONS_C
#define KHTPM_LOCATIONS_C

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int loc_resolve_saved(const char *house, const char *saved, char *out, size_t n) {
    if (!saved || !saved[0]) { if (n) out[0] = '\0'; return 0; }
    if (saved[0] != '/') {
        snprintf(out, n, "%s/%s", house ? house : ".", saved);
        return access(out, R_OK) == 0;
    }
    if (access(saved, R_OK) == 0) { snprintf(out, n, "%s", saved); return 1; }
    {
        const char *x = strstr(saved, "/xyzfs/");
        if (x && house && house[0]) {
            char rebased[4096];
            snprintf(rebased, sizeof(rebased), "%s%s", house, x);
            if (access(rebased, R_OK) == 0) { snprintf(out, n, "%s", rebased); return 1; }
        }
    }
    snprintf(out, n, "%s", saved);
    return 0;
}

static void loc_make_relative(const char *house, const char *abs, char *out, size_t n) {
    size_t hl = house ? strlen(house) : 0;
    if (hl && !strncmp(abs, house, hl) && abs[hl] == '/') snprintf(out, n, "%s", abs + hl + 1);
    else snprintf(out, n, "%s", abs);
}

#endif
