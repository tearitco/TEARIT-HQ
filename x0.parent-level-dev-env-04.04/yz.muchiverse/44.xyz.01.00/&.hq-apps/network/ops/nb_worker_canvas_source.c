/* nb_worker_canvas_source.c - canvas/image source pixel resolver (mechanical extract, single-TU include) */
/* resolve an ImageData-like {width,height,data} or canvas element to pixels */
static int nb_c2d_source(JSContext *ctx, JSValueConst v,
                         int *sw, int *sh, unsigned char **spx, int *owned) {
    *sw = 0; *sh = 0; *spx = NULL; *owned = 0;
    if (!JS_IsObject(v)) return 0;
    /* img elements with decoded pixels (data: URLs decode inline during
     * the walk; remote-src Image onload wiring is out of slice) */
    {
        NbNode *n = get_node(ctx, v);
        if (n) {
            int dw = 0, dh = 0;
            if (img_get_decoded(n, &dw, &dh)) {
                for (int i = 0; i < g_img_decoded_count; i++)
                    if (g_img_decoded[i].n == n && g_img_decoded[i].data) {
                        *sw = g_img_decoded[i].w; *sh = g_img_decoded[i].h;
                        *spx = g_img_decoded[i].data;
                        return 1;
                    }
            }
        }
    }
    JSValue cid = JS_GetPropertyStr(ctx, v, "__nb_ctx");
    int id = -1;
    if (JS_IsObject(cid)) {
        JSValue iv = JS_GetPropertyStr(ctx, cid, "__nb_canvas_id");
        if (JS_IsNumber(iv)) JS_ToInt32(ctx, &id, iv);
        JS_FreeValue(ctx, iv);
    }
    JS_FreeValue(ctx, cid);
    if (id >= 0 && id < NB_CANVAS_MAX && g_canvases[id].used) {
        *sw = g_canvases[id].w; *sh = g_canvases[id].h;
        *spx = g_canvases[id].px;
        return 1;
    }
    JSValue wv = JS_GetPropertyStr(ctx, v, "width");
    JSValue hv = JS_GetPropertyStr(ctx, v, "height");
    JSValue dv = JS_GetPropertyStr(ctx, v, "data");
    int32_t w = 0, h = 0;
    if (JS_IsNumber(wv)) JS_ToInt32(ctx, &w, wv);
    if (JS_IsNumber(hv)) JS_ToInt32(ctx, &h, hv);
    int ok = 0;
    if (w > 0 && h > 0 && (size_t)w * h < (size_t)NB_CANVAS_PX_MAX && JS_IsObject(dv)) {
        JSValue lv = JS_GetPropertyStr(ctx, dv, "length");
        int32_t len = 0;
        if (JS_IsNumber(lv)) JS_ToInt32(ctx, &len, lv);
        JS_FreeValue(ctx, lv);
        if (len >= 4 * w * h) {
            unsigned char *buf = malloc((size_t)4 * w * h);
            if (buf) {
                ok = 1;
                for (int32_t i = 0; i < 4 * w * h; i++) {
                    JSValue e = JS_GetPropertyUint32(ctx, dv, (uint32_t)i);
                    int32_t ev = 0;
                    if (JS_IsNumber(e)) JS_ToInt32(ctx, &ev, e);
                    JS_FreeValue(ctx, e);
                    buf[i] = (unsigned char)(ev < 0 ? 0 : ev > 255 ? 255 : ev);
                }
                *spx = buf; *owned = 1; *sw = w; *sh = h;
            }
        }
    }
    JS_FreeValue(ctx, wv); JS_FreeValue(ctx, hv); JS_FreeValue(ctx, dv);
    return ok;
}
