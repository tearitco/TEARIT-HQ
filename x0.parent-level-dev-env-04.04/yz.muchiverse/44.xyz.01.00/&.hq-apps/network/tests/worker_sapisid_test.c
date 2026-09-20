/* worker_sapisid_test.c — hermetic real-login-surface test for the generic
 * worker: proves the google-shaped SAPISIDHASH handshake, page-originated.
 * Row 34b (the "real login" half of row 34).  Everything is done by ordinary
 * page JS against an engine that provides only ONE new generic primitive,
 * __nb_sha1 (browsers have no LOGIN op; the site's own script signs itself).
 *
 * The fixture is a loopback-only HTTP server (127.0.0.1, ephemeral port, no
 * external routes).  The page JS makes THREE page-originated XHRs:
 *   1. GET /login  -> server replies Set-Cookie: SAPISID=<rnd>; Path=/ and a
 *                     session cookie; body login-ok.  Engine ingests to jar.
 *   2. GET /guard  -> page reads document.cookie, builds
 *                     SAPISIDHASH = <ts>_<base64(sha1(<ts> " " <SAPISID>
 *                     " " <origin>))>   (Date.now() + __nb_sha1, stock JS),
 *                     sends it as `Authorization: SAPISIDHASH <ts>_<b64>`
 *                     plus `Origin:` on a /youtubei/v1-style path.  Server
 *                     RECOMPUTES the same sha1 from the received ts + the
 *                     SAPISID it granted + the received Origin, and replies
 *                     guard-ok only on an exact match, else sign-fail (401).
 *   3. GET /reagent -> the session cookie from step 1 must still be attached
 *                     (one jar, egress on every request).
 * The page renders SIGN=<body>|SID=<sapisid>|RE=<body>, and the driver asserts
 * signature match + jar re-attach.  Zero per-site hardcoding in the engine,
 * NB_COOKIES_FILE jar in tmpdir only, loopback only, openssl-vector-checked
 * sha1 shared in nb_sha1.h.
 *
 * Usage: worker_sapisid_test <worker-binary> <worker_sapisid_test.js> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#define _POSIX_C_SOURCE 200809L
#include "../nb_dom.h"
#include "../ops/nb_sha1.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NB_SAPISID "sapisid_w7k9q2"   /* fixture-hosted login secret, not engined */

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
    if (strncmp(buf, "LIVE|", 5) == 0) return wreply(fd, buf, cap);  /* keepalive */
    return 1;
}

/* --- tiny loopback fixture server (child process) -----------------------
 * accepts `want` connections; reads the request head (capturing Cookie:,
 * Origin: and Authorization: headers); answers the well-known /login,
 * /guard and /reagent routes; records what it saw into `log` for the driver
 * to assert.  /guard VERIFIES the SAPISIDHASH signature by recomputing the
 * sha1 over received-timestamp + the SAPISID it granted + received Origin,
 * using the SAME header implementation the worker exposes to page JS. */
