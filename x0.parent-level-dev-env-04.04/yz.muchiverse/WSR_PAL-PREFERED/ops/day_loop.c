/*
 * day_loop.c - the daily market tick (ROADMAP gap 1).
 *
 * WHY THIS FILE EXISTS
 *
 * PORT-FIDELITY.md gap 1 is the largest unfaithful divergence in wsr-pal, and
 * this closes it. In the original, the market moves on its own:
 *
 *     MSR-DEPRACATED/presets/schedule.txt   1_day  ./+x/day_loop.+x
 *     MSR-DEPRACATED/day_loop.c:25          system("./+x/analysis_loop.+x");
 *
 * So every game day, the original re-priced EVERY corporation from its
 * fundamentals, whether or not the human pressed End Turn. In wsr-pal the
 * price was only recalculated from scripts/tick_all, which fires on End Turn -
 * so a player who never pressed it watched a permanently frozen economy. That
 * is not a cosmetic difference: it changes what real time means to economic
 * time, and it is why the simulation had no macro pressure.
 *
 * This is the same shape as the original's day_loop, with the original's own
 * "Hello, DAy!" marker line kept because data/day.txt is part of the original's
 * on-disk shape and other tooling may read it. The stub's real payload - that
 * line and nothing else - is preserved alongside the call it always meant to
 * make. The call now actually happens, which is the entire point: the original
 * shipped day_loop.c calling an analysis_loop that did exist.
 *
 * WHAT IT DOES
 *
 * Walks projects/wsr-pal/pieces/corp_ (each corp_* directory) and runs
 * corp_update_price on each,
 * exactly as the original's analysis_loop did over
 * corporations/generated/*. Discovery is by directory scan, never a hardcoded
 * ticker list, for the same reason analysis_loop.c:288-318 uses opendir.
 *
 * PRICING STAYS WHERE IT IS
 *
 * This does NOT reimplement the price formula. corp_update_price.c is an exact
 * constant-for-constant port of analysis_loop.c:99-137 - same
 * book_value/shares_outstanding, same market_cap_multiplier, same leverage
 * branch, same 0.7/0.3 momentum blend. The formula is the original's and stays
 * the original's. This file only decides WHEN it runs.
 *
 * CAVEAT, NAMED NOT HIDDEN
 *
 * The clock is wall-clock driven; the rest of the economy is still End Turn
 * driven (ROADMAP 2.6). Prices now move on game days while balances still
 * settle on turns, so for one cycle the two cadences disagree. That seam is
 * tracked explicitly rather than papered over, because the alternative -
 * keeping the faithful daily repricing out until 2.6 lands - is a second,
 * larger unfaithfulness.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef MAX_LINE
#define MAX_LINE 1024
#endif

/* windows.h defines MAX_PATH as 260; this op builds path buffers from an
 * arbitrarily long PRISC_PROJECT_ROOT, same as every other op in this family. */
#ifdef _WIN32
#undef MAX_PATH
#endif

static char g_root[1024];

/* Mirrors how the rest of ops/ resolves the project root, so behaviour matches
 * the siblings rather than inventing a second convention. */
static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        snprintf(g_root, sizeof(g_root), "%s", env);
        return;
    }
    snprintf(g_root, sizeof(g_root), ".");
}

static int is_dir(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static int run_op(const char *op_name, const char *piece_id) {
    char bin[1400];
    char cmd[2048];
    int rc;

    snprintf(bin, sizeof(bin), "ops/+x/%s.+x", op_name);

    fflush(stdout);
#ifdef _WIN32
    /* cmd.exe does not treat ' as a quote character, so the path must be
     * double-quoted here or the op is looked up with a literal apostrophe in
     * its filename. Same reason the other Windows ops in this family do it. */
    rc = snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && %s %s 2>&1\"",
                  g_root, bin, piece_id);
#else
    rc = snprintf(cmd, sizeof(cmd), "cd '%s' && %s %s 2>&1", g_root, bin, piece_id);
#endif
    if (rc < 0 || (size_t)rc >= sizeof(cmd)) {
        fprintf(stderr, "day_loop: command too long for %s\n", op_name);
        return -1;
    }

    return system(cmd);
}

int main(void) {
    char pieces_root[1400];
    DIR *dir;
    struct dirent *ent;
    int priced = 0, failed = 0;

    resolve_root();
    snprintf(pieces_root, sizeof(pieces_root), "%s/projects/wsr-pal/pieces", g_root);

    /* The original's own marker line, preserved verbatim. It is how you can
     * tell a daily tick happened from the outside. */
    {
        char day_path[1400];
        FILE *fp;
        snprintf(day_path, sizeof(day_path), "%s/projects/wsr-pal/data/day.txt", g_root);
        fp = fopen(day_path, "a");
        if (fp) {
            fprintf(fp, "Hello, DAy!\n");
            fclose(fp);
        } else {
            fprintf(stderr, "day_loop: could not append to data/day.txt (non-fatal)\n");
        }
    }

    dir = opendir(pieces_root);
    if (!dir) {
        fprintf(stderr, "day_loop: cannot open %s - no market repricing today\n", pieces_root);
        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {
        char path[1600];
        if (strncmp(ent->d_name, "corp_", 5) != 0) continue;
        snprintf(path, sizeof(path), "%s/%s", pieces_root, ent->d_name);
        if (!is_dir(path)) continue;

        if (run_op("corp_update_price", ent->d_name) == 0) {
            priced++;
        } else {
            /* Reported, not swallowed. A day that repriced nothing is a bug
             * someone needs to see, and this line is how they see it. */
            fprintf(stderr, "day_loop: corp_update_price failed for %s\n", ent->d_name);
            failed++;
        }
    }
    closedir(dir);

    printf("day_loop: repriced %d corporation(s)", priced);
    if (failed) printf(", %d FAILED", failed);
    printf("\n");

    return failed ? 1 : 0;
}
