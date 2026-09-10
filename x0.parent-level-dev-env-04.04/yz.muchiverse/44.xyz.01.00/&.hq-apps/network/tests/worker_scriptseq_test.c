/* worker_scriptseq_test.c — phase-2 document-order script runs for the
 * NB-JS worker.
 *
 * The manager writes one <script> (inline or src-fetched) per slice of
 * page.js, separated by a sentinel comment line (see SCRIPT_BOUNDARY in
 * worker.c / collect_scripts in the manager). The worker compiles and runs
 * each slice as its own program (browser classic-script parity): document
 * order, a syntax error in one slice does not stop the others, and top-level
 * `var` still lands on the shared global. This harness drives the real
 * worker binary over the plan §4 line-RPC with crafted page.js contents and
 * asserts the RENDER rows that follow.
 *
 * Usage: worker_scriptseq_test <worker-binary> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#include "../nb_dom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BOUNDARY "/*nbjs-script-boundary*/\n"

static const char *canned_html =
    "<html><body>"
    "<div id=\"out\">init</div>"
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

/* Run one LOAD against the worker with js_content as page.js. Collects the
 * RENDER rows; passes iff STATUS ok and render contains expect[]. */
static int run_case(const char *worker, const char *tmpdir,
                    const char *label, const char *js_content,
                    const char *expect) {
    char dom_path[1024], page_path[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/wps.fetch.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/wps.page.js", tmpdir);

    NbNode *tree = nb_parse_html(canned_html, strlen(canned_html));
    if (!tree) { fprintf(stderr, "FAIL: %s (nb_parse_html)\n", label); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: %s (open fetch.dom)\n", label); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    FILE *out = fopen(page_path, "wb");
    if (!out) { fprintf(stderr, "FAIL: %s (open page.js)\n", label); return 1; }
    fputs(js_content, out);
    fclose(out);

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
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://localhost/wp\nWPS",
             page_path, dom_path);
    wsend(to_child[1], load);
    char render[4096] = "";
    char reply[4096];
    char *status = NULL;
    for (;;) {
        if (!wreply(from_child[0], reply, sizeof(reply))) {
            fprintf(stderr, "FAIL: %s (no reply from worker)\n", label);
            int st; waitpid(pid, &st, 0);
            if (WIFSIGNALED(st))
                fprintf(stderr, "harness: worker killed by signal %d - see WERR| stderr above\n", WTERMSIG(st));
            return 1;
        }
        if (strncmp(reply, "RENDER\n", 7) == 0) {
            if (strlen(reply + 7) + 1 < sizeof(render))
                memcpy(render, reply + 7, strlen(reply + 7) + 1);
            continue;
        }
        status = reply;
        break;
    }
    int pass = (strncmp(status, "STATUS ok", 9) == 0) &&
               (expect[0] == '\0' || strstr(render, expect) != NULL);
    if (!pass) {
        printf("WORKER said: %s\nrender: %s\n", status, render);
    } else {
        printf("PASS: %s\n", label);
    }
    wsend(to_child[1], "QUIT");
    int st; waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d - see WERR| stderr above\n", WTERMSIG(st));
    return pass ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <worker> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *tmpdir = argv[2];

    int rc = 0;

    /* A: document order + isolation — script 2 is a syntax error; scripts
     * 1 and 3 still run and share the `var seq` global. */
    rc |= run_case(worker, tmpdir,
        "wps[order+isolation] seq=1,3 despite bad slice 2",
        BOUNDARY "var seq=[];\nseq.push(1);\n"
        BOUNDARY "bad-syntax-<<<\n"
        BOUNDARY "seq.push(3);\ndocument.getElementById(\"out\").textContent=\"seq=\"+seq.join(\",\");\n",
        "seq=1,3");

    /* B: top-level var crosses slices (browser classic-script parity). */
    rc |= run_case(worker, tmpdir,
        "wps[globals] var in slice 1 visible in slice 2",
        BOUNDARY "var gv=\"G\";\n"
        BOUNDARY "document.getElementById(\"out\").textContent=\"got:\"+gv;\n",
        "got:G");

    /* C: external src slice executes at its DOM position (middle). */
    rc |= run_case(worker, tmpdir,
        "wps[external-order] mid slice runs between inline slices",
        BOUNDARY "var s=[];s.push(\"a\");\n"
        BOUNDARY "s.push(\"b\");\n"
        BOUNDARY "s.push(\"c\");document.getElementById(\"out\").textContent=s.join(\",\");\n",
        "a,b,c");

    /* D: legacy single-program page.js (no sentinel) still runs. */
    rc |= run_case(worker, tmpdir,
        "wps[legacy] single-program page.js without sentinel",
        "document.getElementById(\"out\").textContent=\"legacy-ok\";\n",
        "legacy-ok");

    printf("%s\n", rc ? "FAIL: worker_scriptseq_test" : "PASS: worker_scriptseq_test");
    return rc ? 1 : 0;
}