#define _POSIX_C_SOURCE 200809L
/* nb_js_worker.c — resident JavaScript worker for network-browser-hq.
 * Owned by network_browser_manager (its direct child, spawned lazily on
 * first <script> presence), talking line-RPC over a socketpair dup2'd to
 * stdin/stdout. NB-JS worker plan §1/§2/§4.
 *
 * This is the step-2 SKELETON: it creates one Duktape heap, reuses the
 * shared rung-1/6 host (nb_host.h), reads LOAD-delivered files, runs the
 * page's JS, and reports STATUS ok|err. It writes nothing to the page yet
 * (the DOM-tree accessors + RENDER merge land in steps 3-4). It stays
 * resident across LOADs and never bare-spins (blocks on stdin read).
 *
 * RPC framing (plan §4): length-prefixed lines.
 *   send:  "%.6d\n" + payload + "\n"
 *   recv:  read a %06d length line, then that many payload bytes.
 *
 * manager -> worker: LOAD\n<path page.js>\n<path fetch.dom>\n<href>\n<title>
 *                    (this step uses only page.js; fetch.dom is read in
 *                     step 3 once the DOM tree lands in the worker)
 * worker  -> manager: STATUS ok
 * worker  -> manager: STATUS err:<message>
 * manager -> worker: QUIT            (shut down cleanly)
 */
#include "nb_host.h"

#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#define MAX_MSG (1024 * 1024)

/* ---- wire framing ---- */
static char g_rbuf[MAX_MSG];      /* command payload buffer */
static size_t g_rlen = 0;

static char g_page_js[4096];
static char g_fetch_dom[4096];
static char g_href[4096];
static char g_title[512];

static void send_payload(const char *payload, size_t n) {
    char lenbuf[16];
    int ln = snprintf(lenbuf, sizeof(lenbuf), "%.6d\n", (int)n);
    (void)!write(STDOUT_FILENO, lenbuf, (size_t)ln);
    if (n) (void)!write(STDOUT_FILENO, payload, n);
    (void)!write(STDOUT_FILENO, "\n", 1);
}

static void send_status(const char *status) {
    send_payload(status, strlen(status));
}

/* Read one length-prefixed payload from stdin into g_rbuf.
 * Returns 1 on success, 0 on EOF/shutdown. */
static int recv_frame(void) {
    char lenbuf[16];
    size_t i = 0;
    for (;;) {
        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r == 0) return 0;                 /* EOF */
        if (r < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (c == '\n') break;
        if (i < sizeof(lenbuf) - 1) lenbuf[i++] = c;
    }
    lenbuf[i] = 0;
    long n = strtol(lenbuf, NULL, 10);
    if (n < 0 || n > MAX_MSG) return 0;
    size_t got = 0;
    while (got < (size_t)n) {
        ssize_t r = read(STDIN_FILENO, g_rbuf + got, (size_t)n - got);
        if (r == 0) return 0;
        if (r < 0) { if (errno == EINTR) continue; return 0; }
        got += (size_t)r;
    }
    g_rbuf[got] = 0;
    g_rlen = got;
    /* consume the trailing '\n' after payload (if any) */
    {
        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r > 0 && c != '\n') {
            /* not a trailing newline: buffer it back conceptually by not
             * over-reading; we simply ignore stray bytes here */
            (void)c;
        }
    }
    return 1;
}

/* Split g_rbuf into the LOAD fields, copying each into a target buffer.
 * LOAD payload layout (newline-delimited, plan §4 + extra href/title):
 *   line 0 = "LOAD"
 *   line 1 = page.js path
 *   line 2 = fetch.dom path
 *   line 3 = href
 *   line 4 = title
 */
static void split_lines(char *fields[8]) {
    int fi = 0;
    char *p = g_rbuf;
    fields[0] = p;
    for (size_t i = 0; i < g_rlen; i++) {
        if (g_rbuf[i] == '\n') {
            g_rbuf[i] = 0;
            if (i + 1 < g_rlen && fi < 7) fields[++fi] = &g_rbuf[i + 1];
        }
    }
    for (int k = fi + 1; k < 8; k++) fields[k] = NULL;
}

/* Run the page script; sends STATUS ok|err across the wire. */
static void run_page(void) {
    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if (!ctx) { send_status("STATUS err:heap"); return; }
    install_host(ctx);

    /* rung-6 prelude: swallow its own failure, page continues */
    if (duk_peval_string(ctx, g_js_prelude) != 0) duk_pop(ctx);
    duk_pop(ctx);

    char *src = NULL;
    size_t src_n = 0;
    if (!read_file(g_page_js, &src, &src_n)) {
        duk_destroy_heap(ctx);
        free(src);
        send_status("STATUS err:cannot read page.js");
        return;
    }
    if (src_n == 0) { free(src); duk_destroy_heap(ctx); send_status("STATUS ok"); return; }

    duk_push_lstring(ctx, src, src_n);
    free(src);
    int rc = duk_peval(ctx);
    if (rc != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        char msg[1024];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "script error");
        duk_destroy_heap(ctx);
        send_status(msg);
        return;
    }
    duk_pop(ctx);
    duk_destroy_heap(ctx);
    send_status("STATUS ok");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    g_out = NULL;   /* step 2: no effects file yet; console goes nowhere */

    for (;;) {
        if (!recv_frame()) break;

        char *f[8];           split_lines(f);
        const char *cmd = f[0] ? f[0] : "";
        if (strcmp(cmd, "QUIT") == 0) {
            break;
        } else if (strcmp(cmd, "LOAD") == 0) {
            g_title[0] = 0; g_href[0] = 0;
            if (f[1]) snprintf(g_page_js, sizeof(g_page_js), "%s", f[1]);
            if (f[2]) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", f[2]);
            if (f[3]) snprintf(g_href, sizeof(g_href), "%s", f[3]);
            if (f[4]) snprintf(g_title, sizeof(g_title), "%s", f[4]);
            run_page();
        } else {
            send_status("STATUS err:unknown command");
        }
    }
    return 0;
}
