/* nb_test_manager.h - FETCH/FETCHED shim for worker test drivers.
 *
 * 2026-10-02. Added because `make check` wedged forever, silently, at the
 * worker_login_test suite.
 *
 * The seam: a page that XHRs an http:// or https:// URL makes
 * nb_js_worker.c's try_fetch_via_manager() emit
 *
 *     FETCH\n<id>\n<method>\n<url>
 *
 * on stdout, and then BLOCK in recv_frame() waiting for
 *
 *     FETCHED\n<id>\n<status>\n<body>
 *
 * Nothing ever replies unless the driver implements it, so the worker
 * waits forever, the driver waits forever in waitpid(), QUIT is never
 * read, and the suite prints nothing at all. That silence is what makes
 * this failure mode so easy to misread as a slow build.
 *
 * These drivers were written before 7e55fc8b9b (2026-09-23) introduced
 * that RPC, so they predate the protocol the worker now requires:
 * worker_sapisid_test, worker_innertube_test and worker_fetch_post_test
 * all point a page XHR at a real loopback http://127.0.0.1:<port>/login
 * and none of them answer. They are the last three suites to go green.
 *
 * A driver uses it by handling FETCH before anything else in its reply
 * loop:
 *
 *     if (nbtm_is_fetch(reply)) {
 *         nbtm_serve_fetch(to_child[1], reply, jar_path, port);
 *         continue;
 *     }
 *
 * Why the driver owns the cookie jar
 * ----------------------------------
 * FETCHED carries NO response headers, so a wire Set-Cookie cannot cross
 * this RPC in either direction. In production the manager owns the
 * unified cookie jar (see network_browser_manager.c); nb_js_worker.c
 * reads the same jar to serve document.cookie. So this shim does what
 * the real manager does - attaches jar cookies on the way out and banks
 * any Set-Cookie it sees on the way back into the jar. That is the whole
 * point of the unified-store seam these suites exist to prove, not a
 * workaround: without the driver writing the jar, the ingress half
 * (Set-Cookie -> document.cookie) is untestable through this path.
 *
 * Jar record shape is the worker's cookie_save_file() format:
 *     host \t path \t name \t value \t expires \t secure
 */
#ifndef NB_TEST_MANAGER_H
#define NB_TEST_MANAGER_H

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

/* Write one frame in the worker's wire format.
 *
 * The framing is LENGTH-PREFIXED, not bare newline-delimited: a 6-digit
 * zero-padded byte count, a newline, then the payload, then a newline.
 * Omitting the "%06d\n" prefix desyncs the worker's recv_frame() and the
 * NEXT frame it reads is garbage - which shows up as a silently empty
 * response body rather than as a parse error, so match wsend() exactly.
 */
