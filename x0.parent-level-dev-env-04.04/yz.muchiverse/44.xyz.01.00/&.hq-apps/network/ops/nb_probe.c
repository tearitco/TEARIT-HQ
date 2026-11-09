/* nb_probe - drive ONE page through the network-browser JS worker with no window and no GUI.
 * C port of tests/nb_probe.py (2026-11-08; the owner's rule is "no Python": a harness is C ops + pal + case files, AGENTS.md).
 *
 * WHY THIS EXISTS: every other live browser test needs a live renderer in a live taskbar, so a question about the PAGE could not be asked without a
 * window, and the answer then depended on window geometry the question did not care about. This asks page questions hermetically, over the SAME framed
 * RPC the manager uses: each frame is six digits, a newline, the payload, a newline (the worker's recv_frame() is length-prefixed; a bare line desyncs it).
 *
 *   nb_probe --worker <nb_js_worker.+x> --page page.js --dom fetch.dom --probe probe.js --console console.txt
 *            [--href URL] [--title T] [--css FILE] [--width N] [--height N] [--budget SECONDS]
 *            [--load-timeout S] [--eval-timeout S]
 * Prints one JSON object on stdout: {status, eval_status, frames[], console[], fetches[], stderr, errors[]}. A non-empty "errors" means the run is NOT evidence.
 * Exit 0 when the worker answered STATUS for the probe's EVAL, 1 otherwise (same contract as the Python it replaces).
 *
 * The one seam it does not implement is the FETCH/FETCHED RPC. It answers each FETCH with a MISMATCHED id; nb_js_worker.c treats that as "the manager
 * could not do it" and falls back to its own direct-curl path (curl does TLS, this harness does not), so the worker's own network layer is what is under test.
 *
 * Footprint: one child process, poll() with the remaining deadline (no busy loop); the worker's stderr goes to a temp file (an undrained pipe could stall
 * the worker), the last 8000 bytes of it are reported and the file is removed. Build: see build.sh (ops/+x/nb_probe.+x). */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_FRAME (1 << 21)

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

/* ---- a growable list of strings (frames, console lines, fetched URLs, errors) ---- */
typedef struct { char **v; int n, cap; } List;
static void list_add(List *l, const char *s) {
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 16; l->v = realloc(l->v, (size_t)l->cap * sizeof(char *)); if (!l->v) exit(2); }
    l->v[l->n++] = strdup(s ? s : "");
}

/* ---- JSON string output (control characters and quotes escaped; invalid UTF-8 bytes become U+FFFD like Python's errors=replace) ---- */
static void json_str(FILE *o, const char *s) {
    fputc('"', o);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', o); fputc(*p, o); }
        else if (*p == '\n') fputs("\\n", o);
        else if (*p == '\r') fputs("\\r", o);
        else if (*p == '\t') fputs("\\t", o);
        else if (*p < 0x20) fprintf(o, "\\u%04x", *p);
        else if (*p < 0x80) fputc(*p, o);
        else {   /* validate a UTF-8 sequence; anything malformed -> U+FFFD */
            int len = (*p >= 0xF0 && *p < 0xF8) ? 4 : (*p >= 0xE0) ? 3 : (*p >= 0xC2 && *p < 0xE0) ? 2 : 0, ok = len > 0;
            for (int i = 1; ok && i < len; i++) if ((p[i] & 0xC0) != 0x80) ok = 0;
            if (ok) { fwrite(p, 1, (size_t)len, o); p += len - 1; } else fputs("\xEF\xBF\xBD", o);
        }
    }
    fputc('"', o);
}
static void json_list(FILE *o, const List *l) {
    fputs("[", o);
    for (int i = 0; i < l->n; i++) { if (i) fputs(", ", o); json_str(o, l->v[i]); }
    fputs("]", o);
}

/* ---- framed I/O ---- */
static int wsend(int fd, const char *payload) {
    size_t n = strlen(payload); char hdr[16]; int hl = snprintf(hdr, sizeof hdr, "%06zu\n", n);
    struct { const char *p; size_t n; } parts[3] = { { hdr, (size_t)hl }, { payload, n }, { "\n", 1 } };
    for (int i = 0; i < 3; i++) for (size_t off = 0; off < parts[i].n; ) {
        ssize_t w = write(fd, parts[i].p + off, parts[i].n - off);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)w;
    }
    return 0;
}

static char rbuf[MAX_FRAME + 64]; static size_t rlen = 0, rpos = 0;   /* one read buffer for the worker's stdout */

/* wait until a byte is readable or the deadline passes; returns 1 readable, 0 timeout, -1 EOF/error */
static int fill(int fd, double deadline) {
    if (rpos < rlen) return 1;
    rpos = rlen = 0;
    for (;;) {
        double left = deadline - now_s(); if (left <= 0) return 0;
        struct pollfd p = { fd, POLLIN, 0 }; int r = poll(&p, 1, (int)(left * 1000) + 1);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return 0;
        ssize_t n = read(fd, rbuf, sizeof rbuf);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) return -1;
        rlen = (size_t)n; return 1;
    }
}

