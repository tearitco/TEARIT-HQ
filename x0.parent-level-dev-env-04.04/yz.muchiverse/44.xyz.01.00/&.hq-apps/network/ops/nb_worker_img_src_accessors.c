/* nb_worker_img_src_accessors.c - image src get/set accessors (mechanical extract, single-TU include) */
/* HTMLImageElement src accessor — Step 1: fetch via nb_fetch_sync and fire load/error.
 * No decode yet; just verifies that img src triggers network and onload. */
static JSValue nb_img_src_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    return JS_NewString(ctx, img_get_src(n));
}
static JSValue nb_img_src_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (argc < 1) return JS_UNDEFINED;
    JSValueConst val2 = argv[0];
    JSValue tmp = JS_ToString(ctx, val2);
    char *s = JS_ToCString(ctx, tmp);
    JS_FreeValue(ctx, tmp);
    if (s) {
        img_set_src(n, s);
        if (s[0]) {
            JSValue args[2];
            args[0] = JS_NewString(ctx, "GET");
            args[1] = JS_NewString(ctx, s);
            JSValue res = nb_fetch_sync(ctx, JS_UNDEFINED, 2, args);
            JSValue okv = JS_GetPropertyStr(ctx, res, "ok");
            int ok = JS_ToBool(ctx, okv);
            JS_FreeValue(ctx, okv);
            if (ok) {
                /* Prefer the binary-safe bytes field; the body string
                 * NUL-truncates binary payloads (PNG dies at IHDR). */
                unsigned char *img_bin = NULL; size_t img_n = 0;
                JSValue bytesv = JS_GetPropertyStr(ctx, res, "bytes");
                JSValue blv = JS_IsObject(bytesv) ? JS_GetPropertyStr(ctx, bytesv, "length") : JS_UNDEFINED;
                int32_t blen = 0;
                if (JS_IsNumber(blv)) JS_ToInt32(ctx, &blen, blv);
                JS_FreeValue(ctx, blv);
                if (blen > 0 && blen <= 64 * 1024 * 1024) {
                    img_bin = malloc((size_t)blen);
                    if (img_bin) {
                        img_n = (size_t)blen;
                        for (int32_t i = 0; i < blen; i++) {
                            JSValue e = JS_GetPropertyUint32(ctx, bytesv, (uint32_t)i);
                            int32_t ev = 0;
                            if (JS_IsNumber(e)) JS_ToInt32(ctx, &ev, e);
                            JS_FreeValue(ctx, e);
                            img_bin[i] = (unsigned char)(ev & 0xFF);
                        }
                    } else img_n = 0;
                }
                JS_FreeValue(ctx, bytesv);
                if (!img_bin) {
                    JSValue bodyv = JS_GetPropertyStr(ctx, res, "body");
                    char *bstr = JS_ToCString(ctx, bodyv);
                    if (bstr) {
                    unsigned char *png_data = NULL; size_t png_len = 0;
                    if (strncmp(s, "data:image/", 11) == 0) {
                        const char *comma = strchr(s, ',');
                        if (comma && strstr(s, ";base64,")) png_data = b64_decode(comma+1, &png_len);
                        else if (comma) { png_data = (unsigned char *)strdup(comma+1); png_len = strlen(comma+1); }
                    } else {
                        png_data = (unsigned char *)strdup(bstr);
                        png_len = strlen(bstr);
                    }
                    if (png_data && png_len) {
                        int w = 0, h = 0, comp = 0;
                        unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &w, &h, &comp, 4);
                        if (rgba) img_set_decoded(n, w, h, rgba);
                        free(png_data);
                    }
                    JS_FreeCString(ctx, bstr);
                }
                JS_FreeValue(ctx, bodyv);
                }
                /* bytes path succeeded above: decode the exact bytes */
                if (img_bin && img_n) {
                    int w = 0, h = 0, comp = 0;
                    unsigned char *rgba = stbi_load_from_memory(img_bin, (int)(img_n > INT_MAX ? INT_MAX : img_n), &w, &h, &comp, 4);
                    if (rgba) img_set_decoded(n, w, h, rgba);
                    free(img_bin);
                }
            }
            const char *evtype = ok ? "load" : "error";
            char js[64];
            snprintf(js, sizeof(js), "new Event('%s',{bubbles:false})", evtype);
            JSValue ev = JS_Eval(ctx, js, strlen(js), "<img src>", JS_EVAL_TYPE_GLOBAL);
            if (!JS_IsException(ev)) {
                dispatch_event(ctx, EVT_NODE, n, ev, 0);
                JS_FreeValue(ctx, ev);
            } else JS_FreeValue(ctx, JS_GetException(ctx));
            JS_FreeValue(ctx, res);
            JS_FreeValue(ctx, args[0]); JS_FreeValue(ctx, args[1]);
        }
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}
