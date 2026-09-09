/* worker_storage_test.c — rung-6 localStorage/sessionStorage test for the worker.
 *
 * Spawns the real worker over pipes and drives TWO LOADs in one process:
 *   1. page_set.js   href http://example.com/app/index.html
 *        localStorage.setItem theme=dark + acct=user@host + multiline
 *        (line1\nline2) — verify read-back in the SAME page, removeItem n,
 *        check length/key, and use sessionStorage within the page.
 *   2. page_get.js   href http://example.com/app/other.html   (fresh heap!)
 *        localStorage.getItem must return the persisted values (jar on disk);
 *        sessionStorage must be EMPTY because each LOAD is a fresh session.
 *
 * The localStorage jar path comes from $NB_LOCALSTORAGE_FILE (set here before
 * exec); sessionStorage never touches disk. The driver also inspects the jar
 * file's percent-encoded content (theme\tdark, acct\tuser%40host,
 * multiline\tline1%0Aline2) and asserts the removed key 'n' is absent.
 *
 * Usage: worker_storage_test <worker-binary> <tmpdir>
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
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\n%s\nStorage Test", js, g_dom_path, href);
    wsend(to, load);
    char reply[4096];
    for (;;) {
        if (!wreply(from, reply, sizeof(reply))) {
            fprintf(stderr, "FAIL[%s]: no reply from worker\n", tag);
            return 0;
        }
        if (strncmp(reply, "RENDER\n", 7) == 0) continue;
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
    "// LOAD 1: write localStorage + sessionStorage, read back in-page\n"
    "localStorage.setItem('theme','dark');\n"
    "localStorage.setItem('acct','user@host');\n"
    "localStorage.setItem('multiline','line1\\nline2');\n"
    "localStorage.setItem('n','1');\n"
    "if (localStorage.getItem('theme') !== 'dark') throw new Error('theme not read same-page');\n"
    "if (localStorage.getItem('acct') !== 'user@host') throw new Error('acct pct-roundtrip failed');\n"
    "if (localStorage.getItem('multiline') !== 'line1\\nline2') throw new Error('multiline roundtrip failed');\n"
    "localStorage.removeItem('n');\n"
    "if (localStorage.getItem('n') !== null) throw new Error('removeItem failed');\n"
    "if (localStorage.length !== 3) throw new Error('length expected 3, got ' + localStorage.length);\n"
    "var k = localStorage.key(0);\n"
    "if (typeof k !== 'string' || localStorage.getItem(k) === null) throw new Error('key(0) bad');\n"
    "sessionStorage.setItem('tmp','x');\n"
    "if (sessionStorage.getItem('tmp') !== 'x') throw new Error('session set/read same-page');\n"
    "sessionStorage.clear();\n"
    "sessionStorage.setItem('live','y');\n"
    "if (sessionStorage.getItem('live') !== 'y') throw new Error('session clear+set failed');\n";

static const char *page_get =
    "// LOAD 2: FRESH heap + FRESH session — localStorage must survive on disk\n"
    "if (localStorage.getItem('theme') !== 'dark') throw new Error('theme not persisted across LOAD');\n"
    "if (localStorage.getItem('acct') !== 'user@host') throw new Error('acct not persisted across LOAD');\n"
    "if (localStorage.getItem('multiline') !== 'line1\\nline2') throw new Error('multiline not persisted across LOAD');\n"
    "if (localStorage.getItem('n') !== null) throw new Error('removed key leaked across LOAD');\n"
    "if (sessionStorage.getItem('tmp') !== null) throw new Error('session tmp leaked across LOAD');\n"
    "if (sessionStorage.getItem('live') !== null) throw new Error('session live leaked across LOAD');\n"
    "if (sessionStorage.length !== 0) throw new Error('session length not reset');\n"
    "sessionStorage.setItem('fresh','1');\n"
    "if (sessionStorage.getItem('fresh') !== '1') throw new Error('session usable in new LOAD');\n";

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <worker> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *tmpdir = argv[2];

    char dom_path[1024], jar_path[1024], js_set[1024], js_get[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/fetch.dom", tmpdir);
    snprintf(jar_path, sizeof(jar_path), "%s/nb_localstorage.txt", tmpdir);
    snprintf(js_set, sizeof(js_set), "%s/page_set.js", tmpdir);
    snprintf(js_get, sizeof(js_get), "%s/page_get.js", tmpdir);
    g_dom_path = dom_path;

    /* hermetic jar: only this tmpdir's file is touched, never $HOME */
    if (setenv("NB_LOCALSTORAGE_FILE", jar_path, 1) != 0) { perror("setenv NB_LOCALSTORAGE_FILE"); return 1; }

    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open %s\n", dom_path); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    write_text(js_set, page_set);
    write_text(js_get, page_get);

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
    pass &= wrk_load(to_child[1], from_child[0], js_set, "http://example.com/app/index.html", "set");
    pass &= wrk_load(to_child[1], from_child[0], js_get, "http://example.com/app/other.html", "get");

    /* jar on disk must have the percent-encoded localStorage, minus 'n'. */
    FILE *jf = fopen(jar_path, "rb");
    if (!jf) { fprintf(stderr, "FAIL: jar %s missing after loads\n", jar_path); pass = 0; }
    else {
        char jb[8192];
        size_t n = fread(jb, 1, sizeof(jb) - 1, jf);
        fclose(jf);
        jb[n] = 0;
        printf("--- jar (%s) ---\n%s--- end jar ---\n", jar_path, jb);
        if (strstr(jb, "theme\tdark") == NULL)      { fprintf(stderr, "FAIL: jar missing theme\n"); pass = 0; }
        if (strstr(jb, "acct\tuser%40host") == NULL) { fprintf(stderr, "FAIL: jar acct not pct-encoded\n"); pass = 0; }
        if (strstr(jb, "multiline\tline1%0Aline2") == NULL) { fprintf(stderr, "FAIL: jar multiline not pct-encoded\n"); pass = 0; }
        if (strstr(jb, "\tn\t") != NULL || strstr(jb, "n\t1") != NULL) { fprintf(stderr, "FAIL: removed key n in jar\n"); pass = 0; }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 0);

    printf("%s\n", pass ? "PASS: worker_storage_test -> localStorage jar + session scoping ok"
                       : "FAIL: worker_storage_test");
    return pass ? 0 : 1;
}