/* pet_scene - draws what the pet window's <canvas sprite=...> shows, as one RGBA32 frame (1:1), in three modes:
 *
 *   pet_scene room   <pet_dir>   <out_raw> <w> <h> <pet_x> <pet_y> <anim> <frame>     the pet's room (fridge, bed, bath) with the pet standing at pet_x,pet_y (feet)
 *   pet_scene manage <state_dir> <out_raw> <w> <h> <active_id> <frame>                 the party strip: every pet in a row (colour and shape differ), the active one raised and framed
 *   pet_scene world  <state_dir> <out_raw> <w> <h> <active_id> <frame>                 the village, top-down: the TRAINER (you), your pet following, the other pets as creatures
 * Writes <out_raw> (RGBA8), <out minus .raw>.receipt.txt (frame_w=, frame_h=) and grows <out_dir>/scene_changed.txt.
 * Pet pictures: <pets>/<id>/art/sprites_hi/<anim>_<NN>/sprite.csv (36x48 inside a 48x48 csv, written by pet_gen). state_dir has party.txt (`id name`), pets/<id>/, world.st, and
 * the map is world_map.txt next to the app (see pet_world.c). Build: gcc -std=c11 -O2 -Wall -Wextra -o ops/+x/pet_scene.+x ops/pet_scene.c */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *fb; static int W, H;
static void px(int x, int y, int r, int g, int b) { if (x < 0 || y < 0 || x >= W || y >= H) return; unsigned char *p = fb + ((size_t)y * W + x) * 4; p[0] = (unsigned char)r; p[1] = (unsigned char)g; p[2] = (unsigned char)b; p[3] = 255; }
static void rect(int x0, int y0, int x1, int y1, int r, int g, int b) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) px(x, y, r, g, b); }
static void frame(int x0, int y0, int x1, int y1, int r, int g, int b) { rect(x0, y0, x1, y0 + 2, r, g, b); rect(x0, y1 - 2, x1, y1, r, g, b); rect(x0, y0, x0 + 2, y1, r, g, b); rect(x1 - 2, y0, x1, y1, r, g, b); }

/* draw a hi-res pet frame: csv cells (r,g,b,a), 48x48 with the 36-wide picture starting at column 6. div = 1 (full), 2 (half size, every 2nd pixel). Feet at (fx_, fy_). */
static int sprite(const char *path, int fx_, int fy_, int sc, int div) {
    FILE *f = fopen(path, "r"); if (!f) return 0; char line[128]; int res = 48, n = 0;
    while (fgets(line, sizeof line, f)) { if (!strncmp(line, "# resolution=", 13)) res = atoi(line + 13); if (!strncmp(line, "r,g,b,a", 7)) break; }
    int ox = fx_ - (res / div * sc) / 2, oy = fy_ - (46 / div) * sc;
    while (fgets(line, sizeof line, f) && n < res * res) {
        int r, g, b, a, x = n % res, y = n / res;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4 && a && x % div == 0 && y % div == 0)
            for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) px(ox + (x / div) * sc + dx, oy + (y / div) * sc + dy, r, g, b);
        n++;
    }
    fclose(f); return 1;
}
static int kvnum(const char *file, const char *key, int def) { FILE *f = fopen(file, "r"); if (!f) return def; char l[256]; size_t k = strlen(key); int v = def; while (fgets(l, sizeof l, f)) if (!strncmp(l, key, k) && l[k] == '=') { v = atoi(l + k + 1); break; } fclose(f); return v; }

