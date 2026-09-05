/* khtpm_draw_core.c — real, shared generic draw layer (2026-08-16,
 * khtpm-merge-how2.md Stage 5 §5d, real generic-binary-merge follow-up
 * to css_layout_pass()). Ported verbatim from db-hq's own
 * khtpm_hq_render.c `alloc_pixel()`/`xft_color()`/`font_for()`/
 * `draw_elem()`/`render_tree()` (the most mature, most recently
 * bugfixed copy — includes the real 2026-08-16 dark-theme fallback
 * fixes) — this is the piece Stage 3 never extracted: css_layout_pass()
 * only computes geometry, every app's own PIXEL drawing was still
 * separate, bespoke C, confirmed via a direct grep finding zero shared
 * `draw_elem()` anywhere before this file.
 *
 * REAL, LOAD-BEARING INCLUDE-ORDER REQUIREMENT, same class as
 * khtpm_relay_utils.c (NOT like khtpm_render_core.c, which is
 * deliberately X11-free and included first): this file needs
 * `Display *dpy`, `int screen`, `Colormap cmap`, `GC gc`, `Pixmap buf`,
 * `XftDraw *xftdraw_buf`, `int g_focus_nav`, and a real `scaled(int)`
 * function ALL already declared by the consumer BEFORE this
 * `#include` — include it AFTER X11/Xft headers and after those
 * globals are declared, not near the top like khtpm_render_core.c.
 * This is a genuinely legitimate #include-shared case per this doc's
 * own "HOUSE STANDARD" decision rule: pure, stateless, per-frame
 * hot-path logic that needs direct access to the caller's own live X11
 * connection/drawable every single frame — real ops/fork-exec doesn't
 * fit here, this isn't a discrete one-shot action. */

/* Colour caches. cmap never changes after startup (DefaultColormap), so a
 * pixel/XftColor allocated for a given spec stays valid for the process
 * lifetime. Without this, a palette redraw (256+ tiles, each draw_elem()
 * calling alloc_pixel()/xft_color() several times) was ~1000+ synchronous
 * X round-trips = ~0.5s wall time per frame - the "palettes are
 * incredibly slow" report. A palette frame uses well under 32 distinct
 * colours. */
#define KH_COLOR_CACHE_N 64
static unsigned long alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    static struct { char spec[24]; unsigned long px; } cache[KH_COLOR_CACHE_N];
    static int ncache = 0;
    for (int i = 0; i < ncache; i++)
        if (strcmp(cache[i].spec, spec) == 0) return cache[i].px;
    XColor c;
    unsigned long px = BlackPixel(dpy, screen);
    if (spec[0] == '#') {
        if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) px = c.pixel;
    } else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) {
        px = c.pixel;
    }
    if (ncache < KH_COLOR_CACHE_N && strlen(spec) < sizeof(cache[0].spec)) {
        snprintf(cache[ncache].spec, sizeof(cache[0].spec), "%s", spec);
        cache[ncache].px = px;
        ncache++;
    }
    return px;
}

/* NOT cached: callers XftColorFree() the result, so a shared/cached
 * XftColor would be use-after-free. XftColorAllocValue is one round-trip
 * (vs alloc_pixel's two) so the payoff is smaller anyway. */
static XftColor xft_color(const char *spec) {
    XftColor xc;
    XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b;
        sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc);
    return xc;
}

/* REAL FIX 2026-09-04, direct live report ("white text doesn't have
 * black background... not readable") - a real, DIFFERENT bug from the
 * badge-blackout fix earlier this same day. Badges always sit on their
 * own fixed dark #141414 chip (that fix correctly stopped computing
 * contrast against the wrong surface, the element's own bg). Plain
 * dock-cell LABEL text has no such chip at all - it draws straight
 * onto the dock window's real fill, which for a dock window specifically
 * is g_theme_bg (redraw()'s own `window_is_dock() ? g_theme_bg :
 * "#1c1c1c"` fill), a user-changeable theme color, NOT the CSS's
 * hardcoded dark `window.dock-header { background: #1c1c1c }` the
 * .dock-cell text color (#cccccc) was tuned against. A light/tan theme
 * (this house's current default) makes light-on-light text unreadable.
 * This time the surface being tested really is the one the text sits
 * on - not the same mistake. Scoped to window_is_dock() only; every
 * other window's plain text keeps its existing, unrelated color. */
static int kh_hex_luma(const char *hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return 255; /* honest default: assume light, matches this house's own light-theme default */
    unsigned int r, g, b;
    if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) != 3) return 255;
    return (int)(r * 299 + g * 587 + b * 114) / 1000;
}

/* Real font cache, same pattern as chat-hai/db-hq's own real
 * measure_text_px() fix (khtpm-merge-how2.md §3.2). Caller must NOT
 * XftFontClose() the returned font - shared, cached handle. */
static XftFont *font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = scaled(st->has_font_size ? st->font_size : 12);
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");

    static char cached_spec[128] = "";
    static XftFont *cached_font = NULL;
    if (cached_font && strcmp(cached_spec, spec) == 0) return cached_font;
    if (cached_font) XftFontClose(dpy, cached_font);
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=10");
    cached_font = f;
    snprintf(cached_spec, sizeof(cached_spec), "%s", spec);
    return f;
}

/* ---------- REAL sprite textures (2026-08-25, Stage 2 palettes port -
 * ported verbatim from khtpm_hq_render.c's own hq_sprite()/
 * hq_blit_sprite(), itself from khtpm_strip_parser.c's tab_sprite()/
 * blit_tab_sprite() - the house's one real emoji->image mechanism.
 * Placed in the SHARED draw layer (not per-mode) since it's pure,
 * stateless, X11-drawing code with zero db-hq/palettes-specific
 * dependencies - any future mode gets it for free. */
#define HQ_SPRITE_PX_MAX 64
typedef struct {
    char path[512];
    unsigned char *rgba;
    int res;
    time_t mtime;
    long last_used; /* real LRU tick - see g_hq_sprite_tick's own comment */
} HqSprite;
/* REAL FIX 2026-08-28, part 2 (live report, direct: "caching isn't
 * having an effect this go round" - correctly caught a SECOND real
 * bug this same session, not the same one already fixed) - a fixed-
 * size cache with NO eviction, no matter how large, eventually
 * overflows once a real user browses enough DISTINCT sprite paths in
 * one session (confirmed live: World's A/B/C tabs alone already total
 * 32+256+256=544 unique paths, past even the 512 this file was just
 * bumped to). Bumping the number again would only move the same
 * failure further out, not fix it - real LRU eviction (least-recently-
 * used slot reused when the cache is full) is the actual durable fix,
 * ported as the standard fixed-capacity-cache pattern, not invented
 * from scratch. g_hq_sprite_tick is a simple monotonic counter, bumped
 * on every real access (hit or fresh load) - the slot with the
 * smallest stamp is the one nothing has touched longest. */
#define HQ_SPRITE_CACHE_N 512
static HqSprite g_hq_sprite_cache[HQ_SPRITE_CACHE_N];
static long g_hq_sprite_tick = 0;

static HqSprite *hq_sprite(const char *dir) {
    if (!dir || !dir[0]) return NULL;
    char pth[512];
    snprintf(pth, sizeof(pth), "%s", dir);
    size_t pl = strlen(pth);
    while (pl > 0 && (pth[pl - 1] == '\n' || pth[pl - 1] == '\r' || pth[pl - 1] == ' ' || pth[pl - 1] == '\t'))
        pth[--pl] = 0;
    if (!pth[0]) return NULL;
    char csv_path[520];
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", pth);
    struct stat st;
    time_t mt = 0;
    if (stat(csv_path, &st) == 0) mt = st.st_mtime;
    for (int i = 0; i < HQ_SPRITE_CACHE_N; i++) {
        if (g_hq_sprite_cache[i].rgba && strcmp(g_hq_sprite_cache[i].path, pth) == 0) {
            if (mt != g_hq_sprite_cache[i].mtime) {
                free(g_hq_sprite_cache[i].rgba);
                memset(&g_hq_sprite_cache[i], 0, sizeof(HqSprite));
                break;
            }
            g_hq_sprite_cache[i].last_used = ++g_hq_sprite_tick;
            return &g_hq_sprite_cache[i];
        }
    }
    FILE *f = fopen(csv_path, "r");
    if (!f) return NULL;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0 || res > 256) { fclose(f); return NULL; }
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
    int slot = -1;
    for (int i = 0; i < HQ_SPRITE_CACHE_N; i++) {
        if (!g_hq_sprite_cache[i].rgba) { slot = i; break; }
    }
    if (slot < 0) {
        /* Cache full - real LRU eviction (2026-08-28), not a silent
         * drop. Find the slot with the OLDEST last_used stamp (the one
         * nothing has touched longest) and reuse it - a real user
         * cycling through more distinct sprites than the cache can
         * hold at once should see the LEAST recently viewed ones
         * re-decoded on return, not a fixed hard wall past which
         * sprites just stop appearing. */
        long oldest = g_hq_sprite_cache[0].last_used;
        slot = 0;
        for (int i = 1; i < HQ_SPRITE_CACHE_N; i++) {
            if (g_hq_sprite_cache[i].last_used < oldest) { oldest = g_hq_sprite_cache[i].last_used; slot = i; }
        }
        free(g_hq_sprite_cache[slot].rgba);
    }
    snprintf(g_hq_sprite_cache[slot].path, sizeof(g_hq_sprite_cache[slot].path), "%s", pth);
    g_hq_sprite_cache[slot].rgba = pixels;
    g_hq_sprite_cache[slot].res = res;
    g_hq_sprite_cache[slot].mtime = mt;
    g_hq_sprite_cache[slot].last_used = ++g_hq_sprite_tick;
    return &g_hq_sprite_cache[slot];
}

