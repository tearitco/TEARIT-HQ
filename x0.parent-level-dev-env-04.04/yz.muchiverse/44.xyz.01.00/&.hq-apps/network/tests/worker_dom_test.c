/* worker_dom_test.c — headless rung-2 DOM test for the NB-JS worker.
 *
 * Phases 1 step 3 verification (plan §7 step 3): builds a small canned
 * HTML document, serializes it to fetch.dom (nb_serialize), writes the
 * companion worker_dom_test.js as page.js, then spawns the real worker
 * binary (ops/+x/nb_js_worker.+x) over a socketpair-style stdin/stdout and
 * speaks the plan §4 line-RPC: LOAD<page.js><fetch.dom><href><title> then
 * QUIT. Passes iff the worker answers STATUS ok (the JS throws on any
 * failed assertion, which the worker reports as STATUS err:...). Since
 * step 4 the worker may emit a RENDER frame before STATUS; it is skipped.
 * No real network is touched.
 *
 * Usage: worker_dom_test <worker-binary> <worker_dom_test.js> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#include "../nb_dom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static const char *canned_html =
    "<html><body>"
    "<div id=\"banner\" class=\"top bar\">Hello <b>world</b> &amp; ciao</div>"
    "<ul id=\"list\"><li class=\"item\">one</li>"
    "<li class=\"item two\">two</li><li>three</li></ul>"
    "<p data-k=\"v\">para</p>"
    "</body></html>";

/* ---- tiny line-RPC client (mirrors the worker's framing) ---- */
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
    if (argc < 4) { fprintf(stderr, "usage: %s <worker> <js> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *js_file = argv[2];
    const char *tmpdir = argv[3];

    /* 1. build fetch.dom from the canned HTML */
    char dom_path[1024], page_path[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/fetch.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/page.js", tmpdir);
    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open fetch.dom\n"); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    /* 2. copy worker_dom_test.js -> page.js */
    {
        FILE *in = fopen(js_file, "rb");
        if (!in) { fprintf(stderr, "FAIL: open %s\n", js_file); return 1; }
        FILE *out = fopen(page_path, "wb");
        if (!out) { fprintf(stderr, "FAIL: open %s\n", page_path); return 1; }
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
        fclose(in); fclose(out);
    }

    /* 3. spawn worker over a pipe */
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

    /* 4. LOAD then QUIT. Skip any RENDER frame (step 4) until STATUS. */
    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://localhost/a\nTest Page",
             page_path, dom_path);
    wsend(to_child[1], load);
    char reply[4096];
    char *status = NULL;
    for (;;) {
        if (!wreply(from_child[0], reply, sizeof(reply))) {
            fprintf(stderr, "FAIL: no reply from worker\n");
            int st; waitpid(pid, &st, 0); return 1;
        }
        if (strncmp(reply, "RENDER\n", 7) == 0) continue;  /* step 4 rows */
        status = reply;
        break;
    }
    int pass = (strncmp(status, "STATUS ok", 9) == 0);
    if (!pass) printf("WORKER said: %s\n", status);
    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 0);

    printf("%s\n", pass ? "PASS: worker_dom_test -> STATUS ok" : "FAIL: worker_dom_test");
    return pass ? 0 : 1;
}
