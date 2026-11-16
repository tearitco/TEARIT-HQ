/* nb_worker_font_parse.c - canvas font string parsing helpers (mechanical extract, single-TU include) */
/* "12px sans-serif" -> 12 (default 10) */
/* Font family mapping for canvas fingerprinting - returns a multiplier for width */
static double nb_font_family_scale(const char *font_family) {
    if (!font_family) return 1.0;
    /* Common font families with different average character widths */
    if (strstr(font_family, "Arial") || strstr(font_family, "Helvetica") || strstr(font_family, "sans-serif")) return 1.0;
    if (strstr(font_family, "Times") || strstr(font_family, "serif")) return 0.85;
    if (strstr(font_family, "Courier") || strstr(font_family, "monospace")) return 1.2;
    if (strstr(font_family, "Verdana")) return 1.1;
    if (strstr(font_family, "Georgia")) return 0.9;
    if (strstr(font_family, "Tahoma")) return 1.05;
    if (strstr(font_family, "Trebuchet")) return 0.95;
    if (strstr(font_family, "Impact")) return 1.3;
    if (strstr(font_family, "Comic")) return 1.15;
    return 1.0;
}

/* Extract font family from canvas font string (e.g., "16px Arial" -> "Arial", "16px 'Times New Roman'" -> "Times New Roman") */
static void nb_font_family_from_ctx(JSContext *ctx, JSValueConst o, char *out, size_t n) {
    JSValue v = JS_GetPropertyStr(ctx, o, "font");
    const char *s = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    if (s) {
        const char *p = s;
        /* Skip leading non-digit, non-dot characters (font style/weight) */
        while (*p && (*p < '0' || *p > '9') && *p != '.') p++;
        if (*p) {
            /* Skip the font size number */
            while (*p && (*p >= '0' && *p <= '9' || *p == '.')) p++;
            /* Skip unit (px, pt, em, rem, etc.) - skip letters */
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) p++;
            /* Skip whitespace */
            while (*p && *p == ' ') p++;
            if (*p) {
                const char *end = p;
                char quote = 0;
                if (*p == '"' || *p == '\'') {
                    quote = *p;
                    p++;
                    end = p;
                    /* Find closing quote, but don't go past end of string */
                    while (*end && *end != quote && end - p < 63) end++;
                } else {
                    /* Unquoted: find end of font family (space, comma, or end of string) */
                    while (*end && *end != '"' && *end != '\'' && *end != ',' && *end != ';' && end - p < 63) end++;
                }
                size_t len = end - p;
                if (len >= n) len = n - 1;
                memcpy(out, p, len);
                out[len] = 0;
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, v);
                return;
            }
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, v);
    snprintf(out, n, "sans-serif");
}

static double nb_font_size_px(JSContext *ctx, JSValueConst o) {
    JSValue v = JS_GetPropertyStr(ctx, o, "font");
    const char *s = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    double px = 10.0;
    if (s) {
        const char *p = s;
        while (*p && (*p < '0' || *p > '9') && *p != '.') p++;
        if (*p) {
            px = strtod(p, NULL);
            if (strstr(p, "pt")) px *= 96.0 / 72.0;
            if (px < 1) px = 1; if (px > 256) px = 256;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, v);
    return px;
}
