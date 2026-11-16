/* nb_worker_color_parse.c - canvas/CSS color parsing helper (mechanical extract, single-TU include) */
/* parse #rgb #rrggbb #rrggbbaa rgb() rgba() + 16 named; else opaque black */
static void nb_color_parse(const char *s, unsigned char out[4]) {
    out[0] = out[1] = out[2] = 0; out[3] = 255;
    if (!s) return;
    while (*s == ' ') s++;
    static const struct { const char *n; unsigned char c[3]; } named[] = {
        {"black",{0,0,0}},{"white",{255,255,255}},{"red",{255,0,0}},
        {"lime",{0,255,0}},{"blue",{0,0,255}},{"yellow",{255,255,0}},
        {"cyan",{0,255,255}},{"magenta",{255,0,255}},{"silver",{192,192,192}},
        {"gray",{128,128,128}},{"grey",{128,128,128}},{"maroon",{128,0,0}},
        {"green",{0,128,0}},{"navy",{0,0,128}},{"olive",{128,128,0}},
        {"teal",{0,128,128}},{"purple",{128,0,128}},{"orange",{255,165,0}},
        {"transparent",{0,0,0}},
    };
    if (!strncasecmp(s, "transparent", 11)) { out[3] = 0; return; }
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++)
        if (!strcasecmp(s, named[i].n)) {
            out[0] = named[i].c[0]; out[1] = named[i].c[1]; out[2] = named[i].c[2];
            return;
        }
    if (s[0] == '#') {
        unsigned r = 0, g = 0, b = 0, a = 255;
        size_t L = strlen(s + 1);
        if (L == 3 || L == 4) {
            char t[3] = {0};
            t[0] = t[1] = s[1]; r = (unsigned)strtoul(t, NULL, 16);
            t[0] = t[1] = s[2]; g = (unsigned)strtoul(t, NULL, 16);
            t[0] = t[1] = s[3]; b = (unsigned)strtoul(t, NULL, 16);
            if (L == 4) { t[0] = t[1] = s[4]; a = (unsigned)strtoul(t, NULL, 16); }
        } else if (L == 6 || L == 8) {
            char t[3] = {0};
            memcpy(t, s + 1, 2); r = (unsigned)strtoul(t, NULL, 16);
            memcpy(t, s + 3, 2); g = (unsigned)strtoul(t, NULL, 16);
            memcpy(t, s + 5, 2); b = (unsigned)strtoul(t, NULL, 16);
            if (L == 8) { memcpy(t, s + 7, 2); a = (unsigned)strtoul(t, NULL, 16); }
        } else return;
        out[0] = (unsigned char)r; out[1] = (unsigned char)g;
        out[2] = (unsigned char)b; out[3] = (unsigned char)a;
        return;
    }
    if (!strncasecmp(s, "rgba(", 5) || !strncasecmp(s, "rgb(", 4)) {
        const char *p = strchr(s, '(');
        if (!p) return;
        double v[4] = {0, 0, 0, 255};
        int got = 0;
        char *end = NULL;
        for (int i = 0; i < 4 && p; i++) {
            double nv = strtod(p + 1, &end);
            if (end == p + 1) break;
            v[i] = nv; got++;
            p = end;
        }
        int is_pct = strchr(s, '%') != NULL;
        for (int i = 0; i < 3; i++) {
            double c = is_pct ? v[i] * 255.0 / 100.0 : v[i];
            if (c < 0) c = 0; if (c > 255) c = 255;
            out[i] = (unsigned char)(c + 0.5);
        }
        /* 4th component (rgba only): 0..1 fraction, else clamp. */
        double al = 255.0;
        if (got >= 4)
            al = (v[3] >= 0.0 && v[3] <= 1.0) ? v[3] * 255.0 : v[3];
        if (al < 0) al = 0; if (al > 255) al = 255;
        out[3] = (unsigned char)(al + 0.5);
        return;
    }
}