static void room(const char *pd, int pxx, int pyy, const char *anim, int fi) {
    int floor_y = H - 30;
    rect(0, 0, W, floor_y, 120, 150, 190);
    for (int y = 0; y < floor_y; y += 24) rect(0, y, W, y + 1, 110, 140, 180);
    rect(0, floor_y, W, H, 150, 110, 70);
    for (int x = 0; x < W; x += 40) rect(x, floor_y, x + 1, H, 130, 95, 60);
    rect(14, floor_y - 120, 74, floor_y, 220, 225, 230); frame(14, floor_y - 120, 74, floor_y, 120, 125, 135);                       /* fridge */
    rect(14, floor_y - 74, 74, floor_y - 71, 120, 125, 135); rect(62, floor_y - 108, 66, floor_y - 84, 90, 95, 105); rect(62, floor_y - 66, 66, floor_y - 40, 90, 95, 105);
    int bx = 96 + (W - 96 - 100 - 96) / 2;                                                                                              /* bed */
    rect(bx, floor_y - 34, bx + 96, floor_y - 10, 70, 90, 170); rect(bx, floor_y - 10, bx + 96, floor_y, 110, 80, 50);
    rect(bx, floor_y - 52, bx + 10, floor_y, 110, 80, 50); rect(bx + 6, floor_y - 44, bx + 34, floor_y - 34, 240, 240, 240);
    int tx = W - 100;                                                                                                                   /* bath */
    rect(tx, floor_y - 40, W - 10, floor_y - 6, 235, 240, 245); frame(tx, floor_y - 40, W - 10, floor_y - 6, 150, 160, 170);
    rect(tx + 6, floor_y - 30, W - 16, floor_y - 12, 110, 180, 230); rect(tx + 8, floor_y - 6, tx + 18, floor_y, 90, 90, 90); rect(W - 28, floor_y - 6, W - 18, floor_y, 90, 90, 90);
    rect(W / 2 - 16, floor_y - 70, W / 2 + 16, floor_y, 90, 60, 40); rect(W / 2 - 12, floor_y - 66, W / 2 + 12, floor_y, 120, 85, 55); rect(W / 2 + 6, floor_y - 34, W / 2 + 9, floor_y - 30, 250, 220, 80);   /* the door (back to the village) */
    char p[1536]; snprintf(p, sizeof p, "%s/art/sprites_hi/%s_%02d/sprite.csv", pd, anim, fi); sprite(p, pxx, pyy, 2, 1);
}

static void manage(const char *sd, const char *active, int fi) {
    for (int y = 0; y < H; y++) { int s = 40 + y * 60 / H; rect(0, y, W, y + 1, s / 2, s * 2 / 3, s + 40); }
    rect(0, 0, W, 22, 20, 30, 60); rect(0, 22, W, 24, 250, 210, 80);
    char pf[PATH_MAX]; snprintf(pf, sizeof pf, "%s/party.txt", sd); FILE *f = fopen(pf, "r"); char ids[8][32]; int n = 0;
    if (f) { char l[128]; while (fgets(l, sizeof l, f) && n < 8) { if (sscanf(l, "%31s", ids[n]) == 1) n++; } fclose(f); }
    int slot = n ? W / n : W;
    for (int i = 0; i < n; i++) {
        int x0 = i * slot, sel = !strcmp(ids[i], active); char p[PATH_MAX], vf[PATH_MAX];
        rect(x0 + 3, 34, x0 + slot - 3, H - 8, sel ? 70 : 34, sel ? 100 : 48, sel ? 160 : 86);
        if (sel) frame(x0 + 3, 34, x0 + slot - 3, H - 8, 255, 225, 90);
        snprintf(p, sizeof p, "%s/pets/%s/art/sprites_hi/%s_%02d/sprite.csv", sd, ids[i], sel ? "happy" : "idle", (fi + i * 3) % 8);
        sprite(p, x0 + slot / 2, H - 70 - (sel ? ((fi % 2) ? 6 : 0) : 0), 1, 1);
        snprintf(vf, sizeof vf, "%s/pets/%s/variables.txt", sd, ids[i]); int lv = kvnum(vf, "rpg_level", 1);
        for (int k = 0; k < 10; k++) rect(x0 + 8 + k * 4, H - 40, x0 + 11 + k * 4, H - 36, k < lv ? 90 : 40, k < lv ? 220 : 50, k < lv ? 110 : 70);       /* level pips */
        snprintf(vf, sizeof vf, "%s/pets/%s/running.txt", sd, ids[i]); FILE *rf = fopen(vf, "r"); int on = rf && fgetc(rf) == '1'; if (rf) fclose(rf);
        rect(x0 + slot / 2 - 4, H - 28, x0 + slot / 2 + 4, H - 20, on ? 60 : 230, on ? 220 : 70, on ? 90 : 70);                                    /* green = started, red = stopped */
        for (int k = 0; k <= i; k++) rect(x0 + 6 + k * 6, 8, x0 + 10 + k * 6, 16, sel ? 255 : 150, sel ? 225 : 160, sel ? 90 : 190);          /* slot number as dots */
    }
}