static void hq_blit_sprite(HqSprite *sp, int x0, int y0, int px, unsigned long bg_pixel) {
    Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    unsigned long rmask = vis->red_mask, gmask = vis->green_mask, bmask = vis->blue_mask;
    int rshift = 0, gshift = 0, bshift = 0;
    while (rmask && !(rmask & (1UL << rshift))) rshift++;
    while (gmask && !(gmask & (1UL << gshift))) gshift++;
    while (bmask && !(bmask & (1UL << bshift))) bshift++;
    unsigned long br = (bg_pixel >> rshift) & 0xff;
    unsigned long bg2 = (bg_pixel >> gshift) & 0xff;
    unsigned long bb = (bg_pixel >> bshift) & 0xff;
    int res = sp->res;
    unsigned char *bufpx = calloc((size_t)px * px, 4);
    if (!bufpx) return;
    for (int y = 0; y < px; y++) {
        int sy = (y * res) / px;
        if (sy >= res) sy = res - 1;
        for (int x = 0; x < px; x++) {
            int sx = (x * res) / px;
            if (sx >= res) sx = res - 1;
            const unsigned char *pix = &sp->rgba[(sy * res + sx) * 4];
            int a = pix[3];
            int r = (pix[0] * a + (int)br * (255 - a)) / 255;
            int g = (pix[1] * a + (int)bg2 * (255 - a)) / 255;
            int b = (pix[2] * a + (int)bb * (255 - a)) / 255;
            unsigned long word = ((unsigned long)r << rshift) | ((unsigned long)g << gshift) | ((unsigned long)b << bshift);
            bufpx[(y * px + x) * 4 + 0] = (unsigned char)(word & 0xff);
            bufpx[(y * px + x) * 4 + 1] = (unsigned char)((word >> 8) & 0xff);
            bufpx[(y * px + x) * 4 + 2] = (unsigned char)((word >> 16) & 0xff);
            bufpx[(y * px + x) * 4 + 3] = (unsigned char)((word >> 24) & 0xff);
        }
    }
    XImage *img = XCreateImage(dpy, vis, depth, ZPixmap, 0, (char *)bufpx, px, px, 32, 0);
    if (img) {
        img->byte_order = LSBFirst;
        XPutImage(dpy, buf, gc, img, 0, 0, x0, y0, px, px);
        XDestroyImage(img);
    } else {
        free(bufpx);
    }
}

/* Real, generic, CSS-driven single-element draw: background fill,
 * border, wraith-alpha-standard focus ring, nav-index badge
 * ("[>]1." / "[ ]1.", bracket holds ONLY the state glyph, number is a
 * separate suffix - verified against the real reference,
 * wraith_parser_alpha.c ~line 2221-2224/2283), and label text via the
 * real font/color cache above. Includes the real, documented
 * `active`-state fallback for `.tab`/`.item` tags (dashboard.css's own
 * `.tab.active`/`.data-item.active` rules are real, confirmed DEAD CSS
 * - `active` is a C struct bool, never pushed into e->classes[] as a
 * matchable string - these hardcoded fallbacks are the REAL active-
 * state colors until that's fixed for real, a separate, not-yet-done
 * follow-up). */
/* ---------- REAL, ported 2026-08-25 (live report: bookmarks' paths
 * carry real emoji dir names, e.g. this house's own folder names, and
 * rendered as tofu boxes - "open-hai has an implementation for this we
 * can steal") - verbatim port of khtpm_open_hai_render.c's own inline
 * text+emoji mixed-run renderer (that file's own header: "chtpm uses a
 * function to convert emoji to .csv first use that"). Pre-generated
 * 16x16 RGBA voxel CSVs (emoji_gen_atlas.+x + emoji_xtract.+x, same
 * house-standard pipeline khtpm_hq_render.c's own sprite tiles use, but
 * a separate lower-res registry meant for INLINE-with-text use, not
 * grid tiles) are loaded once and blitted between Xft-drawn text runs.
 * Placed in the shared draw layer (not open-hai-only) since ANY label
 * text in ANY consumer of this file can legitimately contain emoji
 * (this house's own directory names prove that, not a hypothetical) -
 * draw_elem()'s own plain-text branch below now calls draw_text_emoji()
 * instead of a bare XftDrawStringUtf8(). Zero-effect for a consumer
 * that never calls khtpm_load_emoji_tiles() (g_emoji_n stays 0,
 * build_segs() finds no matches, falls straight through to one plain
 * text run - same bytes drawn as before this port). ---------- */
#define EMOJI_TILE 16
#define EMOJI_ADV 18
typedef struct {
    unsigned int cp;
    unsigned char px[EMOJI_TILE * EMOJI_TILE * 4];
} EmojiTile;
static EmojiTile g_emoji_tiles[512];
static int g_emoji_n = 0;
static int g_px_rshift = 0, g_px_gshift = 0, g_px_bshift = 0;
static int g_emoji_tiles_loaded = 0;

static int khtpm_utf8_decode(const unsigned char *s, unsigned int *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { *cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) { *cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) { *cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); return 4; }
    *cp = 0xFFFD; return 1;
}

static int khtpm_mask_shift(unsigned long m) {
    int s = 0;
    while (m && !(m & 1UL)) { m >>= 1; s++; }
    return s;
}

/* Call once, after dpy/screen open - house_root is used to derive the
 * SAME registry path open-hai's own AUDIT_EMOJI_REL points at (a
 * house-wide asset cache, not open-hai-private data). */
static void khtpm_load_emoji_tiles(const char *house_root) {
    if (g_emoji_tiles_loaded) return;
    g_emoji_tiles_loaded = 1;
    Visual *v = DefaultVisual(dpy, screen);
    g_px_rshift = khtpm_mask_shift(v->red_mask);
    g_px_gshift = khtpm_mask_shift(v->green_mask);
    g_px_bshift = khtpm_mask_shift(v->blue_mask);
    char emoji_dir[PATH_BUF];
    snprintf(emoji_dir, sizeof(emoji_dir), "%s/&.widgits/open-hai/pieces/registry/emoji_assets", house_root);
    DIR *d = opendir(emoji_dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_emoji_n < 512) {
        if (e->d_name[0] == '.') continue;
        char csv[PATH_BUF];
        snprintf(csv, sizeof(csv), "%s/%s/voxels_16.csv", emoji_dir, e->d_name);
        FILE *f = fopen(csv, "r");
        if (!f) continue;
        unsigned int cp = (unsigned int)strtoul(e->d_name, NULL, 16);
        EmojiTile *t = &g_emoji_tiles[g_emoji_n];
        memset(t, 0, sizeof(*t));
        t->cp = cp;
        char line[64];
        int npix = 0;
        while (fgets(line, sizeof(line), f) && npix < EMOJI_TILE * EMOJI_TILE) {
            if (line[0] == '#' || line[0] == '\n') continue;
            if (line[0] == 'r' && line[1] == ',') continue;
            int r, g, b, a;
            if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
                size_t o = (size_t)npix * 4;
                t->px[o] = (unsigned char)r; t->px[o + 1] = (unsigned char)g;
                t->px[o + 2] = (unsigned char)b; t->px[o + 3] = (unsigned char)a;
                npix++;
            }
        }
        fclose(f);
        if (npix == EMOJI_TILE * EMOJI_TILE) g_emoji_n++;
    }
    closedir(d);
}

static const EmojiTile *khtpm_emoji_for_cp(unsigned int cp) {
    for (int i = 0; i < g_emoji_n; i++) if (g_emoji_tiles[i].cp == cp) return &g_emoji_tiles[i];
    return NULL;
}

static void khtpm_blit_emoji_tile(const EmojiTile *t, int x, int ytop) {
    Visual *v = DefaultVisual(dpy, screen);
    for (int yy = 0; yy < EMOJI_TILE; yy++) {
        for (int xx = 0; xx < EMOJI_TILE; xx++) {
            size_t o = ((size_t)yy * EMOJI_TILE + xx) * 4;
            if (t->px[o + 3] < 128) continue;
            unsigned long px = ((((unsigned long)t->px[o]) << g_px_rshift) & v->red_mask) |
                               ((((unsigned long)t->px[o + 1]) << g_px_gshift) & v->green_mask) |
                               ((((unsigned long)t->px[o + 2]) << g_px_bshift) & v->blue_mask);
            XSetForeground(dpy, gc, px);
            XDrawPoint(dpy, buf, gc, x + xx, ytop + yy);
        }
    }
}

typedef struct { const char *s; int len; int is_emoji; const EmojiTile *tile; } KhtpmDrawSeg;
static int khtpm_build_segs(const char *text, KhtpmDrawSeg *segs, int maxsegs) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *run = p;
    while (*p) {
        unsigned int cp; int clen = khtpm_utf8_decode(p, &cp);
        int zero_w = (cp == 0xFE0F || cp == 0x200D || cp == 0x200C || cp == 0x200B);
        const EmojiTile *t = zero_w ? NULL : khtpm_emoji_for_cp(cp);
        if (t || zero_w) {
            if (p > run && n < maxsegs) { segs[n].s = (const char *)run; segs[n].len = (int)(p - run); segs[n].is_emoji = 0; segs[n].tile = NULL; n++; }
            if (t && n < maxsegs) { segs[n].s = (const char *)p; segs[n].len = clen; segs[n].is_emoji = 1; segs[n].tile = t; n++; }
            p += clen; run = p;
        } else {
            p += clen;
        }
    }
    if (p > run && n < maxsegs) { segs[n].s = (const char *)run; segs[n].len = (int)(p - run); segs[n].is_emoji = 0; segs[n].tile = NULL; n++; }
    return n;
}

/* Drop-in replacement for a plain XftDrawStringUtf8() label draw -
 * same signature shape (font/color/x/baseline-y/text). */
static void draw_text_emoji(XftFont *f, XftColor *c, int x, int y, const char *s) {
    if (!s || !*s) return;
    KhtpmDrawSeg segs[512];
    int n = khtpm_build_segs(s, segs, 512);
    int sx = x;
    int tile_top = y - 13;
    for (int i = 0; i < n; i++) {
        if (segs[i].is_emoji) {
            khtpm_blit_emoji_tile(segs[i].tile, sx, tile_top);
            sx += EMOJI_ADV;
        } else {
            XftDrawStringUtf8(xftdraw_buf, c, f, sx, y, (const FcChar8 *)segs[i].s, segs[i].len);
            XGlyphInfo gi;
            XftTextExtentsUtf8(dpy, f, (const FcChar8 *)segs[i].s, segs[i].len, &gi);
            sx += gi.xOff;
        }
    }
}

