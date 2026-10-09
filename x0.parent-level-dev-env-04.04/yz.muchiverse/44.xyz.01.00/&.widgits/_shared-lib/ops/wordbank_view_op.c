/* wordbank_view_op - render an entity's word bank as an HTML table
 * (the "visual" viewer called for in ENTITY-WORD-BANK-DESIGN.md).
 *
 * Produces a self-contained HTML document with a <table> whose cells
 * ride the INLINE TABLE COLUMNS contract (cells= payloads, \x1F
 * delimited) in network_browser_manager.c - standard <table>/<tr>/
 * <th>/<td> markup, no custom C renderer needed.
 *
 * Text-included from &.widgits/_shared-lib/khtpm_wordbank.c.
 * Design: 08-roadmap/design-docs/ENTITY-WORD-BANK-DESIGN.md sec 7.
 *
 * Usage:
 *   wordbank_view_op.+x --render <entity_dir> [> out.html]
 *     Prints a full HTML document to stdout.
 *   wordbank_view_op.+x --selftest
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <glob.h>
#include "../khtpm_wordbank.c"

#define WB_LINE 4096

/* strip trailing \r\n (own helper; not in shared core) */
static void wb_strip_trailing_crnl(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

/* ---- HTML escaping ---- */
static void wb_html_escape(const char *in, char *out, size_t n) {
    const char *s;
    size_t o = 0;
    for (s = in; *s && o + 6 < n; s++) {
        switch (*s) {
            case '&':  memcpy(out + o, "&amp;",  5); o += 5; break;
            case '<':  memcpy(out + o, "&lt;",   4); o += 4; break;
            case '>':  memcpy(out + o, "&gt;",   4); o += 4; break;
            case '"':  memcpy(out + o, "&quot;", 6); o += 6; break;
            case '\'': memcpy(out + o, "&#39;",  5); o += 5; break;
            default:   out[o++] = (unsigned char)*s; break;
        }
    }
    out[o] = '\0';
}

/* weight -> css class: high / low / neutral */
static const char *wb_weight_class(double w) {
    if (w >= 0.95) return "wb-weight-high";
    if (w <= 0.05) return "wb-weight-low";
    return "wb-weight-mid";
}

/* ---- word row parsed from one words.txt line ---- */
typedef struct {
    char canon[256];
    char alias[256];
    char kind[32];
    char verb[192];
    double weight;
    char source[32];
} WbRow;

/* CANON=name:asa|ALIAS=asa|WEIGHT=0.5|SOURCE=seed -> fields */
static int wb_parse_word_line(const char *line, WbRow *r) {
    memset(r, 0, sizeof(*r));
    r->weight = 0.5;
    /* walk KEY=VALUE| pairs */
    char buf[WB_LINE], *seg, *save = NULL;
    snprintf(buf, sizeof(buf), "%s", line);
    seg = strtok_r(buf, "|", &save);
    while (seg) {
        char *eq = strchr(seg, '=');
        if (eq) {
            char *val = eq + 1;
            *eq = '\0';
            if      (!strcmp(seg, "CANON"))  snprintf(r->canon, sizeof(r->canon), "%s", val);
            else if (!strcmp(seg, "ALIAS"))  snprintf(r->alias, sizeof(r->alias), "%s", val);
            else if (!strcmp(seg, "WEIGHT")) r->weight = atof(val);
            else if (!strcmp(seg, "SOURCE")) snprintf(r->source, sizeof(r->source), "%s", val);
        }
        /* split CANON into KIND and VALUE (e.g. "name:asa" -> kind=name, verb=asa) */
        seg = strtok_r(NULL, "|", &save);
    }
    if (r->canon[0]) {
        char *colon = strchr(r->canon, ':');
        if (colon) {
            size_t kl = colon - r->canon;
            snprintf(r->kind, sizeof(r->kind), "%.*s", (int)kl, r->canon);
            snprintf(r->verb, sizeof(r->verb), "%s", colon + 1);
        } else {
            snprintf(r->kind, sizeof(r->kind), "word");
            snprintf(r->verb, sizeof(r->verb), "%s", r->canon);
        }
    }
    return r->canon[0] != '\0';
}

/* count rows in words.txt */
static int wb_count_rows(const char *words_path) {
    FILE *f = fopen(words_path, "r");
    if (!f) return 0;
    char line[WB_LINE];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        wb_strip_trailing_crnl(line);
        if (line[0] && line[0] != '#') n++;
    }
    fclose(f);
    return n;
}

