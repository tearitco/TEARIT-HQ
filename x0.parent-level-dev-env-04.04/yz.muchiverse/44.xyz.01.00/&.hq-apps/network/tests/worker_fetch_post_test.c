/* worker_fetch_post_test.c — hermetic receipt proving the page-side fetch()
 * surface (the real youtube bundle's innerTube calls use fetch(), not XHR).
 * Same three row-35 legs as worker_innertube_test (visitor cookie + API key
 * + SAPISIDHASH signature) but driven through the host-world Promise polyfill
 * fetch() in nb_host.h: method POST, headers, JSON body, then response.json()
 * — the exact shape the youtube page's own JS uses for /youtubei/v1/browse.
 *
 * The fixture is a loopback-only HTTP server (127.0.0.1, ephemeral, no
 * external routes):
 *   1. GET /login  -> Set-Cookie yt-visitor_data + SAPISID + sid (=one jar)
 *   2. POST /youtubei/v1/browse?key=<KEY>&prettyPrint=false -> the page JS
 *      fetch(): read visitor+SAPISID from document.cookie, Promise-chain to
 *      build SAPISIDHASH via __nb_sha1, POST JSON body with Content-Type,
 *      X-Goog-Visitor-Id, Origin and Authorization headers. Fixture accepts
 *      (JSON {"legs":"ok"}) only when visitor+key+signature+JSON body all
 *      verify; any missing leg -> 401 innerFail:<reason>.
 *      The page also asserts the fetch rejection path (bad signature ->
 *      401 -> Promise.catch -> "authfail" marker), proving the chronicle
 *      prelude surfaces network failure to JS like a browser.
 *   3. GET /visitor -> cookies reattach (one jar egress), body vis-ok.
 * Renders: FETCH=<ok|authfail>|J=<json.legs>|SID=<sapisid>|VIS=<vis-ok>.
 *
 * Usage: worker_fetch_post_test <worker-binary> <worker_fetch_post_test.js> <tmpdir>
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

/* verify a SAPISIDHASH Authorization value against granted SAPISID + origin.
 * authval must be "SAPISIDHASH <ts>_<b64>".  Returns 1 on exact recompute. */
static int check_sig(const char *authval, const char *origin, char *detail, size_t dc) {
    const char *val = authval;
    if (strncmp(val, "SAPISIDHASH ", 12) != 0) {
        snprintf(detail, dc, "prefix"); return 0;
    }
    val += 12;
    const char *us = strchr(val, '_');
    if (!us) { snprintf(detail, dc, "nounderscore"); return 0; }
    char ts[64];
    if ((size_t)(us - val) >= sizeof(ts)) { snprintf(detail, dc, "tslen"); return 0; }
    memcpy(ts, val, (size_t)(us - val));
    ts[us - val] = 0;
    char sent[40];
    size_t sn = 0;
    const char *sp = us + 1;
    while (sp[sn] && sp[sn] != ' ' && sn + 1 < sizeof(sent)) { sent[sn] = sp[sn]; sn++; }
    sent[sn] = 0;
    char oreq[256];
    size_t on = 0;
    const char *osp = origin;
    while (*osp == ' ') osp++;
    while (osp[on] && osp[on] != ' ' && on + 1 < sizeof(oreq)) { oreq[on] = osp[on]; on++; }
    oreq[on] = 0;
    uint8_t in[1024];
    size_t in_len = 0;
    for (const char *q = ts; *q; q++) in[in_len++] = (uint8_t)*q;
    in[in_len++] = ' ';
    for (const char *q = NB_SAPISID; *q; q++) in[in_len++] = (uint8_t)*q;
    in[in_len++] = ' ';
    for (size_t q = 0; q < on; q++) in[in_len++] = (uint8_t)oreq[q];
    uint8_t dig[20];
    nbsha1(in, in_len, dig);
    char exp[29];
    nbsha1_b64_20(dig, exp);
    if (strcmp(exp, sent) != 0) { snprintf(detail, dc, "hash"); return 0; }
    return 1;
}

