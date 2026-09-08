/* worker_cookie_test.c — rung-6 document.cookie file-jar test for the worker.
 *
 * Spawns the real worker over pipes and drives THREE LOADs in one process:
 *   1. page_set.js   href http://example.com/dir/page.html
 *        write alpha (path=/) + beta (path=/, max-age=3600), then delete
 *        "gone" via max-age=0 and expire "old" in the past; assert read-back.
 *   2. page_get.js   href http://example.com/deep/other.html   (fresh heap!)
 *        read the JAR from disk — asserts persistence across LOADs (each LOAD
 *        runs a new Duktape heap, so the file is the only persistence).
 *   3. page_scope.js href http://other.test/x
 *        assert other.test does NOT see example.com cookies (host scoping).
 *
 * The jar path comes from $NB_COOKIES_FILE (set here before exec), so the
 * test is hermetic — nothing touches $HOME or the app dir.
 *
 * Usage: worker_cookie_test <worker-binary> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#define _POSIX_C_SOURCE 200809L
#include "../nb_dom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static const char *canned_html = "<html><body><div id=\"u\"></div></body></html>";

/* ---- tiny line-RPC client (same framing as worker_dom_test.c) ---- */
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

static const char *g_dom_path;

static int wrk_load(int to, int from, const char *js, const char *href, const char *tag) {
    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\n%s\nCookie Test", js, g_dom_path, href);
    wsend(to, load);
    char reply[4096];
    for (;;) {
        if (!wreply(from, reply, sizeof(reply))) {
            fprintf(stderr, "FAIL[%s]: no reply from worker\n", tag);
            return 0;
        }
        if (strncmp(reply, "RENDER\n", 7) == 0) continue;   /* step-4 rows */
        if (strncmp(reply, "STATUS ok", 9) == 0) { printf("PASS[%s] STATUS ok\n", tag); return 1; }
        fprintf(stderr, "FAIL[%s]: worker said %s\n", tag, reply);
        return 0;
    }
}

static void write_text(const char *path, const char *s) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(2); }
    fwrite(s, 1, strlen(s), f);
    fclose(f);
}

static const char *page_set =
    "// LOAD 1: write + read back in the same page\n"
    "document.cookie = 'alpha=hello world; path=/';\n"
    "document.cookie = 'beta=2; path=/; max-age=3600';\n"
    "document.cookie = 'gone=0; path=/; max-age=0';\n"
    "document.cookie = 'old=0; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT';\n"
    "var c = document.cookie;\n"
    "console.log('SETREAD', JSON.stringify(c));\n"
    "if (c.indexOf('alpha=hello world') < 0) throw new Error('alpha not readable after set');\n"
    "if (c.indexOf('beta=2') < 0) throw new Error('beta not readable after set');\n"
    "if (c.indexOf('gone') >= 0) throw new Error('max-age=0 cookie persisted');\n"
    "if (c.indexOf('old') >= 0) throw new Error('expired cookie persisted');\n";

static const char *page_get =
    "// LOAD 2: FRESH heap — read the persisted jar from disk\n"
    "var c = document.cookie;\n"
    "console.log('GETREAD', JSON.stringify(c));\n"
    "if (c.indexOf('alpha=hello world') < 0) throw new Error('alpha not persisted across LOAD');\n"
    "if (c.indexOf('beta=2') < 0) throw new Error('beta not persisted across LOAD');\n"
    "if (c.indexOf('gone') >= 0) throw new Error('deleted cookie leaked across LOAD');\n"
    "if (c.indexOf('old') >= 0) throw new Error('expired cookie leaked across LOAD');\n";

static const char *page_scope =
    "// LOAD 3: other.test must NOT see example.com cookies\n"
    "var c = document.cookie;\n"
    "console.log('SCOPEREAD', JSON.stringify(c));\n"
    "if (c.indexOf('alpha') >= 0) throw new Error('cookie leaked across hosts');\n"
    "if (c.indexOf('beta') >= 0) throw new Error('cookie leaked across hosts');\n";

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <worker> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *tmpdir = argv[2];

    char dom_path[1024], jar_path[1024], js_set[1024], js_get[1024], js_scope[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/fetch.dom", tmpdir);
    snprintf(jar_path, sizeof(jar_path), "%s/nb_cookies.txt", tmpdir);
    snprintf(js_set, sizeof(js_set), "%s/page_set.js", tmpdir);
    snprintf(js_get, sizeof(js_get), "%s/page_get.js", tmpdir);
    snprintf(js_scope, sizeof(js_scope), "%s/page_scope.js", tmpdir);
    g_dom_path = dom_path;

    /* hermetic jar: only this tmpdir's file is touched, never $HOME */
    if (setenv("NB_COOKIES_FILE", jar_path, 1) != 0) { perror("setenv NB_COOKIES_FILE"); return 1; }

    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    write_text(js_set, page_set);
    write_text(js_get, page_get);
    write_text(js_scope, page_scope);

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

    int pass = 1;
    pass &= wrk_load(to_child[1], from_child[0], js_set, "http://example.com/dir/page.html", "set");
    pass &= wrk_load(to_child[1], from_child[0], js_get, "http://example.com/deep/other.html", "get");
    pass &= wrk_load(to_child[1], from_child[0], js_scope, "http://other.test/x", "scope");

    /* jar on disk must have persisted alpha+beta under example.com, and must
     * not contain the deleted/expired entries or the other host. */
    FILE *jf = fopen(jar_path, "rb");
    if (!jf) { fprintf(stderr, "FAIL: jar %s missing after loads\n", jar_path); pass = 0; }
    else {
        char jb[8192];
        size_t n = fread(jb, 1, sizeof(jb) - 1, jf);
        fclose(jf);
        jb[n] = 0;
        printf("--- jar (%s) ---\n%s--- end jar ---\n", jar_path, jb);
        if (strstr(jb, "example.com\t/\talpha\thello world") == NULL) {
            fprintf(stderr, "FAIL: jar missing alpha entry\n"); pass = 0; }
        if (strstr(jb, "example.com\t/\tbeta\t2") == NULL) {
            fprintf(stderr, "FAIL: jar missing beta entry\n"); pass = 0; }
        if (strstr(jb, "\tgone") != NULL) { fprintf(stderr, "FAIL: deleted cookie in jar\n"); pass = 0; }
        if (strstr(jb, "\told\t") != NULL) { fprintf(stderr, "FAIL: expired cookie in jar\n"); pass = 0; }
        if (strstr(jb, "other.test") != NULL) { fprintf(stderr, "FAIL: foreign host in jar\n"); pass = 0; }
    }
    if (pass) printf("PASS: worker_cookie_test -> jar persisted + scoped\n");
    else printf("FAIL: worker_cookie_test\n");

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 0);
    return pass ? 0 : 1;
}