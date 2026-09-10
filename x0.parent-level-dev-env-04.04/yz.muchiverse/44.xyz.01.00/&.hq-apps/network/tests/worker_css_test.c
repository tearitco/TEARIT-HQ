/* worker_css_test.c — rung 7 slice 1: CSS-cascade subset for the NB-JS
 * worker.
 *
 * The manager ships the page's CSS (inline <style> + linked stylesheets)
 * as a 5th LOAD line; the worker parses it (nb_css) and serves
 * getComputedStyle + the metrics family (offsetWidth/Height, clientWidth/
 * Height, getBoundingClientRect, offsetParent) with display:none /
 * visibility:hidden semantics. This harness drives the real worker binary
 * over the plan §4 line-RPC with canned HTML + CSS + a page.js whose
 * assertions throw on any failure (the worker reports STATUS err:...).
 *
 * Usage: worker_css_test <worker-binary> <tmpdir>
 * Exit 0 on pass, 1 on any failure.
 */
#include "../nb_dom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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

/* Run one LOAD+style file against the worker. Passes iff STATUS ok and
 * the RENDER rows contain expect (the page writes its verdict to #out). */
static int run_case(const char *worker, const char *tmpdir,
                    const char *label, const char *html,
                    const char *css, const char *js_content,
                    const char *expect) {
    char dom_path[1024], page_path[1024], style_path[1024];
    snprintf(dom_path, sizeof(dom_path), "%s/wcs.fetch.dom", tmpdir);
    snprintf(page_path, sizeof(page_path), "%s/wcs.page.js", tmpdir);
    snprintf(style_path, sizeof(style_path), "%s/wcs.style.css", tmpdir);

    NbNode *tree = nb_parse_html(html, strlen(html));
    if (!tree) { fprintf(stderr, "FAIL: %s (nb_parse_html)\n", label); return 1; }
    FILE *df = fopen(dom_path, "wb");
    if (!df) { fprintf(stderr, "FAIL: %s (open fetch.dom)\n", label); return 1; }
    nb_serialize(df, tree);
    fclose(df);
    nb_node_free(tree);

    FILE *cf = fopen(style_path, "wb");
    if (!cf) { fprintf(stderr, "FAIL: %s (open style.css)\n", label); return 1; }
    if (css) fputs(css, cf);
    fclose(cf);

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

    char load[3072];
    snprintf(load, sizeof(load), "LOAD\n%s\n%s\nhttp://localhost/css\nWCS\n%s",
             page_path, dom_path, style_path);
    wsend(to_child[1], load);
    char render[4096] = "";
    char reply[4096];
    char *status = NULL;
    for (;;) {
        if (!wreply(from_child[0], reply, sizeof(reply))) {
            fprintf(stderr, "FAIL: %s (no reply from worker)\n", label);
            int st; waitpid(pid, &st, 0);
            if (WIFSIGNALED(st))
                fprintf(stderr, "harness: worker killed by signal %d\n", WTERMSIG(st));
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
    if (WIFSIGNALED(st)) fprintf(stderr, "harness: worker killed by signal %d\n", WTERMSIG(st));
    return pass ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <worker> <tmpdir>\n", argv[0]); return 2; }
    const char *worker = argv[1];
    const char *tmpdir = argv[2];
    int rc = 0;

    /* A: display:none class hides (metrics 0, offsetParent null); visible
     * sibling still has an offsetParent. */
    rc |= run_case(worker, tmpdir,
        "wcs[display-none] hidden element metrics + offsetParent",
        "<html><body><div id=\"out\">o</div><div id=\"gone\" class=\"hidden\">x</div>"
        "<div id=\"sib\">y</div></body></html>",
        ".hidden{display:none}\n",
        "var O=document.getElementById(\"out\");\n"
        "var g=document.getElementById(\"gone\");\n"
        "var cs=getComputedStyle(g);\n"
        "if(cs.display!==\"none\")throw \"A1 display=\"+cs.display;\n"
        "if(g.offsetWidth!==0||g.offsetHeight!==0)throw \"A2 offsets\";\n"
        "if(g.offsetParent!==null)throw \"A3 offsetParent\";\n"
        "var s=document.getElementById(\"sib\");\n"
        "if(s.offsetParent===null)throw \"A4 sib\";\n"
        "if(s.offsetWidth!==0)throw \"A5 sib width\";\n"
        "O.textContent=\"A-ok\";\n",
        "A-ok");

    /* B: px width/height flow into offset + client metrics and the rect. */
    rc |= run_case(worker, tmpdir,
        "wcs[px-size] declared sizes reach rect/offset metrics",
        "<html><body><div id=\"out\">o</div><div id=\"box\">b</div></body></html>",
        "#box{width:120px;height:90px}\n",
        "var O=document.getElementById(\"out\");\n"
        "var b=document.getElementById(\"box\");\n"
        "if(b.offsetWidth!==120)throw \"B1 ow=\"+b.offsetWidth;\n"
        "if(b.offsetHeight!==90)throw \"B2 oh=\"+b.offsetHeight;\n"
        "if(b.clientWidth!==120)throw \"B3 cw\";\n"
        "if(b.clientHeight!==90)throw \"B4 ch\";\n"
        "var r=b.getBoundingClientRect();\n"
        "if(!r||r.width!==120||r.height!==90)throw \"B5 rect\";\n"
        "if(r.x!==0||r.y!==0||r.top!==0||r.left!==0)throw \"B6 rect0\";\n"
        "if(r.right!==120||r.bottom!==90)throw \"B7 rect2\";\n"
        "var cs=getComputedStyle(b);\n"
        "if(cs.getPropertyValue(\"width\")!==\"120\")throw \"B8 gpw\";\n"
        "O.textContent=\"B-ok\";\n",
        "B-ok");

    /* C: inline style overrides the stylesheet (display:block un-hides). */
    rc |= run_case(worker, tmpdir,
        "wcs[inline] style attr beats stylesheet, block un-hides",
        "<html><body><div id=\"out\">o</div><div id=\"ib\" class=\"box\" style=\"height:200px;display:block\">x</div></body></html>",
        ".box{width:100px;display:none}\n",
        "var O=document.getElementById(\"out\");\n"
        "var ib=document.getElementById(\"ib\");\n"
        "var cs=getComputedStyle(ib);\n"
        "if(cs.display!==\"\")throw \"C1 disp=\"+cs.display;\n"
        "if(ib.offsetWidth!==100)throw \"C2 ow=\"+ib.offsetWidth;\n"
        "if(ib.offsetHeight!==200)throw \"C3 oh=\"+ib.offsetHeight;\n"
        "if(ib.style.height!==\"200px\")throw \"C4 style\";\n"
        "O.textContent=\"C-ok\";\n",
        "C-ok");

    /* D: visibility:hidden on a type selector hides + zeroes the box. */
    rc |= run_case(worker, tmpdir,
        "wcs[visibility] p{visibility:hidden} reports hidden",
        "<html><body><div id=\"out\">o</div><p id=\"pv\">x</p></body></html>",
        "p{visibility:hidden}\n",
        "var O=document.getElementById(\"out\");\n"
        "var pv=document.getElementById(\"pv\");\n"
        "var cs=getComputedStyle(pv);\n"
        "if(cs.visibility!==\"hidden\")throw \"D1 vis=\"+cs.visibility;\n"
        "if(pv.offsetWidth!==0)throw \"D2 ow\";\n"
        "var r=pv.getBoundingClientRect();\n"
        "if(r.width!==0||r.height!==0)throw \"D3 rect\";\n"
        "O.textContent=\"D-ok\";\n",
        "D-ok");

    /* E: specificity — #main .item beats div.item beats .item. */
    rc |= run_case(worker, tmpdir,
        "wcs[specificity] id+class beats tag+class beats class",
        "<html><body><div id=\"out\">o</div><div id=\"main\"><span id=\"it\" class=\"item\">z</span></div></body></html>",
        "div.item{width:60px}\n#main .item{width:50px}\n.item{width:30px}\n",
        "var O=document.getElementById(\"out\");\n"
        "var it=document.getElementById(\"it\");\n"
        "if(it.offsetWidth!==50)throw \"E1 ow=\"+it.offsetWidth;\n"
        "O.textContent=\"E-ok\";\n",
        "E-ok");

    /* F: descendant combinator + comma list. */
    rc |= run_case(worker, tmpdir,
        "wcs[descendant] descendant selector + comma list",
        "<html><body><div id=\"out\">o</div><div id=\"main\"><div class=\"child\"><span id=\"sp\" class=\"deep\">s</span></div></div></body></html>",
        "#main .deep,#other{width:40px}\n",
        "var O=document.getElementById(\"out\");\n"
        "var sp=document.getElementById(\"sp\");\n"
        "if(sp.offsetWidth!==40)throw \"F1 ow=\"+sp.offsetWidth;\n"
        "O.textContent=\"F-ok\";\n",
        "F-ok");

    /* G: hidden ancestor hides descendants too. */
    rc |= run_case(worker, tmpdir,
        "wcs[ancestor-hidden] hidden parent hides child",
        "<html><body><div id=\"out\">o</div><div id=\"ph\" class=\"hidden\"><span id=\"kid\">k</span></div></body></html>",
        ".hidden{display:none}\n",
        "var O=document.getElementById(\"out\");\n"
        "var kid=document.getElementById(\"kid\");\n"
        "if(kid.offsetWidth!==0)throw \"G1 ow\";\n"
        "if(kid.getBoundingClientRect().width!==0)throw \"G2 rect\";\n"
        "if(kid.offsetParent!==null)throw \"G3 op\";\n"
        "O.textContent=\"G-ok\";\n",
        "G-ok");

    /* H: empty stylesheet — sane defaults, no crash. */
    rc |= run_case(worker, tmpdir,
        "wcs[empty-css] no rules: defaults visible",
        "<html><body><div id=\"out\">v</div></body></html>",
        "",
        "var O=document.getElementById(\"out\");\n"
        "var cs=getComputedStyle(O);\n"
        "if(cs.display!==\"\")throw \"H1 disp=\"+cs.display;\n"
        "if(cs.opacity!==\"1\")throw \"H2 op=\"+cs.opacity;\n"
        "if(O.offsetWidth!==0)throw \"H3 ow\";\n"
        "if(typeof O.getBoundingClientRect!==\"function\")throw \"H4 rectfn\";\n"
        "O.textContent=\"H-ok\";\n",
        "H-ok");

    /* I: comments + @media blocks skipped, later rule wins the tie. */
    rc |= run_case(worker, tmpdir,
        "wcs[parse] comments/@media skipped; source order tie-break",
        "<html><body><div id=\"out\">o</div><div id=\"tb\">t</div></body></html>",
        "/* bang {broken brace} */\n"
        "@media (max-width: 10000px) { #tb { width: 9999px; } }\n"
        "@import url(x.css);\n"
        "#tb{width:80px}\n#tb{width:70px}\n",
        "var O=document.getElementById(\"out\");\n"
        "var tb=document.getElementById(\"tb\");\n"
        "if(tb.offsetWidth!==70)throw \"I1 ow=\"+tb.offsetWidth;\n"
        "O.textContent=\"I-ok\";\n",
        "I-ok");

    printf("%s\n", rc ? "FAIL: worker_css_test" : "PASS: worker_css_test");
    return rc ? 1 : 0;
}