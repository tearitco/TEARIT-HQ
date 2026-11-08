/* deskopoly_frames <sheet.png> <charset> <frames_dir>  - make the 3D walk frames of an RPG Maker MV character sheet (2026-11-08).
 * The 3D engine draws a map event (events.txt row: x y r g b <charset> <index> <dir> <pattern>) as a 16x24 RGBA billboard from <frames_dir>/<charset>_<index>_<dir>_<pattern>.rgba; the 2D picture blits the sheet itself.
 * Only the frames somebody used had ever been made, so a player on an unused sheet index showed in 2D but not in 3D. This writes the MISSING ones for the 8 characters x 4 directions x 3 patterns and never overwrites a frame
 * that exists (other games own those). Sheet layout (MV, 576x384): characters 4 across x 2 down, each 3 pattern columns x 4 direction rows of 48 px; rows are down(2) left(4) right(6) up(8).
 * Each 48x48 cell is cropped to its opaque box and area-averaged to fit 16x24 (aspect kept, centred, standing on the bottom edge). Prints "frames made=<n> existed=<m>". Exit 0 ok, 1 failure, 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: deskopoly_frames <sheet.png> <charset> <frames_dir>\n"); return 2; }
    int w, h, ch; unsigned char *px = stbi_load(argv[1], &w, &h, &ch, 4); if (!px || w != 576 || h != 384) { fprintf(stderr, "deskopoly_frames: %s is not a 576x384 MV character sheet\n", argv[1]); return 1; }
    int dirs[4] = { 2, 4, 6, 8 }, made = 0, existed = 0;
    for (int k = 0; k < 8; k++) for (int d = 0; d < 4; d++) for (int p = 0; p < 3; p++) {
        char fp[4400]; snprintf(fp, sizeof fp, "%s/%s_%d_%d_%d.rgba", argv[3], argv[2], k, dirs[d], p);
        int fd = open(fp, O_WRONLY | O_CREAT | O_EXCL, 0644); if (fd < 0) { if (errno == EEXIST) { existed++; continue; } perror(fp); return 1; }
        int cx = ((k % 4) * 3 + p) * 48, cy = ((k / 4) * 4 + d) * 48, x0 = 48, x1 = -1, y0 = 48, y1 = -1;
        for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) if (px[((size_t)(cy + y) * w + cx + x) * 4 + 3] > 8) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        unsigned char out[16 * 24 * 4]; for (int i = 0; i < 16 * 24; i++) { out[i * 4] = 255; out[i * 4 + 1] = 255; out[i * 4 + 2] = 255; out[i * 4 + 3] = 0; }
        if (x1 >= x0 && y1 >= y0) {
            double bw = x1 - x0 + 1, bh = y1 - y0 + 1, sc = 16.0 / bw < 24.0 / bh ? 16.0 / bw : 24.0 / bh; int dw = (int)(bw * sc + 0.5), dh = (int)(bh * sc + 0.5); if (dw < 1) dw = 1; if (dh < 1) dh = 1; int ox = (16 - dw) / 2, oy = 24 - dh;
            for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
                double sx0 = x0 + x / sc, sx1 = x0 + (x + 1) / sc, sy0 = y0 + y / sc, sy1 = y0 + (y + 1) / sc, ar = 0, ag = 0, ab = 0, aa = 0, n = 0;
                for (int sy = (int)sy0; sy < (int)(sy1 + 0.999) && sy < 48; sy++) for (int sx = (int)sx0; sx < (int)(sx1 + 0.999) && sx < 48; sx++) { const unsigned char *s = px + ((size_t)(cy + sy) * w + cx + sx) * 4; double a = s[3] / 255.0; ar += s[0] * a; ag += s[1] * a; ab += s[2] * a; aa += a; n += 1; }
                if (n > 0 && aa / n > 0.35) { unsigned char *o = out + ((size_t)(oy + y) * 16 + ox + x) * 4; o[0] = (unsigned char)(ar / aa); o[1] = (unsigned char)(ag / aa); o[2] = (unsigned char)(ab / aa); o[3] = 255; } } }
        if (write(fd, out, sizeof out) != (ssize_t)sizeof out) { close(fd); return 1; } close(fd); made++;
    }
    printf("frames made=%d existed=%d\n", made, existed); return 0;
}
