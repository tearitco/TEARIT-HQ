/* sprite_cache.c - decoded-sprite disk cache + per-frame load budget for the palette tile grids.
 * Text-included (pure logic, no X11, 2+ consumers: khtpm_draw_core.c's hq_sprite() and ops/sprite_cache_op.c).
 * Doc: BOOK/03-pitfalls/palette-tile-load-lag.md  (measured cause, design, proof, limits)
 *
 * Why: a palette tile is a text file <dir>/sprite.csv ("# resolution=48" then one "r,g,b,a" line per pixel, ~31 KB for a 48x48 tile).
 * Decoding one costs ~0.7-1.8 ms (fgets + sscanf per pixel), and a palette switch decodes every visible tile in the paint,
 * so the window froze for 0.2-0.5 s on a quiet machine (measured, XO/palette_lag/parse_cost.c) and longer under load.
 *
 * Two independent fixes live here:
 *   1. DISK CACHE. The first decode writes <cache dir>/<hash of dir path>.rgba (24-byte header + raw RGBA); every later load reads that
 *      one block back (~9 KB for 48x48) instead of parsing ~1000 text lines. The header records the source csv's size and mtime, so an
 *      edited sprite.csv is re-decoded. The cache lives OUTSIDE the repo ($HOME/.cache/lyfedesk-sprites, override LYFEDESK_SPRITE_CACHE,
 *      value "off" disables it) because the csv files are tracked in git. Writes are tmp + rename; a torn or foreign file fails the size
 *      check and is ignored (never trusted), so the cache can be deleted at any time.
 *   2. FRAME BUDGET (progressive load). A caller that can be repainted later asks spc_may_load() before a cold decode. Once this frame has
 *      spent LYFEDESK_SPRITE_BUDGET_MS (default 60, 0 = unlimited) and done at least one load, further cold tiles are DEFERRED: the caller
 *      draws nothing for them and spc_frame_deferred() tells the renderer to repaint again. The window stays responsive and the grid fills
 *      in over a few frames. At least one load per frame always happens, so progress is guaranteed.
 * Tiles that are not drawn (scrolled away, replaced by another palette) are never asked for, so a stale load is cancelled for free.
 */
#ifndef SPRITE_CACHE_C
#define SPRITE_CACHE_C
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SPC_RES_MAX 256

typedef struct {
    char magic[4];       /* "SPR1" */
    uint32_t res;
    int64_t csv_mtime;
    int64_t csv_size;
} SpcHdr;                /* 24 bytes, no padding: written/read raw (a local cache, never moved between machines) */

static inline double spc_now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* cache directory, created on first use; NULL when disabled or not creatable */
static const char *spc_cache_dir(void) {
    static char dir[1024];
    static int state = 0;               /* 0 unknown, 1 ready, -1 off */
    if (state) return state > 0 ? dir : NULL;
    const char *e = getenv("LYFEDESK_SPRITE_CACHE");
    if (e && strcmp(e, "off") == 0) { state = -1; return NULL; }
    if (e && e[0]) snprintf(dir, sizeof(dir), "%s", e);
    else {
        const char *h = getenv("HOME");
        if (!h || !h[0]) { state = -1; return NULL; }
        char base[1024];
        snprintf(base, sizeof(base), "%s/.cache", h);
        mkdir(base, 0755);
        snprintf(dir, sizeof(dir), "%s/.cache/lyfedesk-sprites", h);
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { state = -1; return NULL; }
    state = 1;
    return dir;
}

static uint64_t spc_hash(const char *s) {   /* FNV-1a 64 */
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    return h;
}

static int spc_cache_path(const char *dir, char *out, size_t n) {
    const char *cd = spc_cache_dir();
    if (!cd) return 0;
    snprintf(out, n, "%s/%016llx.rgba", cd, (unsigned long long)spc_hash(dir));
    return 1;
}

/* the decoded pixels from the cache file when it is present, well-formed and matches the csv (size + mtime in *cst), else NULL */
static unsigned char *spc_read_cache(const char *cpath, const struct stat *cst, int *res_out) {
    FILE *f = fopen(cpath, "rb");
    if (!f) return NULL;
    SpcHdr h;
    unsigned char *px = NULL;
    if (fread(&h, sizeof(h), 1, f) == 1 && memcmp(h.magic, "SPR1", 4) == 0 &&
        h.res >= 1 && h.res <= SPC_RES_MAX &&
        h.csv_mtime == (int64_t)cst->st_mtime && h.csv_size == (int64_t)cst->st_size) {
        size_t n = (size_t)h.res * h.res * 4;
        px = malloc(n);
        if (px) {
            /* the file must hold exactly the pixels the header promises: a torn write is shorter, a foreign file is longer */
            int c = 0;
            if (fread(px, 1, n, f) != n || fgetc(f) != EOF) c = 1;
            if (c) { free(px); px = NULL; }
            else *res_out = (int)h.res;
        }
    }
    fclose(f);
    return px;
}

/* the original text decode (khtpm_draw_core.c hq_sprite, unchanged semantics): NULL on any problem */
static unsigned char *spc_decode_csv(const char *csv_path, int *res_out) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return NULL;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0 || res > SPC_RES_MAX) { fclose(f); return NULL; }
    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return NULL; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int rr, gg, bb, aa;
        if (sscanf(line, "%d,%d,%d,%d", &rr, &gg, &bb, &aa) == 4) {
            pixels[count * 4 + 0] = (unsigned char)rr;
            pixels[count * 4 + 1] = (unsigned char)gg;
            pixels[count * 4 + 2] = (unsigned char)bb;
            pixels[count * 4 + 3] = (unsigned char)aa;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return NULL; }
    *res_out = res;
    return pixels;
}

