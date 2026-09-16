/* worker_login_test.c — hermetic login-engine surface test for the generic
 * worker: proves the unified cookie store (rung-6 seam).  Row 34's jar half
 * was already proven by worker_cookie_test.c (document.cookie in, re-read)
 * and worker_page_test.c (page-originated next XHR + pre-seeded jar attach);
 * this driver proves the ONE thing never proven: that a SERVER's Set-Cookie
 * response header lands in the SAME jar document.cookie reads, and that a
 * subsequent page-originated request re-attaches it (ingress + egress through
 * one store, Chromium-parity).
 *
 * The fixture is a loopback-only HTTP server (127.0.0.1, ephemeral port, no
 * external routes) — the rung-4 "custom fixture server" precedent.  The page
 * JS makes TWO XHRs against it:
 *   1. GET /auth   -> server replies with Set-Cookie: sid=wlt456 (and body
 *                     "auth-ok").  The engine ingests it into NB_COOKIES_FILE.
 *   2. GET /guard  -> server inspects the Cookie header; replies "guard-ok"
 *                     only if sid=wlt456 came back, else "guard-fail".
 * The page renders "SID=<document.cookie sid>|GUARD=<body>", and the driver
 * asserts BOTH halves: the wire cookie is visible to document.cookie (unifi-
 * cation) AND the reattached cookie reached the server (egress).  Zero per-
 * site hardcoding, NB_COOKIES_FILE jar in tmpdir only, loopback only.
 *
 * Usage: worker_login_test <worker-binary> <worker_login_test.js> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#define _POSIX_C_SOURCE 200809L
#include "../nb_dom.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char *canned_html =
    "<html><body>"
    "<div id=\"r\">init</div>"
    "</body></html>";

static void wsend(int fd, const char *payload) {
    size_t n = strlen(payload);
    char lenbuf[16];
    int ln = snprintf(lenbuf, sizeof(lenbuf), "%.6d\n", (int)n);
    (void)!write(fd, lenbuf, (size_t)ln);
    if (n) (void)!write(fd, payload, n);
    (void)!write(fd, "\n", 1);
}
static int rread(int fd, char *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r <= 0) return 0;
        got += (size_t)r;
    }
    buf[got] = 0;
    return 1;
}
static int wreply(int fd, char *buf, size_t cap) {
    char lenbuf[32];
    size_t i = 0;
    for (;;) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return 0;
        if (c == '\n') break;
        if (i < sizeof(lenbuf) - 1) lenbuf[i++] = c;
    }
    lenbuf[i] = 0;
    long n = strtol(lenbuf, NULL, 10);
    if (n < 0 || (size_t)n >= cap) return 0;
    if (!rread(fd, buf, (size_t)n)) return 0;
    char t; if (read(fd, &t, 1) != 1) return 0;   /* trailing '\n' */
    return 1;
}

/* --- tiny loopback fixture server (child process) -------------------------
 * accept()s `want` connections; for each, reads the request line + headers
 * (capturing any Cookie: line), answers the well-known /auth + /guard
 * routes, and records what it saw into `log` for the driver to assert. */
