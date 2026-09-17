/* worker_innertube_test.c — hermetic "feed InnerTube from the browser" receipt
 * (roadmap row 35).  The page's own JS issues a google-shaped InnerTube
 * `browse` API call THROUGH the browser's network layer, exactly like the
 * real youtube page does: visitor-data cookie, API key on the query string,
 * and a SAPISIDHASH-signed Authorization header, with a JSON browse body.
 *
 * The fixture is a loopback-only HTTP server (127.0.0.1, ephemeral port, no
 * external routes).
 *   1. GET /login  -> Set-Cookie: yt-visitor_data=<v>; Set-Cookie:
 *                     SAPISID=<secret>; sid=ssr77 (body login-ok)
 *   2. POST /youtubei/v1/browse?key=<KEY>&prettyPrint=false  -> page reads
 *                     document.cookie, then sends:
 *                       Content-Type: application/json
 *                       X-Goog-Visitor-Id: <v>
 *                       Authorization: SAPISIDHASH <ts>_<b64(sha1(ts " "
 *                                     SAPISID " " origin))>
 *                       Origin: https://www.youtube.com
 *                       body: {"context":{"client":{"clientName":"WEB",
 *                             "clientVersion":"2.2026..."}},
 *                             "browseId":"FEwhat_to_watch"}
 *                     The fixture requires ALL THREE legs of row 35 — valid
 *                     yt-visitor_data cookie, matching API key, VALID
 *                     signature (recomputed from granted SAPISID + received
 *                     ts + Origin) — plus a well-formed innerTube JSON body;
 *                     replies innerYes only when every leg passes.
 *   3. GET /visitor -> the same visitor + sid cookies must still be attached
 *                      (one jar; egress on every subsequent request).
 * The page renders IT=<body>|SID=<sapisid>|VIS=<body>, driver asserts the
 * accept triple.  Zero per-site hardcoding in the engine (the page JS + the
 * fixture carry the youtube shape, like wss).
 *
 * Usage: worker_innertube_test <worker-binary> <worker_innertube_test.js> <tmpdir>
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

#define NB_VISITOR "CAMoOzY9iAIRABAK"
#define NB_SAPISID "sapisid_w7k9q2"
#define NB_APIKEY  "AIzaSyHERMETICKEY0123456789"
#define NB_ORIGIN  "https://www.youtube.com"

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

/* --- tiny loopback fixture server (child process) --------------------- */
static void fixture_server(int lstfd, int want, const char *log) {
    FILE *lf = fopen(log, "wb");
    for (int i = 0; i < want; i++) {
        int cfd = accept(lstfd, NULL, NULL);
        if (cfd < 0) break;
        char req[16384]; size_t rl = 0;
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
        /* read the POST body (Content-Length bytes after the blank line) */
        char bodybuf[4096] = "";
        char *body = NULL;
        if (rl + 1 < sizeof(req) && strstr(req, "\r\n\r\n")) {
            char *nlx = strstr(req, "Content-Length:");
            long clen = 0;
            if (nlx) {
                char *dst = (char *)strchr(nlx, ':') + 1;
                while (*dst && (*dst == ' ')) dst++;
                clen = strtol(dst, NULL, 10);
            }
            if (clen > 0 && clen < (long)sizeof(bodybuf) - 1) {
                size_t have = 0;
                size_t hdrs = (size_t)(strstr(req, "\r\n\r\n") + 4 - req);
                if (rl > hdrs) have = rl - hdrs;          /* already buffered */
                size_t need = (size_t)clen - have;
                while (need) {
                    ssize_t r = read(cfd, bodybuf + have, need > 4096 ? 4096 : need);
                    if (r <= 0) break;
                    have += (size_t)r;
                    need -= (size_t)r;
                }
                bodybuf[have] = 0;
                body = bodybuf;
            }
        }
        char *cookie = strstr(req, "Cookie:");
        char *origin = strstr(req, "Origin:");
        char *auth = strstr(req, "Authorization:");
        char *ctype = strstr(req, "Content-Type:");
        char *gvis = strstr(req, "X-Goog-Visitor-Id:");
        for (char *p = req; p && *p; p++) {
            if (*p == '\r' || *p == '\n') *p = ' ';
        }

        if (strstr(req, "/login")) {
            const char *resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Set-Cookie: yt-visitor_data=" NB_VISITOR "; Path=/\r\n"
                "Set-Cookie: SAPISID=" NB_SAPISID "; Path=/\r\n"
                "Set-Cookie: sid=ssr77; Path=/\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "login-ok";
            (void)!write(cfd, resp, strlen(resp));
        } else if (strstr(req, "/youtubei/v1/browse")) {
            const char *body_s = body ? body : "";
            const char *fail = NULL;

            /* leg 1: valid yt-visitor_data cookie */
            if (!(cookie && strstr(cookie + 7, "yt-visitor_data=" NB_VISITOR)))
                fail = "visitor";
            /* leg 2: API key on the query string */
            if (!fail && !strstr(req, "?key=" NB_APIKEY)
                     && !strstr(req, "&key=" NB_APIKEY))
                fail = "nokey";
            /* leg 2b: well-formed innerTube browse body */
            if (!fail && (!strstr(body_s, "\"clientName\":\"WEB\"")
                       || !strstr(body_s, "\"browseId\"")))
                fail = "notjson";
            /* leg 3: valid SAPISIDHASH signature (recompute server-side) */
            if (!fail) {
                int sigok = 0;
                if (auth && origin && strncmp(auth + 15, "SAPISIDHASH ", 12) == 0) {
                    const char *val = auth + 27;
                    char ts[64];
                    const char *us = strchr(val, '_');
                    if (us && (size_t)(us - val) < sizeof(ts)) {
                        memcpy(ts, val, (size_t)(us - val));
                        ts[us - val] = 0;
                        char sent[40];
                        size_t sn = 0;
                        const char *sp = us + 1;
                        while (sp[sn] && sp[sn] != ' ' && sn + 1 < sizeof(sent)) {
                            sent[sn] = sp[sn]; sn++;
                        }
                        sent[sn] = 0;
                        const char *osp = origin + 7;
                        while (*osp == ' ') osp++;
                        char oreq[256];
                        size_t on = 0;
                        while (osp[on] && osp[on] != ' ' && on + 1 < sizeof(oreq)) {
                            oreq[on] = osp[on]; on++;
                        }
                        oreq[on] = 0;
                        uint8_t in[1024];
                        size_t in_len = 0;
                        for (const char *q = ts; *q; q++) in[in_len++] = (uint8_t)*q;
                        in[in_len++] = ' ';
                        for (const char *q = NB_SAPISID; *q; q++) in[in_len++] = (uint8_t)*q;
                        in[in_len++] = ' ';
                        for (int q = 0; q < on; q++) in[in_len++] = (uint8_t)oreq[q];
                        uint8_t dig[20];
                        nbsha1(in, in_len, dig);
                        char exp[29];
                        nbsha1_b64_20(dig, exp);
                        sigok = (strcmp(exp, sent) == 0);
                    }
                }
                if (!sigok) fail = "sign";
            }
            if (!fail) {
                const char *body2 = "innerYes";
                char resp[512];
                int n = snprintf(resp, sizeof(resp),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    (int)strlen(body2), body2);
                (void)!write(cfd, resp, (size_t)n);
            } else {
                char resp[512];
                int n = snprintf(resp, sizeof(resp),
                    "HTTP/1.1 401 Unauthorized\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "innerFail:%s",
                    (int)strlen("innerFail:") + (int)strlen(fail), fail);
                (void)!write(cfd, resp, (size_t)n);
            }
        } else {   /* /visitor: cookies must still ride along */
            int ok = (cookie != NULL
                      && strstr(cookie + 7, "sid=ssr77") != NULL
                      && strstr(cookie + 7, "yt-visitor_data=" NB_VISITOR) != NULL);
            const char *body2 = ok ? "vis-ok" : "vis-fail";
            char resp[512];
            int n = snprintf(resp, sizeof(resp),
                "HTTP/1.1 %d\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                ok ? 200 : 401, (int)strlen(body2), body2);
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
    snprintf(dom_path, sizeof(dom_path), "%s/it.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/it_page.js", tmpdir);
    snprintf(srvlog, sizeof(srvlog), "%s/it_fixture.log", tmpdir);
    snprintf(jar_path, sizeof(jar_path), "%s/nb_it_cookies.txt", tmpdir);

    if (setenv("NB_COOKIES_FILE", jar_path, 1) != 0) { perror("setenv NB_COOKIES_FILE"); return 1; }
    unsetenv("NB_CURL_COOKIES_FILE");

    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

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

    /* substitute __HOSTPORT__ + __APIKEY__ in the page template */
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
        const struct { const char *tok; const char *val; } subs[] = {
            { "__HOSTPORT__", hostport },
            { "__APIKEY__",   NB_APIKEY },
        };
        unsigned char o[1024 * 1024];
        size_t oi = 0;
        size_t i = 0;
        while (i < got) {
            int done = 0;
            for (size_t s = 0; s < sizeof(subs) / sizeof(subs[0]); s++) {
                size_t tlen = strlen(subs[s].tok);
                if (got - i >= tlen && memcmp(src + i, subs[s].tok, tlen) == 0) {
                    for (const unsigned char *c = (const unsigned char *)subs[s].val; *c && oi + 1 < sizeof(o); c++)
                        o[oi++] = *c;
                    i += tlen;
                    done = 1;
                    break;
                }
            }
            if (!done) {
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

    unlink(jar_path);

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

    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://127.0.0.1:%d/login\nWall-6 InnerTube Feed Test",
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

    if (pass) {
        if (!strstr(render, "IT=<innerYes>")) {
            fprintf(stderr, "FAIL: browner innerTube accept triple (visitor+key+sign)\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "SID=<" NB_SAPISID ">")) {
            fprintf(stderr, "FAIL: SAPISID not visible to document.cookie\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "VIS=<vis-ok>")) {
            fprintf(stderr, "FAIL: visitor+session cookies not reattached\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        char jar[512] = "";
        FILE *jf = fopen(jar_path, "rb");
        if (jf) {
            size_t jn = fread(jar, 1, sizeof(jar) - 1, jf);
            jar[jn] = 0;
            fclose(jf);
        }
        if (!strstr(jar, NB_VISITOR) || !strstr(jar, NB_SAPISID) || !strstr(jar, "ssr77")) {
            fprintf(stderr, "FAIL: jar missing granted cookies\n---- jar ----\n%s\n", jar);
            pass = 0;
        }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 00);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d — see above\n", WTERMSIG(st));
    if (srv > 0) waitpid(srv, &st, WNOHANG);

    printf("%s (row-35 in-page InnerTube feed: page JS POSTs /youtubei/v1/browse "
           "with visitor_data + API key + SAPISIDHASH sign; fixture recomputes sha1 "
           "and accepts only all three: %s)\n",
           pass ? "PASS: worker_innertube_test" : "FAIL: worker_innertube_test",
           pass ? "IT=innerYes + VIS=vis-ok (loopback only)" : "no");
    return pass ? 0 : 1;
}