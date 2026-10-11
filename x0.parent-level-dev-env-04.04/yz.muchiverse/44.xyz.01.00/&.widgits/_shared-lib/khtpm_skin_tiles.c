/* khtpm_skin_tiles.c - the RPG Maker tile bar skin for plain-Xlib windows (text-include, prefix skt_).
 *
 * khtpm_draw_core.c draws the skin inside khtpm_core_render. A few windows draw with raw Xlib instead (the entity
 * window's Show Text popup - the book-stack Bible verse); this is the same idea for them: read bar_skin= from
 * <house>/#.desktop/hq_ui.pdl, find its SKIN row in &.widgits/taskbar-settings/bar_skins.pdl (left / middle / right
 * folders, each a 48px sprite.csv), and paint whole square blocks (left cap, repeated middle, right cap, no stretching)
 * into any rectangle of a Drawable. No state beyond the loaded tiles; skt_load() again after bar_skin changes.
 * Needs <X11/Xlib.h>, <stdio.h>, <stdlib.h>, <string.h>, <unistd.h>. */
#ifndef KHTPM_SKIN_TILES_C
#define KHTPM_SKIN_TILES_C

static int skt_on = 0;                       /* a skin is selected and all three tiles loaded */
static unsigned char *skt_px[3];             /* 48*48*4 RGBA, left / middle / right */
static int skt_res[3];

static unsigned char *skt_read_csv(const char *dir, int *res) {
    char path[1400], line[256];
    snprintf(path, sizeof(path), "%s/sprite.csv", dir);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int r = 0;
    while (fgets(line, sizeof(line), f)) if (strncmp(line, "# resolution=", 13) == 0) { r = atoi(line + 13); break; }
    if (r <= 0 || r > 256) { fclose(f); return NULL; }
    unsigned char *p = malloc((size_t)r * r * 4);
    int n = 0, a, b, c, d;
    while (p && n < r * r && fgets(line, sizeof(line), f))
        if (sscanf(line, "%d,%d,%d,%d", &a, &b, &c, &d) == 4) { p[n*4] = (unsigned char)a; p[n*4+1] = (unsigned char)b; p[n*4+2] = (unsigned char)c; p[n*4+3] = (unsigned char)d; n++; }
    fclose(f);
    if (!p || n != r * r) { free(p); return NULL; }
    *res = r;
    return p;
}

static void skt_load(const char *house) {
    char want[48] = "", path[1400], line[1400];
    for (int k = 0; k < 3; k++) { free(skt_px[k]); skt_px[k] = NULL; }
    skt_on = 0;
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "bar_skin=", 9) == 0) { snprintf(want, sizeof(want), "%s", line + 9); want[strcspn(want, "\r\n")] = 0; break; }
    fclose(f);
    if (!want[0]) return;
    snprintf(path, sizeof(path), "%s/&.widgits/taskbar-settings/bar_skins.pdl", house);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SKIN", 4) != 0) continue;
        char *fld[6]; int n = 0; char *p = line;
        while (n < 6) { fld[n++] = p; char *bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; }
        if (n < 6) continue;
        for (int i = 1; i < 6; i++) {
            while (*fld[i] == ' ') fld[i]++;
            size_t l = strcspn(fld[i], "\r\n"); while (l > 0 && fld[i][l-1] == ' ') l--; fld[i][l] = 0;
        }
        if (strcmp(fld[1], want) != 0) continue;
        int ok = 1;
        for (int k = 0; k < 3; k++) {
            char dir[1400]; snprintf(dir, sizeof(dir), "%s/%s", house, fld[3 + k]);
            skt_px[k] = skt_read_csv(dir, &skt_res[k]);
            if (!skt_px[k]) ok = 0;
        }
        skt_on = ok;
        break;
    }
    fclose(f);
}

/* one tile scaled (nearest) to px x px, alpha-over the flat background `bg` (0xRRGGBB), onto d at x,y */
static void skt_blit(Display *dpy, Drawable d, GC gc, int k, int x, int y, int px, unsigned int bg) {
    int screen = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, screen);
    unsigned long rm = vis->red_mask, gm = vis->green_mask, bm = vis->blue_mask;
    int rs = 0, gs = 0, bs = 0;
    while (rm && !(rm & (1UL << rs))) rs++;
    while (gm && !(gm & (1UL << gs))) gs++;
    while (bm && !(bm & (1UL << bs))) bs++;
    unsigned char *out = calloc((size_t)px * px, 4);
    if (!out) return;
    int res = skt_res[k];
    for (int j = 0; j < px; j++)
        for (int i = 0; i < px; i++) {
            const unsigned char *s = &skt_px[k][(((j * res) / px) * res + (i * res) / px) * 4];
            int a = s[3];
            int r = (s[0] * a + (int)((bg >> 16) & 255) * (255 - a)) / 255;
            int g = (s[1] * a + (int)((bg >> 8) & 255) * (255 - a)) / 255;
            int b = (s[2] * a + (int)(bg & 255) * (255 - a)) / 255;
            unsigned long w = ((unsigned long)r << rs) | ((unsigned long)g << gs) | ((unsigned long)b << bs);
            unsigned char *o = &out[(j * px + i) * 4];
            o[0] = (unsigned char)(w & 255); o[1] = (unsigned char)((w >> 8) & 255); o[2] = (unsigned char)((w >> 16) & 255); o[3] = (unsigned char)((w >> 24) & 255);
        }
    XImage *img = XCreateImage(dpy, vis, DefaultDepth(dpy, screen), ZPixmap, 0, (char *)out, px, px, 32, 0);
    if (!img) { free(out); return; }
    img->byte_order = LSBFirst;
    XPutImage(dpy, d, gc, img, 0, 0, x, y, px, px);
    XDestroyImage(img);
}

/* whole blocks (block = h) across w, centred; returns 1 when painted */
static int skt_bar(Display *dpy, Drawable d, GC gc, int x, int y, int w, int h, unsigned int bg) {
    if (!skt_on || h < 8) return 0;
    int n = w / h;
    if (n < 1) return 0;
    int x0 = x + (w - n * h) / 2;
    for (int i = 0; i < n; i++) skt_blit(dpy, d, gc, n == 1 ? 1 : (i == 0 ? 0 : (i == n - 1 ? 2 : 1)), x0 + i * h, y, h, bg);
    return 1;
}
#endif