static char mapr[64][66]; static int mh_, mw_;
static void tile(int x, int y, char c, int px0, int py0, int T) {
    switch (c) {
        case 'T': rect(px0, py0, px0 + T, py0 + T, 86, 160, 70); rect(px0 + T / 2 - 2, py0 + T - 8, px0 + T / 2 + 2, py0 + T, 100, 70, 40); for (int i = 0; i < 4; i++) rect(px0 + 3 + i, py0 + 2 + i * 2, px0 + T - 3 - i, py0 + 4 + i * 2, 30, 110 + i * 15, 50); break;
        case 'W': rect(px0, py0, px0 + T, py0 + T, 70, 130, 210); if (((x + y) & 1) == 0) rect(px0 + 4, py0 + 8, px0 + 12, py0 + 9, 170, 210, 250); else rect(px0 + 10, py0 + 15, px0 + 18, py0 + 16, 170, 210, 250); break;
        case '=': rect(px0, py0, px0 + T, py0 + T, 205, 180, 130); rect(px0 + 5, py0 + 6, px0 + 7, py0 + 8, 180, 155, 105); rect(px0 + 15, py0 + 14, px0 + 17, py0 + 16, 180, 155, 105); break;
        case '^': rect(px0, py0, px0 + T, py0 + T, 190, 80, 70); for (int i = 0; i < T; i += 6) rect(px0, py0 + i, px0 + T, py0 + i + 1, 150, 55, 50); break;
        case '#': rect(px0, py0, px0 + T, py0 + T, 235, 225, 195); rect(px0 + 3, py0 + 4, px0 + 8, py0 + 9, 120, 170, 210); break;
        case 'D': case 'd': rect(px0, py0, px0 + T, py0 + T, 235, 225, 195); rect(px0 + 4, py0 + 3, px0 + T - 4, py0 + T, c == 'D' ? 150 : 110, c == 'D' ? 95 : 100, c == 'D' ? 55 : 100);
                  rect(px0 + T - 9, py0 + 12, px0 + T - 6, py0 + 15, 250, 220, 80); break;
        default: rect(px0, py0, px0 + T, py0 + T, 120, 190, 100); if (((x * 7 + y * 13) % 5) == 0) rect(px0 + 6, py0 + 7, px0 + 8, py0 + 11, 90, 160, 80);
                 if (c == ',') { rect(px0 + 6, py0 + 6, px0 + 10, py0 + 10, 250, 120, 160); rect(px0 + 14, py0 + 14, px0 + 18, py0 + 18, 250, 230, 90); } break;
    }
}
static void trainer(int cx, int by, const char *dir) {
    rect(cx - 4, by - 6, cx - 1, by, 40, 50, 110); rect(cx + 1, by - 6, cx + 4, by, 40, 50, 110);                     /* legs */
    rect(cx - 6, by - 14, cx + 6, by - 6, 215, 60, 60);                                                                 /* jacket */
    rect(cx - 8, by - 13, cx - 6, by - 7, 215, 60, 60); rect(cx + 6, by - 13, cx + 8, by - 7, 215, 60, 60);
    rect(cx - 4, by - 21, cx + 4, by - 14, 245, 205, 170);                                                              /* head */
    rect(cx - 5, by - 24, cx + 5, by - 20, 250, 250, 250); rect(cx - 5, by - 22, cx + 5, by - 20, 215, 60, 60);        /* cap */
    if (!strcmp(dir, "down")) { rect(cx - 3, by - 18, cx - 2, by - 16, 30, 30, 40); rect(cx + 2, by - 18, cx + 3, by - 16, 30, 30, 40); rect(cx - 6, by - 21, cx + 6, by - 19, 215, 60, 60); }
    else if (!strcmp(dir, "left")) { rect(cx - 3, by - 18, cx - 2, by - 16, 30, 30, 40); rect(cx - 8, by - 21, cx, by - 19, 215, 60, 60); }
    else if (!strcmp(dir, "right")) { rect(cx + 2, by - 18, cx + 3, by - 16, 30, 30, 40); rect(cx, by - 21, cx + 8, by - 19, 215, 60, 60); }
    else rect(cx - 5, by - 24, cx + 5, by - 18, 215, 60, 60);
}
static void world(const char *sd, const char *app, const char *active, int fi) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/world_map.txt", app); FILE *f = fopen(p, "r"); mh_ = 0; mw_ = 0;
    if (f) { char l[128]; while (fgets(l, sizeof l, f) && mh_ < 64) { l[strcspn(l, "\r\n")] = 0; snprintf(mapr[mh_], sizeof mapr[0], "%s", l); if ((int)strlen(l) > mw_) mw_ = (int)strlen(l); mh_++; } fclose(f); }
    snprintf(p, sizeof p, "%s/world.st", sd); int tx = kvnum(p, "tx", 1), ty = kvnum(p, "ty", 1), fx = kvnum(p, "fx", 1), fy = kvnum(p, "fy", 1), nn = kvnum(p, "n_npc", 0);
    char dir[8] = "down"; { FILE *w = fopen(p, "r"); if (w) { char l[64]; while (fgets(l, sizeof l, w)) if (!strncmp(l, "dir=", 4)) { l[strcspn(l, "\r\n")] = 0; snprintf(dir, sizeof dir, "%s", l + 4); } fclose(w); } }
    int T = 24, cols = W / T, rows = (H - 16) / T, cx0 = tx - cols / 2, cy0 = ty - rows / 2;
    if (cx0 < 0) cx0 = 0; if (cy0 < 0) cy0 = 0; if (cx0 > mw_ - cols) cx0 = mw_ - cols; if (cy0 > mh_ - rows) cy0 = mh_ - rows; if (cx0 < 0) cx0 = 0; if (cy0 < 0) cy0 = 0;
    rect(0, 0, W, H, 20, 24, 30);
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) { int mx = cx0 + x, my = cy0 + y; char c = (my < mh_ && mx < (int)strlen(mapr[my])) ? mapr[my][mx] : 'T'; if (c == 'P' || c == 'N') c = '.'; tile(mx, my, c, x * T, y * T, T); }
    for (int i = 0; i < nn; i++) {                                                                                         /* the other pets, small, on their tiles */
        char key[48], id[32] = ""; snprintf(key, sizeof key, "npc_%d_id", i);
        { FILE *w = fopen(p, "r"); if (w) { char l[96]; size_t kl = strlen(key); while (fgets(l, sizeof l, w)) if (!strncmp(l, key, kl) && l[kl] == '=') { l[strcspn(l, "\r\n")] = 0; snprintf(id, sizeof id, "%s", l + kl + 1); } fclose(w); } }
        snprintf(key, sizeof key, "npc_%d_x", i); int nx = kvnum(p, key, -9); snprintf(key, sizeof key, "npc_%d_y", i); int ny = kvnum(p, key, -9);
        if (!strcmp(id, active) || nx < cx0 || ny < cy0 || nx >= cx0 + cols || ny >= cy0 + rows) continue;
        char sp[PATH_MAX]; snprintf(sp, sizeof sp, "%s/pets/%s/art/sprites_hi/idle_%02d/sprite.csv", sd, id, (fi + i * 3) % 8); sprite(sp, (nx - cx0) * T + T / 2, (ny - cy0) * T + T - 1, 1, 2);
    }
    if (fx >= cx0 && fy >= cy0 && fx < cx0 + cols && fy < cy0 + rows) {                                                    /* your pet follows one tile behind */
        char sp[PATH_MAX]; snprintf(sp, sizeof sp, "%s/pets/%s/art/sprites_hi/walk_%02d/sprite.csv", sd, active, fi % 8); sprite(sp, (fx - cx0) * T + T / 2, (fy - cy0) * T + T - 1, 1, 2);
    }
    trainer((tx - cx0) * T + T / 2, (ty - cy0) * T + T - 2, dir);                                                          /* the trainer: you, not a pokemon */
    rect(0, H - 16, W, H, 30, 36, 50); rect(0, H - 16, W, H - 15, 250, 210, 80);
    for (int k = 0; k < nn; k++) rect(8 + k * 12, H - 11, 16 + k * 12, H - 4, 120, 190, 255);
}

