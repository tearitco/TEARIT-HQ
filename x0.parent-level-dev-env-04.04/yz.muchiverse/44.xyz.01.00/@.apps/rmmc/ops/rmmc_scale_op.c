/* rmmc_scale_op: copy a desk (page) of a book at N times the size, ground-plan only.
 *
 *   rmmc_scale_op <src_desk_dir> <dst_desk_dir> <factor>
 *
 * Why (owner 2026-11-25): book RMMC page rmmc-0001 is the Centroid floating island (book centroid, page small) at 3x, so a walker has room to move; no indoor/outdoor
 * teleports, so events.txt (door, window, roof boxes) and door.txt are NOT copied.
 * The desk shape is the rpg-space one (BOOK 16.game): map.txt (one glyph per cell), cells.txt (header, then `cells`, then one row of cell tokens per map row),
 * extrusion.pdl (glyph -> height), the tile atlas cells.rgba. A cell token is copied byte for byte, so the op does not depend on how many numbers a cell holds.
 * Only width= and height= change, every glyph and token repeats factor times along x and every row factor times along y. The atlas, extrusion and skin are copied.
 * cells.marker is a projection, written last: one byte longer than any earlier one is not needed because the page is new, but the engine reloads on a SIZE change,
 * so the size is the new cell count mod 4000 plus one (the same grow rule as tear_chain_op desk).
 * Exit: 0 ok, 2 usage / bad factor, 3 a source file is missing, 4 a row did not have width tokens (nothing is half written: files are written after the whole read).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LINE 262144

static char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)sz + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)sz, f); b[r] = 0; fclose(f); if (n) *n = r; return b;
}
static int copyf(const char *src, const char *dst) {
    size_t n; char *b = slurp(src, &n); if (!b) return -1;
    FILE *f = fopen(dst, "wb"); if (!f) { free(b); return -1; } fwrite(b, 1, n, f); fclose(f); free(b); return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: rmmc_scale_op <src_desk_dir> <dst_desk_dir> <factor 1..4>\n"); return 2; }
    int F = atoi(argv[3]); if (F < 1 || F > 4) { fprintf(stderr, "factor must be 1..4\n"); return 2; }
    const char *S = argv[1], *D = argv[2]; char p[4400], q[4400];
    snprintf(p, sizeof p, "%s/cells.txt", S); size_t cn; char *ct = slurp(p, &cn); if (!ct) { fprintf(stderr, "missing %s\n", p); return 3; }
    snprintf(p, sizeof p, "%s/map.txt", S); size_t mn; char *mt = slurp(p, &mn); if (!mt) { fprintf(stderr, "missing %s\n", p); return 3; }
    mkdir(D, 0755);

    /* cells.txt */
    char *out = malloc(cn * (size_t)F * (size_t)F + 4096); size_t on = 0; int w = 0, h = 0, in_cells = 0, rows = 0;
    char *save = NULL;
    for (char *l = strtok_r(ct, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
        if (!in_cells) {
            if (!strncmp(l, "width=", 6)) { w = atoi(l + 6); on += (size_t)sprintf(out + on, "width=%d\n", w * F); }
            else if (!strncmp(l, "height=", 7)) { h = atoi(l + 7); on += (size_t)sprintf(out + on, "height=%d\n", h * F); }
            else { on += (size_t)sprintf(out + on, "%s\n", l); if (!strncmp(l, "cells", 5)) in_cells = 1; }
            continue;
        }
        if (rows >= h) break;
        /* split the row into tokens (single spaces), each repeated F times; then the row F times */
        char *row = malloc(strlen(l) * (size_t)F + 16); size_t rn = 0; int tok = 0; char *sv2 = NULL; char *lc = strdup(l);
        for (char *t = strtok_r(lc, " ", &sv2); t; t = strtok_r(NULL, " ", &sv2)) { for (int k = 0; k < F; k++) rn += (size_t)sprintf(row + rn, "%s%s", (tok || k) ? " " : "", t); tok++; }
        free(lc);
        if (tok != w) { fprintf(stderr, "row %d has %d tokens, want %d\n", rows, tok, w); return 4; }
        for (int k = 0; k < F; k++) on += (size_t)sprintf(out + on, "%s\n", row);
        free(row); rows++;
    }
    if (rows != h) { fprintf(stderr, "cells.txt has %d rows, want %d\n", rows, h); return 4; }

    /* map.txt: one glyph per cell, same scaling */
    char *mo = malloc(mn * (size_t)F * (size_t)F + 16); size_t mon = 0; int mrows = 0; save = NULL;
    /* keep empty lines: split by hand */
    for (char *a = mt; *a; ) {
        char *e = strchr(a, '\n'); size_t len = e ? (size_t)(e - a) : strlen(a);
        if (mrows >= h) break;
        char *line = malloc(len * (size_t)F + 2); size_t ln = 0;
        for (size_t i = 0; i < len; i++) for (int k = 0; k < F; k++) line[ln++] = a[i];
        line[ln] = 0;
        for (int k = 0; k < F; k++) mon += (size_t)sprintf(mo + mon, "%s\n", line);
        free(line); mrows++; a = e ? e + 1 : a + len;
    }

    snprintf(q, sizeof q, "%s/cells.txt", D); FILE *f = fopen(q, "wb"); if (!f) return 3; fwrite(out, 1, on, f); fclose(f);
    snprintf(q, sizeof q, "%s/map.txt", D); f = fopen(q, "wb"); if (!f) return 3; fwrite(mo, 1, mon, f); fclose(f);
    const char *keep[] = { "cells.rgba", "extrusion.pdl", "skin.txt", "parallax.pdl", "parallax.rgba", NULL };
    for (int i = 0; keep[i]; i++) { snprintf(p, sizeof p, "%s/%s", S, keep[i]); snprintf(q, sizeof q, "%s/%s", D, keep[i]); copyf(p, q); }
    snprintf(q, sizeof q, "%s/events.txt", D); f = fopen(q, "wb"); if (f) fclose(f);        /* no door / window / roof boxes: no indoor teleports */
    snprintf(q, sizeof q, "%s/cells.marker", D); f = fopen(q, "wb");
    if (f) { long n = ((long)w * F * h * F) % 4000 + 1; for (long i = 0; i < n; i++) fputc('1', f); fclose(f); }
    printf("rmmc_scale: %dx%d -> %dx%d (factor %d)\n", w, h, w * F, h * F, F);
    return 0;
}
