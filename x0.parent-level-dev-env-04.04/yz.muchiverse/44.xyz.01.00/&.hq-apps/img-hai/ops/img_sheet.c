/* img_sheet - contact sheet of many images: img_sheet <out.png> <columns> <thumb_px> <image files...>
 * Each image is fitted into a thumb_px square (box average down, nearest up) on a dark grid, left to right, top to bottom. Prints "<row>,<col> <file>" per image so a person can match a picture to its name.
 * Reads png jpg bmp gif tga psd hdr pnm (stb_image). Pure: no network, no window. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: img_sheet out.png columns thumb_px files...\n"); return 2; }
    int cols = atoi(argv[2]), th = atoi(argv[3]), n = argc - 4; if (cols < 1 || th < 8) return 2; int rows = (n + cols - 1) / cols, gap = 6, W = cols * (th + gap) + gap, H = rows * (th + gap) + gap;
    unsigned char *o = malloc((size_t)W * H * 4); if (!o) return 1; for (size_t i = 0; i < (size_t)W * H; i++) { o[i * 4] = 24; o[i * 4 + 1] = 24; o[i * 4 + 2] = 30; o[i * 4 + 3] = 255; }
    int placed = 0;
    for (int k = 0; k < n; k++) { int w, h, c; unsigned char *src = stbi_load(argv[4 + k], &w, &h, &c, 4); int r = k / cols, cc = k % cols; printf("%d,%d %s%s\n", r + 1, cc + 1, argv[4 + k], src ? "" : "  (could not read)"); if (!src) continue;
        double s = (double)th / (w > h ? w : h); int dw, dh; if (s >= 1.0) { int f = (int)s; dw = w * f; dh = h * f; } else { dw = (int)(w * s + .5); dh = (int)(h * s + .5); } if (dw < 1) dw = 1; if (dh < 1) dh = 1;
        int ox = gap + cc * (th + gap) + (th - dw) / 2, oy = gap + r * (th + gap) + (th - dh) / 2;
        for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) { int x0 = x * w / dw, x1 = (x + 1) * w / dw, y0 = y * h / dh, y1 = (y + 1) * h / dh; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1; long a[4] = { 0, 0, 0, 0 }, m = 0;
            for (int yy = y0; yy < y1 && yy < h; yy++) for (int xx = x0; xx < x1 && xx < w; xx++) { unsigned char *p = src + ((size_t)yy * w + xx) * 4; for (int q = 0; q < 4; q++) a[q] += p[q]; m++; }
            unsigned char *d = o + ((size_t)(oy + y) * W + (ox + x)) * 4; if (m) { float al = a[3] / (float)m / 255.0f; for (int q = 0; q < 3; q++) d[q] = (unsigned char)(d[q] * (1 - al) + (a[q] / m) * al); } }
        stbi_image_free(src); placed++; }
    stbi_write_png(argv[1], W, H, 4, o, W * 4); free(o); fprintf(stderr, "wrote %s (%dx%d, %d of %d images)\n", argv[1], W, H, placed, n); return placed ? 0 : 1;
}
