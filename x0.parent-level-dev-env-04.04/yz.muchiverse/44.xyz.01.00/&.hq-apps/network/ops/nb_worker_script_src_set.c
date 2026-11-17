/* nb_worker_script_src_set.c - script src setter binding (mechanical extract, single-TU include) */
static JSValue nb_script_src_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (argc < 1 || !JS_IsString(argv[0])) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) return JS_UNDEFINED;

    fprintf(stderr, "DEBUG: script src set to: %s\n", s);

    /* Update the attribute and our cache */
    char *new_attrs = attrs_set(n, "src", s);
    free(n->attrs);
    n->attrs = new_attrs;
    script_set_src(n, s);

    /* Resolve URL against current page URL */
    char resolved_url[2048];
    resolve_doc_url(s, resolved_url, sizeof(resolved_url));
    fprintf(stderr, "DEBUG: resolved script URL: %s\n", resolved_url);

    /* Fetch the script content using nbFetchSync logic */
    char *rb = NULL; size_t rn = 0; int status = 0; char errbuf[256] = "";

    if (strncmp(resolved_url, "file:", 5) == 0) {
        const char *p = resolved_url + 5;
        while (*p == '/') p++;
        if (strncmp(p, "localhost", 9) == 0 && p[9] == '/') p += 10;
        char abspath[2048];
        snprintf(abspath, sizeof(abspath), "/%s", p);
        if (read_file(abspath, &rb, &rn)) status = 200;
        else snprintf(errbuf, sizeof(errbuf), "cannot read %s", abspath);
    } else if (strncmp(resolved_url, "http:", 5) == 0 || strncmp(resolved_url, "https:", 6) == 0) {
        /* Use the same direct curl fallback as nb_fetch_sync for script loading */
        char cfgpath[1024] = "";
        char t1[] = "/tmp/nbfetch.XXXXXX";
        int fd1 = mkstemp(t1);
        if (fd1 < 0) {
            snprintf(errbuf, sizeof(errbuf), "mkstemp failed");
        } else {
            close(fd1);
            snprintf(cfgpath, sizeof(cfgpath), "%s", t1);
            FILE *cf = fopen(cfgpath, "w");
            if (!cf) {
                snprintf(errbuf, sizeof(errbuf), "cannot write curl config");
            } else {
                cfg_line(cf, "url", resolved_url);
                cfg_line(cf, "user-agent", "Mozilla/5.0 (NNEST network-browser-hq nb-js-worker rung4)");
                cfg_line(cf, "max-time", "8");
                fputs("silent\nlocation\nfail\n", cf);
                {
                    char ckhdr[4096];
                    cookie_header_for_url(resolved_url, ckhdr, sizeof(ckhdr));
                    if (ckhdr[0]) cfg_line(cf, "header", ckhdr);
                }
                char hdrpath[256] = "";
                {
                    char t3[] = "/tmp/nbfetchhdr.XXXXXX";
                    int fd3 = mkstemp(t3);
                    if (fd3 >= 0) { close(fd3); snprintf(hdrpath, sizeof(hdrpath), "%s", t3); cfg_line(cf, "dump-header", hdrpath); }
                }
                cfg_line(cf, "output", "/tmp/nbfetchbody");
                fclose(cf);

                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "curl -K '%s' 2>/dev/null", cfgpath);
                int rc = system(cmd);
                unlink(cfgpath);

                if (rc == 0) {
                    FILE *bf = fopen("/tmp/nbfetchbody", "rb");
                    if (bf) {
                        fseek(bf, 0, SEEK_END);
                        long fsize = ftell(bf);
                        fseek(bf, 0, SEEK_SET);
                        if (fsize > 0 && fsize < 4 * 1024 * 1024) {
                            rb = malloc((size_t)fsize + 1);
                            if (rb) {
                                rn = fread(rb, 1, (size_t)fsize, bf);
                                rb[rn] = 0;
                                status = 200;
                            }
                        }
                        fclose(bf);
                    }
                    unlink("/tmp/nbfetchbody");
                    if (hdrpath[0]) unlink(hdrpath);
                }
            }
        }
    }

    if (status == 200 && rb && rn > 0) {
        /* Execute the script in the worker context */
        nb_execute_script_in_worker(ctx, resolved_url, rb);
        free(rb);
    } else {
        if (rb) free(rb);
        if (errbuf[0]) fprintf(stderr, "WERR| script load %s: %s\n", resolved_url, errbuf);
    }

    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