/* REMOVED 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md-adjacent
 * finding - live report: "nav numbers get blacked out... this also
 * happens in tb and other windows"). badge_contrast_color()/badge_
 * focus_color() used to live here, picking a color by the ELEMENT'S
 * OWN background luminance (dark text for a light element bg). That
 * made sense in 2026-08-25 when they were written - at the time, a
 * badge could be drawn directly on an element's own bg with no
 * backing chip. Some time after that, drawing a fixed dark #141414
 * backing chip behind EVERY badge (sprite tiles, swatch tiles, and -
 * unconditionally, no gate at all - every other element via this same
 * function's own general branch) became universal, but nobody removed
 * the now-obsolete contrast-vs-own-bg logic when that happened - so
 * any element anywhere in the house with a light/bright own bg-color
 * (a themed dock cell, a light row, a bright swatch) got a DARK badge
 * color chosen to contrast against ITS OWN bg, rendered on the
 * ALWAYS-dark chip instead, and read as invisible. Confirmed by direct
 * grep this was their only 2 call sites, both replaced with the same
 * fixed colors every other case already used correctly - see the
 * badge-color block below. */

/* Generic XML/HTML entity decode for on-screen labels. chtpm attribute
 * values must stay escaped in the file (`&amp;` `&gt;` for well-formed
 * tags). apply_attr() already decodes label= at parse; this runs in the
 * shared draw path so a raw leftover `&amp;`/`&gt;` never paints as the
 * escape sequence. No-op if the string is already decoded. Not
 * network-specific. */