static void fixture_server(int lstfd, int want, const char *log) {
    FILE *lf = fopen(log, "wb");
    for (int i = 0; i < want; i++) {
        int cfd = accept(lstfd, NULL, NULL);
        if (cfd < 0) break;
        char req[8192]; size_t rl = 0;
        while (rl + 1 < sizeof(req)) {
            char b;
            ssize_t r = read(cfd, &b, 1);
            if (r <= 0) break;
            req[rl++] = b;
            if (rl >= 4 && req[rl-4] == '\r' && req[rl-3] == '\n'
                && req[rl-2] == '\r' && req[rl-1] == '\n') break;
        }
        req[rl < sizeof(req) ? rl : sizeof(req)-1] = 0;
        if (lf) fprintf(lf, "req%d: %s\n", i, req);
        char *cookie = strstr(req, "Cookie:");
        char *origin = strstr(req, "Origin:");
        char *auth = strstr(req, "Authorization:");
        for (char *p = req; p && *p; p++) {
            if (*p == '\r' || *p == '\n') *p = ' ';
        }

        if (strstr(req, "/login")) {
            const char *resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Set-Cookie: SAPISID=" NB_SAPISID "; Path=/\r\n"
                "Set-Cookie: sid=ssr77; Path=/\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "login-ok";
            (void)!write(cfd, resp, strlen(resp));
        } else if (strstr(req, "/guard")) {
            /* recompute the signature: sha1(<ts> " " <SAPISID> " " <origin>) */
            int ok = 0;
            if (auth && origin) {
                /* "Authorization:" is 14 chars + one space -> value at +15 */
                const char *val = auth + 15;
                if (strncmp(val, "SAPISIDHASH ", 12) == 0) {
                    val += 12;                                    /* ts part */
                    char ts[64];
                    const char *us = strchr(val, '_');
                    if (us && (size_t)(us - val) < sizeof(ts)) {
                        memcpy(ts, val, (size_t)(us - val));
                        ts[us - val] = 0;
                        const char *sp = us + 1;                 /* b64 token start (inside req buf) */
                        char sent[40];
                        size_t sn = 0;
                        while (sp[sn] && sp[sn] != ' ' && sn + 1 < sizeof(sent)) {
                            sent[sn] = sp[sn]; sn++;
                        }
                        sent[sn] = 0;
                        const char *osp = origin + 7;            /* after "Origin:" */
                        while (*osp == ' ') osp++;
                        char oreq[256];
                        size_t on = 0;
                        while (osp[on] && osp[on] != ' ' && on + 1 < sizeof(oreq)) {
                            oreq[on] = osp[on]; on++;
                        }
                        oreq[on] = 0;
                        int origin_len = (int)on;
                        /* input = <ts> " " SAPISID " " origin */
                        uint8_t in[1024];
                        size_t in_len = 0;
                        for (const char *q = ts; *q; q++) in[in_len++] = (uint8_t)*q;
                        in[in_len++] = ' ';
                        for (const char *q = NB_SAPISID; *q; q++) in[in_len++] = (uint8_t)*q;
                        in[in_len++] = ' ';
                        for (int q = 0; q < origin_len; q++) in[in_len++] = (uint8_t)oreq[q];
                        uint8_t dig[20];
                        nbsha1(in, in_len, dig);
                        char exp[29];
                        nbsha1_b64_20(dig, exp);
                        ok = (strcmp(exp, sent) == 0);
                    }
                }
            }
            const char *body = ok ? "guard-ok" : "sign-fail";
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
        } else {   /* /reagent: the earlier session cookie must be attached */
            int ok = (cookie != NULL && strstr(cookie + 7, "sid=ssr77") != NULL);
            const char *body = ok ? "re-ok" : "re-fail";
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
    snprintf(dom_path, sizeof(dom_path), "%s/sapisid.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/sapisid_page.js", tmpdir);
    snprintf(srvlog, sizeof(srvlog), "%s/sapisid_fixture.log", tmpdir);
    snprintf(jar_path, sizeof(jar_path), "%s/nb_sapisid_cookies.txt", tmpdir);

    if (setenv("NB_COOKIES_FILE", jar_path, 1) != 0) { perror("setenv NB_COOKIES_FILE"); return 1; }
    unsetenv("NB_CURL_COOKIES_FILE");

    /* 1. build sapisid.dom from the canned HTML */
    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    /* 2. loopback fixture server (127.0.0.1, ephemeral port, 3 requests) */
    int lstfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lstfd < 0) { perror("socket"); return 1; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(lstfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(lstfd, 4) < 0) { perror("listen"); return 1; }
    socklen_t alen = sizeof(addr);
    if (getsockname(lstfd, (struct sockaddr *)&addr, &alen) < 0) { perror("getsockname"); return 1; }
    int port = ntohs(addr.sin_port);
    pid_t srv = fork();
    if (srv == 0) {
        fixture_server(lstfd, 3, srvlog);
    }
    close(lstfd);

    /* 3. template -> sapisid_page.js with __HOSTPORT__ substituted */
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

    /* 4. ensure the jar starts empty (everything must come from wire) */
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
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://127.0.0.1:%d/login\nWall6 SAPISID Login Test",
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

    /* 7. assert the real-login invariants */
    if (pass) {
        if (!strstr(render, "SIGN=<guard-ok>")) {
            fprintf(stderr, "FAIL: SAPISIDHASH signature not accepted\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "SID=<" NB_SAPISID ">")) {
            fprintf(stderr, "FAIL: SAPISID not visible to document.cookie\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "RE=<re-ok>")) {
            fprintf(stderr, "FAIL: session cookie not reattached on later request\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        /* jar file holds the wire-granted SAPISID + session cookie (byte proof) */
        char jar[512] = "";
        FILE *jf = fopen(jar_path, "rb");
        if (jf) {
            size_t jn = fread(jar, 1, sizeof(jar) - 1, jf);
            jar[jn] = 0;
            fclose(jf);
        }
        if (!strstr(jar, NB_SAPISID) || !strstr(jar, "ssr77")) {
            fprintf(stderr, "FAIL: jar file missing granted cookies\n---- jar ----\n%s\n", jar);
            pass = 0;
        }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 00);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d — see above\n", WTERMSIG(st));
    if (srv > 0) waitpid(srv, &st, WNOHANG);

    printf("%s (row-34b real login: page JS signs SAPISIDHASH via __nb_sha1, "
           "fixture recomputes sha1 from granted SAPISID + Origin + ts: %s)\n",
           pass ? "PASS: worker_sapisid_test" : "FAIL: worker_sapisid_test",
           pass ? "SIGN=guard-ok + SID=" NB_SAPISID " + RE=re-ok (loopback only)" : "no");
    return pass ? 0 : 1;
}