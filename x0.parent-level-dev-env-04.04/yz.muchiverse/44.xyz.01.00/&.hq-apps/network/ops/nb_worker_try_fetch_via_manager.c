/* nb_worker_try_fetch_via_manager.c - manager RPC fetch helper (mechanical extract, single-TU include) */
/* Try manager RPC for fetch (async per spec §8.2): worker -> manager FETCH, manager -> worker FETCHED.
 * Returns 1 if manager handled it (out_body/status set), 0 to fallback to direct curl. */
/* How long to wait for the manager's FETCHED before giving up and letting
 * the caller fall back to a direct curl. The direct path's own max-time is
 * 8s, so this is a little more than double: generous enough that a slow
 * manager is never mistaken for a dead one, bounded enough that an
 * unanswered FETCH cannot wedge a resident worker. */
#define FETCH_RPC_WAIT_MS 15000

/* Worker asks the manager to perform a fetch (async, spec 8.2).
 *
 * Frame out:  FETCH\n<id>\n<method>\n<url>\n<nreq>\n<h1>..\n<hN>\n\n<body>
 * Frame in :  FETCHED\n<id>\n<status>\n<nresp>\n<r1>..\n<rM>\n\n<body>
 *
 * The nreq/nresp tail is what the protocol was missing. It used to be
 * FETCH\n<id>\n<method>\n<url> -> FETCHED\n<id>\n<status>\n<body>, which
 * dropped every page-controlled request header and the whole request body
 * and could not return Set-Cookie. worker_sapisid_test could not deliver
 * its Authorization: SAPISIDHASH header (fixture said sign-fail) and
 * worker_fetch_post_test hung with the POST body stranded here.
 *
 * Cookie handling deliberately mirrors the direct-curl path rather than
 * being left to the manager: attach jar cookies outbound, ingest Set-Cookie
 * from the returned headers inbound. Both sides share one NB_COOKIES_FILE,
 * and one implementation means the two paths cannot drift.
 *
 * Returns 0 for "manager could not do it" in every failure mode - no
 * manager, timeout, short/mismatched reply, unparsable URL - and the
 * caller then uses the direct path. A manager that never replies therefore
 * costs one bounded wait, not a hang.
 */
static int try_fetch_via_manager(const char *method, const char *url,
                                  const char *headers, const char *body,
                                  char **out_body, size_t *out_len,
                                  int *out_status, char *errbuf, size_t errcap) {
    if (g_cli) return 0;
    if (isatty(STDIN_FILENO)) return 0;
    static int next_id = 1;
    int id = next_id++;

    /* split the page's headers into individual lines */
    char reqh[16][512];
    int nreq = 0;
    if (headers && *headers) {
        const char *hp = headers;
        while (*hp && nreq < 16) {
            const char *nl = strchr(hp, '\n');
            size_t n = nl ? (size_t)(nl - hp) : strlen(hp);
            while (n && (hp[n-1] == '\r' || hp[n-1] == ' ')) n--;
            if (n && memchr(hp, ':', n) && n < sizeof reqh[0]) {
                memcpy(reqh[nreq], hp, n); reqh[nreq][n] = '\0';
                nreq++;
            }
            if (!nl) break;
            hp = nl + 1;
        }
    }
    /* unified jar, same as the direct path: cookies go out with the request */
    char ckhdr[4096];
    ckhdr[0] = '\0';
    cookie_header_for_url(url, ckhdr, sizeof(ckhdr));
    if (ckhdr[0] && nreq < 16) {
        snprintf(reqh[nreq], sizeof reqh[0], "%s", ckhdr);   /* already "Cookie: ..." */
        nreq++;
    }

    static char payload[65536];
    int n = snprintf(payload, sizeof(payload), "FETCH\n%d\n%s\n%s\n%d",
                     id, method, url, nreq);
    for (int i = 0; i < nreq && n > 0 && (size_t)n < sizeof(payload); i++)
        n += snprintf(payload + n, sizeof(payload) - n, "\n%s", reqh[i]);
    if (n > 0 && (size_t)n < sizeof(payload))
        n += snprintf(payload + n, sizeof(payload) - n, "\n\n%s", (body && *body) ? body : "");
    if (n < 0 || (size_t)n >= sizeof(payload)) {
        if (errbuf && errcap) snprintf(errbuf, errcap, "fetch request too large");
        return 0;
    }
    send_payload(payload, (size_t)n);

    if (!recv_frame_budget(FETCH_RPC_WAIT_MS)) {
        if (errbuf && errcap) snprintf(errbuf, errcap, "manager fetch: no reply");
        return 0;
    }
    if (strncmp(g_rbuf, "FETCHED\n", 8) != 0) {
        if (errbuf && errcap) snprintf(errbuf, errcap, "manager fetch: unexpected reply");
        return 0;
    }
    char *p = g_rbuf + 8;
    char *n1 = strchr(p, '\n');
    if (!n1) return 0;
    *n1 = '\0';
    int r_id = atoi(p);
    if (r_id != id) { *n1 = '\n'; return 0; }
    char *q2 = n1 + 1;
    char *n2 = strchr(q2, '\n');
    if (!n2) { *n1 = '\n'; return 0; }
    *n2 = '\0';
    int status = atoi(q2);

    char *rest = n2 + 1;
    /* <nresp> - if this field is not a plain count, the manager is old and
     * the remainder of the frame is the body (back-compat). */
    char *e3 = strchr(rest, '\n');
    int nresp = 0;
    int old_format = 1;
    if (e3) {
        char cnt[16];
        size_t cl = (size_t)(e3 - rest);
        if (cl < sizeof(cnt)) {
            memcpy(cnt, rest, cl); cnt[cl] = '\0';
            char *endp = NULL;
            long v = strtol(cnt, &endp, 10);
            if (endp && *endp == '\0' && v >= 0 && v <= 32) { nresp = (int)v; old_format = 0; }
        }
    }
    if (!old_format) {
        char *cur = e3 + 1;
        for (int i = 0; i < nresp; i++) {
            char *e = strchr(cur, '\n');
            size_t hl = e ? (size_t)(e - cur) : strlen(cur);
            if (hl && strncasecmp(cur, "Set-Cookie:", 11) == 0) {
                char val[512];
                if (hl - 11 < sizeof(val)) {
                    memcpy(val, cur + 11, hl - 11); val[hl - 11] = '\0';
                    char *v = val;
                    while (*v == ' ') v++;
                    cookie_set_from_wire(v, url);
                }
            }
            if (!e) { cur += hl; break; }
            cur = e + 1;
        }
        /* blank separator line, then the body */
        if (*cur == '\n') cur++;
        rest = cur;
    }

    size_t body_len = g_rlen - (size_t)(rest - g_rbuf);
    char *rb = (char *)malloc(body_len + 1);
    if (!rb) { *n1 = '\n'; *n2 = '\n'; return 0; }
    memcpy(rb, rest, body_len);
    rb[body_len] = '\0';
    *out_body = rb;
    *out_len = body_len;
    *out_status = status;
    *n1 = '\n'; *n2 = '\n';
    if (errbuf && errcap) errbuf[0] = '\0';
    return 1;
}
