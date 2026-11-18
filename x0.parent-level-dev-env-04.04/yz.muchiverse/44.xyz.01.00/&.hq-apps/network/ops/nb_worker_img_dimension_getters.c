/* nb_worker_img_dimension_getters.c - image dimension/complete getters (mechanical extract, single-TU include) */
static JSValue nb_img_naturalWidth_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewInt32(ctx, 0);
    int w, h; if (img_get_decoded(n, &w, &h)) return JS_NewInt32(ctx, w);
    const char *src = img_get_src(n);
    if (src && strncmp(src, "data:image/", 11) == 0) {
        const char *comma = strchr(src, ',');
        if (comma && strstr(src, ";base64,")) {
            size_t png_len = 0; unsigned char *png_data = b64_decode(comma+1, &png_len);
            if (png_data && png_len) {
                int ww = 0, hh = 0, comp = 0;
                unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &ww, &hh, &comp, 4);
                if (rgba) { img_set_decoded(n, ww, hh, rgba); w = ww; h = hh; free(png_data); return JS_NewInt32(ctx, w); }
                free(png_data);
            }
        }
    }
    return JS_NewInt32(ctx, 0);
}
static JSValue nb_img_naturalHeight_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewInt32(ctx, 0);
    int w, h; if (img_get_decoded(n, &w, &h)) return JS_NewInt32(ctx, h);
    const char *src = img_get_src(n);
    if (src && strncmp(src, "data:image/", 11) == 0) {
        const char *comma = strchr(src, ',');
        if (comma && strstr(src, ";base64,")) {
            size_t png_len = 0; unsigned char *png_data = b64_decode(comma+1, &png_len);
            if (png_data && png_len) {
                int ww = 0, hh = 0, comp = 0;
                unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &ww, &hh, &comp, 4);
                if (rgba) { img_set_decoded(n, ww, hh, rgba); w = ww; h = hh; free(png_data); return JS_NewInt32(ctx, h); }
                free(png_data);
            }
        }
    }
    return JS_NewInt32(ctx, 0);
}
static JSValue nb_img_complete_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewBool(ctx, 0);
    int w, h; if (img_get_decoded(n, &w, &h)) return JS_NewBool(ctx, 1);
    const char *src = img_get_src(n);
    if (src && strncmp(src, "data:image/", 11) == 0) {
        const char *comma = strchr(src, ',');
        if (comma && strstr(src, ";base64,")) {
            size_t png_len = 0; unsigned char *png_data = b64_decode(comma+1, &png_len);
            if (png_data && png_len) {
                int ww = 0, hh = 0, comp = 0;
                unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &ww, &hh, &comp, 4);
                if (rgba) { img_set_decoded(n, ww, hh, rgba); return JS_NewBool(ctx, 1); }
                free(png_data);
            }
        }
    }
    return JS_NewBool(ctx, 0);
}
