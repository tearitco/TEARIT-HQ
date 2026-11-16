/* nb_worker_el_getcontext.c - canvas element getContext implementation (mechanical extract, single-TU include) */
static JSValue nb_el_getContext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
#ifdef NB_HAVE_GLES
    /* WebGL modes get a real GLES context (own object per mode); anything
     * else falls through to the 2d rasterizer below. webgl2 maps to the
     * same GLES2 core (noted limitation, not silent: VERSION reports ES). */
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char *mode = JS_ToCString(ctx, argv[0]);
        int is_gl = mode && (!strcmp(mode, "webgl") || !strcmp(mode, "experimental-webgl") ||
                             !strcmp(mode, "webgl2"));
        if (mode) JS_FreeCString(ctx, mode);
        if (is_gl) {
            JSValue cached = JS_GetPropertyStr(ctx, this_val, "__nb_glctx");
            if (JS_IsObject(cached)) return cached;
            JS_FreeValue(ctx, cached);
            int w = (int)nb_ctx_num(ctx, this_val, "width", 300);
            int h = (int)nb_ctx_num(ctx, this_val, "height", 150);
            if (w < 1) w = 1; if (h < 1) h = 1;
            if ((size_t)w * h > (size_t)NB_CANVAS_PX_MAX) { w = 2048; h = 2048; }
            /* share the canvas backing slot so GL readback publishes
             * through the existing PNG publish path */
            int cid = -1;
            for (int i = 0; i < NB_CANVAS_MAX; i++)
                if (!g_canvases[i].used) { cid = i; break; }
            if (cid < 0) return JS_NULL;
            unsigned char *px = calloc((size_t)w * h, 4);
            if (!px) return JS_NULL;
            g_canvases[cid].used = 1;
            g_canvases[cid].w = w; g_canvases[cid].h = h;
            g_canvases[cid].px = px;
            g_canvases[cid].dirty = 1;
            JSValue glc = nb_gl_new_context(ctx, this_val, cid, w, h);
            if (!JS_IsObject(glc)) {
                free(px);
                g_canvases[cid].used = 0;
                g_canvases[cid].px = NULL;
                return JS_NULL;
            }
            return glc;
        }
    }
#else
    (void)argc; (void)argv;
