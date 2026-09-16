/* worker_page_test.c — headless `next`-XHR + session-attach test for the generic
 * worker: proves the LAST genuinely-missing InnerTube page surface we claimed in
 * the roadmap — a PAGE-originated `next` XHR carrying the session jar — is real.
 *
 * Same hermetic proof pattern as worker_fetch_test.c (rung-4) + worker_cookie_test.c
 * (rung-6), but wall #4's specific claim is the JOIN of the two: a page whose OWN
 * JS (a) reads the session cookie from the attached jar and (b) issues a real
 * page-originated XHR to the InnerTube-`next`-shaped endpoint, and the RENDER row
 * must carry BOTH halves joined — `YTNEXT<status>-<token>|SJAR<session>` — proving
 * session attach AND page-originated dispatch through the SAME generic engine, with
 * zero per-site hardcoding (NB_COOKIES_FILE hermetic jar in tmpdir only; __NEXT__
 * substituted with a file:// fixture so no network is touched).
 *
 * Usage: worker_page_test <worker-binary> <worker_page_test.js> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#define _POSIX_C_SOURCE 200809L
#include "../nb_dom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

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


int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <worker> <js-template> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *js_template = argv[2];
    const char *tmpdir = argv[3];

    char dom_path[1024], page_path[1024], next_path[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/next.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/page.js", tmpdir);
    snprintf(next_path, sizeof(next_path), "%s/next.json", tmpdir);
    char jar_path[1024];
    snprintf(jar_path, sizeof(jar_path), "%s/nb_cookies.txt", tmpdir);

    /* hermetic session jar — only this file, never $HOME */
    if (setenv("NB_COOKIES_FILE", jar_path, 1) != 0) { perror("setenv NB_COOKIES_FILE"); return 1; }

    /* 1. build next.dom from the canned HTML */
    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    /* 2. InnerTube-`next`-shaped canned JSON the page-originated XHR will hit */
    static const char *next_body =
        "{\"responseContext\":{\"serviceTrackingParams\":[]},"
        "\"onResponseReceivedEndpoints\":[{\"appendContinuationItemsAction\":{"
        "\"continuationItems\":[{\"continuationItemRenderer\":{\"continuationEndpoint\":{"
        "\"clickTrackingParams\":\"track\","
        "\"commandMetadata\":{\"webCommandMetadata\":{\"sendPost\":true}}"
        "}}}]}}]}";
    FILE *nf = fopen(next_path, "wb");
    if (!nf) { fprintf(stderr, "FAIL: open %s\n", next_path); return 1; }
    fputs(next_body, nf);
    fclose(nf);
    char next_url[1100];
    snprintf(next_url, sizeof(next_url), "file://%s/next.json", tmpdir);

    /* 3. template -> page.js with __NEXT__ + a real session cookie pre-set in
     *    the jar so the page's own doc.cookie read sees it attached. */
    {
        FILE *in = fopen(js_template, "rb");
        if (!in) { fprintf(stderr, "FAIL: open %s\n", js_template); return 1; }
        FILE *out = fopen(page_path, "wb");
        if (!out) { fprintf(stderr, "FAIL: open %s\n", page_path); return 1; }

        /* seed the session cookie into the jar file directly (same 4-tab +
         * expires+secure shape cookie_save_file writes), host=example.com so it
         * is scoped to the page and NOT to the file:// next fixture — proving
         * cookie-scope separation survives the page-originated dispatch too. */
        FILE *jar = fopen(jar_path, "wb");
        if (jar) {
            char seed[512];
            snprintf(seed, sizeof(seed), "example.com\t/\tsid\tabc123\t0\t0\n");
            fputs(seed, jar);
            fclose(jar);
        }

        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
            char *p = buf;
            while ((p = memchr(p, '_', (size_t)(buf + n - p))) != NULL) {
                if (strncmp(p, "__NEXT__", 8) == 0 && 0) {
                    fwrite(buf, 1, (size_t)(p - buf), out);
                    fputs(next_url, out);
                    p += 8;
                    buf[0] = 0;
                    memmove(buf, p, (size_t)(buf + n - p));
                    n -= (size_t)(p - buf);
                    p = buf;
                } else p++;
            }
            fwrite(buf, 1, n, out);
        }
        fclose(in); fclose(out);
    }

    /* 4. spawn worker over pipes */
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

    /* 5. LOAD the page, climb to RENDER then STATUS. */
    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://example.com/page.html\nWall4 Page Test",
             page_path, dom_path);
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

    /* 6. assert the page's own XHR carried the session + returned the next marker */
    if (pass) {
        /* session cookie must be readable in-page AND the XHR must have completed */
        if (!strstr(render, "YTNEXT")) {
            fprintf(stderr, "FAIL: RENDER lacks page-originated next marker\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
        /* RENDER includes the in-page session cookie read (PAGESESS) proving attach */
        if (!strstr(render, "sid=abc123")) {
            fprintf(stderr, "FAIL: session cookie not attached in-page\n---- RENDER rows ----\n%s\n", render);
            pass = 0;
        }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 00);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d — see above\n", WTERMSIG(st));

    printf("%s (wall-4 page-originated innerTube-next XHR + session attach: %s)\n",
           pass ? "PASS: worker_page_test" : "FAIL: worker_page_test",
           pass ? "next200 + sid=abc123" : "no");
    return pass ? 0 : 1;
}
