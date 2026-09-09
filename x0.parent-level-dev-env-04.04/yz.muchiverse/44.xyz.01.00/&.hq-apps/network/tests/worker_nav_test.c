/* worker_nav_test.c — rung-6 slice 2 navigation test for the worker.
 *
 * Spawns the real worker over pipes and drives LOADs that read location /
 * history inside the page and assert the emitted NAV frames (the manager
 * consumes NAV frames from worker_load and re-fetches; this test verifies
 * the worker side of that contract exactly).
 *
 * Expected NAV frame format (length-prefixed payload, may contain \n):
 *   NAV\n<kind>\n<url-or-count>\n
 * where kind ∈ GO/REPLACE/RELOAD/BACK/FORWARD/ADDR.
 *
 * LOADs:
 *   assign.js      location.assign('c/next.html')  -> GO   (dir-relative)
 *   href.js        location.href = '/root.html'     -> GO   (root-relative)
 *   replace.js     location.replace('https://...')  -> REPLACE (absolute)
 *   reload.js      location.reload()                -> RELOAD
 *   state.js       history.replaceState + assert    -> ADDR
 *   push.js        history.pushState('next/x')      -> ADDR (dir-relative)
 *   back.js        history.back()                   -> BACK 1
 *   goback.js      history.go(-2)                   -> BACK 2
 *   gofwd.js       history.go(2)                    -> FORWARD 2
 *   none.js        no navigation call               -> NO NAV frame
 *   hrefcheck.js   re-LOAD with the nav target href -> state moved on
 *
 * Usage: worker_nav_test <worker-binary> <tmpdir>
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

/* ---- tiny line-RPC client (same framing as worker_cookie_test.c) ---- */
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

/* LOAD a page; capture the LAST NAV frame ("" if the page issued none),
 * and require the worker to finish with STATUS ok. Returns 1 on pass. */
static int nav_load(int to, int from, const char *js, const char *href,
                    char *nav, size_t navcap) {
    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\n%s\nNav Test", js, g_dom_path, href);
    wsend(to, load);
    nav[0] = 0;
    char reply[8192];
    for (;;) {
        if (!wreply(from, reply, sizeof(reply))) {
            fprintf(stderr, "FAIL: no reply from worker for %s\n", href);
            return 0;
        }
        if (strncmp(reply, "RENDER\n", 7) == 0) continue;   /* step-4 rows */
        if (strncmp(reply, "NAV\n", 4) == 0) {
            snprintf(nav, navcap, "%s", reply);
            continue;
        }
        if (strncmp(reply, "STATUS ok", 9) == 0) return 1;
        fprintf(stderr, "FAIL: unexpected worker reply: %s\n", reply);
        return 0;
    }
}

static int expect_nav(const char *tag, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL[%s]: nav frame mismatch\n  want: [%s]\n  got:  [%s]\n",
                tag, want, got);
        return 0;
    }
    printf("PASS[%s] NAV ok (%d bytes)\n", tag, (int)strlen(got));
    return 1;
}

static void write_text(const char *path, const char *s) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(2); }
    fwrite(s, 1, strlen(s), f);
    fclose(f);
}

static const char *page_assign =
    "// LOAD: location.assign relative -> GO frame\n"
    "location.assign('c/next.html');\n";

static const char *page_href =
    "// LOAD: location.href = -> GO frame (root-relative)\n"
    "location.href = '/root.html';\n";

static const char *page_replace =
    "// LOAD: location.replace absolute -> REPLACE frame\n"
    "location.replace('https://other.test/x');\n";

static const char *page_reload =
    "// LOAD: location.reload -> RELOAD frame\n"
    "location.reload();\n";

static const char *page_state =
    "// LOAD: history.replaceState updates the address bar (ADDR), no fetch\n"
    "history.replaceState({x:1}, 'T', '/r?q=1');\n"
    "if (!history.state || history.state.x !== 1) throw new Error('replaceState state lost');\n";

static const char *page_push =
    "// LOAD: history.pushState -> ADDR frame, address-only update\n"
    "history.pushState({a:1}, 'T', 'next/x');\n"
    "if (!history.state || history.state.a !== 1) throw new Error('pushState state lost');\n";

static const char *page_back =
    "// LOAD: history.back -> BACK frame, 1 step\n"
    "history.back();\n";

