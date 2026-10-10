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
#include <math.h>
#include <time.h>

static unsigned char *fb; static int W, H; static char g_app[1024] = ".", g_shared[1024] = ".";
static FILE *pdl_open(const char *stem) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s_all.pdl", g_shared, stem); FILE *f = fopen(p, "r"); if (f) return f; snprintf(p, sizeof p, "%s/%s.pdl", g_app, stem); return fopen(p, "r"); }      /* built rooms overlay (state) else the shipped file */
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


/* the big window on the back wall: what the pet sees outside (sky, sun, drifting clouds, hills, moving water, flying birds). Time is quantized to 250 ms so an unchanged frame is byte-identical (no repaint). */
static long long qtime_ms(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); long long ms = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; return ms / 800 * 800; }

/* day and night is EVENT driven: the pet clock fires dawn/day/dusk/night (time.pdl) and each writes state/daylight.txt (phase=...); the scene only reads that phase. g_f = how bright (0 night .. 1 day), g_t = where the sun/moon is on its arc (0 left .. 1 right). PET_SCENE_PHASE overrides for tests. */
static double g_f = 1, g_t = 0.5; static int g_sun = 1;
static void load_phase(void) {
    char p[PATH_MAX], l[64], ph[16] = "day"; const char *e = getenv("PET_SCENE_PHASE"), *sh = getenv("PET_SHARED");
    if (e && e[0]) snprintf(ph, sizeof ph, "%s", e); else { snprintf(p, sizeof p, "%s/daylight.txt", sh && sh[0] ? sh : "state"); if (!sh || !sh[0]) snprintf(p, sizeof p, "%s/state/daylight.txt", g_app);
        FILE *f = fopen(p, "r"); if (f) { if (fgets(l, sizeof l, f)) sscanf(l, "phase=%15s", ph); fclose(f); } }
    if (!strcmp(ph, "dawn")) { g_f = 0.45; g_t = 0.06; g_sun = 1; } else if (!strcmp(ph, "dusk")) { g_f = 0.45; g_t = 0.94; g_sun = 1; }
    else if (!strcmp(ph, "night")) { g_f = 0; g_t = 0.55; g_sun = 0; } else { g_f = 1; g_t = 0.5; g_sun = 1; }
}
static void skycol(int k, int *r, int *g, int *b) {                                                                                       /* k 0..255 = top to horizon */
    double f = g_f, w = (f > 0 && f < 1) ? (1 - f) * f * 4 : 0;                                                                           /* dawn/dusk warmth */
    *r = (int)((10 + k / 8) * (1 - f) + (110 + k / 4) * f + w * 90); *g = (int)((14 + k / 8) * (1 - f) + (175 + k / 5) * f + w * 25); *b = (int)((40 + k / 4) * (1 - f) + (235 - k / 8) * f - w * 40);
    if (*r > 255) *r = 255; if (*g > 255) *g = 255; if (*b < 0) *b = 0;
}
static int dim(int v) { return (int)(v * (0.28 + 0.72 * g_f)); }
static void body(int x0, int y0, int w, int hgt) {                                                                                         /* the sun or the moon on its arc */
    int cx = x0 + 12 + (int)((w - 24) * g_t), cy = y0 + hgt - 14 - (int)(sin(3.14159265 * g_t) * (hgt - 28)), R = 9;
    for (int dy = -R; dy <= R; dy++) for (int dx = -R; dx <= R; dx++) if (dx * dx + dy * dy <= R * R) { if (g_sun) px(cx + dx, cy + dy, 255, g_f < 1 ? 190 : 232, g_f < 1 ? 90 : 120); else if ((dx + 4) * (dx + 4) + dy * dy > 49) px(cx + dx, cy + dy, 232, 232, 212); }
}
static void stars(int x0, int y0, int w, int hgt) {
    if (g_f > 0.6) return; for (int i = 0; i < 40; i++) { unsigned s = (unsigned)(i * 2654435761u); int x = x0 + (int)(s % (unsigned)w), y = y0 + (int)((s >> 11) % (unsigned)(hgt > 1 ? hgt : 1)); if ((i + (int)(qtime_ms() / 1000)) % 7) px(x, y, 235, 235, 255); }
}
static void big_window(int x0, int y0, int w, int h, int floor_y) {
    long long T = qtime_ms(); int fr = 5, ix0 = x0 + fr, iy0 = y0 + fr, iw = w - 2 * fr, ih = h - 2 * fr, hz = iy0 + ih * 55 / 100;     /* hz = horizon (water starts here) */
    rect(x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2, 150, 105, 60);                                                                         /* wooden frame */
    for (int y = iy0; y < hz; y++) { int k = (y - iy0) * 255 / (hz - iy0 + 1); int sr, sg, sb; skycol(k, &sr, &sg, &sb); rect(ix0, y, ix0 + iw, y + 1, sr, sg, sb); }
    stars(ix0, iy0, iw, hz - iy0);   /* sky gradient */
    body(ix0, iy0, iw, hz - iy0);                                                                                                        /* sun or moon */
    for (int c = 0; c < (g_f > 0.2 ? 3 : 0); c++) { int cx = ix0 + (int)((T / 160 * (c + 1) + c * 97) % (iw + 60)) - 30, cy = iy0 + 14 + c * 14;                       /* clouds drift left to right, wrapping */
        for (int dy = -4; dy <= 4; dy++) for (int dx = -14; dx <= 14; dx++) if ((dx * dx) / 3 + dy * dy * 3 <= 40) { int X = cx + dx, Y = cy + dy; if (X >= ix0 && X < ix0 + iw && Y >= iy0 && Y < hz) px(X, Y, 250, 252, 255); } }
    for (int x = 0; x < iw; x++) { int hh = 7 + (int)(6 * sin(x * 0.045) + 4 * sin(x * 0.11 + 1.3)); rect(ix0 + x, hz - hh, ix0 + x + 1, hz, dim(88), dim(150 + (x % 7)), dim(96)); }   /* far hills */
    for (int y = hz; y < iy0 + ih; y++) { int d = y - hz, base = 60 - d / 2; rect(ix0, y, ix0 + iw, y + 1, dim(50), dim(120 + base / 2), dim(200 - d / 2) + (g_f < 0.2 ? 18 : 0));                                  /* water, darker toward us */
        for (int x = 0; x < iw; x++) if (((x * 3 + d * 7 + (int)(T / 200)) % 23) < 2 + d / 8) px(ix0 + x, y, dim(190), dim(225), dim(250) + (g_f < 0.2 ? 10 : 0)); }                                             /* moving wave glints */
    for (int b = 0; b < (g_f > 0.3 ? 3 : 0); b++) { int bx = ix0 + (int)((T / 55 * (b + 2) / 2 + b * 70) % (iw + 40)) - 20, by = iy0 + 10 + b * 11 + (int)(4 * sin(T * 0.004 + b)); int up = (T / 250 + b) % 2;   /* birds */
        for (int k = 0; k <= 4; k++) { int dy = up ? -(4 - k) / 2 : (4 - k) / 2; px(bx - k, by + dy, 40, 40, 50); px(bx + k, by + dy, 40, 40, 50); } px(bx, by + 1, 40, 40, 50);
        if (bx < ix0 + 6 || bx > ix0 + iw - 6) { rect(ix0, by - 6, ix0 + 1, by + 6, 0, 0, 0); } }
    rect(ix0 + iw / 2 - 2, iy0, ix0 + iw / 2 + 2, iy0 + ih, 150, 105, 60); rect(ix0, iy0 + ih / 2 - 2, ix0 + iw, iy0 + ih / 2 + 2, 150, 105, 60);                              /* cross bars */
    rect(x0 - 8, y0 + h + 2, x0 + w + 8, y0 + h + 8, 200, 165, 110); (void)floor_y;                                                                                       /* sill */
}

