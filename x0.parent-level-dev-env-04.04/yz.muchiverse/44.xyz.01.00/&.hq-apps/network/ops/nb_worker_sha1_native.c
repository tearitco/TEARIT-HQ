/* nb_worker_sha1_native.c - generic SHA-1 native helper (mechanical extract, single-TU include) */
/* __nb_sha1(str) -> base64 of the 20-byte SHA-1 digest (plain ASCII). */
static JSValue nb_sha1_native(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    size_t n = 0;
    char *own = NULL;
    const char *t = "";
    if (argc > 0) t = (own = JS_ToCStringLen(ctx, &n, argv[0]));
    uint8_t out[20];
    nbsha1((const uint8_t *)t, n, out);
    char b64[29];
    nbsha1_b64_20(out, b64);
    JS_FreeCString(ctx, own);
    return JS_NewString(ctx, b64);
}
