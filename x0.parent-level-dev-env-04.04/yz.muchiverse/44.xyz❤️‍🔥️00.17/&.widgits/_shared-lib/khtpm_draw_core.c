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
static void draw_elem(Elem *e, int hover_id_hash) {
    (void)hover_id_hash;
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
    if (e->nav_index > 0) {
        char badge[16];
        int focused = (e->nav_index == g_focus_nav);
        snprintf(badge, sizeof(badge), "[%c]%d.", focused ? '>' : ' ', e->nav_index);
        char numspec[48];
        snprintf(numspec, sizeof(numspec), "DejaVu Sans Mono:pixelsize=%d", scaled(9));
        XftFont *numfont = XftFontOpenName(dpy, screen, numspec);
        if (!numfont) { snprintf(numspec, sizeof(numspec), "DejaVu Sans:pixelsize=%d", scaled(9)); numfont = XftFontOpenName(dpy, screen, numspec); }
        XftColor numcol = xft_color(focused ? "#ff8c00" : "#9a9a9a");
        XGlyphInfo numext;
        XftTextExtentsUtf8(dpy, numfont, (const FcChar8 *)badge, (int)strlen(badge), &numext);
        int numy = e->y + (e->h + numfont->ascent - numfont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &numcol, numfont, label_x, numy, (const FcChar8 *)badge, (int)strlen(badge));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
        label_x += numext.width + 5;
        XftFontClose(dpy, numfont);
    }
    if (e->label[0]) {
        XftFont *font = font_for(&e->style);
        XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : "#cccccc");
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)e->label, (int)strlen(e->label), &extents);
        int ty = e->y + (e->h + font->ascent - font->descent) / 2;
        if (ty < e->y + font->ascent) ty = e->y + font->ascent + pad / 2;
        XftDrawStringUtf8(xftdraw_buf, &col, font, label_x, ty, (const FcChar8 *)e->label, (int)strlen(e->label));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
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
