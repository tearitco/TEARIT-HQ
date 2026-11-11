/* map_json_op — turn one page map.txt into a JSON copy and a page that reads it.
 * Usage: map_json_op <map.txt> <out.json>
 * Writes three files next to each other:
 *   <out.json>   the map, its size, and any sibling notes it found
 *   <out.js>     the same JSON as window.PAGE_MAP, so a folder open can read it
 *   <out.html>   opens that .js and shows the max size, counts, and a picture
 * A line in map.txt is one row. A character is one cell. Space is empty.
 * When the map's folder also has them, these are copied into the JSON as notes:
 *   land.seed.pdl, extrusion.pdl, view.pdl, and the header of cells.txt (stops at the cells row).
 * Exit 0 written, 2 usage or a map that will not fit (over 8192 on a side, or unreadable).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/map_json_op.+x map_json_op.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_SIDE 8192

static void json_str(FILE *o, const char *s) {
    fputc('"', o);
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') fprintf(o, "\\%c", c);
        else if (c == '\n') fputs("\\n", o);
        else if (c < 32) fprintf(o, "\\u%04x", c);
        else fputc(c, o);
    }
    fputc('"', o);
}

static char *base_of(const char *path) {
    const char *s = strrchr(path, '/');
    return (char *)(s ? s + 1 : path);
}

static void swap_ext(const char *json, const char *ext, char *out, size_t n) {
    snprintf(out, n, "%s", json);
    char *dot = strrchr(out, '.');
    char *sl = strrchr(out, '/');
    if (!dot || (sl && dot < sl)) dot = out + strlen(out);
    snprintf(dot, n - (size_t)(dot - out), "%s", ext);
}

static int read_sidecar(const char *dir, const char *name, char *buf, size_t n, int cells_header) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = 0; return 0; }
    size_t used = 0;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (cells_header && !strncmp(line, "cells", 5)) break;
        size_t L = strlen(line);
        if (used + L + 1 >= n) break;
        memcpy(buf + used, line, L);
        used += L;
    }
    buf[used] = 0;
    fclose(f);
    return used > 0;
}

static void write_html(FILE *h, const char *js_name) {
    fprintf(h,
        "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
        "<title>Page map</title>\n<style>\n"
        "body{font:16px/1.45 sans-serif;background:#14171c;color:#e8e4dc;margin:24px;}\n"
        "h1{font-size:22px;margin:0 0 8px;}\n"
        ".max{font-size:28px;font-weight:700;margin:8px 0 16px;}\n"
        "table{border-collapse:collapse;margin:12px 0 20px;}\n"
        "td,th{border:1px solid #3a404a;padding:4px 10px;text-align:left;}\n"
        "pre{white-space:pre-wrap;background:#0e1116;padding:12px;max-width:52em;}\n"
        "canvas{background:#0e1116;image-rendering:pixelated;max-width:100%%;height:auto;border:1px solid #3a404a;}\n"
        ".err{color:#e07070;}\n</style></head><body>\n"
        "<h1>Page map</h1>\n<div id=\"out\"><p>Reading the map…</p></div>\n"
        "<script src=\"%s\"></script>\n<script>\n"
        "const out = document.getElementById('out');\n"
        "const m = window.PAGE_MAP;\n"
        "if (!m) { out.innerHTML = '<p class=\"err\">This page could not read the map file next to it.</p>'; }\n"
        "else {\n"
        "  const g = m.glyphs || {};\n"
        "  let rows = '';\n"
        "  Object.keys(g).sort().forEach(k => { rows += '<tr><td>' + (k === ' ' ? '(empty)' : k) + '</td><td>' + g[k] + '</td></tr>'; });\n"
        "  const b = m.bounds;\n"
        "  out.innerHTML = '<p>' + (m.source || '') + '</p>' +\n"
        "    '<div class=\"max\">' + m.width + ' × ' + m.height + '</div>' +\n"
        "    '<p>Largest side ' + m.max_side + '. File is ' + m.bytes + ' bytes. ' +\n"
        "    m.land + ' filled cells, ' + m.empty + ' empty.' +\n"
        "    (b ? ' Land runs from ' + b.x0 + ',' + b.y0 + ' to ' + b.x1 + ',' + b.y1 + '.' : ' No filled cells.') +\n"
        "    (m.ragged ? ' Rows are not all the same length.' : '') + '</p>' +\n"
        "    '<table><tr><th>Mark</th><th>Cells</th></tr>' + rows + '</table>' +\n"
        "    (m.notes ? '<pre>' + m.notes.replace(/[&<>]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[c])) + '</pre>' : '') +\n"
        "    '<canvas id=\"c\" width=\"' + m.width + '\" height=\"' + m.height + '\"></canvas>';\n"
        "  const cv = document.getElementById('c');\n"
        "  const ctx = cv.getContext('2d');\n"
        "  const img = ctx.createImageData(m.width, m.height);\n"
        "  const pal = { ' ': [14,17,22], a: [106,143,78], b: [138,115,68], c: [216,230,238] };\n"
        "  (m.rows || []).forEach((row, y) => {\n"
        "    for (let x = 0; x < m.width; x++) {\n"
        "      const ch = x < row.length ? row[x] : ' ';\n"
        "      const p = pal[ch] || [224,112,64];\n"
        "      const i = (y * m.width + x) * 4;\n"
        "      img.data[i] = p[0]; img.data[i+1] = p[1]; img.data[i+2] = p[2]; img.data[i+3] = 255;\n"
        "    }\n"
        "  });\n"
        "  ctx.putImageData(img, 0, 0);\n"
        "}\n</script></body></html>\n",
        js_name);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: map_json_op <map.txt> <out.json>\n");
        return 2;
    }
    FILE *in = fopen(argv[1], "r");
    if (!in) { fprintf(stderr, "map_json_op: cannot read %s\n", argv[1]); return 2; }
    struct stat st;
    long bytes = 0;
    if (fstat(fileno(in), &st) == 0) bytes = (long)st.st_size;

    char **rows = NULL;
    int *lens = NULL;
    int h = 0, cap = 0, width = 0, ragged = 0;
    unsigned long counts[256];
    memset(counts, 0, sizeof counts);
    char *line = NULL;
    size_t lcap = 0;
    while (1) {
        ssize_t n = getline(&line, &lcap, in);
        if (n < 0) break;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (n > MAX_SIDE || h >= MAX_SIDE) {
            fprintf(stderr, "map_json_op: map is past %d on a side\n", MAX_SIDE);
            fclose(in); free(line); return 2;
        }
        if (h == cap) {
            cap = cap ? cap * 2 : 64;
            rows = realloc(rows, (size_t)cap * sizeof *rows);
            lens = realloc(lens, (size_t)cap * sizeof *lens);
            if (!rows || !lens) { fprintf(stderr, "map_json_op: out of memory\n"); return 2; }
        }
        rows[h] = malloc((size_t)n + 1);
        if (!rows[h]) { fprintf(stderr, "map_json_op: out of memory\n"); return 2; }
        memcpy(rows[h], line, (size_t)n + 1);
        lens[h] = (int)n;
        if ((int)n > width) width = (int)n;
        for (ssize_t i = 0; i < n; i++) counts[(unsigned char)line[i]]++;
        h++;
    }
    fclose(in);
    free(line);
    for (int y = 0; y < h; y++) if (lens[y] != width) ragged = 1;

    int x0 = width, y0 = h, x1 = -1, y1 = -1;
    unsigned long land = 0, empty = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < width; x++) {
            char ch = x < lens[y] ? rows[y][x] : ' ';
            if (ch == ' ') empty++;
            else {
                land++;
                if (x < x0) x0 = x;
                if (y < y0) y0 = y;
                if (x > x1) x1 = x;
                if (y > y1) y1 = y;
            }
        }
    }

    char dir[1024];
    snprintf(dir, sizeof dir, "%s", argv[1]);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0; else snprintf(dir, sizeof dir, ".");
    char notes[4096];
    notes[0] = 0;
    const char *names[] = { "land.seed.pdl", "extrusion.pdl", "view.pdl", "cells.txt" };
    for (int i = 0; i < 4; i++) {
        char chunk[2048];
        if (!read_sidecar(dir, names[i], chunk, sizeof chunk, i == 3)) continue;
        size_t used = strlen(notes), add = strlen(chunk);
        if (used + add + 64 < sizeof notes) {
            snprintf(notes + used, sizeof notes - used, "%s\n%s", names[i], chunk);
        }
    }

    char js_path[1024], html_path[1024];
    swap_ext(argv[2], ".js", js_path, sizeof js_path);
    swap_ext(argv[2], ".html", html_path, sizeof html_path);
    FILE *jf = fopen(argv[2], "w");
    FILE *sf = fopen(js_path, "w");
    FILE *hf = fopen(html_path, "w");
    if (!jf || !sf || !hf) {
        fprintf(stderr, "map_json_op: cannot write %s\n", argv[2]);
        return 2;
    }
    fputs("window.PAGE_MAP = ", sf);
    for (int pass = 0; pass < 2; pass++) {
        FILE *o = pass ? sf : jf;
        fprintf(o, "{\n  \"source\": "); json_str(o, argv[1]);
        fprintf(o, ",\n  \"bytes\": %ld,\n  \"width\": %d,\n  \"height\": %d,\n  \"max_side\": %d,\n  \"land\": %lu,\n  \"empty\": %lu,\n  \"ragged\": %s",
            bytes, width, h, width > h ? width : h, land, empty, ragged ? "true" : "false");
        if (x1 >= 0) fprintf(o, ",\n  \"bounds\": {\"x0\": %d, \"y0\": %d, \"x1\": %d, \"y1\": %d}", x0, y0, x1, y1);
        else fputs(",\n  \"bounds\": null", o);
        fputs(",\n  \"glyphs\": {", o);
        int first = 1;
        for (int c = 0; c < 256; c++) if (counts[c]) {
            char key[2] = { (char)c, 0 };
            fprintf(o, "%s", first ? "" : ", ");
            json_str(o, key);
            fprintf(o, ": %lu", counts[c]);
            first = 0;
        }
        fputs("},\n  \"notes\": ", o);
        json_str(o, notes[0] ? notes : "");
        fputs(",\n  \"rows\": [\n", o);
        for (int y = 0; y < h; y++) {
            if (y) fputs(",\n", o);
            fputs("    ", o);
            json_str(o, rows[y]);
        }
        fputs("\n  ]\n}\n", o);
    }
    fputs(";\n", sf);
    write_html(hf, base_of(js_path));
    fclose(jf); fclose(sf); fclose(hf);
    for (int y = 0; y < h; y++) free(rows[y]);
    free(rows); free(lens);
    printf("MAP %dx%d max_side=%d bytes=%ld land=%lu -> %s\n", width, h, width > h ? width : h, bytes, land, argv[2]);
    return 0;
}
