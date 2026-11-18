/* nb_worker_cli_main.c - node-mode CLI entry (mechanical extract, single-TU include) */
static int cli_main(int argc, char **argv) {
    g_cli = 1; g_cli_log = 1;         /* bare console lines -> stdout */
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);

    /* parse: duk [--node] file.js|-- [args...] */
    int script_i = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require()+fs, ESM import/export)\n");
            fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
            fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
            return 2;
        }
        if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) continue;
        script_i = i; break;
    }
    if (script_i < 0) {
        fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require()+fs, ESM import/export)\n");
        fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
        fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
        return 2;
    }
    const char *pg = argv[script_i];
    if (strcmp(pg, "-") != 0 && access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }

    JSRuntime *rt = JS_NewRuntime();
    nb_watchdog_install(rt);
    if (!rt) return 1;
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) { JS_FreeRuntime(rt); return 1; }

    /* node-like host: console/print only, plus process. No DOM, no events,
     * no timers, no browser prelude — window/document/location are absent. */
    {
        JSValue g = JS_GetGlobalObject(ctx);
        JSValue console = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, console, "log",   JS_NewCFunction(ctx, native_log, "log", 1));
        JS_SetPropertyStr(ctx, console, "info",  JS_NewCFunction(ctx, native_log, "info", 1));
        JS_SetPropertyStr(ctx, console, "warn",  JS_NewCFunction(ctx, native_log, "warn", 1));
        JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, nb_cli_error, "error", 1));
        JS_SetPropertyStr(ctx, g, "print",  JS_NewCFunction(ctx, native_log, "print", 1));
        JS_SetPropertyStr(ctx, g, "console", console);
        JS_FreeValue(ctx, g);
    }

    /* process.argv = [interp, script, args...] (node convention) */
    {
        int nargv = 1 + (argc - script_i);
        char **a = calloc((size_t)nargv, sizeof(char *));
        if (!a) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
        a[0] = argv[0];
        for (int i = 0; i + script_i < argc; i++) a[i + 1] = argv[script_i + i];
        nb_cli_install(ctx, nargv, a);
        free(a);
    }

    /* CLI-2 CommonJS + CLI-3 fs: the only host hook the JS loader needs is
     * a module file reader; it defines `require`/`__dirname`/`__filename`
     * itself. `require('fs')` resolves to the fs-lite natives below. */
    JSValue rg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, rg, "__nb_read_file",
                      JS_NewCFunction(ctx, nb_cjs_read_file, "__nb_read_file", 1));
    JS_FreeValue(ctx, rg);
    nb_install_fs(ctx);

    /* Entry directory/file, absolute, for the entry script's require base. */
    static char entry_dir[PATH_MAX], entry_file[PATH_MAX];
    {
        if (strcmp(pg, "-") == 0) {
            if (!getcwd(entry_dir, sizeof(entry_dir))) snprintf(entry_dir, sizeof(entry_dir), ".");
            snprintf(entry_file, sizeof(entry_file), "[stdin]");
        } else {
            static char rp[PATH_MAX];
            if (!realpath(pg, rp)) snprintf(rp, sizeof(rp), "%s", pg);
            const char *abs = (rp[0] == '/' ? rp : pg);
            snprintf(entry_file, sizeof(entry_file), "%s", abs);
            const char *sl = strrchr(abs, '/');
            if (sl == abs) snprintf(entry_dir, sizeof(entry_dir), "/");
            else if (sl) { size_t d = (size_t)(sl - abs); memcpy(entry_dir, abs, d); entry_dir[d] = 0; }
            else if (!getcwd(entry_dir, sizeof(entry_dir))) snprintf(entry_dir, sizeof(entry_dir), ".");
        }
    }
    char errbuf[512];
    if (peval_budget(ctx, g_cjs_prelude, strlen(g_cjs_prelude), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "%s\n", errbuf[0] ? errbuf : "loader error");
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JSValue install = get_global_attr(ctx, "__nb_install_cjs");
    if (!JS_IsFunction(ctx, install)) {
        JS_FreeValue(ctx, install);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JSValue av[2];
    av[0] = JS_NewString(ctx, entry_dir);
    av[1] = JS_NewString(ctx, entry_file);
    JSValue g1 = JS_GetGlobalObject(ctx);
    nb_watchdog_begin();
    JSValue rr = JS_Call(ctx, install, g1, 2, av);
    JS_FreeValue(ctx, g1);
    JS_FreeValue(ctx, install);
    nb_watchdog_pause();
    JS_FreeValue(ctx, av[0]); JS_FreeValue(ctx, av[1]);
    if (JS_IsException(rr)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        fprintf(stderr, "%s\n", m ? m : "loader error");
        JS_FreeValue(ctx, rr);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JS_FreeValue(ctx, rr);

    char *src = NULL; size_t n = 0;
    if (strcmp(pg, "-") == 0) {
        /* read the whole script from stdin */
        {
            size_t cap = 1 << 16, len = 0;
            char *b = malloc(cap);
            if (!b) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
            for (;;) {
                size_t got = fread(b + len, 1, cap - len, stdin);
                len += got;
                if (len == cap) { cap *= 2; b = realloc(b, cap); if (!b) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; } }
                if (feof(stdin) || got == 0) break;
            }
            b[len] = 0; src = b; n = len;
        }
    } else if (!read_file(pg, &src, &n)) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 2;
    }

    /* CLI-4: source-level ESM — ask the loader's __nb_esm_prepare for
     * transpiled source (or the original unchanged). The loader prelude
     * must have run already (__nb_install_cjs installs it). */
    JSValue prep = get_global_attr(ctx, "__nb_esm_prepare");
    if (JS_IsFunction(ctx, prep)) {
        JSValue av2[1]; av2[0] = JS_NewStringLen(ctx, src, n);
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue rp = JS_Call(ctx, prep, g2, 1, av2);
        JS_FreeValue(ctx, g2);
        if (JS_IsException(rp)) {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        } else if (JS_IsString(rp)) {
            size_t sl;
            char *ps = JS_ToCStringLen(ctx, &sl, rp);
            if (ps) {
                char *ns = malloc(sl + 1);
                if (ns) { memcpy(ns, ps, sl + 1); free(src); src = ns; n = sl; }
                JS_FreeCString(ctx, ps);
            }
        }
        JS_FreeValue(ctx, rp);
        JS_FreeValue(ctx, av2[0]);
    }
    JS_FreeValue(ctx, prep);

    /* Strip a leading shebang line (the page runner has the same rule), then
     * run the module body with a CPU budget. */
    if (n > 2 && src[0] == '#' && src[1] == '!') {
        char *nl = strchr(src, '\n');
        if (nl) { size_t off = (size_t)(nl + 1 - src); n -= off; memmove(src, nl + 1, n); src[n] = 0; }
    }
    if (peval_budget(ctx, src, n, errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "%s\n", errbuf[0] ? errbuf : "script error");
        free(src);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    free(src);
    if (g_out) fflush(g_out);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