static void spc_write_cache(const char *cpath, const struct stat *cst, const unsigned char *px, int res) {
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s.tmp%d", cpath, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    SpcHdr h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, "SPR1", 4);
    h.res = (uint32_t)res;
    h.csv_mtime = (int64_t)cst->st_mtime;
    h.csv_size = (int64_t)cst->st_size;
    size_t n = (size_t)res * res * 4;
    int ok = fwrite(&h, sizeof(h), 1, f) == 1 && fwrite(px, 1, n, f) == n;
    if (fclose(f) != 0) ok = 0;
    if (ok && rename(tmp, cpath) == 0) return;
    unlink(tmp);
}

/* Load the decoded RGBA of <dir>/sprite.csv (caller already stat'ed the csv into *cst). Returns a malloc'd res*res*4 buffer or NULL.
 * *src: 1 = read from the disk cache, 2 = decoded from the csv (and the cache written). */
static unsigned char *spc_load(const char *dir, const struct stat *cst, int *res_out, int *src) {
    char cpath[1100], csv_path[1300];
    int have_cache = spc_cache_path(dir, cpath, sizeof(cpath));
    if (have_cache) {
        unsigned char *px = spc_read_cache(cpath, cst, res_out);
        if (px) { if (src) *src = 1; return px; }
    }
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", dir);
    unsigned char *px = spc_decode_csv(csv_path, res_out);
    if (!px) return NULL;
    if (have_cache) spc_write_cache(cpath, cst, px, *res_out);
    if (src) *src = 2;
    return px;
}

/* ---- per-frame budget (progressive load) ---- */
static double g_spc_frame_spent_ms = 0;
static int    g_spc_frame_loads = 0;
static int    g_spc_deferred = 0;

static double spc_budget_ms(void) {
    static double b = -2;
    if (b == -2) {
        const char *e = getenv("LYFEDESK_SPRITE_BUDGET_MS");
        b = (e && e[0]) ? atof(e) : 60.0;
        if (b < 0) b = 0;
    }
    return b;
}

static void spc_frame_begin(void) { g_spc_frame_spent_ms = 0; g_spc_frame_loads = 0; g_spc_deferred = 0; }

/* 1 = go ahead and decode; 0 = this frame's budget is spent, defer (and remember that a repaint is owed) */
static int spc_may_load(void) {
    double b = spc_budget_ms();
    if (b <= 0 || g_spc_frame_loads < 1 || g_spc_frame_spent_ms < b) return 1;
    g_spc_deferred = 1;
    return 0;
}

static void spc_loaded(double ms) { g_spc_frame_spent_ms += ms; g_spc_frame_loads++; }
static int spc_frame_deferred(void) { return g_spc_deferred; }

#endif