#endif
    JSValue cached = JS_GetPropertyStr(ctx, this_val, "__nb_ctx");
    if (JS_IsObject(cached)) return cached;
    JS_FreeValue(ctx, cached);
    JSValue c = JS_NewObject(ctx);
    const char *strprops[] = {
        "fillStyle", "strokeStyle", "filter", "font", "textAlign", "textBaseline",
        "lineCap", "lineJoin", "globalCompositeOperation", "shadowColor",
        "letterSpacing", "wordSpacing", "textTransform", "direction"
    };
    for (size_t i = 0; i < sizeof(strprops) / sizeof(strprops[0]); i++)
        JS_SetPropertyStr(ctx, c, strprops[i], JS_NewString(ctx, ""));
    const char *numprops[] = {
        "lineWidth", "globalAlpha", "shadowBlur", "shadowOffsetX", "shadowOffsetY",
        "miterLimit", "lineDashOffset"
    };
    for (size_t i = 0; i < sizeof(numprops) / sizeof(numprops[0]); i++)
        JS_SetPropertyStr(ctx, c, numprops[i], JS_NewInt32(ctx, 0));
    /* spec defaults (a zeroed globalAlpha would silently disable all
     * drawing - caught live: fillRect wrote nothing until this) */
    JS_SetPropertyStr(ctx, c, "globalAlpha", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, c, "lineWidth", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, c, "fillStyle", JS_NewString(ctx, "#000000"));
    JS_SetPropertyStr(ctx, c, "strokeStyle", JS_NewString(ctx, "#000000"));
    JS_SetPropertyStr(ctx, c, "font", JS_NewString(ctx, "10px sans-serif"));
    JS_SetPropertyStr(ctx, c, "textAlign", JS_NewString(ctx, "start"));
    JS_SetPropertyStr(ctx, c, "textBaseline", JS_NewString(ctx, "alphabetic"));
    const char *noops[] = {
        "beginPath", "closePath", "moveTo",
        "lineTo", "rect", "arc", "arcTo", "bezierCurveTo", "quadraticCurveTo",
        "fill", "stroke", "clip", "setLineDash",
        "reset", "isPointInPath", "setTransformMatrix", "drawFocusIfNeeded"
    };
    for (size_t i = 0; i < sizeof(noops) / sizeof(noops[0]); i++)
        JS_SetPropertyStr(ctx, c, noops[i], JS_NewCFunction(ctx, nb_c2d_noop, noops[i], 0));
    JS_SetPropertyStr(ctx, c, "fillRect", JS_NewCFunction(ctx, nb_c2d_fillRect, "fillRect", 4));
    JS_SetPropertyStr(ctx, c, "strokeRect", JS_NewCFunction(ctx, nb_c2d_strokeRect, "strokeRect", 4));
    JS_SetPropertyStr(ctx, c, "clearRect", JS_NewCFunction(ctx, nb_c2d_clearRect, "clearRect", 4));
    JS_SetPropertyStr(ctx, c, "save", JS_NewCFunction(ctx, nb_c2d_save, "save", 0));
    JS_SetPropertyStr(ctx, c, "restore", JS_NewCFunction(ctx, nb_c2d_restore, "restore", 0));
    JS_SetPropertyStr(ctx, c, "translate", JS_NewCFunction(ctx, nb_c2d_translate, "translate", 2));
    JS_SetPropertyStr(ctx, c, "scale", JS_NewCFunction(ctx, nb_c2d_scale, "scale", 2));
    JS_SetPropertyStr(ctx, c, "rotate", JS_NewCFunction(ctx, nb_c2d_rotate, "rotate", 1));
    JS_SetPropertyStr(ctx, c, "setTransform", JS_NewCFunction(ctx, nb_c2d_setTransform, "setTransform", 6));
    JS_SetPropertyStr(ctx, c, "transform", JS_NewCFunction(ctx, nb_c2d_transform, "transform", 6));
    JS_SetPropertyStr(ctx, c, "resetTransform", JS_NewCFunction(ctx, nb_c2d_resetTransform, "resetTransform", 0));
    JS_SetPropertyStr(ctx, c, "drawImage", JS_NewCFunction(ctx, nb_c2d_drawImage, "drawImage", 9));
    JS_SetPropertyStr(ctx, c, "putImageData", JS_NewCFunction(ctx, nb_c2d_putImageData, "putImageData", 7));
    JS_SetPropertyStr(ctx, c, "fillText", JS_NewCFunction(ctx, nb_c2d_fillText, "fillText", 4));
    JS_SetPropertyStr(ctx, c, "strokeText", JS_NewCFunction(ctx, nb_c2d_strokeText, "strokeText", 4));
    JS_SetPropertyStr(ctx, c, "getImageData", JS_NewCFunction(ctx, nb_c2d_getImageData_real, "getImageData", 4));
    JS_SetPropertyStr(ctx, c, "createImageData", JS_NewCFunction(ctx, nb_c2d_createImageData, "createImageData", 2));
    JS_SetPropertyStr(ctx, c, "getLineDash", JS_NewCFunction(ctx, nb_c2d_lineDash, "getLineDash", 0));
    JS_SetPropertyStr(ctx, c, "measureText", JS_NewCFunction(ctx, nb_c2d_measureText, "measureText", 1));
    JS_SetPropertyStr(ctx, c, "createLinearGradient", JS_NewCFunction(ctx, nb_c2d_gradient, "createLinearGradient", 4));
    JS_SetPropertyStr(ctx, c, "createRadialGradient", JS_NewCFunction(ctx, nb_c2d_gradient, "createRadialGradient", 6));
    JS_SetPropertyStr(ctx, c, "createPattern", JS_NewCFunction(ctx, nb_c2d_createPattern, "createPattern", 2));
    JS_SetPropertyStr(ctx, this_val, "__nb_ctx", JS_DupValue(ctx, c));
    /* Backing raster for this canvas element (one live context per
     * element - getContext caches __nb_ctx). Sized from the element's
     * width/height props, spec default 300x150. */
    {
        int slot = -1;
        for (int i = 0; i < NB_CANVAS_MAX; i++)
            if (!g_canvases[i].used) { slot = i; break; }
        if (slot >= 0) {
            int w = (int)nb_ctx_num(ctx, this_val, "width", 300);
            int h = (int)nb_ctx_num(ctx, this_val, "height", 150);
            if (w < 1) w = 1; if (h < 1) h = 1;
            if ((size_t)w * h > (size_t)NB_CANVAS_PX_MAX) { w = 2048; h = 2048; }
            unsigned char *px = calloc((size_t)w * h, 4);
            if (px) {
                g_canvases[slot].used = 1;
                g_canvases[slot].w = w; g_canvases[slot].h = h;
                g_canvases[slot].px = px;
                g_canvases[slot].m[0] = 1; g_canvases[slot].m[1] = 0;
                g_canvases[slot].m[2] = 0; g_canvases[slot].m[3] = 1;
                g_canvases[slot].m[4] = 0; g_canvases[slot].m[5] = 0;
                g_canvases[slot].alpha = 1;
                g_canvases[slot].save_depth = 0;
                g_canvases[slot].dirty = 0;
                g_canvases[slot].fillstyle[0] = 0;
                JS_SetPropertyStr(ctx, c, "__nb_canvas_id", JS_NewInt32(ctx, slot));
                JS_SetPropertyStr(ctx, c, "__nb_elem", JS_DupValue(ctx, this_val));
            }
        }
    }
    return c;
}
