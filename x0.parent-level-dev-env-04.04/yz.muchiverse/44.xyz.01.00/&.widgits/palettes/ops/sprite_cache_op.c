/* sprite_cache_op.c - drive the palette sprite cache (shared-lib/sprite_cache.c) from the command line: warm it, measure it, test it.
 * Same code the renderer's hq_sprite() runs, so a number printed here is the number a palette switch pays. No X11.
 * Doc: BOOK/03-pitfalls/palette-tile-load-lag.md
 *
 *   sprite_cache_op bench <palettes-X_state.txt>     decode every tile row (3rd TSV column = sprite dir); print cache hits / csv decodes / failures + ms
 *   sprite_cache_op warm <root dir>                  walk <root> for sprite.csv files and fill the cache (run it niced: nice -n 15 ionice -c3)
 *   sprite_cache_op frames <state.tsv> [budget_ms]   simulate the renderer's paints with the frame budget: frames needed, ms per frame, tiles per frame
 *   sprite_cache_op stat <sprite dir>                is there a valid cache file for this tile? (valid / stale / missing)
 *   sprite_cache_op purge                            delete the cache files (the cache is always safe to delete)
 * Env: LYFEDESK_SPRITE_CACHE=<dir>|off  (cache location / disable), LYFEDESK_SPRITE_BUDGET_MS (default 60, 0 = unlimited).
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <ftw.h>
#include "../../_shared-lib/sprite_cache.c"

static long g_n, g_hit, g_dec, g_fail;

static int load_one(const char *dir) {
    char csv[1200];
    struct stat st;
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    if (stat(csv, &st) != 0) { g_fail++; return 0; }
    int res = 0, src = 0;
    unsigned char *px = spc_load(dir, &st, &res, &src);
    g_n++;
    if (!px) { g_fail++; return 0; }
    free(px);
    if (src == 1) g_hit++; else g_dec++;
    return 1;
}

/* the sprite directory column of a state row: "<glyph>\t<label>\t<dir>" */
static int row_dir(char *line, char **dir) {
    char *t1 = strchr(line, '\t'); if (!t1) return 0;
    char *t2 = strchr(t1 + 1, '\t'); if (!t2) return 0;
    *dir = t2 + 1;
    (*dir)[strcspn(*dir, "\r\n")] = 0;
    return (*dir)[0] != 0;
}

