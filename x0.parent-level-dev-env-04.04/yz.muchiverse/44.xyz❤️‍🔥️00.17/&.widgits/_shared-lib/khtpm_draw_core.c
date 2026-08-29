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

static unsigned long alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') {
        if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    } else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) {
        return c.pixel;
    }
    return BlackPixel(dpy, screen);
}

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

/* REAL FIX 2026-08-25 (live report: "green on gold isn't readable" - the
 * nav badge's fixed #cccccc unfocused color read as a muddy green-ish
 * smear against bright #ffd700 gold tiles/bookmark rows). Picks black or
 * light gray by the element's own background luminance instead of a
 * single hardcoded color - readable on both the dark #141414 chrome AND
 * any light/gold tile bg, without a per-mode special case. */
static const char *badge_contrast_color(const CssStyle *st) {
    if (!st->has_bg_color || st->bg_color[0] != '#' || strlen(st->bg_color) < 7) return "#cccccc";
    unsigned int r, g, b;
    sscanf(st->bg_color + 1, "%02x%02x%02x", &r, &g, &b);
    double luma = 0.299 * r + 0.587 * g + 0.114 * b;
    return luma > 140 ? "#000000" : "#cccccc";
}
/* REAL FIX 2026-08-25 (live report: "bright yellow highlight and orange
 * nav text" unreadable on cursword's bookmark rows) - the FOCUSED badge
 * used a single hardcoded #ff8c00 unconditionally, the exact same bug
 * class badge_contrast_color() above already fixed for the UNFOCUSED
 * case, just never ported to the focused branch. On the dark #141414
 * chrome, orange-on-dark is fine (matches the focus rectangle); on a
 * light/gold row background (e.g. bookmarks' own #d9b64a), orange-on-
 * gold has almost no contrast. Same luma test as badge_contrast_color(),
 * just a different pair of colors so focus stays visually distinct from
 * the plain unfocused badge even on a light bg. */
static const char *badge_focus_color(const CssStyle *st) {
    if (!st->has_bg_color || st->bg_color[0] != '#' || strlen(st->bg_color) < 7) return "#ff8c00";
    unsigned int r, g, b;
    sscanf(st->bg_color + 1, "%02x%02x%02x", &r, &g, &b);
    double luma = 0.299 * r + 0.587 * g + 0.114 * b;
    return luma > 140 ? "#7a1a00" : "#ff8c00";
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
    if (e->style.has_bg_color) {
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
    if (e->nav_index > 0) {
        int focused = (e->nav_index == g_focus_nav);
        char prefix[8];
        int is_scope = (g_dbhq_active_scope_root && e == g_dbhq_active_scope_root);
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
            int box_w = e->w - 2 * pad_s, box_h = e->h - 2 * pad_s;
            int px = box_w < box_h ? box_w : box_h;
            if (px > HQ_SPRITE_PX_MAX) px = HQ_SPRITE_PX_MAX;
            if (px > 0) {
                unsigned long bg_pixel = e->style.has_bg_color
                    ? alloc_pixel(e->style.bg_color)
                    : WhitePixel(dpy, screen);
                hq_blit_sprite(sp, e->x + (e->w - px) / 2, e->y + (e->h - px) / 2, px, bg_pixel);
                drew_sprite = 1;
            }
        }
    }
    if (!drew_sprite && e->label[0]) {
        XftFont *font = font_for(&e->style);
        XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : "#cccccc");
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)e->label, (int)strlen(e->label), &extents);
        int ty = e->y + (e->h + font->ascent - font->descent) / 2;
        if (ty < e->y + font->ascent) ty = e->y + font->ascent + pad / 2;
        draw_text_emoji(font, &col, badge_label_x, ty, e->label);
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
    }
    /* Badge draws LAST - see the big comment above. For sprite tiles,
     * position it in the real ~16px row-gap ABOVE the tile (matching
     * khtpm_hq_render.c's own live-verified fix, "i think they should go
     * above the tile, not on it") with a solid backing chip for
     * contrast against any sprite color; non-sprite elements keep the
     * original inline position. */
    if (e->nav_index > 0 && nav_badge_font) {
        int focused = (e->nav_index == g_focus_nav);
        int numy = e->y + (e->h + nav_badge_font->ascent - nav_badge_font->descent) / 2;
        if (e->sprite[0]) {
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
        }
        /* Sprite tiles draw the badge on the dark #141414 backing chip
         * above (not on the tile's own bg), so it always needs the
         * light color - badge_contrast_color() would otherwise read the
         * GOLD tile's own bg and (wrongly) pick black for a badge that's
         * actually sitting on a dark chip. */
        XftColor numcol = xft_color(focused ? (e->sprite[0] ? "#ff8c00" : badge_focus_color(&e->style)) : (e->sprite[0] ? "#cccccc" : badge_contrast_color(&e->style)));
        /* REAL, NEW 2026-08-25 (live report: a narrow, right-edge-pinned
         * element's badge ran off the visible window - see
         * badge_align_left's own declaration comment in khtpm_render_
         * core.c). Ends the badge AT the element's own left edge instead
         * of starting it at label_x and growing rightward off-screen. */
        int draw_x = e->badge_align_left ? (e->x - (int)nav_badge_ext.width - scaled(4)) : label_x;
        XftDrawStringUtf8(xftdraw_buf, &numcol, nav_badge_font, draw_x, numy, (const FcChar8 *)nav_badge, (int)strlen(nav_badge));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
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
        draw_elem(c, 0);
        render_tree(c, depth + 1);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) draw_elem(c, 0);
    }
}