/* one frame: six digits, newline, payload, optional newline. Returns a malloc'ed payload, or NULL on EOF, timeout or a bad header. */
static char *read_frame(int fd, double timeout) {
    double deadline = now_s() + timeout; char hdr[32]; size_t hn = 0;
    for (;;) {   /* header line, blank lines skipped */
        if (fill(fd, deadline) <= 0) return NULL;
        char c = rbuf[rpos++];
        if (c == '\n') { if (hn) break; continue; }
        if (c == '\r' || c == ' ') continue;
        if (hn + 1 < sizeof hdr) hdr[hn++] = c; else return NULL;
    }
    hdr[hn] = 0; char *end; long n = strtol(hdr, &end, 10);
    if (*end || n < 0 || n > MAX_FRAME) return NULL;
    char *buf = malloc((size_t)n + 1); if (!buf) return NULL; size_t got = 0;
    while (got < (size_t)n) {
        if (fill(fd, deadline) <= 0) { free(buf); return NULL; }
        size_t take = rlen - rpos; if (take > (size_t)n - got) take = (size_t)n - got;
        memcpy(buf + got, rbuf + rpos, take); rpos += take; got += take;
    }
    buf[n] = 0;
    if (rpos < rlen && rbuf[rpos] == '\n') rpos++;   /* the optional trailing newline, when it is already here */
    else if (rpos >= rlen) { struct pollfd p = { fd, POLLIN, 0 }; if (poll(&p, 1, 0) > 0) { ssize_t r = read(fd, rbuf, 1); if (r == 1 && rbuf[0] != '\n') { rlen = 1; rpos = 0; } } }
    return buf;
}

/* FETCH\n<id>\n<?>\n<url>...: answer with a MISMATCHED id so the worker falls back to its own curl path */
static void serve_fetch(int fd, const char *frame, List *fetch_log) {
    long rid = -1; const char *l1 = strchr(frame, '\n'); const char *url = "?"; char urlbuf[4096];
    if (l1) { rid = strtol(l1 + 1, NULL, 10); const char *l2 = strchr(l1 + 1, '\n'); const char *l3 = l2 ? strchr(l2 + 1, '\n') : NULL;
        if (l3) { size_t k = strcspn(l3 + 1, "\n"); if (k >= sizeof urlbuf) k = sizeof urlbuf - 1; memcpy(urlbuf, l3 + 1, k); urlbuf[k] = 0; url = urlbuf; } }
    list_add(fetch_log, url);
    char out[96]; snprintf(out, sizeof out, "FETCHED\n%ld\n0\n0\n\n", rid + 1000000); wsend(fd, out);
}

/* read frames until STATUS (or timeout/EOF), servicing FETCH; returns the STATUS frame (malloc) or NULL */
static char *pump(int wfd, int rfd, List *fetch_log, double timeout, List *frames) {
    double deadline = now_s() + timeout;
    while (now_s() < deadline) {
        double left = deadline - now_s(); char *fr = read_frame(rfd, left < 0.1 ? 0.1 : left);
        if (!fr) return NULL;
        if (!strncmp(fr, "FETCH\n", 6)) { serve_fetch(wfd, fr, fetch_log); free(fr); continue; }
        if (!strncmp(fr, "LIVE|", 5)) { free(fr); continue; }
        list_add(frames, fr);
        if (!strncmp(fr, "STATUS", 6)) return fr;
        free(fr);
    }
    return NULL;
}

static char *slurp(const char *path, size_t cap, int tail) {   /* whole file (or its last cap bytes) as a malloc'ed string, NULL if unreadable */
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); long start = 0; if (tail && sz > (long)cap) start = sz - (long)cap; else if (!tail && sz > (long)cap) sz = (long)cap;
    fseek(f, start, SEEK_SET); size_t n = (size_t)(sz - start); char *b = malloc(n + 1); if (!b) { fclose(f); return NULL; }
    n = fread(b, 1, n, f); b[n] = 0; fclose(f); return b;
}

