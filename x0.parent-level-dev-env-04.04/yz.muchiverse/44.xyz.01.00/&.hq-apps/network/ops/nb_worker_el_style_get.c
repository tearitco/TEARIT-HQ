/* nb_worker_el_style_get.c - element style getter (mechanical extract, single-TU include) */
static JSValue nb_el_style_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue cached = JS_GetPropertyStr(ctx, this_val, STYK);
    if (JS_IsObject(cached)) return cached;         /* identity-cached snapshot */
    JS_FreeValue(ctx, cached);
    JSValue o = JS_NewObject(ctx);                  /* style snapshot */
    NbNode *n = get_this(ctx, this_val);
    if (n) {
        const char *attr = nb_attr_get(n, "style");
        if (attr && *attr) {
            const char *p = attr;
            while (*p) {
                while (*p && (*p == ';' || *p == ' ' || *p == '\t' || *p == '\n')) p++;
                if (!*p) break;
                const char *cs = p;
                while (*p && *p != ';') p++;
                const char *colon = memchr(cs, ':', (size_t)(p - cs));
                if (colon && p > cs) {
                    size_t kl = (size_t)(colon - cs);
                    while (kl && (cs[kl - 1] == ' ' || cs[kl - 1] == '\t')) kl--;
                    if (kl > 0 && kl < 48) {
                        char kbuf[48];
                        for (size_t i = 0; i < kl; i++)
                            kbuf[i] = (char)tolower((unsigned char)cs[i]);
                        kbuf[kl] = 0;
                        const char *vs = colon + 1;
                        size_t vn = (size_t)(p - vs);
                        while (vn && (vs[vn - 1] == ' ' || vs[vn - 1] == '\t' ||
                                      vs[vn - 1] == '\n')) vn--;
                        char vbuf[96];
                        size_t vv = vn < sizeof(vbuf) - 1 ? vn : sizeof(vbuf) - 1;
                        memcpy(vbuf, vs, vv);
                        vbuf[vv] = 0;
                        JS_SetPropertyStr(ctx, o, kbuf, JS_NewString(ctx, vbuf));
                    }
                }
            }
        }
    }
    JS_SetPropertyStr(ctx, this_val, STYK, JS_DupValue(ctx, o));
    return o;
}
