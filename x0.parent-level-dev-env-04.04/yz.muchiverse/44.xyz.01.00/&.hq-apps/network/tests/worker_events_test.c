/* worker_events_test.c — headless rung-3 events + timers test.
 *
 * Speaks the plan §4 line-RPC to the real worker binary, same framing as
 * worker_fetch_test.c: writes a canned DOM (log + btn elements) to
 * fetch.dom, copies worker_events_test.js verbatim as page.js, then LOADs
 * the page with a file:// href. Passes iff STATUS ok AND the RENDER frame's
 * TEXT row carries every expected event/timer/onprop token — proving the
 * lifecycle fire (DOMContentLoaded/load), the microtask + interval drain,
 * dispatchEvent bubbling, Event-constructor semantics, el.on* props and
 * el.click() — and carries no token proving a cleared/stopped path ran.
 *
 * Usage: worker_events_test <worker-binary> <worker_events_test.js> <tmpdir>
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
    "<div id=\"log\">init</div>"
    "<div id=\"btn\"></div>"
    "</body></html>";

static const char *markers[] = {
    "A", "B-dcl", "C-load", "D-winonload", "E-micro",
    "F-t0", "G-t10", "H-int1", "H-int2", "H-int3",
    "I-listener", "J-onclick", "K-bubble", "L-change",
    "M-onchange", "N-later", "O-stopped", NULL
};
static const char *forbidden[] = { "H-int4", NULL };

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
    const char *js_template = argv[2];
    const char *tmpdir = argv[3];

    char dom_path[1024], page_path[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/fetch.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/page.js", tmpdir);

    /* 1. build fetch.dom from the canned HTML */
    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: nb_parse_html\n"); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: open fetch.dom\n"); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    /* 2. js template -> page.js verbatim */
    {
        FILE *in = fopen(js_template, "rb");
        if (!in) { fprintf(stderr, "FAIL: open %s\n", js_template); return 1; }
        FILE *out = fopen(page_path, "wb");
        if (!out) { fprintf(stderr, "FAIL: open %s\n", page_path); return 1; }
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
        fclose(in); fclose(out);
    }

    /* 3. spawn worker over pipes */
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

    /* 4. LOAD then QUIT. Capture RENDER rows, then the STATUS frame. */
    char load[2048];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nfile://%s/a\nRung3 Test",
             page_path, dom_path, tmpdir);
    wsend(to_child[1], load);
    char reply[8192];
    char render[8192] = "";
    char *status = NULL;
    for (;;) {
        if (!wreply(from_child[0], reply, sizeof(reply))) {
            fprintf(stderr, "FAIL: no reply from worker\n");
            int st; waitpid(pid, &st, 0);
            if (WIFSIGNALED(st))
                fprintf(stderr, "harness: worker killed by signal %d - see WERR| stderr above\n", WTERMSIG(st));
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

    /* RENDER rows wrap long text lines across rows; markers are contiguous
     * payload text, so the check runs against a newline-stripped copy. */
    char render_flat[8192];
    {
        size_t j = 0;
        for (size_t i = 0; render[i] && j + 1 < sizeof(render_flat); i++)
            if (render[i] != '\n') render_flat[j++] = render[i];
        render_flat[j] = 0;
    }

    for (int i = 0; pass && markers[i]; i++) {
        if (!strstr(render_flat, markers[i])) {
            printf("FAIL: RENDER lacks marker '%s'\n---- RENDER rows ----\n%s\n",
                   markers[i], render);
            pass = 0;
        }
    }
    for (int i = 0; pass && forbidden[i]; i++) {
        if (strstr(render_flat, forbidden[i])) {
            printf("FAIL: RENDER ran forbidden path '%s'\n---- RENDER rows ----\n%s\n",
                   forbidden[i], render);
            pass = 0;
        }
    }

    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d - see WERR| stderr above\n", WTERMSIG(st));

    printf("%s (rung3 events+timers: all %d markers rendered)\n",
           pass ? "PASS: worker_events_test" : "FAIL: worker_events_test",
           (int)(sizeof(markers) / sizeof(markers[0])) - 1);
    return pass ? 0 : 1;
}