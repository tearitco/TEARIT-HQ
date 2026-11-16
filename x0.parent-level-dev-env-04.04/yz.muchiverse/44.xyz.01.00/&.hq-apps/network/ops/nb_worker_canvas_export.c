/* nb_worker_canvas_export.c - canvas export helpers (mechanical extract, single-TU include) */
static char *nb_b64enc(const unsigned char *in, size_t n) {
    static const char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = ((n + 2) / 3) * 4;
    char *s = malloc(o + 1);
    if (!s) return NULL;
    size_t i = 0, k = 0;
    while (i < n) {
        int n0 = (int)(n - i);
        unsigned a = in[i++], b = n0 > 1 ? in[i++] : 0, c = n0 > 2 ? in[i++] : 0;
        unsigned triple = (a << 16) | (b << 8) | c;
        s[k++] = tab[(triple >> 18) & 63];
        s[k++] = tab[(triple >> 12) & 63];
        s[k++] = n0 > 1 ? tab[(triple >> 6) & 63] : '=';
        s[k++] = n0 > 2 ? tab[triple & 63] : '=';
    }
    s[k] = 0;
    return s;
}
/* toDataURL: PNG-encode the backing store (stb_image_write, already
 * linked for the /tmp decode path) */
static JSValue nb_canvas_toDataURL(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbCanvas *cv = NULL;
    JSValue cid = JS_GetPropertyStr(ctx, this_val, "__nb_ctx");
    if (JS_IsObject(cid)) {
        JSValue iv = JS_GetPropertyStr(ctx, cid, "__nb_canvas_id");
        int id = -1;
        if (JS_IsNumber(iv)) JS_ToInt32(ctx, &id, iv);
        JS_FreeValue(ctx, iv);
        if (id >= 0 && id < NB_CANVAS_MAX && g_canvases[id].used) cv = &g_canvases[id];
    }
    JS_FreeValue(ctx, cid);
    if (!cv || !cv->px) return JS_NewString(ctx, "data:,");
    int pnglen = 0;
    unsigned char *png = stbi_write_png_to_mem(cv->px, cv->w * 4, cv->w, cv->h, 4, &pnglen);
    if (!png || pnglen <= 0) { free(png); return JS_NewString(ctx, "data:,"); }
    char *b64 = nb_b64enc(png, (size_t)pnglen);
    free(png);
    if (!b64) return JS_NewString(ctx, "data:,");
    size_t total = strlen("data:image/png;base64,") + strlen(b64);
    char *out = malloc(total + 1);
    if (!out) { free(b64); return JS_NewString(ctx, "data:,"); }
    snprintf(out, total + 1, "data:image/png;base64,%s", b64);
    free(b64);
    JSValue r = JS_NewString(ctx, out);
    free(out);
    return r;
}