static void nbtm_wsend(int fd, const char *payload) {
    size_t n = strlen(payload), off = 0;
    char lenbuf[16];
    int ln = snprintf(lenbuf, sizeof(lenbuf), "%.6d\n", (int)n);
    (void)!write(fd, lenbuf, (size_t)ln);
    while (off < n) {
        ssize_t w = write(fd, payload + off, n - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    (void)!write(fd, "\n", 1);
}

static int nbtm_is_fetch(const char *reply) {
    return strncmp(reply, "FETCH\n", 6) == 0;
}

/* Hop-by-hop / transport headers we must NOT copy from the page's request:
 * we regenerate Host and Content-Length ourselves, and forwarding
 * Accept-Encoding could hand back a body this shim cannot decode. */
static int nbtm_skip_header(const char *h) {
    static const char *skip[] = { "host:", "connection:", "content-length:",
                                  "transfer-encoding:", "accept-encoding:",
                                  "keep-alive:", "proxy-connection:", NULL };
    for (int i = 0; skip[i]; i++)
        if (strncasecmp(h, skip[i], strlen(skip[i])) == 0) return 1;
    return 0;
}

static const char *nbtm_line(const char *p, char *out, size_t cap) {
    if (!p || !*p) return NULL;
    /* Scan to the REAL line end first, then copy at most cap-1. Stopping the
     * scan at cap-1 and returning p+n+1 would advance into the MIDDLE of an
     * over-long line instead of past it - which silently desynchronises
     * every field after it. A URL is routinely longer than a small buffer. */
    size_t n = 0;
    while (p[n] && p[n] != '\n') n++;
    size_t c = (cap > 1) ? ((n < cap - 1) ? n : cap - 1) : 0;
    memcpy(out, p, c); out[c] = '\0';
    return p[n] ? p + n + 1 : p + n;
}

/* Service one FETCH frame and reply FETCHED.
 *
 * Frame in : FETCH\n<id>\n<method>\n<url>\n<nreq>\n<h1>..\n<hN>\n\n<body>
 * Frame out: FETCHED\n<id>\n<status>\n<nresp>\n<r1>..\n<rM>\n\n<body>
 *
 * The nreq/nresp tail used to be missing, which silently dropped every
 * page-controlled request header and the whole request body. That is why
 * worker_sapisid_test could not deliver its Authorization: SAPISIDHASH
 * header and worker_fetch_post_test hung with the POST body stranded.
 *
 * This shim does NO cookie handling. The worker attaches jar cookies to
 * the outgoing request and ingests Set-Cookie from the response headers we
 * return, exactly as its direct-curl path already does, and both share one
 * NB_COOKIES_FILE. An earlier version had the driver write the jar instead;
 * now that the protocol carries response headers that is redundant, and two
 * writers is how jars drift apart.
 *
 * Always replies, including with status 0 on failure, so no driver bug can
 * leave the worker blocked waiting for a FETCHED that never comes.
 */
static void nbtm_serve_fetch(int to_child, const char *frame, int fallback_port) {
    int id = 0, status = 0;
    char method[16] = {0}, url[2048] = {0};
    if (sscanf(frame, "FETCH\n%d\n%15[^\n]\n%2047[^\n]", &id, method, url) < 3) {
        char bad[64];
        snprintf(bad, sizeof(bad), "FETCHED\n%d\n0\n0\n\n", id);
        nbtm_wsend(to_child, bad);
        return;
    }

    const char *cur = strchr(frame, '\n');
    cur = cur ? strchr(cur + 1, '\n') : NULL;      /* id line */
    cur = cur ? strchr(cur + 1, '\n') : NULL;      /* method line */
    cur = cur ? cur + 1 : NULL;

    char reqh[16][512];
    int nreq = 0;
    char urlbuf[2048];
    cur = nbtm_line(cur, urlbuf, sizeof urlbuf);   /* skip url line */
    char nb[16] = "";
    cur = nbtm_line(cur, nb, sizeof nb);           /* <nreq> */
    int want = atoi(nb);
    if (want > 0 && want <= 16) nreq = want;
    for (int i = 0; i < nreq; i++) cur = nbtm_line(cur, reqh[i], sizeof reqh[i]);
    if (cur) cur = nbtm_line(cur, nb, sizeof nb);  /* blank separator */
    const char *reqbody = cur ? cur : "";

    const char *u = url;
    if      (strncmp(u, "http://",  7) == 0) u += 7;
    else if (strncmp(u, "https://", 8) == 0) u += 8;
    char host[256] = {0};
    int  port = fallback_port;
    const char *path = "/";
    const char *colon = strchr(u, ':');
    const char *slash = strchr(u, '/');
    if (colon && (!slash || colon < slash)) {
        size_t hl = (size_t)(colon - u);
        if (hl >= sizeof(host)) hl = sizeof(host) - 1;
        memcpy(host, u, hl); host[hl] = '\0';
        port = atoi(colon + 1);
        path = slash ? slash : "/";
    } else {
        size_t hl = slash ? (size_t)(slash - u) : strlen(u);
        if (hl >= sizeof(host)) hl = sizeof(host) - 1;
        memcpy(host, u, hl); host[hl] = '\0';
        path = slash ? slash : "/";
    }
    if (!host[0]) snprintf(host, sizeof(host), "127.0.0.1");

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((unsigned short)port);
    if (fd < 0 || inet_pton(AF_INET, host, &sa.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        if (fd >= 0) close(fd);
        char bad[64];
        snprintf(bad, sizeof(bad), "FETCHED\n%d\n0\n0\n\n", id);
        nbtm_wsend(to_child, bad);
        return;
    }

    static char req[16384];
    int rn = snprintf(req, sizeof req,
        "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n"
        "Content-Length: %zu\r\n",
        method, path, host, port, strlen(reqbody));
    for (int i = 0; i < nreq && rn > 0 && (size_t)rn < sizeof(req); i++) {
        if (nbtm_skip_header(reqh[i])) continue;
        rn += snprintf(req + rn, sizeof(req) - rn, "%s\r\n", reqh[i]);
    }
    if (rn > 0 && (size_t)rn < sizeof(req))
        rn += snprintf(req + rn, sizeof(req) - rn, "\r\n");
    size_t roff = (size_t)rn;
    if (*reqbody && roff + strlen(reqbody) < sizeof(req)) {
        memcpy(req + roff, reqbody, strlen(reqbody));
        rn = (int)(roff + strlen(reqbody));
    }
    size_t soff = 0;
    while (soff < (size_t)rn) {
        ssize_t w = write(fd, req + soff, (size_t)rn - soff);
        if (w <= 0) break;
        soff += (size_t)w;
    }

    static char resp[65536];
    size_t rl = 0;
    for (;;) {
        if (rl + 1 >= sizeof resp) break;
        ssize_t r = read(fd, resp + rl, sizeof resp - rl - 1);
        if (r <= 0) break;
        rl += (size_t)r;
    }
    resp[rl] = '\0';
    close(fd);

    if (strncmp(resp, "HTTP/", 5) == 0) {
        const char *sp = strchr(resp, ' ');
        if (sp) status = atoi(sp + 1);
    }

    char resphdr[32][512];
    int nresp = 0;
    char *body = strstr(resp, "\r\n\r\n");
    if (body) {
        *body = '\0';
        body += 4;
        char *h = resp;
        char *eol = strstr(h, "\r\n");
        if (eol) *eol = '\0';                 /* drop the status line */
        h = eol ? eol + 2 : h;
        while (h && *h && nresp < 32) {
            eol = strstr(h, "\r\n");
            if (eol) *eol = '\0';
            if (*h && !nbtm_skip_header(h)) {
                size_t n = strlen(h);
                if (n >= sizeof resphdr[0]) n = sizeof resphdr[0] - 1;
                memcpy(resphdr[nresp], h, n + 1);
                nresp++;
            }
            if (!eol) break;
            h = eol + 2;
        }
    } else {
        body = resp + rl;
        *body = '\0';
    }

    static char out[140000];
    int on = snprintf(out, sizeof out, "FETCHED\n%d\n%d\n%d", id, status, nresp);
    for (int i = 0; i < nresp && on > 0 && (size_t)on < sizeof(out); i++)
        on += snprintf(out + on, sizeof(out) - on, "\n%s", resphdr[i]);
    if (on > 0 && (size_t)on < sizeof(out))
        snprintf(out + on, sizeof(out) - on, "\n\n%s", body);
    nbtm_wsend(to_child, out);
}

#endif /* NB_TEST_MANAGER_H */