/* keep a second origin with a space-split first token ("Origin: https://...") */
static const char *origin_token(const char *origin) {
    if (strncmp(origin, "Origin:", 7) == 0) {
        const char *p = origin + 7;
        while (*p == ' ') p++;
        return p;
    }
    return origin;
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
        if (lf) { fprintf(lf, "req%d: %s\n", i, req); fflush(lf); }
        char bodybuf[4096] = "";
        char *body = NULL;
        if (strstr(req, "\r\n\r\n")) {
            char *nlx = strstr(req, "Content-Length:");
            long clen = 0;
            if (nlx) {
                char *dst = (char *)strchr(nlx, ':') + 1;
                while (*dst == ' ') dst++;
                clen = strtol(dst, NULL, 10);
            }
            if (clen > 0 && clen < (long)sizeof(bodybuf) - 1) {
                size_t have = 0;
                size_t hdrs = (size_t)(strstr(req, "\r\n\r\n") + 4 - req);
                if (rl > hdrs) have = rl - hdrs;
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
        } else if (strstr(req, "/badsig")) {
            /* deliberately failing route: same legs but the page signs a
             * WRONG secret so the fixture's recompute rejects -> 401. Used
             * to prove the fetch() rejection path surfaces to page JS. */
            const char *body2 = "innerFail:hash";
            char resp[512];
            int n = snprintf(resp, sizeof(resp),
                "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                (int)strlen(body2), body2);
            (void)!write(cfd, resp, (size_t)n);
        } else if (strstr(req, "/youtubei/v1/browse")) {
            const char *body_s = body ? body : "";
            const char *fail = NULL;
            if (!(cookie && strstr(cookie + 7, "yt-visitor_data=" NB_VISITOR)))
                fail = "visitor";
            if (!fail && !strstr(req, "?key=" NB_APIKEY) && !strstr(req, "&key=" NB_APIKEY))
                fail = "nokey";
            if (!fail && (!strstr(body_s, "\"clientName\":\"WEB\"") || !strstr(body_s, "\"browseId\"")))
                fail = "notjson";
            if (!fail) {
                char detail[64] = "";
                if (!auth || !origin ||
                    !check_sig(auth + 15, origin_token(origin ? origin + 7 : ""), detail, sizeof(detail)))
                    fail = "sign";
            }
            if (!fail) {
                const char *json = "{\"legs\":\"ok\"}";
                char resp[512];
                int n = snprintf(resp, sizeof(resp),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    (int)strlen(json), json);
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
        } else {   /* /visitor */
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
    snprintf(dom_path, sizeof(dom_path), "%s/fp.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/fp_page.js", tmpdir);
    snprintf(srvlog, sizeof(srvlog), "%s/fp_fixture.log", tmpdir);
    snprintf(jar_path, sizeof(jar_path), "%s/nb_fp_cookies.txt", tmpdir);

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
        fixture_server(lstfd, 4, srvlog);
    }
    close(lstfd);

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
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://127.0.0.1:%d/login\nWall-6 fetch() innerTube feed Test",
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
        if (!strstr(render, "FETCH=<ok>")) {
            fprintf(stderr, "FAIL: fetch() innerTube POST not accepted\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "J=<ok>")) {
            fprintf(stderr, "FAIL: response.json() did not return legs=ok\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "AUTH=<authfail>")) {
            fprintf(stderr, "FAIL: fetch() rejection path (401) not surfaced to JS\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        if (!strstr(render, "SID=<" NB_SAPISID ">") || !strstr(render, "VIS=<vis-ok>")) {
            fprintf(stderr, "FAIL: jar/egress markers missing\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 00);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d — see above\n", WTERMSIG(st));
    if (srv > 0) waitpid(srv, &st, WNOHANG);

    printf("%s (fetch() innerTube surface: page JS Promise-chain smart fetch POST with "
           "visitor+key+sign, response.json(), and 401 rejection all verified: %s)\n",
           pass ? "PASS: worker_fetch_post_test" : "FAIL: worker_fetch_post_test",
           pass ? "FETCH=ok + J=ok + AUTH=authfail (loopback only)" : "no");
    return pass ? 0 : 1;
}