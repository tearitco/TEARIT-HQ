/* nb_worker_font_load.c - minimal TTF font load and table parsing (mechanical extract, single-TU include) */
static void nb_font_load(void) {
    static int tried = 0;
    if (g_font_data || tried) return;
    tried = 1;
    const char *paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0 && n < 64 * 1024 * 1024) {
            unsigned char *d = malloc((size_t)n);
            if (d && fread(d, 1, (size_t)n, f) == (size_t)n) {
                g_font_data = d; g_font_len = (size_t)n;
                fclose(f);
                break;
            }
            free(d);
        }
        fclose(f);
    }
    if (!g_font_data) return;
    const unsigned char *t = NULL; unsigned ln = 0;
    if (nb_font_tab("head", &t, &ln) && ln >= 54) {
        g_font_upm = u16r(t + 18); if (!g_font_upm) g_font_upm = 2048;
        g_font_loca_short = (u16r(t + 50) == 0);
    }
    if (nb_font_tab("maxp", &t, &ln) && ln >= 6) g_font_nglyph = u16r(t + 4);
    if (nb_font_tab("hhea", &t, &ln) && ln >= 36) {
        g_font_asc = i16r(t + 4); g_font_desc = i16r(t + 6);
        g_font_nhmtx = u16r(t + 34);
    }
    nb_font_tab("loca", &g_font_loca, &ln);
    nb_font_tab("glyf", &g_font_glyf, &ln);
    nb_font_tab("hmtx", &g_font_hmtx, &ln);
    /* cmap: prefer format 4, windows BMP (3,1) else unicode (0,3) */
    if (nb_font_tab("cmap", &t, &ln) && ln >= 4) {
        unsigned nn = u16r(t + 2);
        const unsigned char *best = NULL;
        for (unsigned i = 0; i < nn; i++) {
            const unsigned char *e = t + 4 + i * 8;
            if (e + 8 > t + ln) break;
            unsigned pid = u16r(e), eid = u16r(e + 2);
            unsigned off = u32r(e + 4);
            if (off + 6 > ln) continue;
            if (u16r(t + off) != 4) continue;
            if (!best || (pid == 3 && eid == 1)) best = t + off;
        }
        g_font_cmap4 = best;
    }
    if (!g_font_cmap4 || !g_font_loca || !g_font_glyf || !g_font_nglyph) {
        free(g_font_data); g_font_data = NULL;
    }
}
