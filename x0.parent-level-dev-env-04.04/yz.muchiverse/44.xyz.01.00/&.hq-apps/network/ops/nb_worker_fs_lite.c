/* nb_worker_fs_lite.c - fs-lite natives and installer (mechanical extract, single-TU include) */
/* ---- CLI-3: fs-lite natives (sync-only, over the same read_file cap).
 * String-only payloads (no Buffer type); an optional encoding arg is
 * accepted so node-style call sites keep working. */
static JSValue nb_fs_readfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = NULL;
    if (argc > 0) p = (own = JS_ToCString(ctx, argv[0]));
    char *s = NULL; size_t n = 0;
    if (!p || !p[0] || !read_file(p, &s, &n)) {
        char eb[512];
        snprintf(eb, sizeof(eb), "ENOENT: cannot read '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, own);
        return JS_EXCEPTION;
    }
    JSValue r = JS_NewStringLen(ctx, s, n);
    free(s);
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_fs_writefile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *po = NULL, *do_ = NULL;
    const char *p = argc > 0 ? (po = JS_ToCString(ctx, argv[0])) : NULL;
    const char *d = argc > 1 ? (do_ = JS_ToCString(ctx, argv[1])) : NULL;
    FILE *f = fopen(p, "wb");
    if (!f) {
        char eb[512];
        snprintf(eb, sizeof(eb), "EIO: cannot write '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
        return JS_EXCEPTION;
    }
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
    return JS_UNDEFINED;
}
static JSValue nb_fs_appendfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *po = NULL, *do_ = NULL;
    const char *p = argc > 0 ? (po = JS_ToCString(ctx, argv[0])) : NULL;
    const char *d = argc > 1 ? (do_ = JS_ToCString(ctx, argv[1])) : NULL;
    FILE *f = fopen(p, "ab");
    if (!f) {
        char eb[512];
        snprintf(eb, sizeof(eb), "EIO: cannot append '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
        return JS_EXCEPTION;
    }
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
    return JS_UNDEFINED;
}
static JSValue nb_fs_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = argc > 0 ? (own = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue r = JS_NewBool(ctx, p && p[0] && access(p, F_OK) == 0);
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_fs_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = argc > 0 ? (own = JS_ToCString(ctx, argv[0])) : NULL;
    if (!p || !p[0]) {
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "EINVAL: empty mkdir path"));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, own);
        return JS_EXCEPTION;
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", p);
    for (char *q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; mkdir(tmp, 0755); *q = '/'; }
    }
    mkdir(tmp, 0755);
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
static void nb_install_fs(JSContext *ctx) {
    JSValue fs = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fs, "readFileSync",   JS_NewCFunction(ctx, nb_fs_readfile, "readFileSync", 1));
    JS_SetPropertyStr(ctx, fs, "writeFileSync",  JS_NewCFunction(ctx, nb_fs_writefile, "writeFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "appendFileSync", JS_NewCFunction(ctx, nb_fs_appendfile, "appendFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "existsSync",     JS_NewCFunction(ctx, nb_fs_exists, "existsSync", 1));
    JS_SetPropertyStr(ctx, fs, "mkdirSync",      JS_NewCFunction(ctx, nb_fs_mkdir, "mkdirSync", 1));
    JSValue fsg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, fsg, "__nb_fs", fs);
    JS_FreeValue(ctx, fsg);
}