static int walk_cb(const char *path, const struct stat *sb, int type, struct FTW *ftw) {
    (void)sb; (void)ftw;
    if (type != FTW_F) return 0;
    size_t n = strlen(path);
    const char *tail = "/sprite.csv";
    size_t tl = strlen(tail);
    if (n <= tl || strcmp(path + n - tl, tail) != 0) return 0;
    char dir[1200];
    snprintf(dir, sizeof(dir), "%.*s", (int)(n - tl), path);
    load_one(dir);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: sprite_cache_op bench|warm|frames|stat|purge ...\n"); return 2; }
    const char *verb = argv[1];

    if (strcmp(verb, "bench") == 0 && argc >= 3) {
        FILE *f = fopen(argv[2], "r");
        if (!f) { fprintf(stderr, "bench: cannot open %s\n", argv[2]); return 1; }
        char line[4096], *dir;
        double t0 = spc_now_ms();
        while (fgets(line, sizeof(line), f)) if (row_dir(line, &dir)) load_one(dir);
        fclose(f);
        double ms = spc_now_ms() - t0;
        printf("bench: tiles=%ld cache_hits=%ld csv_decodes=%ld failed=%ld total_ms=%.1f per_tile_ms=%.3f\n",
               g_n, g_hit, g_dec, g_fail, ms, g_n ? ms / g_n : 0.0);
        return 0;
    }

    if (strcmp(verb, "warm") == 0 && argc >= 3) {
        double t0 = spc_now_ms();
        nftw(argv[2], walk_cb, 16, FTW_PHYS);
        printf("warm: files=%ld already_cached=%ld decoded=%ld failed=%ld ms=%.0f cache=%s\n",
               g_n, g_hit, g_dec, g_fail, spc_now_ms() - t0, spc_cache_dir() ? spc_cache_dir() : "off");
        return 0;
    }

    if (strcmp(verb, "frames") == 0 && argc >= 3) {
        if (argc >= 4) setenv("LYFEDESK_SPRITE_BUDGET_MS", argv[3], 1);
        enum { MAXT = 4096 };
        static char dirs[MAXT][1200];
        static unsigned char *px[MAXT];
        int n = 0;
        FILE *f = fopen(argv[2], "r");
        if (!f) { fprintf(stderr, "frames: cannot open %s\n", argv[2]); return 1; }
        char line[4096], *d;
        while (n < MAXT && fgets(line, sizeof(line), f)) if (row_dir(line, &d)) snprintf(dirs[n++], sizeof(dirs[0]), "%s", d);
        fclose(f);
        int frames = 0, done = 0;
        double worst = 0, total = 0;
        while (done < n && frames < 100000) {       /* each pass = one paint: ask for every tile that is still missing */
            double t0 = spc_now_ms();
            int got = 0;
            spc_frame_begin();
            for (int i = 0; i < n; i++) {
                if (px[i]) continue;
                if (!spc_may_load()) continue;       /* deferred to the next paint */
                struct stat st; char csv[1300]; int res = 0, src = 0;
                snprintf(csv, sizeof(csv), "%s/sprite.csv", dirs[i]);
                double l0 = spc_now_ms();
                if (stat(csv, &st) == 0) px[i] = spc_load(dirs[i], &st, &res, &src);
                spc_loaded(spc_now_ms() - l0);
                if (!px[i]) px[i] = calloc(1, 1);      /* a tile that can never load must not stall the simulation */
                got++;
            }
            done += got;
            double ms = spc_now_ms() - t0;
            if (ms > worst) worst = ms;
            total += ms;
            frames++;
            if (!spc_frame_deferred()) break;        /* nothing was deferred: the renderer would not schedule another paint */
        }
        printf("frames: tiles=%d done=%d frames=%d worst_frame_ms=%.1f total_ms=%.1f budget_ms=%.0f\n",
               n, done, frames, worst, total, spc_budget_ms());
        return done == n ? 0 : 1;
    }

    if (strcmp(verb, "stat") == 0 && argc >= 3) {
        char cpath[1100], csv[1200];
        struct stat st;
        snprintf(csv, sizeof(csv), "%s/sprite.csv", argv[2]);
        if (stat(csv, &st) != 0) { printf("no sprite.csv in %s\n", argv[2]); return 1; }
        if (!spc_cache_path(argv[2], cpath, sizeof(cpath))) { printf("cache off\n"); return 1; }
        int res = 0;
        unsigned char *px = spc_read_cache(cpath, &st, &res);
        if (px) { free(px); printf("valid %s res=%d\n", cpath, res); return 0; }
        printf("%s %s\n", access(cpath, F_OK) == 0 ? "stale" : "missing", cpath);
        return 1;
    }

    if (strcmp(verb, "purge") == 0) {
        const char *cd = spc_cache_dir();
        if (!cd) { printf("purge: cache off\n"); return 0; }
        DIR *dd = opendir(cd);
        long removed = 0;
        struct dirent *de;
        while (dd && (de = readdir(dd))) {
            size_t n = strlen(de->d_name);
            if (n > 5 && (strcmp(de->d_name + n - 5, ".rgba") == 0 || strstr(de->d_name, ".rgba.tmp"))) {
                char p[1400]; snprintf(p, sizeof(p), "%s/%s", cd, de->d_name);
                if (unlink(p) == 0) removed++;
            }
        }
        if (dd) closedir(dd);
        printf("purge: removed %ld cache files from %s\n", removed, cd);
        return 0;
    }

    fprintf(stderr, "usage: sprite_cache_op bench|warm|frames|stat|purge ...\n");
    return 2;
}