int main(int argc, char **argv) {
    if (argc < 8) { fprintf(stderr, "usage: pet_scene room|manage|world ...\n"); return 2; }
    const char *mode = argv[1], *dir = argv[2], *out = argv[3]; W = atoi(argv[4]); H = atoi(argv[5]);
    if (W < 64 || H < 64 || W > 1200 || H > 900) return 2;
    fb = calloc((size_t)W * H, 4); if (!fb) return 1;
    char self[PATH_MAX]; char app[PATH_MAX] = "."; if (realpath(argv[0], self)) { snprintf(app, sizeof app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(app, '/'); if (s) *s = 0; } }
    if (!strcmp(mode, "room") && argc >= 10) room(dir, atoi(argv[6]), atoi(argv[7]), argv[8], atoi(argv[9]) & 7);
    else if (!strcmp(mode, "manage")) manage(dir, argv[6], atoi(argv[7]));
    else if (!strcmp(mode, "world")) world(dir, app, argv[6], atoi(argv[7]));
    else return 2;
    FILE *o = fopen(out, "wb"); if (!o) return 1; fwrite(fb, 1, (size_t)W * H * 4, o); fclose(o);
    char rp[1536]; snprintf(rp, sizeof rp, "%s", out); char *dot = strrchr(rp, '.'); if (dot) *dot = 0; strcat(rp, ".receipt.txt");
    FILE *r = fopen(rp, "w"); if (r) { fprintf(r, "frame_w=%d\nframe_h=%d\n", W, H); fclose(r); }
    char mp[1536]; snprintf(mp, sizeof mp, "%s", out); char *sl = strrchr(mp, '/'); if (sl) *sl = 0; strcat(mp, "/scene_changed.txt");
    FILE *m = fopen(mp, "a"); if (m) { fputc('x', m); fclose(m); }
    free(fb); return 0;
}