static void door_at(int x0, int floor_y, int lit) {                                                                                       /* a door 40 wide; lit = a light over it */
    rect(x0, floor_y - 74, x0 + 40, floor_y, 90, 60, 40); rect(x0 + 4, floor_y - 70, x0 + 36, floor_y, 125, 88, 56); rect(x0 + 28, floor_y - 36, x0 + 31, floor_y - 31, 250, 220, 80);
    if (lit) rect(x0 + 14, floor_y - 80, x0 + 26, floor_y - 77, 255, 240, 160);
}
/* one door per DOOR row of this room (rooms.pdl, next to the app): x is the door centre; a lit door leads outside (village) */
static void doors_for(const char *me, int floor_y) {
    FILE *f = pdl_open("rooms"); if (!f) return; char l[240];
    while (fgets(l, sizeof l, f)) { char rm[32], ds[32]; int dx = 0, ar = 0; if (sscanf(l, "DOOR | %31s | %d | %31s | %d", rm, &dx, ds, &ar) == 4 && !strcmp(rm, me)) door_at(dx - 20, floor_y, !strcmp(ds, "village")); }
    fclose(f);
}
static void computer_desk(int tx, int floor_y) {                                                                                           /* monitor, keyboard, tower on a desk */
    rect(tx, floor_y - 34, tx + 96, floor_y - 29, 120, 84, 52); rect(tx + 3, floor_y - 29, tx + 9, floor_y, 96, 66, 40); rect(tx + 87, floor_y - 29, tx + 93, floor_y, 96, 66, 40);
    rect(tx + 28, floor_y - 44, tx + 44, floor_y - 34, 70, 70, 78);
    rect(tx + 8, floor_y - 88, tx + 76, floor_y - 44, 50, 52, 60); rect(tx + 12, floor_y - 84, tx + 72, floor_y - 48, 20, 30, 56);
    { long long cq = qtime_ms() / 500; for (int l = 0; l < 5; l++) { int len = 10 + ((l * 13 + 7) % 34); rect(tx + 16, floor_y - 80 + l * 7, tx + 16 + len, floor_y - 77 + l * 7, 110, 220, 150); }
      if (cq % 2) rect(tx + 16, floor_y - 51, tx + 22, floor_y - 48, 240, 240, 120); }
    rect(tx + 10, floor_y - 38, tx + 74, floor_y - 34, 200, 200, 208);
    rect(tx + 78, floor_y - 62, tx + 92, floor_y - 34, 218, 212, 196); rect(tx + 82, floor_y - 58, tx + 88, floor_y - 56, 80, 200, 90);
}
static void fridge_at(int fx0, int floor_y) {
    rect(fx0, floor_y - 120, fx0 + 60, floor_y, 220, 225, 230); frame(fx0, floor_y - 120, fx0 + 60, floor_y, 120, 125, 135);
    rect(fx0, floor_y - 74, fx0 + 60, floor_y - 71, 120, 125, 135); rect(fx0 + 48, floor_y - 108, fx0 + 52, floor_y - 84, 90, 95, 105); rect(fx0 + 48, floor_y - 66, fx0 + 52, floor_y - 40, 90, 95, 105);
}
/* rid 0 = the bedroom (door to the village at the left, bed, big window, computer, door to the living room at the right); rid 1 = the living room (door back to the bedroom, fridge, sofa, TV).
 * The doors are TELEPORT events (rooms.pdl lists them: x, destination, arrival x); the pet walks to one on its own and the event moves it. */
