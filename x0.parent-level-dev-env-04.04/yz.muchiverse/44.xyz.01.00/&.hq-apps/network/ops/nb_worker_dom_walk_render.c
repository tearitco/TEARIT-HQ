/* nb_worker_dom_walk_render.c - DOM walk/render serializer (mechanical extract, single-TU include) */
static void dom_walk_render(const NbNode *n, int *titled, SB *b, int no_text) {
    if (!n) return;
    const char *tg = n->tag;
    if (tg && !strcmp(tg, "#comment")) return;
    /* Raw-text/RCDATA bodies are data, not visible prose: script/style/
     * noscript/xmp/iframe/noembed/noframes/plaintext text now survives parsing
     * (JSON data islands, style rules, embedded markup strings) and must never
     * become a TEXT row. A <title> is handled below. */
    if (tg && (!strcasecmp(tg, "script") || !strcasecmp(tg, "style") || !strcasecmp(tg, "noscript")
            || !strcasecmp(tg, "xmp") || !strcasecmp(tg, "iframe") || !strcasecmp(tg, "noembed")
            || !strcasecmp(tg, "noframes") || !strcasecmp(tg, "plaintext")))
        return;
    /* REAL, NEW 2026-10-07 (live scroll-through: Wikipedia's own footer
     * farm rendered as article content) - prune chrome SUBTREES here,
     * structurally: nav/footer/aside landmarks + ARIA sectioning roles.
     * Same predicate family as the static extractor's tag_is_chrome()
     * (worker side of the lane split) - never text matching, so no site
     * can ever be hardcoded. Whole subtree unwalked. */
    if (tg) {
        if (!strcasecmp(tg, "nav") || !strcasecmp(tg, "footer") || !strcasecmp(tg, "aside"))
            return;
        const char *role = nb_attr_get(n, "role");
        if (role && (!strcasecmp(role, "navigation") || !strcasecmp(role, "banner") ||
                     !strcasecmp(role, "complementary") || !strcasecmp(role, "search") ||
                     !strcasecmp(role, "contentinfo")))
            return;
    }
    int caption_used = 0;   /* element's own text already surfaced as TITLE/LINK/IMG */
    if (tg && !strcasecmp(tg, "title") && !*titled) {
        SB t = {0, 0, 0};
        node_text_content(n, &t);
        rw_sel(b, n);
        rw_row(b, "TITLE", t.s ? t.s : "");
        free(t.s);
        *titled = 1;
        caption_used = 1;
    } else if (tg && !strcasecmp(tg, "a")) {
        const char *href = nb_attr_get(n, "href");
        if (href && *href) {
            SB t = {0, 0, 0};
            node_text_content(n, &t);
            char linkbuf[2048];
            snprintf(linkbuf, sizeof(linkbuf), "%s|%s", href, t.s && t.s[0] ? t.s : href);
            rw_sel(b, n);
            rw_row(b, "LINK", linkbuf);
            free(t.s);
            caption_used = 1;
        }
    } else if (tg && !strcasecmp(tg, "img")) {
        char srcbuf[1024] = "", altbuf[1024] = "";
        snprintf(srcbuf, sizeof(srcbuf), "%s", img_get_src(n));
        snprintf(altbuf, sizeof(altbuf), "%s", nb_attr_get(n, "alt"));
        if (srcbuf[0]) {
            int dw = 0, dh = 0;
            if (!img_get_decoded(n, &dw, &dh) && srcbuf[0]) {
                if (strncmp(srcbuf, "data:image/", 11) == 0) {
                    const char *comma = strchr(srcbuf, ',');
                    if (comma && strstr(srcbuf, ";base64,")) {
                        size_t png_len = 0;
                        unsigned char *png_data = b64_decode(comma+1, &png_len);
                        if (png_data && png_len) {
                            int w = 0, h = 0, comp = 0;
                            unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &w, &h, &comp, 4);
                            if (rgba) { img_set_decoded((NbNode *)n, w, h, rgba); dw = w; dh = h; }
                            free(png_data);
                        }
                    }
                }
            }
            char imgbuf[2048];
            if (img_get_decoded(n, &dw, &dh) && dw > 0 && dh > 0) {
                // Write decoded RGBA to temp file for renderer (Step 3 wire)
                char imgpath[256];
                snprintf(imgpath, sizeof(imgpath), "/tmp/nb_img_%p.png", (void*)n);
                // Find decoded data
                unsigned char *rgba = NULL;
                for (int i = 0; i < g_img_decoded_count; i++) if (g_img_decoded[i].n == n) { rgba = g_img_decoded[i].data; break; }
                if (rgba) stbi_write_png(imgpath, dw, dh, 4, rgba, dw * 4);
                snprintf(imgbuf, sizeof(imgbuf), "%s|%d|%d|%s|%s", srcbuf, dw, dh, imgpath, altbuf);
            } else {
                snprintf(imgbuf, sizeof(imgbuf), "%s|%s", srcbuf, altbuf);
            }
            rw_sel(b, n);
            rw_row(b, "IMG", imgbuf);
            caption_used = 1;
        }
    }
    /* the fetch.dom wire form carries an element's direct text inline on
     * the element node, so surface n->text itself (skip captions above and
     * whitespace-only runs, matching the static extractor) */
    if (!no_text && !caption_used && n->text && n->text[0] && strspn(n->text, " \t\r\n") < strlen(n->text)) {
        size_t L = strlen(n->text);
        char *copy = malloc(L + 1);
        /* #text nodes have no selector of their own; a click on their row
         * belongs to the element that owns them, as events would bubble. */
        const NbNode *seln = (n->tag && n->tag[0]) ? n : (n->parent ? n->parent : n);
        if (copy) { memcpy(copy, n->text, L + 1); rw_wrap(b, copy, seln); free(copy); }
    }
    /* A caption row (TITLE/LINK/IMG) already carries this element's whole
     * textContent, so a #text child must not be emitted a second time as a
     * TEXT row. Only TEXT is silenced; nested elements still render. */
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        dom_walk_render(c, titled, b, no_text || caption_used);
}