/* ---- core render: build full HTML document into `out`, return bytes ---- */
static int wb_render_html(char *out, size_t n, const char *entity_dir) {
    char wbdir[WB_BUF], words[WB_BUF];
    if (!entity_dir || !*entity_dir) return 0;
    /* entity_dir is the entity root; wordbank lives under inventory/zz.wordbank/ */
    wb_join(wbdir, sizeof(wbdir), entity_dir, "inventory");
    wb_join(wbdir, sizeof(wbdir), wbdir, WB_WORDBANK_DIR);
    if (!wb_is_dir(wbdir)) {
        /* maybe user passed the wordbank dir directly */
        snprintf(wbdir, sizeof(wbdir), "%s", entity_dir);
        wb_join(wbdir, sizeof(wbdir), wbdir, WB_WORDBANK_DIR);
    }
    wb_join(words, sizeof(words), wbdir, "words.txt");
    int nrows = wb_count_rows(words);
    if (nrows == 0) return 0;

    out[0] = '\0';
    FILE *mem = fmemopen(out, n, "w");
    if (!mem) return 0;
    fputs("<!doctype html>\n<html><head><meta charset=\"utf-8\">\n"
          "<title>Word Bank</title>\n"
          "<style>\n"
          "body{font:400 11px/1.4 -apple-system,monospace;margin:8px}\n"
          "table{border-collapse:collapse}\n"
          "th,td{border:1px solid #aaa;padding:2px 6px;text-align:left;font-size:11px}\n"
          "th{background:#eee;font-weight:700}\n"
          ".wb-weight-high{background:#e6ffe6}\n"
          ".wb-weight-low{background:#ffe6e6}\n"
          ".wb-weight-mid{background:#fffff0}\n"
          ".wb-source-user{font-weight:700}\n"
          "</style>\n"
          "</head><body>\n", mem);

    char entity_name[256];
    snprintf(entity_name, sizeof(entity_name), "%s", wb_base(entity_dir));

    fprintf(mem, "<h1>%s - Word Bank</h1>\n", entity_name);
    fprintf(mem, "<table>\n<thead><tr>\n"
                 "<th>Canon</th><th>Alias</th><th>Weight</th><th>Source</th>\n"
                 "</tr></thead>\n<tbody>\n");

    FILE *f = fopen(words, "r");
    if (f) {
        char line[WB_LINE];
        WbRow r;
        while (fgets(line, sizeof(line), f)) {
            wb_strip_trailing_crnl(line);
            if (!line[0] || line[0] == '#') continue;
            if (!wb_parse_word_line(line, &r)) continue;
            char canon_esc[512], alias_esc[512], weight_esc[64];
            wb_html_escape(r.canon, canon_esc, sizeof(canon_esc));
            wb_html_escape(r.alias, alias_esc, sizeof(alias_esc));
            wb_html_escape(r.canon, weight_esc, sizeof(weight_esc));
            fprintf(mem, "<tr class=\"%s wb-source-%s\">\n"
                         "<td>%s</td><td>%s</td>"
                         "<td class=\"%s\">%.2f</td><td>%s</td>\n"
                         "</tr>\n",
                         wb_weight_class(r.weight),
                         r.source,
                         canon_esc, alias_esc,
                         wb_weight_class(r.weight), r.weight,
                         r.source);
        }
        fclose(f);
    }
    fputs("</tbody></table>\n</body></html>\n", mem);
    fclose(mem);
    return strlen(out);
}

static int selftest(void) {
    char tmpdir[] = "/tmp/wb_view_selftest_XXXXXX";
    int bad = 0;
    if (!mkdtemp(tmpdir)) { fprintf(stderr, "FAIL mkdtemp\n"); return 1; }
    char invdir[512], wbdir[512], words[512];
    snprintf(invdir, sizeof(invdir), "%s/inventory", tmpdir); mkdir(invdir, 0755);
    snprintf(wbdir, sizeof(wbdir), "%s/zz.wordbank", invdir); mkdir(wbdir, 0755);
    snprintf(words, sizeof(words), "%s/words.txt", wbdir);
    FILE *f = fopen(words, "w");
    fprintf(f, "CANON=name:asa|ALIAS=asa|WEIGHT=0.5|SOURCE=seed\n");
    fprintf(f, "CANON=action:Chat|ALIAS=Chat|WEIGHT=1.0|SOURCE=user\n");
    fprintf(f, "CANON=action:Cancel|ALIAS=Cancel|WEIGHT=0.0|SOURCE=user\n");
    fclose(f);
    char out[65536];
    int olen = wb_render_html(out, sizeof(out), tmpdir);
    if (olen <= 0) { fprintf(stderr, "FAIL render returned %d\n", olen); bad++; }
    char *p;
    p = strstr(out, "<table>");
    if (!p) { fprintf(stderr, "FAIL: no <table>\n"); bad++; }
    p = strstr(out, "wb-weight-high");
    if (!p) { fprintf(stderr, "FAIL: no wb-weight-high (Chat 1.0)\n"); bad++; }
    p = strstr(out, "wb-weight-low");
    if (!p) { fprintf(stderr, "FAIL: no wb-weight-low (Cancel 0.0)\n"); bad++; }
    p = strstr(out, "asa");
    if (!p) { fprintf(stderr, "FAIL: 'asa' not in output\n"); bad++; }
    /* count rows: header + 3 data = 4 TR */
    int trs = 0; const char *c = out;
    while ((c = strstr(c, "<tr"))) if (c[3] == ' ' || c[3] == '>') { trs++; c += 4; }
    if (trs != 4) { fprintf(stderr, "FAIL: %d <tr> (want 4)\n", trs); bad++; }
    snprintf(out, sizeof(out), "rm -rf '%s'", tmpdir);
    system(out);
    printf(bad ? "selftest FAILED (%d)\n" : "selftest ok (render table, classify 3 weights, 4 rows)\n", bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc > 1 && !strcmp(argv[1], "--render") && argc >= 3) {
        char out[65536];
        int n = wb_render_html(out, sizeof(out), argv[2]);
        if (n > 0) { fwrite(out, 1, n, stdout); fflush(stdout); return 0; }
        fprintf(stderr, "error: no wordbank or empty output for %s\n", argv[2]);
        return 1;
    }
    fprintf(stderr, "usage: wordbank_view_op.+x --render <entity_dir> [> out.html]\n"
                    "       wordbank_view_op.+x --selftest\n");
    return 2;
}