static void room(const char *pd, int pxx, int pyy, const char *anim, int fi, int rid, const char *loc) {
    int floor_y = H - 30;
    if (rid == 2) {                                                                                                                         /* the garden: sky, hills, a house wall with the door, crop rows */
        for (int y = 0; y < floor_y - 40; y++) { int k = y * 255 / (floor_y - 40), sr, sg, sb; skycol(k, &sr, &sg, &sb); rect(0, y, W, y + 1, sr, sg, sb); }
        stars(0, 0, W, floor_y - 40); body(0, 0, W, floor_y - 40);
        for (int x = 0; x < W; x++) { int hh = 24 + (int)(10 * sin(x * 0.03) + 6 * sin(x * 0.09 + 1)); rect(x, floor_y - 40 - hh, x + 1, floor_y - 40, dim(90), dim(150 + (x % 9)), dim(100)); }
        rect(0, floor_y - 40, W, floor_y, dim(96), dim(160), dim(84));
        rect(0, 0, 76, floor_y, 205, 182, 146); rect(0, floor_y - 52, 76, floor_y - 48, 160, 128, 96);                                       /* the house wall the door is in */
        for (int r = 0; r < 3; r++) { int ry = floor_y - 34 + r * 9; rect(110, ry, W - 20, ry + 5, 110, 78, 52);
            for (int c = 0; c < 9; c++) { int cx = 118 + c * 24, gr = (int)((qtime_ms() / 4000 + c + r * 3) % 4); rect(cx, ry - 2 - gr * 2, cx + 3, ry, 60, 150 + gr * 20, 70); if (gr == 3) rect(cx - 2, ry - 9, cx + 5, ry - 6, 230, 90, 60); } }   /* soil rows with growing crops */
        for (int x = 96; x < W; x += 14) { rect(x, floor_y - 52, x + 3, floor_y - 40, 180, 140, 90); } rect(96, floor_y - 48, W, floor_y - 45, 180, 140, 90);   /* fence */
        doors_for(loc, floor_y);
        char p2[1536]; snprintf(p2, sizeof p2, "%s/art/sprites_hi/%s_%02d/sprite.csv", pd, anim, fi); sprite(p2, pxx, pyy, 2, 1); return;
    }
    if (rid == 4) {                                                                                                                         /* the rooftop: the village (world_map.txt) seen from above, a house flag per pet, the roof ledge in front */
        for (int y = 0; y < floor_y; y++) { int k = y * 255 / floor_y, sr, sg, sb; skycol(k, &sr, &sg, &sb); rect(0, y, W, y + 1, sr, sg, sb); }
        stars(0, 0, W, 60); body(0, 0, W, 90);
        char mp[PATH_MAX]; snprintf(mp, sizeof mp, "%s/world_map.txt", g_app); FILE *mf = fopen(mp, "r"); int T = 9, ox = 36, oy = 26, ry = 0, flag = 0; const int pc[6][3] = {{240,120,150},{230,200,90},{110,170,240},{150,220,120},{200,140,230},{240,160,90}};
        if (mf) { char l[128]; while (fgets(l, sizeof l, mf) && ry < 24) { l[strcspn(l, "\r\n")] = 0;
            for (int x = 0; l[x] && x < 40; x++) { int X = ox + x * T, Y = oy + ry * T; char c = l[x]; int r = 120, g = 190, b = 100;
                if (c == 'T') { r = 70; g = 140; b = 70; } else if (c == 'W') { r = 70; g = 130; b = 210; if (((x + ry + (int)(qtime_ms() / 800)) & 1) == 0) { r = 110; g = 170; b = 235; } }
                else if (c == '=') { r = 205; g = 180; b = 130; } else if (c == '^') { r = 190; g = 80; b = 70; } else if (c == '#') { r = 235; g = 225; b = 195; }
                else if (c == 'D' || c == 'd') { r = 150; g = 95; b = 55; } else if (c == ',') { r = 240; g = 150; b = 170; }
                rect(X, Y, X + T, Y + T, dim(r), dim(g), dim(b));
                if ((c == 'D' || c == 'd') && flag < 6) { int fx = X + T / 2; rect(fx, Y - 18, fx + 1, Y, 90, 90, 90); rect(fx + 1, Y - 18, fx + 8, Y - 12, pc[flag][0], pc[flag][1], pc[flag][2]); flag++; }       /* a flag on each pet's door */
            } ry++; } fclose(mf); }
        rect(0, floor_y - 14, W, floor_y, dim(150), dim(140), dim(130)); rect(0, floor_y - 14, W, floor_y - 12, dim(185), dim(175), dim(165));                 /* the parapet */
        rect(0, floor_y, W, H, dim(96), dim(92), dim(100)); for (int x = 0; x < W; x += 36) rect(x, floor_y, x + 1, H, dim(70), dim(66), dim(74));            /* roof tiles */
        rect(168, 214, 214, floor_y, dim(150), dim(80), dim(70)); rect(164, 210, 218, 216, dim(110), dim(60), dim(55));                                     /* the chimney */
        doors_for(loc, floor_y);
        char p4[1536]; snprintf(p4, sizeof p4, "%s/art/sprites_hi/%s_%02d/sprite.csv", pd, anim, fi); sprite(p4, pxx, pyy, 2, 1); return;
    }
    if (rid == 3) {                                                                                                                         /* a room the pet built: plain walls, a rug and a lamp (furniture comes with items later) */
        rect(0, 0, W, floor_y, 168, 190, 176); for (int y = 0; y < floor_y; y += 24) rect(0, y, W, y + 1, 156, 178, 164);
        rect(0, floor_y, W, H, 150, 110, 70); for (int x = 0; x < W; x += 40) rect(x, floor_y, x + 1, H, 130, 95, 60);
        doors_for(loc, floor_y); rect(110, floor_y - 6, 250, floor_y, 120, 150, 190); rect(300, floor_y - 70, 304, floor_y, 90, 90, 100); rect(290, floor_y - 84, 314, floor_y - 66, 250, 230, 140);
        char p3[1536]; snprintf(p3, sizeof p3, "%s/art/sprites_hi/%s_%02d/sprite.csv", pd, anim, fi); sprite(p3, pxx, pyy, 2, 1); return;
    }
    if (rid == 1) { rect(0, 0, W, floor_y, 205, 182, 146); rect(0, floor_y - 52, W, floor_y - 48, 160, 128, 96); for (int y = 0; y < floor_y - 52; y += 24) rect(0, y, W, y + 1, 195, 172, 136); }
    else { rect(0, 0, W, floor_y, 120, 150, 190); for (int y = 0; y < floor_y; y += 24) rect(0, y, W, y + 1, 110, 140, 180); }
    rect(0, floor_y, W, H, 150, 110, 70);
    for (int x = 0; x < W; x += 40) rect(x, floor_y, x + 1, H, 130, 95, 60);
    if (rid == 0) {
        big_window(116, 24, 190, 120, floor_y);
        int bx = 62; rect(bx, floor_y - 34, bx + 96, floor_y - 10, 70, 90, 170); rect(bx, floor_y - 10, bx + 96, floor_y, 110, 80, 50);
        rect(bx, floor_y - 52, bx + 10, floor_y, 110, 80, 50); rect(bx + 6, floor_y - 44, bx + 34, floor_y - 34, 240, 240, 240);        /* bed */
        computer_desk(206, floor_y);
        doors_for(loc, floor_y);                                                                                                             /* every door in rooms.pdl for the bedroom */
    } else {
        doors_for(loc, floor_y);
        fridge_at(66, floor_y);                                                                                                            /* the kitchen corner */
        rect(130, floor_y - 6, 262, floor_y, 150, 70, 70);                                                                                 /* rug */
        rect(140, floor_y - 40, 240, floor_y - 12, 150, 50, 60); rect(134, floor_y - 52, 246, floor_y - 40, 120, 38, 48); rect(134, floor_y - 40, 146, floor_y - 12, 120, 38, 48); rect(234, floor_y - 40, 246, floor_y - 12, 120, 38, 48);   /* sofa */
        rect(252, floor_y - 30, 312, floor_y, 100, 72, 44);                                                                                /* TV stand */
        rect(256, floor_y - 74, 308, floor_y - 30, 30, 30, 34); { long long cq = qtime_ms() / 500; for (int k = 0; k < 5; k++) rect(260 + k * 10, floor_y - 70, 270 + k * 10, floor_y - 34, 60 + ((k + (int)cq) % 6) * 30, 90 + ((k * 2 + (int)cq) % 5) * 28, 160 + ((k + 2 * (int)cq) % 4) * 20); }   /* TV: moving colour bars */
        rect(112, 40, 150, 76, 90, 60, 40); rect(116, 44, 146, 72, 150, 200, 230); rect(118, 58, 144, 72, 90, 150, 100);                 /* a picture on the wall */
        rect(104, floor_y - 40, 112, floor_y, 90, 150, 80); rect(98, floor_y - 70, 118, floor_y - 40, 70, 160, 80);                      /* plant */
    }
    char p[1536]; snprintf(p, sizeof p, "%s/art/sprites_hi/%s_%02d/sprite.csv", pd, anim, fi); sprite(p, pxx, pyy, 2, 1);
}

