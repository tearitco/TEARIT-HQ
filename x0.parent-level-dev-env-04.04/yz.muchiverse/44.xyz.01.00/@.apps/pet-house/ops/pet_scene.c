/* pet_scene - draws the pet's room and the pet into one RGBA32 frame a layout <canvas sprite=...> shows 1:1.
 *
 *   pet_scene <pet_dir> <out_raw> <width> <height> <pet_x> <pet_y> <anim> <frame>
 *
 * Writes <out_raw> (RGBA8, width*height*4, row-major), <out_raw minus .raw>.receipt.txt (frame_w=, frame_h=) and grows <out_dir>/scene_changed.txt.
 * The room: back wall, floor (its top edge is the line the pet stands on = height - 30), a fridge (left), a bed (middle right), a bath tub (far right).
 * The pet: the 36x48 hi-res frame <pet_dir>/art/sprites_hi/<anim>_<NN>/sprite.csv drawn 2x, feet at (pet_x, pet_y), centred on pet_x.
 * Build: gcc -std=c11 -O2 -Wall -Wextra -o ops/+x/pet_scene.+x ops/pet_scene.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *fb; static int W, H;
static void px(int x, int y, int r, int g, int b) { if (x < 0 || y < 0 || x >= W || y >= H) return; unsigned char *p = fb + ((size_t)y * W + x) * 4; p[0] = (unsigned char)r; p[1] = (unsigned char)g; p[2] = (unsigned char)b; p[3] = 255; }
static void rect(int x0, int y0, int x1, int y1, int r, int g, int b) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) px(x, y, r, g, b); }
static void frame(int x0, int y0, int x1, int y1, int r, int g, int b) { rect(x0, y0, x1, y0 + 2, r, g, b); rect(x0, y1 - 2, x1, y1, r, g, b); rect(x0, y0, x0 + 2, y1, r, g, b); rect(x1 - 2, y0, x1, y1, r, g, b); }

int main(int argc, char **argv) {
    if (argc < 9) { fprintf(stderr, "usage: pet_scene <pet_dir> <out_raw> <w> <h> <pet_x> <pet_y> <anim> <frame>\n"); return 2; }
    const char *pd = argv[1], *out = argv[2]; W = atoi(argv[3]); H = atoi(argv[4]);
    int pxx = atoi(argv[5]), pyy = atoi(argv[6]); const char *anim = argv[7]; int fi = atoi(argv[8]) & 7;
    if (W < 64 || H < 64 || W > 1200 || H > 900) return 2;
    fb = calloc((size_t)W * H, 4); if (!fb) return 1;
    int floor_y = H - 30;
    rect(0, 0, W, floor_y, 120, 150, 190);                       /* back wall */
    for (int y = 0; y < floor_y; y += 24) rect(0, y, W, y + 1, 110, 140, 180);
    rect(0, floor_y, W, H, 150, 110, 70);                        /* floor */
    for (int x = 0; x < W; x += 40) rect(x, floor_y, x + 1, H, 130, 95, 60);
    /* fridge, left */
    rect(14, floor_y - 120, 74, floor_y, 220, 225, 230); frame(14, floor_y - 120, 74, floor_y, 120, 125, 135);
    rect(14, floor_y - 74, 74, floor_y - 71, 120, 125, 135); rect(62, floor_y - 108, 66, floor_y - 84, 90, 95, 105); rect(62, floor_y - 66, 66, floor_y - 40, 90, 95, 105);
    /* bed, centre right */
    int bx = 96 + (W - 96 - 100 - 96) / 2;
    rect(bx, floor_y - 34, bx + 96, floor_y - 10, 70, 90, 170); rect(bx, floor_y - 10, bx + 96, floor_y, 110, 80, 50);
    rect(bx, floor_y - 52, bx + 10, floor_y, 110, 80, 50); rect(bx + 6, floor_y - 44, bx + 34, floor_y - 34, 240, 240, 240);
    /* bath tub, far right */
    int tx = W - 100;
    rect(tx, floor_y - 40, W - 10, floor_y - 6, 235, 240, 245); frame(tx, floor_y - 40, W - 10, floor_y - 6, 150, 160, 170);
    rect(tx + 6, floor_y - 30, W - 16, floor_y - 12, 110, 180, 230); rect(tx + 8, floor_y - 6, tx + 18, floor_y, 90, 90, 90); rect(W - 28, floor_y - 6, W - 18, floor_y, 90, 90, 90);
    /* pet: hi-res frame, 2x */
    char p[1536]; snprintf(p, sizeof p, "%s/art/sprites_hi/%s_%02d/sprite.csv", pd, anim, fi);
    FILE *f = fopen(p, "r");
    if (f) {
        char line[128]; int res = 48, n = 0;
        while (fgets(line, sizeof line, f)) { if (!strncmp(line, "# resolution=", 13)) res = atoi(line + 13); if (!strncmp(line, "r,g,b,a", 7)) break; }
        int sc = 2, ox = pxx - (res * sc) / 2, oy = pyy - 46 * sc;     /* feet sit about row 46 of the 48-row frame */
        while (fgets(line, sizeof line, f) && n < res * res) {
            int r, g, b, a; if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4 && a) { int x = n % res, y = n / res; for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) px(ox + x * sc + dx, oy + y * sc + dy, r, g, b); }
            n++;
        }
        fclose(f);
    }
    FILE *o = fopen(out, "wb"); if (!o) return 1; fwrite(fb, 1, (size_t)W * H * 4, o); fclose(o);
    char rp[1536]; snprintf(rp, sizeof rp, "%s", out); char *dot = strrchr(rp, '.'); if (dot) *dot = 0; strcat(rp, ".receipt.txt");
    FILE *r = fopen(rp, "w"); if (r) { fprintf(r, "frame_w=%d\nframe_h=%d\n", W, H); fclose(r); }
    char mp[1536]; snprintf(mp, sizeof mp, "%s", out); char *sl = strrchr(mp, '/'); if (sl) *sl = 0; strcat(mp, "/scene_changed.txt");
    FILE *m = fopen(mp, "a"); if (m) { fputc('x', m); fclose(m); }
    free(fb); return 0;
}