static void khtpm_decode_label_entities(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '&') {
            if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; continue; }
            if (strncmp(r, "&apos;", 6) == 0) { *w++ = '\''; r += 6; continue; }
            if (strncmp(r, "&nbsp;", 6) == 0) { *w++ = ' '; r += 6; continue; }
            if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; continue; }
            if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; continue; }
            if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; continue; }
            if (r[1] == '#') {
                const char *q = r + 2;
                int hex = 0;
                if (*q == 'x' || *q == 'X') { hex = 1; q++; }
                if (*q) {
                    char *end = NULL;
                    unsigned long cp = strtoul(q, &end, hex ? 16 : 10);
                    if (end && end > q && *end == ';') {
                        if (cp == 39 || cp == 8216 || cp == 8217) *w++ = '\'';
                        else if (cp == 34 || cp == 8220 || cp == 8221) *w++ = '"';
                        else if (cp == 160) *w++ = ' ';
                        else if (cp >= 32 && cp < 128) *w++ = (char)cp;
                        r = end + 1;
                        continue;
                    }
                }
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

/* REAL, NEW 2026-09-03 (direct live report: co-lab-hai's own agent
 * messages, often long, drew with the plain single-line "..." clip
 * instead of wrapping like chat-hai's own rows appear to - root cause
 * found by reading this file directly: the real word-wrap path a few
 * lines down was gated to `tag=="cli_io"` specifically (2026-09-01's
 * own "build word-wrap into the generic cli_io FIRST" - a real,
 * deliberate first slice, not a permanent restriction). A plain
 * <text> row in a scrolllist is always exactly one ROW_H tall by
 * default (scroll_row_span()'s own real default, khtpm_core_render.c),
 * so it never had the height headroom the wrap path already checks
 * for even once the tag restriction is lifted - see that function's
 * own 2026-09-03 fix for the layout-side half of this.
 *
 * This helper counts how many real wrapped lines a label needs at a
 * given available width - same greedy word-wrap algorithm draw_elem()
 * itself uses below, factored out so the LAYOUT side (scroll_row_span())
 * can ask "how tall a box does this text really need" before draw_elem()
 * ever runs, without duplicating the wrap logic or re-deriving it by
 * guesswork. Real, generic, not co-lab-hai-specific - any future
 * scrolllist consumer with long text gets this for free. */
static int wrap_line_count(XftFont *font, const char *text, int avail_w) {
    if (!font || !text || !text[0] || avail_w <= 0) return 1;
    int n = 0;
    const char *p = text;
    while (*p) {
        int i = 0, last_good_space = -1;
        for (;;) {
            char c = p[i];
            if (c == '\0') break;
            if (c == ' ') last_good_space = i;
            XGlyphInfo lw;
            XftTextExtentsUtf8(dpy, font, (const FcChar8 *)p, i + 1, &lw);
            if (lw.width > avail_w) break;
            i++;
        }
        int cut = i;
        int has_more_after = (p[i] != '\0');
        if (has_more_after && last_good_space >= 0) cut = last_good_space;
        if (cut == 0 && p[i] != '\0') cut = 1;
        n++;
        p += cut;
        while (*p == ' ') p++;
    }
    return n < 1 ? 1 : n;
}

/* <canvas sprite="<raw-RGBA-path>"/> - a live pixel framebuffer element.
 * Reads dims from "<path>.receipt.txt" (overlay_w= / overlay_h=), reads
 * the raw BGRA/RGBA bytes, and XPutImage-s them at native size clipped
 * to the element rect. XImage + read buffer cached per (path,w,h). The
 * board-viewer 2D/3D view for piececraft-hq is the first consumer; some
 * other process keeps the .raw fresh (this only ever reads it). */
static void kh_draw_canvas(Elem *e) {
    static char  c_path[512];
    static int   c_w, c_h;
    static XImage *c_img;
    static unsigned char *c_buf;
    if (!e->sprite[0]) {
        XSetForeground(dpy, gc, alloc_pixel("#101014"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, (unsigned)e->w, (unsigned)e->h);
        return;
    }
    /* the board-viewer convention is <base>.raw next to <base>.receipt.txt
     * (sibling, NOT <base>.raw.receipt.txt) - strip a trailing ".raw"
     * before appending, falling back to the naive append for any other
     * producer that really does use "<path>.receipt.txt". */
    char rc[512];
    { const char *dot = strrchr(e->sprite, '.');
      if (dot && strcmp(dot, ".raw") == 0)
          snprintf(rc, sizeof(rc), "%.*s.receipt.txt", (int)(dot - e->sprite), e->sprite);
      else
          snprintf(rc, sizeof(rc), "%s.receipt.txt", e->sprite);
    }
    int w = 0, h = 0;
    FILE *rf = fopen(rc, "r");
    if (rf) {
        char l[128];
        while (fgets(l, sizeof(l), rf)) {
            if (!strncmp(l, "overlay_w=", 10)) w = atoi(l + 10);
            else if (!strncmp(l, "overlay_h=", 10)) h = atoi(l + 10);
        }
        fclose(rf);
    }
    /* REAL FIX 2026-09-04, live report ("render for pc-hq is sometimes
     * blanking, maybe every 10 seconds - the old one didn't do this") -
     * the receipt/raw pair is written by a SEPARATE producer process
     * (bv_render_3d.c) with no atomicity guarantee visible from this
     * side; a read landing mid-write can see w/h==0 for one tick, or
     * (below) a short/partial .raw read. This used to unconditionally
     * XFillRectangle a dark background FIRST, every call, then only
     * overpaint it with the real frame if that tick's read succeeded -
     * so a single bad tick (out of ~30/sec) blanked the canvas for one
     * visible frame, repeating however often the producer's own write
     * cycle collides with our read. Real fix: only fill-and-clear when
     * there is no previous good frame cached yet (c_img == NULL, i.e.
     * genuinely nothing to show); once a real frame has been decoded
     * once, a bad tick simply leaves the LAST good frame's XImage
     * on screen (below) instead of blanking - no visible flicker, same
     * spirit as the old implementation apparently already had. */
    if (w <= 0 || h <= 0) {
        if (!c_img) {
            XSetForeground(dpy, gc, alloc_pixel("#101014"));
            XFillRectangle(dpy, buf, gc, e->x, e->y, (unsigned)e->w, (unsigned)e->h);
        }
        return;
    }
    if (strcmp(c_path, e->sprite) != 0 || c_w != w || c_h != h || !c_img) {
        snprintf(c_path, sizeof(c_path), "%s", e->sprite);
        c_w = w; c_h = h;
        free(c_buf); c_buf = (unsigned char *)malloc((size_t)w * h * 4);
        if (c_img) { XDestroyImage(c_img); c_img = NULL; }
        char *data = (char *)malloc((size_t)w * h * 4);
        c_img = data ? XCreateImage(dpy, DefaultVisual(dpy, screen),
                                    (unsigned)DefaultDepth(dpy, screen), ZPixmap, 0,
                                    data, (unsigned)w, (unsigned)h, 32, 0)
                     : NULL;
    }
    if (!c_img || !c_buf) return;
    FILE *of = fopen(e->sprite, "rb");
    if (of) {
        size_t need = (size_t)w * h * 4, got = fread(c_buf, 1, need, of);
        fclose(of);
        if (got == need) {
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    size_t o = ((size_t)y * w + x) * 4;
                    unsigned long px = ((unsigned long)c_buf[o]   << 16) |
                                       ((unsigned long)c_buf[o+1] <<  8) |
                                        (unsigned long)c_buf[o+2];
                    XPutPixel(c_img, x, y, px);
                }
        }
    }
    int bw = w < e->w ? w : e->w;
    int bh = h < e->h ? h : e->h;
    XPutImage(dpy, buf, gc, c_img, 0, 0, e->x, e->y, (unsigned)bw, (unsigned)bh);
}

static void draw_elem(Elem *e, int hover_id_hash) {
    (void)hover_id_hash;
    /* REAL FIX 2026-08-29 (EVENTS-HQ-RENDER-UNIFICATION-PLAN.md's own
     * open "ghosting" regression, root-caused: evhq_zero_subtree()
     * zeros an Elem's w/h to hide a whole subtree when a view mode
     * switches away from it, but this function's label-drawing branch
     * below (`if (!drew_sprite && e->label[0])`) never checked w/h at
     * all - a 0x0 XFillRectangle/XDrawRectangle is a real no-op, but
     * text was drawn regardless, so "hidden" titles like Scripting
     * mode's "Trigger"/"Commands" block-title labels kept bleeding
     * through as faint ghosts over Scratch/Blueprints content. This
     * guard was believed to already exist here (see the plan doc's own
     * now-corrected note) but never actually did - fixing it here,
     * once, fixes every mode that relies on zeroing a subtree to hide
     * it, not just events-hq. */
    if (e->w <= 0 || e->h <= 0) return;
    if (strcmp(e->tag, "canvas") == 0) { kh_draw_canvas(e); return; }
    /* REAL, NEW 2026-09-04, direct live request ("can we add grey and
     * brown to swatch colors... that shouldn't be hardcoded, should
     * be from layout/module") - a real, generic `bg="${var}"`
     * attribute (parsed in apply_attr(), khtpm_core_render.c), letting
     * a manager/projector publish a genuinely arbitrary hex color per
     * element - not just a fixed set of compile-time CSS classes.
     * Applied once, here, at the top of the ONE shared draw function
     * every element already funnels through, so it overrides whatever
     * `css_compute_style()` (35 call sites, one per layout mode) may
     * have already computed for this element's style.bg_color - no
     * changes needed at any of those call sites. First real consumer:
     * taskbar-settings-pal.xhtpm's own swatch grid, generated by a
     * <repeat> over a manager-owned color list instead of a fixed set
     * of hardcoded <item class="sw-<name>"> tags + matching compile-
     * time CSS rules (khtpm-house-standards's own "static template +
     * real manager, no hardcoding" rule, applied to color DATA, not
     * just item count). */
    if (e->bg[0]) {
        snprintf(e->style.bg_color, sizeof(e->style.bg_color), "%s", e->bg);
        e->style.has_bg_color = 1;
    }
    /* checkerboard is a PNG-transparency cue for sprite tiles only. A
     * class="swatch" item is by definition a plain filled colour square
     * (taskbar-settings-pal.xhtpm's colour picker, etc.) - it never
     * wants the transparency cue, in any window. Only "pal-tile" (real
     * sprite art) gets it. 2026-09-04: previously gated on the
     * (permanently unreachable) swatch-picker window-class flag -
     * root cause of the settings colour picker showing checkerboard
     * instead of solid colours, since its real live template never set
     * that class. */
    int is_grid_tile_bg = elem_has_class(e, "pal-tile");
    if (e->style.has_bg_color && is_grid_tile_bg) {
        /* PNG-style transparency checkerboard behind a sprite cell -
         * reads as "this holds an image with alpha", instead of the flat
         * tile colour showing through every transparent edge.
         * PERF: built ONCE as a 12x12 tile Pixmap and stamped with a
         * FillTiled GC (one XFillRectangle per cell, zero XAllocColor
         * round-trips) - the earlier per-6px-square alloc_pixel()+fill
         * loop was ~13k colour round-trips per palette redraw. */
        static Pixmap ck_tile = 0;
        if (!ck_tile) {
            enum { CK_P = 12, CK_SQ = 6 };
            ck_tile = XCreatePixmap(dpy, DefaultRootWindow(dpy), CK_P, CK_P, (unsigned)DefaultDepth(dpy, screen));
            XSetFillStyle(dpy, gc, FillSolid);
            XSetForeground(dpy, gc, alloc_pixel("#ffffff"));
            XFillRectangle(dpy, ck_tile, gc, 0, 0, CK_P, CK_P);
            XSetForeground(dpy, gc, alloc_pixel("#b8b8b8"));
            XFillRectangle(dpy, ck_tile, gc, 0, 0, CK_SQ, CK_SQ);
            XFillRectangle(dpy, ck_tile, gc, CK_SQ, CK_SQ, CK_SQ, CK_SQ);
        }
        XGCValues tv;
        tv.fill_style = FillTiled; tv.tile = ck_tile;
        tv.ts_x_origin = e->x; tv.ts_y_origin = e->y;
        XChangeGC(dpy, gc, GCFillStyle | GCTile | GCTileStipXOrigin | GCTileStipYOrigin, &tv);
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
        XSetFillStyle(dpy, gc, FillSolid);
    } else if (e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.bg_color));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (e->style.has_border_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.border_color));
        int bw = e->style.has_border_width ? e->style.border_width : 1;
        for (int i = 0; i < bw; i++)
            XDrawRectangle(dpy, buf, gc, e->x + i, e->y + i, e->w - 1 - 2 * i, e->h - 1 - 2 * i);
    }
    if (strcmp(e->tag, "tab") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (strcmp(e->tag, "item") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#2f5f8f"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (e->nav_index > 0 && e->nav_index == g_focus_nav) {
        XSetForeground(dpy, gc, alloc_pixel("#ff8c00"));
        XDrawRectangle(dpy, buf, gc, e->x - 1, e->y - 1, e->w + 1, e->h + 1);
    }
    /* REAL, NEW 2026-09-05 (08-roadmap/design-docs/GRID-ELEMENT-DESIGN.md,
     * direct live request: "truly expecting cross hatch lines, like
     * tic-tac-toe") - a real, self-contained draw branch, returning
     * before the generic label-drawing chain below (same early-return
     * shape "canvas" already uses above) since a grid's content is a
     * genuine 2D table, not one wrappable string. Real, deliberate v1
     * scope: cell BORDERS/header row/row numbers/cursor highlight are
     * real and drawn here; actual cell VALUES are NOT wired yet (step 5
     * of the design doc's own build order - a consumer's per-cell vars
     * aren't published/read yet) - every data cell draws empty except
     * the one currently being edited, which shows its own live
     * grid_cell_buffer text + cursor bar. Fixed pixel cell sizing
     * (CELL_W_PX/CELL_H_PX) rather than measuring real content width -
     * a real, working table now, real column-width-fits-content sizing
     * is a future refinement, not attempted here. */
    if (strcmp(e->tag, "grid") == 0) {
        enum { CELL_W_PX = 64, CELL_H_PX = 22, ROWLABEL_W_PX = 32, STATUS_H_PX = 18 };
        int visible_cols = (e->w - ROWLABEL_W_PX) / CELL_W_PX;
        if (visible_cols < 1) visible_cols = 1;
        int table_y = e->y + STATUS_H_PX; /* real status row reserved above the table itself */
        int visible_rows = (e->h - STATUS_H_PX) / CELL_H_PX - 1; /* -1 for the header row */
        if (visible_rows < 1) visible_rows = 1;
        int armed = (g_default_input_elem && e->id[0] && strcmp(e->id, g_default_input_elem->id) == 0);
        int cur_row = armed ? g_default_input_elem->grid_cur_row : e->grid_cur_row;
        int cur_col = armed ? g_default_input_elem->grid_cur_col : e->grid_cur_col;
        int edit_mode = armed ? g_default_input_elem->grid_edit_mode : 0;
        XftFont *gfont = font_for(&e->style);
        const char *default_fg = (window_is_dock() && kh_hex_luma(g_theme_bg) > 140) ? "#1c1c1c" : "#cccccc";
        XftColor gcol = xft_color(e->style.has_fg_color ? e->style.fg_color : default_fg);
        /* REAL FIX 2026-09-05, direct live report ("i still dont see
         * its nav button for human entry") - this branch `return`s
         * before the generic "[ ]N."/"[>]N." nav-badge draw further
         * down in this function (the same real gap every early-return
         * tag, e.g. canvas, already has to solve for itself) - a human
         * had no visible way to tell the grid was even a real,
         * clickable/nav-reachable element. elem_cursor_prefix() only
         * knows "[ ]"/"[>]"/"[^]" (2-3 states); grid needs a real 4th
         * (# = armed-navigating vs ^ = armed-editing, see the design
         * doc's own "Decided" section), so this is a small, deliberate
         * duplicate of that function's own logic rather than a forced
         * generalization of a house-wide helper for one new tag.
         * REAL, NEW 2026-09-05 (direct live follow-up: "it should have
         * space to show digit combo jump accumulation") - the badge AND
         * the pending jump buffer now share a real, dedicated status
         * row reserved above the table itself (STATUS_H_PX), instead of
         * either cramming into the small corner cell or overlapping a
         * data cell's own real content (which would be confusable with
         * actually-typed cell text). */
        {
            const char *prefix = "[ ]";
            if (armed) prefix = edit_mode ? "[^]" : "[#]";
            else if (e->nav_index > 0 && e->nav_index == g_focus_nav) prefix = "[>]";
            char status_line[48];
            /* REAL, NEW 2026-09-05, direct live follow-up ("it should
             * echo input, can have a cursor, would that help?") - a
             * real, always-visible cursor glyph right after the jump
             * buffer text (even when the buffer is still empty) while
             * armed-navigating, so a human watching this exact spot can
             * tell at a glance whether a keypress reached this process
             * at all - real diagnostic value on top of real UX, not
             * either/or. */
            if (armed && !edit_mode)
                snprintf(status_line, sizeof(status_line), "%s%d. jump: %s_", prefix, e->nav_index, g_default_input_elem->grid_jump_buffer);
            else
                snprintf(status_line, sizeof(status_line), "%s%d.", prefix, e->nav_index);
            const char *badge_fg = armed ? (edit_mode ? "#ffcc00" : "#ff8c00") :
                                    (e->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
            XftColor bcol = xft_color(badge_fg);
            draw_text_emoji(gfont, &bcol, e->x + 2, e->y + gfont->ascent + 1, status_line);
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &bcol);
        }
        XSetForeground(dpy, gc, alloc_pixel("#444444"));
        for (int r = 0; r <= visible_rows + 1; r++) {
            int ly = table_y + r * CELL_H_PX;
            XDrawLine(dpy, buf, gc, e->x, ly, e->x + ROWLABEL_W_PX + visible_cols * CELL_W_PX, ly);
        }
        for (int c = 0; c <= visible_cols + 1; c++) {
            int lx = e->x + (c == 0 ? 0 : ROWLABEL_W_PX + (c - 1) * CELL_W_PX);
            XDrawLine(dpy, buf, gc, lx, table_y, lx, table_y + (visible_rows + 1) * CELL_H_PX);
        }
        for (int c = 0; c < visible_cols; c++) {
            /* Base-26 column letter (A=0..Z=25, AA=26...) - a small,
             * deliberately duplicated inline version of
             * grid_col_to_letters() (khtpm_core_render.c): that helper
             * is defined LATER in this same translation unit (this
             * file is #included before it), so it isn't callable here
             * yet - the algorithm is tiny enough that duplicating it is
             * simpler and safer than reordering the whole file. */
            char letters[8]; int nl = 0; long v = c + 1;
            while (v > 0 && nl < (int)sizeof(letters)) { long rem = (v - 1) % 26; letters[nl++] = (char)('A' + rem); v = (v - 1) / 26; }
            char colbuf[9]; int li = 0;
            for (int k = nl - 1; k >= 0; k--) colbuf[li++] = letters[k];
            colbuf[li] = '\0';
            int cx = e->x + ROWLABEL_W_PX + c * CELL_W_PX + 4;
            int cy = table_y + gfont->ascent + 2;
            draw_text_emoji(gfont, &gcol, cx, cy, colbuf);
        }
        for (int r = 0; r < visible_rows; r++) {
            char rowbuf[8];
            snprintf(rowbuf, sizeof(rowbuf), "%d", r + 1);
            int rx = e->x + 4;
            int ry = table_y + (r + 1) * CELL_H_PX + gfont->ascent + 2;
            draw_text_emoji(gfont, &gcol, rx, ry, rowbuf);
        }
        /* REAL FIX 2026-09-05, direct live follow-up ("does it remember
         * input?") - step 5's own edit-SEED wiring (default_grid_handle_
         * key()'s Enter-into-edit branch) only ever populated
         * grid_cell_buffer for the ONE cell being actively edited; every
         * OTHER cell's real published value (cell_<r>_<c>, the same var
         * a consumer's manager writes) was never actually drawn - a
         * committed edit really did reach the manager (confirmed via
         * status="Set A2." + cell_1_0=42 in the real UI file) but had no
         * visible way back onto the screen once the cursor moved off
         * it. Skips the cell currently mid-edit (drawn separately,
         * below, from the LIVE grid_cell_buffer - the manager's last-
         * published value there is one tick stale by definition while
         * a human is actively typing). */
        for (int r = 0; r < visible_rows; r++) {
            for (int c = 0; c < visible_cols; c++) {
                if (armed && edit_mode && r == cur_row && c == cur_col) continue;
                char varname[80];
                snprintf(varname, sizeof(varname), "%s%d_%d", e->target_id[0] ? e->target_id : "cell_", r, c);
                const char *val = kh_get_var(varname);
                if (!val[0]) continue;
                int vx = e->x + ROWLABEL_W_PX + c * CELL_W_PX + 4;
                int vy = table_y + (r + 1) * CELL_H_PX + gfont->ascent + 2;
                draw_text_emoji(gfont, &gcol, vx, vy, val);
            }
        }
        /* Current-cell highlight - only meaningful while this exact
         * grid is the armed element (g_default_input_elem); an
         * unarmed grid still draws its real border/header lines above,
         * just no cursor. */
        if (armed && cur_row >= 0 && cur_row < visible_rows && cur_col >= 0 && cur_col < visible_cols) {
            int hx = e->x + ROWLABEL_W_PX + cur_col * CELL_W_PX;
            int hy = table_y + (cur_row + 1) * CELL_H_PX;
            /* # = navigating (state 0), ^ = editing (state 1) - see the
             * design doc's own "Decided" section for why these two
             * badges, not one. The pending jump buffer (state 0) is
             * shown in the real status row above, not here - see that
             * row's own comment for why. */
            XSetForeground(dpy, gc, alloc_pixel(edit_mode ? "#ffcc00" : "#ff8c00"));
            XDrawRectangle(dpy, buf, gc, hx, hy, CELL_W_PX, CELL_H_PX);
            if (edit_mode) {
                /* The one real cell being edited shows its own live
                 * grid_cell_buffer text (real per-cell VALUE display
                 * for every other cell is step 5, not this pass) plus
                 * a real cursor bar at e->cursor's byte offset. */
                draw_text_emoji(gfont, &gcol, hx + 4, hy + gfont->ascent + 2, g_default_input_elem->grid_cell_buffer);
                XGlyphInfo pre_ext;
                XftTextExtentsUtf8(dpy, gfont, (const FcChar8 *)g_default_input_elem->grid_cell_buffer,
                                    g_default_input_elem->cursor, &pre_ext);
                int ccx = hx + 4 + pre_ext.width;
                XSetForeground(dpy, gc, alloc_pixel(e->style.has_fg_color ? e->style.fg_color : default_fg));
                XDrawLine(dpy, buf, gc, ccx, hy + 2, ccx, hy + CELL_H_PX - 2);
            }
        }
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &gcol);
        return;
    }
    int pad = e->style.has_padding ? e->style.padding : 4;
    int label_x = e->x + pad;
    /* REAL FIX 2026-08-25 (Stage 2 palettes port, direct carry-over from
     * the SAME bug already found+fixed live in khtpm_hq_render.c this
     * session, "i dont see the nav on the emojis"): badge geometry is
     * computed here (reserves room in label_x for the text path below),
     * but the ACTUAL badge draw happens LAST, after sprite/label - see
     * the end of this function. Porting the OLD draw-badge-first order
     * would silently reintroduce the identical sprite-paints-over-badge
     * bug fresh in this binary; not repeating that mistake. */
    char nav_badge[16] = "";
    XftFont *nav_badge_font = NULL;
    XGlyphInfo nav_badge_ext = {0};
    int badge_label_x = label_x;
    /* 2026-09-02: always draw "[ ]N." when nav_index>0. Digit-jump
     * and AI control of the window need the visible brackets. class=quiet
     * may still color the fill; it does not hide the badge. */
    if (e->nav_index > 0) {
        int focused = (e->nav_index == g_focus_nav);
        char prefix[8];
        /* REAL, NEW 2026-08-31 - a real, generic ARMED cli_io field
         * reuses this SAME existing "[^]" active-scope visual (direct
         * instruction: cli_io should show "^" once armed, not the plain
         * "[>]" a merely-focused-but-not-yet-typing element gets) -
         * not a new bespoke prefix state, the exact one db-hq's own
         * scope root already uses. Real nav-blocking while armed is
         * already handled separately, in handle_key()'s own real
         * key-order check (g_default_input_elem is tested before any
         * Up/Down/Enter dispatch reaches the generic nav code at all).
         * REAL FIX 2026-08-31 (found live, same investigation as
         * dbhq_serialize_frame_elem()'s own input_buffer fix): compared
         * by POINTER at first, which can never match here - the default/
         * popup mode's own real content draw (redraw()'s "now the
         * shared, generic render_tree()" path) calls draw_elem() on a
         * fresh, freshly-built temp Elem parsed from a text frame file
         * (dbhq_paint_frame_line()), never on the live g_pool[] Elem a
         * human is actually typing into - `e == g_default_input_elem`
         * was comparing two different objects' addresses and could
         * never be true from that path. Real fix: compare by id, the
         * one identifying field the frame-file round trip already
         * carries faithfully - safe because a real .chtpm's ids are
         * already relied on to be unique per window (find_page()/
         * dispatch() etc. all key off id the same way). */
        /* TPMOS chtpm_parser.c render_element():
         *   is_active  -> [^]
         *   is_focused && (no scope || navigable) -> [>]
         *   else [ ]
         * Active = the trigger id (and the remembered <tab> id while
         * confined). Draw copies have no parent pointers — do not gate
         * this on kh_elem_in_scope(). */
        int is_scope = (g_dbhq_active_scope_root && e == g_dbhq_active_scope_root) ||
                       (g_default_input_elem && e->id[0] && strcmp(e->id, g_default_input_elem->id) == 0) ||
                       (g_default_active_scope_id[0] && e->id[0] &&
                        strcmp(e->id, g_default_active_scope_id) == 0) ||
                       (g_default_scope_confine && g_default_active_tab_id[0] && e->id[0] &&
                        strcmp(e->id, g_default_active_tab_id) == 0) ||
                       /* REAL, NEW 2026-09-04 - Interact Mode trigger:
                        * "^" while g_interact_relay_on genuinely holds
                        * the keyboard (matches run_pchq_board_mode's own
                        * is_engaged badge, see g_interact_relay_on's own
                        * declaration comment in khtpm_core_render.c). */
                       (g_interact_relay_on && e->relay[0]);
        elem_cursor_prefix(e, g_focus_nav, is_scope, prefix, sizeof(prefix));
        snprintf(nav_badge, sizeof(nav_badge), "%s%d.", prefix, e->nav_index);
        (void)focused;
        /* REAL FIX 2026-08-25 (live perf report: "nav is really slow" with
         * 113 palette tiles on screen) - this was opening a fresh XftFont
         * via XftFontOpenName() for EVERY nav-badged element, EVERY redraw
         * (a full-tree redraw fires on every nav keypress) - same bug
         * class font_for() above already fixed once for labels, never
         * ported to the badge path. 113 tiles x 1 font-server round trip
         * each, per keypress, was the actual bottleneck - not sprites,
         * which were already cached via hq_sprite(). Same cache pattern
         * as font_for(): keyed on pixel size (the only thing that varies
         * here), reused across the whole tree/frame. */
        static char badge_cached_spec[48] = "";
        static XftFont *badge_cached_font = NULL;
        char numspec[48];
        snprintf(numspec, sizeof(numspec), "DejaVu Sans Mono:pixelsize=%d", scaled(9));
        if (badge_cached_font && strcmp(badge_cached_spec, numspec) == 0) {
            nav_badge_font = badge_cached_font;
        } else {
            if (badge_cached_font) XftFontClose(dpy, badge_cached_font);
            nav_badge_font = XftFontOpenName(dpy, screen, numspec);
            if (!nav_badge_font) { snprintf(numspec, sizeof(numspec), "DejaVu Sans:pixelsize=%d", scaled(9)); nav_badge_font = XftFontOpenName(dpy, screen, numspec); }
            badge_cached_font = nav_badge_font;
            snprintf(badge_cached_spec, sizeof(badge_cached_spec), "%s", numspec);
        }
        if (nav_badge_font) {
            XftTextExtentsUtf8(dpy, nav_badge_font, (const FcChar8 *)nav_badge, (int)strlen(nav_badge), &nav_badge_ext);
            badge_label_x = label_x + nav_badge_ext.width + 5;
        }
    }
    /* REAL, ported 2026-08-25 (Stage 2 palettes port) - an element
     * carrying a real sprite= texture draws the image INSTEAD of its own
     * label text, same convention as khtpm_hq_render.c's own palettes
     * matrix. Sprite draws BEFORE the badge (see above) so the badge is
     * never painted over. */
    int drew_sprite = 0;
    if (e->sprite[0]) {
        HqSprite *sp = hq_sprite(e->sprite);
        if (sp) {
            int pad_s = e->style.has_padding ? e->style.padding : 4;
            int has_under_label = (e->h >= 64 && e->label[0] && !(e->label[0] == ' ' && e->label[1] == '\0'));
            int label_reserve = has_under_label ? 18 : 0;
            int box_w = e->w - 2 * pad_s, box_h = e->h - 2 * pad_s - label_reserve;
            int px = box_w < box_h ? box_w : box_h;
            if (px > HQ_SPRITE_PX_MAX) px = HQ_SPRITE_PX_MAX;
            if (px > 0) {
                /* REAL FIX 2026-09-03 - sprite alpha is flattened onto
                 * bg_pixel at blit time (this window's visual has no
                 * per-pixel alpha), so bg_pixel MUST match the colour
                 * actually painted behind the tile or transparent PNG
                 * edges show a wrong-coloured box/halo. The fallback was
                 * a hardcoded "#1c1c1c"; after a theme change (swatch
                 * picker -> livedesk_theme.pdl COLOR|bg) the real
                 * surface is g_theme_bg, so every transparent icon sat
                 * on a stale grey square. Follow the theme. */
                unsigned long bg_pixel =
                    (e->style.has_bg_color && elem_has_class(e, "pal-tile"))
                        ? alloc_pixel("#dcdcdc")               /* checkerboard mean - matte transparent edges to a light grey */
                    : e->style.has_bg_color
                        ? alloc_pixel(e->style.bg_color)
                        : alloc_pixel(g_theme_bg[0] ? g_theme_bg : "#1c1c1c");
                int blit_x, blit_y;
                /* A palette/swatch GRID tile is short (h<64) too, but it
                 * wants the pre-2026-09-03 behaviour: sprite centered,
                 * filling the cell, no 24px cap, no left/nav offset. Only
                 * real taskbar-strip items (short cell + a caption beside
                 * the icon) take the left-anchored path. */
                int is_grid_tile = elem_has_class(e, "pal-tile") ||
                                   elem_has_class(e, "swatch");
                int short_bar = (e->h < 64) && !is_grid_tile;
                if (short_bar) {
                    /* Taskbar-height cells: sprite LEFT of the label,
                     * after the nav badge — not centered over it. */
                    if (px > 24) px = 24;
                    blit_x = e->x + pad_s + (e->nav_index > 0 ? 36 : 0);
                    blit_y = e->y + (e->h - px) / 2;
                    badge_label_x = blit_x + px + 4;
                } else {
                    blit_x = e->x + (e->w - px) / 2;
                    blit_y = (e->h >= 64)
                        ? (e->y + pad_s)
                        : (e->y + (e->h - px) / 2);
                }
                hq_blit_sprite(sp, blit_x, blit_y, px, bg_pixel);
                drew_sprite = 1;
            }
        }
    }
    /* REAL, NEW 2026-08-31 (generic capability #2 - see Elem's own
     * input_buffer field comment) - a real, generic cli_io tag shows
     * its own live-typed input_buffer appended after its static label,
     * with a real cursor glyph while it's the currently focused (armed)
     * field - zero per-app code needed for any consumer of this shared
     * draw path. */
    char cli_io_shown[256 + 300];
    static char text_area_shown[4096 + 300]; /* static: too big for this function's own stack budget alongside everything else already declared here */
    char label_decoded[600];
    const char *shown_label = e->label;
    int cli_io_armed = 0;
    if (strcmp(e->tag, "text_area") == 0) {
        /* REAL, NEW 2026-09-05 - same label+buffer convention as
         * cli_io just below, own (much bigger) buffer. label_decoded
         * (600B) can't hold this - the text_area draw branch further
         * down reads `shown_label` directly, never through
         * label_decoded, so HTML-entity decoding is intentionally
         * skipped for text_area's own content this pass (a real
         * document is far more likely to contain a literal `&` the
         * user typed than an intentional entity reference - decoding
         * it would corrupt real typed content, unlike cli_io's short
         * composer strings where entities are the more common real
         * case). */
        snprintf(text_area_shown, sizeof(text_area_shown), "%s%s", e->label, e->text_area_buffer);
        shown_label = text_area_shown;
    } else if (strcmp(e->tag, "cli_io") == 0) {
        /* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-
         * EDITING-DESIGN.md) - the old trailing "_" glyph was a crude
         * stand-in for a real cursor, always at the very end regardless
         * of where e->cursor actually was. A real cursor bar is drawn
         * at its own real position further down (single-line path
         * only, see that comment) - no fake trailing glyph needed here
         * anymore. */
        snprintf(cli_io_shown, sizeof(cli_io_shown), "%s%s", e->label, e->input_buffer);
        shown_label = cli_io_shown;
        /* REAL FIX 2026-09-05, direct live report ("i noticed that
         * interact mode... was not enabled '^' nor was nav even on
         * 'text input' area") - found while re-verifying: this was
         * checking nav-FOCUS equality, not real armed state, so the
         * cursor bar could render on a field a human just tabbed past
         * without ever arming it for typing - misleading, and NOT the
         * same real check the "^" badge just above already uses
         * correctly (g_default_input_elem, matched by id, since this
         * is a fresh frame-round-trip tmp Elem, never the same pointer
         * as the live one). Use that exact same real armed-state check
         * here too, instead of a second, looser, home-grown one. */
        cli_io_armed = (g_default_input_elem && e->id[0] && strcmp(e->id, g_default_input_elem->id) == 0);
    }
    if (strcmp(e->tag, "text_area") != 0) {
        /* REAL, NEW 2026-09-05 - text_area's own shown_label (up to
         * ~4400 bytes) skips this: label_decoded is only 600B, and
         * would silently truncate a real document every single draw.
         * See text_area_shown's own declaration comment for why entity
         * decoding is skipped for it anyway, not just this buffer-size
         * mechanics. */
        snprintf(label_decoded, sizeof(label_decoded), "%s", shown_label);
        khtpm_decode_label_entities(label_decoded);
        shown_label = label_decoded;
    }
    int sprite_under_label = drew_sprite && e->h >= 64 && shown_label[0] && !(shown_label[0] == ' ' && shown_label[1] == '\0');
    int sprite_beside_label = drew_sprite && e->h < 64 && shown_label[0] &&
                              !elem_has_class(e, "pal-tile") && !elem_has_class(e, "swatch");
    /* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-
     * EDITING-DESIGN.md) - text_area gets its own real, separate draw
     * path rather than folding into cli_io's word-wrap loop below:
     * that loop treats its whole string as ONE continuously-wrappable
     * stream with no concept of a real `\n` - exactly the thing
     * text_area exists to have. Real logical lines (split on actual
     * `\n` bytes) are wrapped independently here, each starting a
     * fresh visual row regardless of how much of its own width budget
     * the previous logical line used - the real distinction this
     * whole feature is about. No ellipsis-on-overflow handling yet
     * (cli_io's single-line/wrap paths both have it) - real, deliberate
     * v1 scope note, not an oversight: text overflowing the box's
     * visible rows is simply not drawn past the last one, matching the
     * word-wrap loop's own max_lines clamp without ellipsis's added
     * real complexity of computing this per REAL logical line boundary
     * too. */
    if (strcmp(e->tag, "text_area") == 0) {
        XftFont *font = font_for(&e->style);
        const char *default_fg = (window_is_dock() && kh_hex_luma(g_theme_bg) > 140) ? "#1c1c1c" : "#cccccc";
        XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : default_fg);
        int avail_w = e->w > 0 ? (e->x + e->w) - (e->x + 4) : -1;
        int line_h = font->ascent - font->descent > 0 ? font->ascent - font->descent : 12;
        line_h += 4;
        int max_lines = e->h / line_h;
        if (max_lines < 1) max_lines = 1;
        int text_x = e->x + 4;
        int ty = e->y + font->ascent + 2;
        /* REAL FIX 2026-09-05 - same real armed-state check as cli_io's
         * own cli_io_armed just above (see its own comment) - not
         * nav-focus equality, which would show a cursor bar on a
         * text_area a human just tabbed past without ever arming it. */
        int armed = (g_default_input_elem && e->id[0] && strcmp(e->id, g_default_input_elem->id) == 0);
        int cursor_off = -1;
        if (armed) {
            int label_len = (int)strlen(shown_label) - (int)strlen(e->text_area_buffer);
            if (label_len < 0) label_len = 0; /* honest guard - shouldn't happen, shown_label is always label+buffer below */
            cursor_off = label_len + e->cursor;
        }
        int consumed = 0; /* absolute offset into `shown_label` already drawn */
        int line_no = 0;
        const char *lp = shown_label; /* walks one REAL logical line at a time */
        while (line_no < max_lines) {
            const char *nl = strchr(lp, '\n');
            int logical_len = nl ? (int)(nl - lp) : (int)strlen(lp);
            /* Word-wrap THIS logical line only - a manual newline
             * always starts a fresh visual row, independent of the
             * greedy width-wrap below. */
            const char *sp = lp;
            int remaining = logical_len;
            do {
                int last_good_space = -1, i = 0;
                XGlyphInfo lw;
                for (;;) {
                    if (i >= remaining) break;
                    if (sp[i] == ' ') last_good_space = i;
                    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)sp, i + 1, &lw);
                    if (avail_w > 0 && lw.width > avail_w) break;
                    i++;
                }
                int cut = i;
                int more_in_logical = (i < remaining);
                if (more_in_logical && last_good_space >= 0) cut = last_good_space;
                if (cut == 0 && more_in_logical) cut = 1; /* single glyph wider than the box - take it anyway, avoid an infinite loop */
                char row_buf[600];
                snprintf(row_buf, sizeof(row_buf), "%.*s", cut, sp);
                draw_text_emoji(font, &col, text_x, ty, row_buf);
                if (cursor_off >= 0) {
                    int row_start_off = (int)(sp - shown_label);
                    int row_end_off = row_start_off + cut;
                    if (cursor_off >= row_start_off && cursor_off <= row_end_off) {
                        XGlyphInfo pre_ext;
                        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)sp, cursor_off - row_start_off, &pre_ext);
                        int cx = text_x + pre_ext.width;
                        int cy0 = ty - font->ascent, cy1 = ty + (font->descent > 0 ? font->descent : 2);
                        XSetForeground(dpy, gc, alloc_pixel(e->style.has_fg_color ? e->style.fg_color : default_fg));
                        XDrawLine(dpy, buf, gc, cx, cy0, cx, cy1);
                        cursor_off = -1;
                    }
                }
                ty += line_h;
                line_no++;
                sp += cut;
                remaining -= cut;
                while (remaining > 0 && *sp == ' ') { sp++; remaining--; } /* consumed break space never starts the next visual row */
            } while (remaining > 0 && line_no < max_lines);
            consumed = (int)(lp - shown_label) + logical_len;
            if (!nl) break; /* no more real logical lines */
            lp = nl + 1;
            /* An armed field whose cursor sits exactly ON the real \n
             * itself (end of a logical line, before the one below)
             * needs the bar drawn here - the per-visual-row check above
             * only ever sees offsets strictly inside a drawn row's own
             * span, never the boundary character itself when it's the
             * `\n` consumed by advancing to the next logical line. */
            if (cursor_off == consumed) {
                XGlyphInfo pre_ext;
                XftTextExtentsUtf8(dpy, font, (const FcChar8 *)sp, 0, &pre_ext); /* honest zero-width - bar lands at row start */
                (void)pre_ext;
                int cy0 = ty - font->ascent, cy1 = ty + (font->descent > 0 ? font->descent : 2);
                XSetForeground(dpy, gc, alloc_pixel(e->style.has_fg_color ? e->style.fg_color : default_fg));
                XDrawLine(dpy, buf, gc, text_x, cy0, text_x, cy1);
                cursor_off = -1;
            }
        }
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
    } else
    if ((!drew_sprite || sprite_under_label || sprite_beside_label) && shown_label[0]) {
        XftFont *font = font_for(&e->style);
        /* See kh_hex_luma()'s own header comment - dock windows fill
         * with the real, user-changeable g_theme_bg, not the CSS's
         * fixed dark window background, so the default label color
         * needs to track it for readability, not stay hardcoded. Only
         * when the item has no explicit CSS fg_color of its own -
         * an explicit fg_color is a deliberate per-element choice,
         * left alone. */
        const char *default_fg = (window_is_dock() && kh_hex_luma(g_theme_bg) > 140) ? "#1c1c1c" : "#cccccc";
        XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : default_fg);
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)shown_label, (int)strlen(shown_label), &extents);
        int avail_w = e->w > 0 ? (e->x + e->w) - badge_label_x : -1;
        int line_h = font->ascent - font->descent > 0 ? font->ascent - font->descent : 12;
        line_h += 4; /* real, small leading - matches this file's own general text-row spacing feel */
        /* REAL, NEW 2026-09-01 (direct instruction: "build word-wrap/
         * multi-line/emoji into the generic cli_io first" - a real,
         * generic capability, not chat-hai-specific, so chat-hai's own
         * eventual migration doesn't lose real features it already has)
         * - a <cli_io> whose own real h is declared taller than roughly
         * 1.5 real text rows gets REAL multi-line word-wrap within that
         * fixed box; a plain single-row cli_io (open-hai's own real
         * composer today, h==ROW_H) is COMPLETELY unaffected - same
         * single-line, vertically-centered path as before, zero risk to
         * anything already working. Real, deliberate scope for this
         * first slice: fixed-height wrap only, no dynamic auto-growth/
         * sibling-reflow as the user types past the box - that's a real
         * layout-engine feature, flagged as separate future work, not
         * silently attempted here under time pressure. */
        /* REAL FIX 2026-09-03 - generalized from cli_io-only (see
         * wrap_line_count()'s own header comment above for the real
         * "co-lab-hai's long agent messages don't wrap" incident this
         * traces back to). The real gate is just "is this box tall
         * enough for more than one line" - which tag asked for it
         * doesn't matter. Every existing single-ROW_H element (every
         * sidebar/panel <text>, every plain composer) is completely
         * unaffected, since scroll_row_span()'s own default height is
         * still exactly one line for anything that doesn't need more -
         * this only activates for a box some other real code path
         * already decided needs to be taller. */
        int is_multiline_box = (e->h > (line_h * 3) / 2) && avail_w > 0;
        if (is_multiline_box) {
            /* Real, generic greedy word-wrap: pack words onto each line
             * (measuring real glyph width via Xft, not a char-count
             * guess - same discipline chai_measure_text_px() already
             * uses elsewhere in this house), starting a new line
             * whenever the next word wouldn't fit; stop once no more
             * real vertical space remains in the box (the last visible
             * line gets a real "..." ellipsis if there's more text than
             * fits, same real convention the single-line clip path
             * already uses). */
            Pixmap wrap_target_buf = buf; /* captured BEFORE the local `char buf[600]` below shadows the outer Pixmap `buf` for the rest of this block */
            char buf[600];
            snprintf(buf, sizeof(buf), "%s", shown_label);
            int max_lines = e->h / line_h;
            if (max_lines < 1) max_lines = 1;
            int ty = e->y + font->ascent + 2;
            int line_no = 0;
            char *p = buf;
            /* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-
             * MULTILINE-EDITING-DESIGN.md) - a cli_io box tall enough
             * to take THIS wrap path (h > ~1.5 lines) is still just as
             * real a cli_io as the single-line one - it needs the same
             * real cursor bar, not silently none at all. Track which
             * wrapped visual line contains e->cursor's own offset as
             * this loop already walks them one at a time; draw the bar
             * on that specific line once found. This is genuinely
             * single-line-content cursor math applied per wrapped
             * row, NOT the harder "cursor moves across rows via real
             * Up/Down" work - that generalization stays <text_area>'s
             * own job, per this plan's own scope split. */
            int cursor_off = -1;
            if (strcmp(e->tag, "cli_io") == 0 && cli_io_armed) {
                int label_len = (int)strlen(e->label);
                cursor_off = label_len + e->cursor;
                if (cursor_off < 0) cursor_off = 0;
                if (cursor_off > (int)strlen(buf)) cursor_off = (int)strlen(buf);
            }
            while (*p && line_no < max_lines) {
                int last_good_space = -1;
                int i = 0;
                XGlyphInfo lw;
                for (;;) {
                    char c = p[i];
                    if (c == '\0') break;
                    if (c == ' ') last_good_space = i;
                    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)p, i + 1, &lw);
                    if (lw.width > avail_w) break;
                    i++;
                }
                int cut = i;
                int has_more_after = (p[i] != '\0');
                if (has_more_after && last_good_space >= 0) cut = last_good_space;
                if (cut == 0 && p[i] != '\0') cut = 1; /* a single glyph wider than the whole box - avoid an infinite loop, take it anyway */
                char line_buf[600];
                int is_last_visible_line = (line_no == max_lines - 1);
                int real_more_remains = has_more_after && (cut < (int)strlen(p) || p[cut] != '\0');
                if (is_last_visible_line && real_more_remains) {
                    /* Real ellipsis on the box's own last visible line
                     * only, matching the single-line clip path's own
                     * real convention - trims further until "..." fits,
                     * UTF-8-safe (never cuts mid-codepoint). */
                    int len2 = cut;
                    static const char *ELLIPSIS = "...";
                    XGlyphInfo ell_ext;
                    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)ELLIPSIS, 3, &ell_ext);
                    int target_w = avail_w - ell_ext.width;
                    if (target_w < 0) target_w = 0;
                    while (len2 > 0) {
                        XGlyphInfo cw;
                        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)p, len2, &cw);
                        if (cw.width <= target_w) break;
                        len2--;
                        while (len2 > 0 && ((unsigned char)p[len2] & 0xC0) == 0x80) len2--;
                    }
                    snprintf(line_buf, sizeof(line_buf), "%.*s%s", len2, p, ELLIPSIS);
                } else {
                    snprintf(line_buf, sizeof(line_buf), "%.*s", cut, p);
                }
                draw_text_emoji(font, &col, badge_label_x, ty, line_buf);
                if (cursor_off >= 0) {
                    int line_start_off = (int)(p - buf);
                    int line_end_off = line_start_off + cut;
                    if (cursor_off >= line_start_off && cursor_off <= line_end_off) {
                        XGlyphInfo pre_ext;
                        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)p, cursor_off - line_start_off, &pre_ext);
                        int cx = badge_label_x + pre_ext.width;
                        int cy0 = ty - font->ascent;
                        int cy1 = ty + (font->descent > 0 ? font->descent : 2);
                        XSetForeground(dpy, gc, alloc_pixel(e->style.has_fg_color ? e->style.fg_color : default_fg));
                        XDrawLine(dpy, wrap_target_buf, gc, cx, cy0, cx, cy1);
                        cursor_off = -1; /* drawn once - don't redraw on a later line if math ever double-matches a boundary */
                    }
                }
                ty += line_h;
                line_no++;
                p += cut;
                while (*p == ' ') p++; /* real, plain word-wrap convention - a consumed break space never starts the next line */
                if (is_last_visible_line) break;
            }
        } else {
            /* REAL, NEW 2026-09-01 (found live testing open-hai's own
             * real sidebar - a real session snippet longer than the
             * sidebar's own real 220px width drew straight past its own
             * element box into the panel column beside it, looking
             * exactly like a garbled double-render until traced back to
             * plain unclipped text overflow) - a real, generic fix, not
             * open-hai-specific: any element with a real w>0 now gets
             * its own label truncated (with a real "..." ellipsis,
             * UTF-8-safe - real message text can and does contain
             * multi-byte emoji, never cut mid-codepoint) to fit the
             * space actually available between its own badge and its
             * own right edge. A harmless no-op for any label that
             * already fits - nothing currently working can regress from
             * this. */
            char clipped_buf[600];
            const char *draw_label = shown_label;
            if (avail_w > 0 && extents.width > avail_w) {
                snprintf(clipped_buf, sizeof(clipped_buf), "%s", shown_label);
                size_t len = strlen(clipped_buf);
                static const char *ELLIPSIS = "...";
                XGlyphInfo ell_ext;
                XftTextExtentsUtf8(dpy, font, (const FcChar8 *)ELLIPSIS, 3, &ell_ext);
                int target_w = avail_w - ell_ext.width;
                if (target_w < 0) target_w = 0;
                while (len > 0) {
                    XGlyphInfo cur_ext;
                    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)clipped_buf, (int)len, &cur_ext);
                    if (cur_ext.width <= target_w) break;
                    len--;
                    while (len > 0 && ((unsigned char)clipped_buf[len] & 0xC0) == 0x80) len--; /* don't cut mid-UTF8-codepoint */
                }
                clipped_buf[len] = '\0';
                snprintf(clipped_buf + len, sizeof(clipped_buf) - len, "%s", ELLIPSIS);
                draw_label = clipped_buf;
            }
            int ty = e->y + (e->h + font->ascent - font->descent) / 2;
            if (ty < e->y + font->ascent) ty = e->y + font->ascent + pad / 2;
            if (sprite_under_label) {
                ty = e->y + e->h - 4;
                if (ty < e->y + font->ascent) ty = e->y + font->ascent;
                badge_label_x = e->x + pad;
            }
            draw_text_emoji(font, &col, badge_label_x, ty, draw_label);
            /* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-
             * MULTILINE-EDITING-DESIGN.md, direct instruction: "i
             * think cli-io should still have a cursor") - a real
             * cursor bar at e->cursor's own actual position, not a
             * fake trailing glyph always at the end. Scoped to the
             * single-line (this branch) case only: `draw_label ==
             * shown_label` means the text was NOT ellipsis-clipped
             * above, so the substring-width measurement below is
             * still measuring the real, on-screen string - a clipped
             * cli_io (rare; only very long typed text in a narrow
             * box) skips the cursor draw rather than show it at a
             * wrong position. The word-wrapped (rows>1) case above
             * does not get a cursor bar yet - real multi-row cursor
             * positioning is exactly the harder half of work this
             * plan hands to <text_area>, not silently guessed at
             * here for cli_io. */
            if (strcmp(e->tag, "cli_io") == 0 && cli_io_armed && draw_label == shown_label) {
                int label_len = (int)strlen(e->label);
                int cursor_off = label_len + e->cursor;
                if (cursor_off < 0) cursor_off = 0;
                if (cursor_off > (int)strlen(shown_label)) cursor_off = (int)strlen(shown_label);
                XGlyphInfo pre_ext;
                XftTextExtentsUtf8(dpy, font, (const FcChar8 *)shown_label, cursor_off, &pre_ext);
                int cx = badge_label_x + pre_ext.width;
                int cy0 = ty - font->ascent;
                int cy1 = ty + (font->descent > 0 ? font->descent : 2);
                XSetForeground(dpy, gc, alloc_pixel(e->style.has_fg_color ? e->style.fg_color : default_fg));
                XDrawLine(dpy, buf, gc, cx, cy0, cx, cy1);
            }
        }
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
    }
    /* Badge draws LAST - see the big comment above. For sprite tiles,
     * position it in the real ~16px row-gap ABOVE the tile (matching
     * khtpm_hq_render.c's own live-verified fix, "i think they should go
     * above the tile, not on it") with a solid backing chip for
     * contrast against any sprite color; non-sprite elements keep the
     * original inline position. REAL, NEW 2026-09-04 - swatch tiles
     * (small color squares without sprites, class="swatch") also get
     * the badge positioned ABOVE the tile, not overlapping it, for
     * readability - was previously gated on the (permanently
     * unreachable) swatch-picker window-class flag, so it never fired
     * for the real live taskbar-settings-pal.xhtpm window. */
    if (e->nav_index > 0 && nav_badge_font) {
        int focused = (e->nav_index == g_focus_nav);
        int numy = e->y + (e->h + nav_badge_font->ascent - nav_badge_font->descent) / 2;
        int is_swatch_tile = elem_has_class(e, "swatch") || elem_has_class(e, "pal-tile");
        int chip_drawn = 0; /* REAL FIX 2026-09-05, direct live report ("nav on [ ]2 <username> and timestamp proves it") - see this block's final `if (!chip_drawn)` for the real story: the "every badge gets a chip" claim from 2026-09-04 (below) was wrong. A SHORT dock-cell WITH a sprite (a taskbar avatar badge, h<64 so it misses the tall-sprite chip, y<16 so it misses the above-tile chip) fell through all three branches - the general chip was gated `!e->sprite[0]`, so it never ran either. Tracking chip_drawn and falling back to the general inline chip whenever NONE of the special-position branches actually drew one closes that gap for real, instead of re-guessing another special case. */
        if (e->sprite[0] && e->h >= 64) {
            /* REAL FIX 2026-09-02 - tall sprite rows (scrolllist) have
             * room INSIDE the box; drawing the chip above e->y painted
             * it onto the previous text line. Palettes tiles stay on
             * the old above-tile path (h < 64). */
            int chip_pad = 1;
            int numy_above = e->y + nav_badge_font->ascent + 3;
            int chip_x0 = e->x - chip_pad;
            int chip_y0 = numy_above - nav_badge_font->ascent - chip_pad;
            int chip_w = nav_badge_ext.width + 2 * chip_pad;
            int chip_h = nav_badge_font->ascent + nav_badge_font->descent + 2 * chip_pad;
            numy = numy_above;
            label_x = e->x;
            XSetForeground(dpy, gc, alloc_pixel("#141414"));
            XFillRectangle(dpy, buf, gc, chip_x0, chip_y0, (unsigned)chip_w, (unsigned)chip_h);
            chip_drawn = 1;
        } else if ((e->sprite[0] || is_swatch_tile) && e->y >= 16) {
            /* Sprite tiles and swatch-picker tiles: draw badge ABOVE the tile
             * with a dark backing chip for contrast. */
            int chip_pad = 1;
            int gap_margin = 2;
            int numy_above = e->y - gap_margin - nav_badge_font->descent;
            int chip_x0 = e->x - chip_pad;
            int chip_y0 = numy_above - nav_badge_font->ascent - chip_pad;
            int chip_w = nav_badge_ext.width + 2 * chip_pad;
            int chip_h = nav_badge_font->ascent + nav_badge_font->descent + 2 * chip_pad;
            numy = numy_above;
            label_x = e->x;
            XSetForeground(dpy, gc, alloc_pixel("#141414"));
            XFillRectangle(dpy, buf, gc, chip_x0, chip_y0, (unsigned)chip_w, (unsigned)chip_h);
            chip_drawn = 1;
        }
        /* Sprite tiles and swatch tiles draw the badge on the dark #141414 backing chip
         * above (not on the tile's own bg), so it always needs the
         * light color - badge_contrast_color() would otherwise read the
         * tile's own bg and (wrongly) pick an unreadable color. */
        /* TPMOS ASCII prefixes sit on a dark frame; tab/sidebar fills
         * here ate [^]/[>] via contrast-on-same-luma. Chip + fixed
         * glyph colors so the three-state cursor is always readable. */
        int draw_x = e->badge_align_left ? (e->x - (int)nav_badge_ext.width - scaled(4)) : label_x;
        if (!chip_drawn) {
            int chip_pad = 1;
            int chip_h = nav_badge_font->ascent + nav_badge_font->descent + 2 * chip_pad;
            int chip_y0 = numy - nav_badge_font->ascent - chip_pad;
            XSetForeground(dpy, gc, alloc_pixel("#141414"));
            XFillRectangle(dpy, buf, gc, draw_x - chip_pad, chip_y0,
                           (unsigned)(nav_badge_ext.width + 2 * chip_pad), (unsigned)chip_h);
        }
        {
            /* REAL FIX 2026-09-04, direct live report ("some nav
             * numbers get blacked out... 2,4,6&7... this also
             * happens in tb and other windows"). Confirmed the bug is
             * systemic, not swatch-specific: EVERY badge in this
             * function is drawn on a fixed dark #141414 backing chip -
             * the tall-sprite branch draws its own chip, the sprite/
             * swatch-tile branch draws its own chip, and this
             * function's own FINAL, GENERAL branch (right above, `if
             * (!e->sprite[0] && !is_swatch_tile) { ... XFillRectangle
             * ... }`) draws the identical chip UNCONDITIONALLY for
             * every other element - a plain <item>/<tab>/dock-cell,
             * no position or style gate at all. There is no code path
             * left where a badge is drawn WITHOUT this chip. So
             * badge_contrast_color()/badge_focus_color() - which pick
             * a DARK color specifically for a LIGHT element background
             * (`luma > 140 -> "#000000"`/`"#7a1a00"`) - were always
             * computing contrast against the wrong surface: the
             * element's own bg, never the dark chip the text actually
             * sits on. Any element anywhere in the house with a
             * light/bright own bg-color (a themed dock cell, a light
             * row, etc.) hit the exact same invisible-badge bug the
             * swatch tiles did, just via the general branch instead of
             * the swatch-specific one. Fix: since a dark chip is now
             * confirmed universal, every badge (focused or not, sprite
             * or not, swatch or not) uses the same two fixed colors
             * every other branch already used correctly - no per-
             * element contrast calculation needed anywhere. */
            const char *badge_fg = "#cccccc";
            if (nav_badge[1] == '^') badge_fg = "#ffd24a";
            else if (nav_badge[1] == '>') badge_fg = "#ff8c00";
            else if (focused) badge_fg = "#ff8c00";
            XftColor numcol = xft_color(badge_fg);
            XftDrawStringUtf8(xftdraw_buf, &numcol, nav_badge_font, draw_x, numy, (const FcChar8 *)nav_badge, (int)strlen(nav_badge));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
        }
        /* REAL FIX 2026-08-25 (live report, corrupted badge rendering) -
         * nav_badge_font is now the shared cache added earlier this pass
         * (same contract as font_for() above: caller must NOT close it).
         * This XftFontClose() used to run after EVERY element's badge
         * draw, closing the very handle the cache had just stored for
         * reuse - the next element to hit the cache-hit branch got a
         * dangling XftFont*, corrupting every badge after the first. */
    }
}

/* absolute-positioned children (a floating block-title) are painted in
 * a later pass than their parent - this walk draws non-title children
 * first, titles last, matching db-hq's own real design intent. */
static void render_tree(Elem *e, int depth) {
    if (depth == 0) draw_elem(e, 0);
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) continue; /* deferred */
        if (strcmp(c->tag, "module") == 0) continue; /* pure config, never visual */
        /* REAL, NEW 2026-09-03 - a real, generic dropdown ("Menu"
         * request, "work for all layouts"): class="dropdown-child"
         * defers to the SAME end-of-list pass <title> already uses,
         * so a dropdown always draws on top of every sibling that
         * comes after its trigger in the tree, regardless of where
         * the trigger itself sits - reusing this file's own already-
         * proven "draw this later" mechanism instead of inventing a
         * second one. Real layout (khtpm_core_render.c) still decides
         * WHERE it draws (or off-screen, when closed) - this only
         * decides WHEN, in draw order. */
        if (elem_has_class(c, "dropdown-child")) continue; /* deferred */
        draw_elem(c, 0);
        render_tree(c, depth + 1);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) draw_elem(c, 0);
        else if (elem_has_class(c, "dropdown-child")) { draw_elem(c, 0); render_tree(c, depth + 1); }
    }
}