static const char *page_goback =
    "// LOAD: history.go(-2) -> BACK frame, 2 steps\n"
    "history.go(-2);\n";

static const char *page_gofwd =
    "// LOAD: history.go(2) -> FORWARD frame, 2 steps\n"
    "history.go(2);\n";

static const char *page_none =
    "// LOAD: no navigation call -> no NAV frame at all\n"
    "console.log('SETUPOK');\n";

static const char *page_hrefcheck =
    "// LOAD: what the manager would do after a GO — re-LOAD with the\n"
    "// navigation target as the new href; the worker must present a fresh\n"
    "// location on this heap.\n"
    "console.log('HREF', location.href);\n"
    "if (location.href !== 'http://example.com/a/c/next.html')\n"
    "  throw new Error('href not updated after navigation LOAD: '+location.href);\n";

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <worker> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *tmpdir = argv[2];

    char dom_path[1024];
    char js[11][1024];
    snprintf(dom_path, sizeof(dom_path), "%s/fetch.dom", tmpdir);
    g_dom_path = dom_path;
    char *names[11];
    int n = 0;
#define P(field, tag) do { snprintf(field, sizeof(field), "%s/%s.js", tmpdir, tag); names[n++] = field; } while (0)
    P(js[0], "assign"); P(js[1], "href"); P(js[2], "replace"); P(js[3], "reload");
    P(js[4], "state"); P(js[5], "push"); P(js[6], "back"); P(js[7], "goback");
    P(js[8], "gofwd"); P(js[9], "none"); P(js[10], "hrefcheck");
#undef P
    (void)names;

    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    write_text(js[0], page_assign);
    write_text(js[1], page_href);
    write_text(js[2], page_replace);
    write_text(js[3], page_reload);
    write_text(js[4], page_state);
    write_text(js[5], page_push);
    write_text(js[6], page_back);
    write_text(js[7], page_goback);
    write_text(js[8], page_gofwd);
    write_text(js[9], page_none);
    write_text(js[10], page_hrefcheck);

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
    char nav[8192];

    pass &= nav_load(to_child[1], from_child[0], js[0], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("assign-relative", nav, "NAV\nGO\nhttp://example.com/a/c/next.html\n");

    pass &= nav_load(to_child[1], from_child[0], js[1], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("href-set", nav, "NAV\nGO\nhttp://example.com/root.html\n");

    pass &= nav_load(to_child[1], from_child[0], js[2], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("replace-absolute", nav, "NAV\nREPLACE\nhttps://other.test/x\n");

    pass &= nav_load(to_child[1], from_child[0], js[3], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("reload", nav, "NAV\nRELOAD\n\n");

    pass &= nav_load(to_child[1], from_child[0], js[4], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("replaceState-addr", nav, "NAV\nADDR\nhttp://example.com/r?q=1\n");

    pass &= nav_load(to_child[1], from_child[0], js[5], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("pushState-addr", nav, "NAV\nADDR\nhttp://example.com/a/next/x\n");

    pass &= nav_load(to_child[1], from_child[0], js[6], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("back", nav, "NAV\nBACK\n1\n");

    pass &= nav_load(to_child[1], from_child[0], js[7], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("go(-2)", nav, "NAV\nBACK\n2\n");

    pass &= nav_load(to_child[1], from_child[0], js[8], "http://example.com/a/b.html",
                     nav, sizeof(nav)) &&
            expect_nav("go(2)", nav, "NAV\nFORWARD\n2\n");

    if (nav_load(to_child[1], from_child[0], js[9], "http://example.com/a/b.html",
                 nav, sizeof(nav))) {
        if (nav[0]) {
            fprintf(stderr, "FAIL[none]: unexpected NAV frame: %s\n", nav);
            pass = 0;
        } else {
            printf("PASS[none] no NAV frame emitted\n");
        }
    } else pass = 0;

    pass &= nav_load(to_child[1], from_child[0], js[10], "http://example.com/a/c/next.html",
                     nav, sizeof(nav)) &&
            expect_nav("follow-through", nav, "");

    if (pass) printf("PASS: worker_nav_test -> NAV frames + follow-through ok\n");
    else printf("FAIL: worker_nav_test\n");

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d - see WERR| stderr above\n", WTERMSIG(st));
    return pass ? 0 : 1;
}