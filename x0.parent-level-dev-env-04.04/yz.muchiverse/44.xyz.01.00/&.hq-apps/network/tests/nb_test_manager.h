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

static void nbtm_jar_append(const char *jar, const char *host,
                            const char *name, const char *val) {
    FILE *f = fopen(jar, "ab");
    if (!f) return;
    fprintf(f, "%s\t/\t%s\t%s\t0\t0\n", host, name, val);
    fclose(f);
}

/* Build "name=value; name=value" for cookies whose host covers `host`
 * (exact match, or the request host is a dot-suffix of a parent domain). */
static void nbtm_jar_cookie_header(const char *jar, const char *host,
                                   char *out, size_t cap) {
    out[0] = '\0';
    FILE *f = fopen(jar, "rb");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = '\0';
        char *fld[6] = {0};
        char *save = NULL;
        int k = 0;
        for (char *t = strtok_r(line, "\t", &save); t && k < 6;
             t = strtok_r(NULL, "\t", &save)) fld[k++] = t;
        if (k < 4 || !fld[0] || !fld[2] || !fld[3]) continue;
        size_t hl = strlen(fld[0]);
        int match = (strcmp(fld[0], host) == 0) ||
                    (hl < strlen(host) && host[hl] == '.' &&
                     strcmp(host + hl + 1, fld[0]) == 0);
        if (!match) continue;
        size_t used = strlen(out);
        if (used + 2 < cap && used) { strcat(out, "; "); }
        if (used + strlen(fld[2]) + strlen(fld[3]) + 2 < cap) {
            strcat(out, fld[2]);
            strcat(out, "=");
            strcat(out, fld[3]);
        }
    }
    fclose(f);
}

static int nbtm_is_fetch(const char *reply) {
    return strncmp(reply, "FETCH\n", 6) == 0;
}

/* Service one FETCH frame and reply FETCHED.
 *
 * Always replies, including on failure (status 0), so the worker can
 * never be left blocked in recv_frame() by a driver bug - that is the
 * entire failure this shim exists to remove. */
static void nbtm_serve_fetch(int to_child, const char *frame,
                             const char *jar, int fallback_port) {
    int id = 0, status = 0;
    char method[32] = {0}, url[2048] = {0};
    if (sscanf(frame, "FETCH\n%d\n%31[^\n]\n%2047[^\n]", &id, method, url) < 3) {
        char bad[64];
        snprintf(bad, sizeof(bad), "FETCHED\n%d\n0\n", id);
        nbtm_wsend(to_child, bad);
        return;
    }

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
        snprintf(bad, sizeof(bad), "FETCHED\n%d\n0\n", id);
        nbtm_wsend(to_child, bad);
        return;
    }

    char ck[2048];
    nbtm_jar_cookie_header(jar, host, ck, sizeof ck);
    char req[4096];
    int n = snprintf(req, sizeof req,
        "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n%s%s%s\r\n",
        method, path, host, port,
        ck[0] ? "Cookie: " : "", ck, ck[0] ? "\r\n" : "");
    if (n > 0) (void)!write(fd, req, (size_t)n);

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

    char *body = strstr(resp, "\r\n\r\n");
    if (body) {
        *body = '\0';
        body += 4;
        for (char *h = resp; h && *h; ) {
            char *eol = strstr(h, "\r\n");
            if (eol) *eol = '\0';
            if (strncasecmp(h, "Set-Cookie:", 11) == 0) {
                char *v = h + 11;
                while (*v == ' ') v++;
                char nm[128] = {0}, vl[512] = {0};
                if (sscanf(v, "%127[^=]=%511[^;\r\n]", nm, vl) == 2)
                    nbtm_jar_append(jar, host, nm, vl);
            }
            if (!eol) break;
            h = eol + 2;
        }
    } else {
        body = resp + rl;
        *body = '\0';
    }

    static char out[70000];
    snprintf(out, sizeof out, "FETCHED\n%d\n%d\n%s", id, status, body);
    nbtm_wsend(to_child, out);
}

#endif /* NB_TEST_MANAGER_H */