static void fixture_server(int lstfd, int want, const char *log) {
    FILE *lf = fopen(log, "wb");
    for (int i = 0; i < want; i++) {
        int cfd = accept(lstfd, NULL, NULL);
        if (cfd < 0) break;
        char req[8192]; size_t rl = 0;
        /* read until we have the blank line ending the request head */
        while (rl + 1 < sizeof(req)) {
            char b;
            ssize_t r = read(cfd, &b, 1);
            if (r <= 0) break;
            req[rl++] = b;
            if (rl >= 4 && req[rl-4] == '\r' && req[rl-3] == '\n'
                && req[rl-2] == '\r' && req[rl-1] == '\n') break;
        }
        req[rl < sizeof(req) ? rl : sizeof(req)-1] = 0;
        char *cookie = strstr(req, "Cookie:");
        for (char *p = req; p && *p; p++) {
            if (*p == '\r' || *p == '\n') *p = ' ';
        }
        if (lf) fprintf(lf, "req%d: %s |COOKIE:%s|\n", i, req,
                        cookie ? cookie + 7 : "(none)");
        int is_auth = (strstr(req, "/auth") != NULL);
        if (is_auth) {
            /* route /auth: hand out a session cookie; body auth-ok */
            const char *resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Set-Cookie: sid=wlt456; Path=/\r\n"
                "Content-Length: 7\r\n"
                "Connection: close\r\n"
                "\r\n"
                "auth-ok";
            (void)!write(cfd, resp, strlen(resp));
        } else {
            /* route /guard: only ok if the sid came back */
            int ok = (cookie != NULL && strstr(cookie + 7, "sid=wlt456") != NULL);
            const char *body = ok ? "guard-ok" : "guard-fail";
            char resp[512];
            int n = snprintf(resp, sizeof(resp),
                "HTTP/1.1 %d\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                ok ? 200 : 401, (int)strlen(body), body);
            (void)!write(cfd, resp, (size_t)n);
        }
        close(cfd);
    }
    if (lf) fclose(lf);
    _exit(0);
}


int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <worker> <js-template> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *js_template = argv[2];
    const char *tmpdir = argv[3];

    char dom_path[1024], page_path[1024];
    char srvlog[1024], jar_path[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/login.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/login_page.js", tmpdir);
    snprintf(srvlog, sizeof(srvlog), "%s/fixture.log", tmpdir);
    snprintf(jar_path, sizeof(jar_path), "%s/nb_cookies.txt", tmpdir);

    /* hermetic uniform jar — only this file, never $HOME */
    if (setenv("NB_COOKIES_FILE", jar_path, 1) != 0) { perror("setenv NB_COOKIES_FILE"); return 1; }
    /* ensure the worker does NOT fall back to the manager's separate curl jar */
    unsetenv("NB_CURL_COOKIES_FILE");

    /* 1. build login.dom from the canned HTML */
    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    /* 2. loopback fixture server (127.0.0.1, ephemeral port, 2 requests) */
    int lstfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lstfd < 0) { perror("socket"); return 1; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;   /* kernel picks a free ephemeral port: hermetic */
    if (bind(lstfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(lstfd, 4) < 0) { perror("listen"); return 1; }
    socklen_t alen = sizeof(addr);
    if (getsockname(lstfd, (struct sockaddr *)&addr, &alen) < 0) { perror("getsockname"); return 1; }
    int port = ntohs(addr.sin_port);
    pid_t srv = fork();
    if (srv == 0) {
        fixture_server(lstfd, 2, srvlog);
    }
    close(lstfd);

    /* 3. template -> login_page.js with __HOSTPORT__ substituted.
     *    (whole-file two-pass: token spans are tiny, crosses are handled by
     *    exact memcmp at every position — unlike the page-test's disabled
     *    substitution, this one MUST actually replace for the test to be real) */
    {
        char hostport[64];
        snprintf(hostport, sizeof(hostport), "127.0.0.1:%d", port);
        FILE *in = fopen(js_template, "rb");
        if (!in) { fprintf(stderr, "FAIL: open %s\n", js_template); return 1; }
        fseek(in, 0, SEEK_END);
        long fsz = ftell(in);
        fseek(in, 0, SEEK_SET);
        if (fsz < 0 || fsz > (1024 * 1024)) fclose(in), fprintf(stderr, "FAIL: template size\n"), exit(1);
        char *src = malloc((size_t)fsz + 1);
        size_t got = fread(src, 1, (size_t)fsz, in);
        src[got] = 0;
        fclose(in);
        unsigned char o[1024 * 1024];
        size_t oi = 0;
        const char *tok = "__HOSTPORT__";
        size_t tlen = strlen(tok);
        for (size_t i = 0; i < got; ) {
            if (got - i >= tlen && memcmp(src + i, tok, tlen) == 0) {
                for (const unsigned char *c = (const unsigned char *)hostport; *c && oi + 1 < sizeof(o); c++)
                    o[oi++] = *c;
                i += tlen;
            } else {
                if (oi + 1 < sizeof(o)) o[oi++] = (unsigned char)src[i];
                i++;
            }
        }
        FILE *out = fopen(page_path, "wb");
        if (!out) { fprintf(stderr, "FAIL: open %s\n", page_path); return 1; }
        fwrite(o, 1, oi, out);
        fclose(out);
        free(src);
    }

    /* 4. ensure the jar starts empty (no seed: everything must come from wire) */
    unlink(jar_path);

    /* 5. spawn worker over pipes */
    int to_child[2], from_child[2];
    if (pipe(to_child) || pipe(from_child)) { perror("pipe"); return 1; }
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        dup2(to_child[0], 0); dup2(from_child[1], 1);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execl(worker, worker, (char *)NULL);
        _exit(127);
    }
    close(to_child[0]); close(from_child[1]);

    /* 6. LOAD the page, climb to RENDER then STATUS. */
    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://127.0.0.1:%d/auth\nWall6 Login Test",
             page_path, dom_path, port);
    wsend(to_child[1], load);
    char reply[4096];
    char render[4096] = "";
    char *status = NULL;
    for (;;) {
        if (!wreply(from_child[0], reply, sizeof(reply))) {
            fprintf(stderr, "FAIL: no reply from worker\n");
            int st; waitpid(pid, &st, 0);
            if (WIFSIGNALED(st))
                fprintf(stderr, "harness: worker killed by signal %d — see WERR| stderr above\n", WTERMSIG(st));
            return 1;
        }
        if (strncmp(reply, "RENDER\n", 7) == 0) {
            size_t rn = strlen(reply + 7);
            if (rn + 1 < sizeof(render)) memcpy(render, reply + 7, rn + 1);
            continue;
        }
        status = reply;
        break;
    }
    int pass = (strncmp(status, "STATUS ok", 9) == 0);
    if (!pass) printf("WORKER said: %s\n", status);

    /* 7. assert the unified-store invariants */
    if (pass) {
        /* ingress: the wire Set-Cookie must be visible to document.cookie */
        if (!strstr(render, "SID=wlt456")) {
            fprintf(stderr, "FAIL: wire Set-Cookie not in document.cookie (unified store?)\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        /* egress: the reattached cookie reached the guard route */
        if (!strstr(render, "GUARD=guard-ok")) {
            fprintf(stderr, "FAIL: cookie not reattached on next request\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        /* jar file itself holds the wire cookie (byte proof) */
        char jar[512] = "";
        FILE *jf = fopen(jar_path, "rb");
        if (jf) {
            size_t jn = fread(jar, 1, sizeof(jar) - 1, jf);
            jar[jn] = 0;
            fclose(jf);
        }
        if (!strstr(jar, "sid") || !strstr(jar, "wlt456")) {
            fprintf(stderr, "FAIL: jar file missing wire cookie\n---- jar ----\n%s\n", jar);
            pass = 0;
        }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 00);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d — see above\n", WTERMSIG(st));
    if (srv > 0) waitpid(srv, &st, WNOHANG);

    printf("%s (wall-6 unified cookie store: wire Set-Cookie -> jar -> reattach: %s)\n",
           pass ? "PASS: worker_login_test" : "FAIL: worker_login_test",
           pass ? "SID=wlt456 + guard-ok (loopback only)" : "no");
    return pass ? 0 : 1;
}