/* debug mini map (key 5): the doll house grid from home.pdl, doors from rooms.pdl as links, the pet's room lit. */
static void house_map(const char *loc) {
    rect(0, 0, W, H, 18, 24, 40); rect(0, 0, W, 22, 20, 30, 60); rect(0, 22, W, 24, 250, 210, 80);
    FILE *f = pdl_open("home"); if (!f) return;
    char ids[24][32]; int cx[24], cy[24], n = 0, maxx = 0, maxy = 0, sc[24]; char l[200];
    FILE *rf = pdl_open("rooms");
    while (fgets(l, sizeof l, f)) { char id[32], k[32]; int x, y; if (n < 24 && sscanf(l, "CELL | %31s | %d | %d | %31s", id, &x, &y, k) == 4) { snprintf(ids[n], 32, "%s", id); cx[n] = x; cy[n] = y; sc[n] = -1; if (x > maxx) maxx = x; if (y > maxy) maxy = y; n++; } }
    fclose(f);
    if (rf) { while (fgets(l, sizeof l, rf)) { char id[32]; int s; if (sscanf(l, "ROOM | %31s | %d", id, &s) == 2) for (int i = 0; i < n; i++) if (!strcmp(ids[i], id)) sc[i] = s; } }
    int miny = 0; for (int i = 0; i < n; i++) if (cy[i] < miny) miny = cy[i]; int cw = (W - 40) / (maxx + 1), ch = 62; if (cw > 100) cw = 100; int ox = (W - cw * (maxx + 1)) / 2, oy = 40 - miny * 0;
    for (int i = 0; i < n; i++) { int x0 = ox + cx[i] * cw + 4, y0 = oy + (cy[i] - miny) * (ch + 10), x1 = x0 + cw - 8, y1 = y0 + ch, lit = !strcmp(ids[i], loc);
        rect(x0, y0, x1, y1, lit ? 90 : 40, lit ? 120 : 56, lit ? 190 : 100); frame(x0, y0, x1, y1, lit ? 255 : 120, lit ? 225 : 130, lit ? 90 : 160);
        if (lit) rect(x0 + cw / 2 - 10, y1 - 22, x0 + cw / 2 - 2, y1 - 6, 255, 200, 90); }                                                       /* the pet marker */
    if (rf) { rewind(rf); while (fgets(l, sizeof l, rf)) { char rm[32], ds[32]; int dx, ar; if (sscanf(l, "DOOR | %31s | %d | %31s | %d", rm, &dx, ds, &ar) == 4) { int a = -1, b = -1; for (int i = 0; i < n; i++) { if (!strcmp(ids[i], rm)) a = i; if (!strcmp(ids[i], ds)) b = i; }
        if (a >= 0 && b >= 0 && cy[a] == cy[b] && a < b) { int xa = ox + cx[a] * cw + cw - 6, xb = ox + cx[b] * cw + 6, y = oy + (cy[a] - miny) * (ch + 10) + ch - 14; rect(xa, y - 2, xb, y + 2, 250, 210, 80); } } } fclose(rf); }   /* door links between side-by-side rooms */
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
    if (argc < 7) { fprintf(stderr, "usage: pet_scene room|manage|world ...\n"); return 2; }
    const char *mode = argv[1], *dir = argv[2], *out = argv[3]; W = atoi(argv[4]); H = atoi(argv[5]);
    if (W < 64 || H < 64 || W > 1200 || H > 900) return 2;
    fb = calloc((size_t)W * H, 4); if (!fb) return 1; { char self[PATH_MAX]; if (realpath(argv[0], self)) { snprintf(g_app, sizeof g_app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(g_app, '/'); if (s) *s = 0; } } }
    char self[PATH_MAX]; char app[PATH_MAX] = "."; if (realpath(argv[0], self)) { snprintf(app, sizeof app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(app, '/'); if (s) *s = 0; } }
    { const char *sh0 = getenv("PET_SHARED"); if (sh0 && sh0[0]) snprintf(g_shared, sizeof g_shared, "%s", sh0); else snprintf(g_shared, sizeof g_shared, "%s/state", g_app); }
    load_phase();
    if (!strcmp(mode, "room") && argc >= 10) room(dir, atoi(argv[6]), atoi(argv[7]), argv[8], atoi(argv[9]) & 7, argc >= 11 ? atoi(argv[10]) : 0, argc >= 12 ? argv[11] : "bedroom");
    else if (!strcmp(mode, "manage")) manage(dir, argv[6], atoi(argv[7]));
    else if (!strcmp(mode, "map") && argc >= 7) house_map(argv[6]);
    else if (!strcmp(mode, "world")) world(dir, app, argv[6], atoi(argv[7]));
    else return 2;
    { const char *zs = getenv("PET_SCENE_ZOOM"); int k = zs ? atoi(zs) : 1;        /* fullscreen: the picture is drawn at its small size and enlarged by a whole number (crisp pixels) */
      if (k > 1 && k <= 8) { unsigned char *big = malloc((size_t)W * k * H * k * 4); if (big) { for (int y = 0; y < H * k; y++) for (int x = 0; x < W * k; x++) memcpy(big + ((size_t)y * W * k + x) * 4, fb + ((size_t)(y / k) * W + x / k) * 4, 4); free(fb); fb = big; W *= k; H *= k; } } }
    { FILE *old = fopen(out, "rb"); if (old) { unsigned char *ob = malloc((size_t)W * H * 4 + 1); size_t got = ob ? fread(ob, 1, (size_t)W * H * 4 + 1, old) : 0; fclose(old);
        if (ob && got == (size_t)W * H * 4 && !memcmp(ob, fb, got)) { free(ob); free(fb); return 0; } free(ob); } }       /* identical frame: touch nothing, the renderer stays idle */
    FILE *o = fopen(out, "wb"); if (!o) return 1; fwrite(fb, 1, (size_t)W * H * 4, o); fclose(o);
    char rp[1536]; snprintf(rp, sizeof rp, "%s", out); char *dot = strrchr(rp, '.'); if (dot) *dot = 0; strcat(rp, ".receipt.txt");
    FILE *r = fopen(rp, "w"); if (r) { fprintf(r, "frame_w=%d\nframe_h=%d\n", W, H); fclose(r); }
    char mp[1536]; snprintf(mp, sizeof mp, "%s", out); char *sl = strrchr(mp, '/'); if (sl) *sl = 0; strcat(mp, "/scene_changed.txt");
    FILE *m = fopen(mp, "a"); if (m) { fputc('x', m); fclose(m); }
    free(fb); return 0;
}
