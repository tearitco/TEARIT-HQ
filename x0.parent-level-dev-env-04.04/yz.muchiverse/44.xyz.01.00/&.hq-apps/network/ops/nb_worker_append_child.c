/* nb_worker_append_child.c - appendChild helper (mechanical extract, single-TU include) */
static JSValue nb_el_appendChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *ch = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (n && n->tag && !strcmp(n->tag, "template")) {   /* appends land in content */
        NbNode *frag = nb_template_content_frag(ctx, this_val, n);
        if (frag) n = frag;
    }
    if (!n || !ch || ch == n) return JS_NULL;
    if (nb_is_doc_fragment_node(ch)) {
        if (nb_node_is_ancestor(ch, n))
            return nb_throw_hierarchy(ctx, "HierarchyRequestError: the fragment contains this node");
        orphan_remove(ch);
        nb_insert_fragment_children(ctx, n, ch, NULL);
        orphan_add(ch);
        return JS_DupValue(ctx, argv[0]);   /* spec returns the fragment */
    }
    node_detach(ch);
    orphan_remove(ch);
    local_append(n, ch);
    nb_mutation_notify_childlist(ctx, n, ch, NULL, NULL, NULL);

    /* If a script element with src is appended to head/body, fetch and execute it */
    if (ch && ch->tag && !strcmp(ch->tag, "script")) {
        fprintf(stderr, "DEBUG: script element appended, tag: %s\n", ch->tag);
        const char *src = nb_attr_get(ch, "src");
        fprintf(stderr, "DEBUG: script src: %s\n", src ? src : "(null)");
        if (src && src[0]) {
            /* Resolve URL against current page URL */
            char resolved_url[2048];
            resolve_doc_url(src, resolved_url, sizeof(resolved_url));

            /* Fetch the script content */
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
                /* Direct curl fallback */
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
                char errbuf2[512];
                if (peval_budget(ctx, rb, rn, errbuf2, sizeof(errbuf2)) != 0) {
                    fprintf(stderr, "WERR| dynamic script appendChild %s: %s\n", resolved_url, errbuf2[0] ? errbuf2 : "eval error");
                }
                free(rb);
            } else {
                if (rb) free(rb);
                if (errbuf[0]) fprintf(stderr, "WERR| script load appendChild %s: %s\n", resolved_url, errbuf);
            }
        }
    }

    return push_node(ctx, ch);
}
