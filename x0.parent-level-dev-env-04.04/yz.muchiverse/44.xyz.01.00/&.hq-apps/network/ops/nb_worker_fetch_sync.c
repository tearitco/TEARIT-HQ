/* nb_worker_fetch_sync.c - synchronous fetch/XHR transport binding (mechanical extract, single-TU include) */
static JSValue nb_fetch_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *m_own = NULL, *u_own = NULL, *h_own = NULL, *b_own = NULL;
    const char *method = NULL, *url = NULL, *headers = NULL, *body = NULL;
    if (argc > 0) method = (m_own = JS_ToCString(ctx, argv[0]));
    if (argc > 1) url = (u_own = JS_ToCString(ctx, argv[1]));
    if (argc > 2) headers = (h_own = JS_ToCString(ctx, argv[2]));
    if (argc > 3) body = (b_own = JS_ToCString(ctx, argv[3]));
    if (!method || !url) {
        JS_FreeCString(ctx, m_own); JS_FreeCString(ctx, u_own);
        JS_FreeCString(ctx, h_own); JS_FreeCString(ctx, b_own);
        JS_ThrowTypeError(ctx, "nbFetchSync needs method and url strings");
        return JS_EXCEPTION;
    }
    if (!headers) headers = "";
    if (!body) body = "";

    char urlb[2300];
    resolve_doc_url(url, urlb, sizeof(urlb));
    url = urlb;

    char *rb = NULL; size_t rn = 0; int status = 0; char errbuf[256] = "";

    nb_watchdog_pause();   /* a blocking curl must never trip the EVAL_BUDGET_SEC watchdog */

    if (strncmp(url, "file:", 5) == 0) {
        /* normalize file://host/path, file:///path, file:/path -> /path */
        const char *p = url + 5;
        while (*p == '/') p++;
        if (strncmp(p, "localhost", 9) == 0 && p[9] == '/') p += 10;
        char abspath[2048];
        snprintf(abspath, sizeof(abspath), "/%s", p);
        if (read_file(abspath, &rb, &rn)) status = 200;
        else snprintf(errbuf, sizeof(errbuf), "cannot read %s", abspath);
    } else if (strncmp(url, "http:", 5) == 0 || strncmp(url, "https:", 6) == 0) {
        char *mgr_body = NULL; size_t mgr_len = 0; int mgr_status = 0; char mgr_err[256] = "";
        /* Manager RPC caps bodies at 60KB and NUL-truncates binary in the
         * text frame: a 200 with an empty body means "too big for RPC",
         * not success - fall through to direct curl below (2026-10-07:
         * every remote image died exactly this way). */
        if (try_fetch_via_manager(method, url, headers, body, &mgr_body, &mgr_len, &mgr_status, mgr_err, sizeof(mgr_err)) &&
            !(mgr_status == 200 && mgr_len == 0)) {
            rb = mgr_body; rn = mgr_len; status = mgr_status;
            if (mgr_err[0]) snprintf(errbuf, sizeof(errbuf), "%s", mgr_err);
        } else {
            free(mgr_body);
        char cfgpath[1024] = "", bodypath[1024] = "";
        char t1[] = "/tmp/nbfetch.XXXXXX", t2[] = "/tmp/nbfetchbody.XXXXXX";
        int fd1 = mkstemp(t1), fd2 = mkstemp(t2);
        if (fd1 < 0 || fd2 < 0) { snprintf(errbuf, sizeof(errbuf), "mkstemp failed"); }
        else {
            close(fd1); close(fd2);
            snprintf(cfgpath, sizeof(cfgpath), "%s", t1);
            snprintf(bodypath, sizeof(bodypath), "%s", t2);
            FILE *cf = fopen(cfgpath, "w");
            if (!cf) snprintf(errbuf, sizeof(errbuf), "cannot write curl config");
            else {
                cfg_line(cf, "url", url);
                cfg_line(cf, "user-agent", "Mozilla/5.0 (NNEST network-browser-hq nb-js-worker rung4)");
                cfg_line(cf, "max-time", "8");
                fputs("silent\nlocation\nfail\n", cf);
                /* unified cookie jar: attach matching cookies from our jar
                 * (Chromium-parity — one store, both directions). */
                {
                    char ckhdr[4096];
                    cookie_header_for_url(url, ckhdr, sizeof(ckhdr));
                    if (ckhdr[0]) cfg_line(cf, "header", ckhdr);
                }
                /* dump response headers so we can parse Set-Cookie into jar */
                char hdrpath[256] = "";
                {
                    char t3[] = "/tmp/nbfetchhdr.XXXXXX";
                    int fd3 = mkstemp(t3);
                    if (fd3 >= 0) { close(fd3); snprintf(hdrpath, sizeof(hdrpath), "%s", t3); }
                }
                if (hdrpath[0]) cfg_line(cf, "dump-header", hdrpath);
                /* one header = line per raw "Name: value" line (no strtok_r) */
                for (const char *p = headers; *p; ) {
                    const char *nl = strchr(p, '\n');
                    size_t n = nl ? (size_t)(nl - p) : strlen(p);
                    while (n && (p[n-1] == '\r' || p[n-1] == ' ')) n--;
                    if (n && memchr(p, ':', n)) {
                        char hb[512];
                        if (n >= sizeof(hb)) n = sizeof(hb) - 1;
                        memcpy(hb, p, n); hb[n] = 0;
                        cfg_line(cf, "header", hb);
                    }
                    p += n + (nl ? 1 : 0);
                }
                if (body[0]) cfg_data(cf, body);
                cfg_line(cf, "request", method);
                fputs("output = \"", cf); cfg_put(cf, bodypath); fputs("\"\n", cf);
                fputs("write-out = \"%{http_code}\"\n", cf);
                fclose(cf);
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "curl --compressed -sS -K '%s' 2>/dev/null", cfgpath);
                FILE *po = popen(cmd, "r");
                if (po) {
                    char code[16] = ""; size_t got = 0;
                    while (got + 1 < sizeof(code)) {
                        int c = fgetc(po);
                        if (c == EOF) break;
                        code[got++] = (char)c;
                    }
                    code[got] = 0;
                    int rc = pclose(po);
                    status = (int)strtol(code, NULL, 10);
                    if (status <= 0) status = 0;
                    /* Page/asset bodies are legitimately larger than the
                     * 512KB fs-lite guard: read_file() returned 0 for a
                     * 940KB homepage, so nbFetchSync reported status 200 with
                     * an empty body (measured 2026-10-26 on
                     * www.coinmarketcap.com). read_file_big is the existing
                     * chunked reader with a 64MB ceiling. */
                    if (status > 0 && read_file_big(bodypath, &rb, &rn)) {
                        /* treats zero-byte bodies as a successful empty read */
                    }
                    if (!rb) snprintf(errbuf, sizeof(errbuf),
                                      "curl rc=%d status=%d url=%s",
                                      rc, status, url);
                } else snprintf(errbuf, sizeof(errbuf), "popen curl failed");
                /* ingest Set-Cookie response headers into the unified jar */
                if (hdrpath[0] && status >= 200 && status < 400) {
                    char *hbuf = NULL; size_t hbn = 0;
                    if (read_file(hdrpath, &hbuf, &hbn)) {
                        char *hp = hbuf;
                        while (hp && *hp) {
                            char *nl = strchr(hp, '\n');
                            if (nl) *nl = 0;
                            char *cr = hp + strlen(hp);
                            if (cr > hp && cr[-1] == '\r') cr[-1] = 0;
                            if (strncasecmp(hp, "Set-Cookie:", 11) == 0) {
                                const char *val = hp + 11;
                                while (*val == ' ') val++;
                                cookie_set_from_wire(val, url);
                            }
                            hp = nl ? nl + 1 : NULL;
                        }
                        free(hbuf);
                    }
                }
                unlink(cfgpath);
                unlink(bodypath);
                if (hdrpath[0]) unlink(hdrpath);
            }
        }
        }
    } else {
        /* data: URLs can carry small inline blobs; everything else is refused */
        if (strncmp(url, "data:", 5) == 0) {
            const char *c = strchr(url, ',');
            rb = strdup(c ? c + 1 : "");
            rn = rb ? strlen(rb) : 0;
            status = 200;
        } else snprintf(errbuf, sizeof(errbuf), "unsupported scheme in %s", url);
    }

    alarm(nb_budget());   /* re-arm the budget for the rest of the drain */

    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "ok", JS_NewBool(ctx, status >= 200 && status < 300 && rb != NULL));
    JS_SetPropertyStr(ctx, o, "status", JS_NewInt32(ctx, status));
    JS_SetPropertyStr(ctx, o, "body", JS_NewString(ctx, rb ? rb : ""));
    /* Binary-safe twin: JS strings NUL-truncate (PNG dies). bytes carries
     * the exact rn bytes; img decode prefers it (2026-10-07: remote Image
     * loads decoded nothing because of the truncation). */
    {
        JSValue len = JS_NewInt32(ctx, (int32_t)(rn > INT32_MAX ? INT32_MAX : rn));
        JSValue gobj = JS_GetGlobalObject(ctx);
        JSValue ctor = JS_GetPropertyStr(ctx, gobj, "Uint8Array");
        JS_FreeValue(ctx, gobj);
        JSValue data = (JS_IsObject(ctor) && !JS_IsNull(ctor))
                           ? JS_CallConstructor(ctx, ctor, 1, &len)
                           : JS_NewArray(ctx);
        JS_FreeValue(ctx, ctor);
        JS_FreeValue(ctx, len);
        size_t fill = rn;
        for (size_t i = 0; i < fill; i++) {
            JSValue e = JS_NewInt32(ctx, rb ? (unsigned char)rb[i] : 0);
            JS_SetPropertyUint32(ctx, data, (uint32_t)i, e);
        }
        JS_SetPropertyStr(ctx, o, "bytes", data);
        JS_SetPropertyStr(ctx, o, "byteLength", JS_NewInt32(ctx, (int32_t)(rn > INT32_MAX ? INT32_MAX : rn)));
    }
    JS_SetPropertyStr(ctx, o, "error", JS_NewString(ctx, errbuf[0] ? errbuf : ""));
    free(rb);
    JS_FreeCString(ctx, m_own); JS_FreeCString(ctx, u_own);
    JS_FreeCString(ctx, h_own); JS_FreeCString(ctx, b_own);
    return o;
}
