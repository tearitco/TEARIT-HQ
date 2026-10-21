/* csv_frames.c - pure text-include (no globals, no I/O beyond the two named files): turn a pet's sprite.csv (64x64 "r,g,b,a" rows after a
 * "# resolution=N" header, the pet-folder / layout-studio format) into the 16x24 RGBA walk frames the board-viewer 3D raymarcher extrudes for a map event:
 *   <frames_dir>/<charset>_<idx>_<dir 2|4|6|8>_<pat 0..2>.rgba     (same names as tsots-characters/frames and mcl-hq write_frame)
 * The pet is box-averaged to 16x16 and stands on the bottom of the 16x24 frame. These pets have one front picture, so: dir 6 is the mirror of dir 4 (2 and 8
 * both show the front), pattern 1 is a 2 px hop (the walk step) and patterns 0 and 2 stand. Returns 1 when frames were written, 0 when the csv is
 * missing or not 64x64 (the caller keeps its colour box). *avg gets the mean colour of the opaque pixels (the 3D box fallback colour in events.txt).
 * Used by rpg-edit-hq; any pet board with sprite.csv art can include it:  #include "../../../&.widgits/_shared-lib/csv_frames.c" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int csvf_load64(const char *path, unsigned char *px /* 64*64*4 */) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char l[128]; int n = 0, res = 0, hdr = 1;
    while (fgets(l, sizeof l, f)) {
        if (l[0] == '#') { if (!strncmp(l, "# resolution=", 13)) res = atoi(l + 13); continue; }
        if (hdr) { hdr = 0; if (l[0] == 'r') continue; }
        int r, g, b, a; if (sscanf(l, "%d,%d,%d,%d", &r, &g, &b, &a) != 4 || n >= 64 * 64) break;
        px[n * 4] = (unsigned char)r; px[n * 4 + 1] = (unsigned char)g; px[n * 4 + 2] = (unsigned char)b; px[n * 4 + 3] = (unsigned char)a; n++;
    }
    fclose(f); return res == 64 && n == 64 * 64;
}
static int csv_write_frames(const char *csv, const char *frames_dir, const char *charset, int idx, int *avg /* int[3] or NULL */) {
    unsigned char src[64 * 64 * 4], base[16 * 16 * 4]; if (!csvf_load64(csv, src)) return 0;
    long sr = 0, sg = 0, sb = 0, so = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        int a[4] = {0}, w = 0;
        for (int dy = 0; dy < 4; dy++) for (int dx = 0; dx < 4; dx++) {
            unsigned char *p = src + ((y * 4 + dy) * 64 + x * 4 + dx) * 4;
            a[0] += p[0] * p[3]; a[1] += p[1] * p[3]; a[2] += p[2] * p[3]; a[3] += p[3]; w += p[3];
        }
        unsigned char *o = base + (y * 16 + x) * 4;
        if (w) { o[0] = (unsigned char)(a[0] / w); o[1] = (unsigned char)(a[1] / w); o[2] = (unsigned char)(a[2] / w); o[3] = (unsigned char)(a[3] / 16); }
        else o[0] = o[1] = o[2] = o[3] = 0;
        if (o[3] > 127) { sr += o[0]; sg += o[1]; sb += o[2]; so++; }
    }
    if (avg && so) { avg[0] = (int)(sr / so); avg[1] = (int)(sg / so); avg[2] = (int)(sb / so); }
    static const int codes[4] = {2, 4, 6, 8};
    for (int c = 0; c < 4; c++) for (int pat = 0; pat < 3; pat++) {
        unsigned char out[16 * 24 * 4]; memset(out, 0, sizeof out);
        int hop = pat == 1 ? 2 : 0, flip = codes[c] == 6;
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int oy = 8 + y - hop; if (oy < 0 || oy > 23) continue;
            memcpy(out + (oy * 16 + x) * 4, base + (y * 16 + (flip ? 15 - x : x)) * 4, 4);
        }
        char p[1536]; snprintf(p, sizeof p, "%s/%s_%d_%d_%d.rgba", frames_dir, charset, idx, codes[c], pat);
        FILE *f = fopen(p, "wb"); if (!f) return 0; fwrite(out, 1, sizeof out, f); fclose(f);
    }
    return 1;
}