int main(int argc, char **argv) {
    const char *worker = NULL, *page = NULL, *dom = NULL, *href = "https://example.com/", *title = "probe", *css = "", *probe = NULL, *console = NULL;
    int width = 800, height = 600, budget = 120; double load_to = 180.0, eval_to = 120.0;
    for (int i = 1; i + 1 < argc; i += 2) {
        const char *k = argv[i], *v = argv[i + 1];
        if (!strcmp(k, "--worker")) worker = v; else if (!strcmp(k, "--page")) page = v; else if (!strcmp(k, "--dom")) dom = v;
        else if (!strcmp(k, "--href")) href = v; else if (!strcmp(k, "--title")) title = v; else if (!strcmp(k, "--css")) css = v;
        else if (!strcmp(k, "--width")) width = atoi(v); else if (!strcmp(k, "--height")) height = atoi(v); else if (!strcmp(k, "--probe")) probe = v;
        else if (!strcmp(k, "--budget")) budget = atoi(v); else if (!strcmp(k, "--load-timeout")) load_to = atof(v); else if (!strcmp(k, "--eval-timeout")) eval_to = atof(v);
        else if (!strcmp(k, "--console")) console = v; else { fprintf(stderr, "nb_probe: unknown option %s\n", k); return 2; }
    }
    if (!worker || !page || !dom || !probe || !console) { fprintf(stderr, "usage: nb_probe --worker W --page P --dom D --probe J --console C [--href U] [--title T] [--css F] [--width N] [--height N] [--budget S] [--load-timeout S] [--eval-timeout S]\n"); return 2; }
    signal(SIGPIPE, SIG_IGN);

    List errors = {0}, frames = {0}, fetches = {0}; char *st = NULL, *st2 = NULL;
    unlink(console);
    char errpath[] = "/tmp/nb_probe_err.XXXXXX"; int efd = mkstemp(errpath); if (efd < 0) { perror("mkstemp"); return 2; }
    int to_w[2], from_w[2]; if (pipe(to_w) || pipe(from_w)) { perror("pipe"); return 2; }
    pid_t pid = fork(); if (pid < 0) { perror("fork"); return 2; }
    if (pid == 0) {
        dup2(to_w[0], 0); dup2(from_w[1], 1); dup2(efd, 2); close(to_w[0]); close(to_w[1]); close(from_w[0]); close(from_w[1]); close(efd);
        char b[32]; snprintf(b, sizeof b, "%d", budget); setenv("NBW_CONSOLE", console, 1); setenv("NB_EVAL_BUDGET", b, 1); setenv("NB_STACK", "1", 1); unsetenv("NB_CRASH_BT");
        execl(worker, worker, (char *)NULL); fprintf(stderr, "nb_probe: cannot exec %s: %s\n", worker, strerror(errno)); _exit(127);
    }
    close(to_w[0]); close(from_w[1]); close(efd); int wfd = to_w[1], rfd = from_w[0];

    char payload[16384]; snprintf(payload, sizeof payload, "LOAD\n%s\n%s\n%s\n%s\n%s\n%d\n%d", page, dom, href, title, css, width, height);
    if (wsend(wfd, payload) < 0) list_add(&errors, "worker died before LOAD could be written");
    if (!errors.n) { st = pump(wfd, rfd, &fetches, load_to, &frames); if (!st) { char m[96]; snprintf(m, sizeof m, "no STATUS from LOAD within %.0fs", load_to); list_add(&errors, m); } }

    char *pr = slurp(probe, 1 << 20, 0); if (!pr) { list_add(&errors, "cannot read the --probe file"); pr = strdup(""); }
    for (char *p = pr; *p; p++) if (*p == '\n') *p = ' ';   /* the worker's EVAL is one line */
    char *ev = malloc(strlen(pr) + 8); sprintf(ev, "EVAL\n%s", pr);
    /* A worker that died during LOAD leaves us writing into a closed pipe: that is a RESULT (the worker crashed) and is reported as one, not as a harness crash. */
    if (wsend(wfd, ev) < 0) list_add(&errors, "worker is gone, EVAL could not be written");
    if (!errors.n) { st2 = pump(wfd, rfd, &fetches, eval_to, &frames); if (!st2) { char m[96]; snprintf(m, sizeof m, "no STATUS from EVAL within %.0fs", eval_to); list_add(&errors, m); } }
    free(ev); free(pr);

    wsend(wfd, "QUIT"); close(wfd); usleep(300000);
    int status; if (waitpid(pid, &status, WNOHANG) == 0) {
        kill(pid, SIGTERM); double dl = now_s() + 5.0; while (waitpid(pid, &status, WNOHANG) == 0 && now_s() < dl) usleep(50000);
        if (waitpid(pid, &status, WNOHANG) == 0) { kill(pid, SIGKILL); waitpid(pid, &status, 0); }
    }
    close(rfd);

    List cons = {0}; char *ct = slurp(console, 16 << 20, 0);
    if (ct) { for (char *p = ct, *q; *p; p = q) { q = strchr(p, '\n'); if (q) *q++ = 0; else q = p + strlen(p); list_add(&cons, p); } free(ct); }
    char *errtxt = slurp(errpath, 8000, 1); unlink(errpath);

    FILE *o = stdout; fputs("{\n  \"status\": ", o); if (st) json_str(o, st); else fputs("null", o);
    fputs(",\n  \"eval_status\": ", o); if (st2) json_str(o, st2); else fputs("null", o);
    fputs(",\n  \"frames\": ", o); json_list(o, &frames); fputs(",\n  \"console\": ", o); json_list(o, &cons); fputs(",\n  \"fetches\": ", o); json_list(o, &fetches);
    fputs(",\n  \"stderr\": ", o); json_str(o, errtxt ? errtxt : ""); fputs(",\n  \"errors\": ", o); json_list(o, &errors); fputs("\n}\n", o); fflush(o);
    return st2 ? 0 : 1;
}
