/* nb_worker_canvas_get.c - canvas context lookup/resize sync helper (mechanical extract, single-TU include) */
static NbCanvas *nb_canvas_get(JSContext *ctx, JSValueConst ctxobj) {
    JSValue v = JS_GetPropertyStr(ctx, ctxobj, "__nb_canvas_id");
    int id = -1;
    if (JS_IsNumber(v)) JS_ToInt32(ctx, &id, v);
    JS_FreeValue(ctx, v);
    if (id < 0 || id >= NB_CANVAS_MAX || !g_canvases[id].used) return NULL;
    /* post-creation resize (spec: setting width/height clears): the props
     * live on the element, so sync here - every draw op funnels through. */
    JSValue ev = JS_GetPropertyStr(ctx, ctxobj, "__nb_elem");
    if (JS_IsObject(ev)) {
        int w = (int)nb_ctx_num(ctx, ev, "width", g_canvases[id].w);
        int h = (int)nb_ctx_num(ctx, ev, "height", g_canvases[id].h);
        if (w < 1) w = 1; if (h < 1) h = 1;
        if ((size_t)w * h > (size_t)NB_CANVAS_PX_MAX) { w = 2048; h = 2048; }
        if ((w != g_canvases[id].w || h != g_canvases[id].h)) {
            unsigned char *px = calloc((size_t)w * h, 4);
            if (px) {
                free(g_canvases[id].px);
                g_canvases[id].px = px;
                g_canvases[id].w = w; g_canvases[id].h = h;
            }
        }
    }
    JS_FreeValue(ctx, ev);
    return &g_canvases[id];
}
