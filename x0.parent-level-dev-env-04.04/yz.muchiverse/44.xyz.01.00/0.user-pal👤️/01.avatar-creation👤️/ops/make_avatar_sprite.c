/* make_avatar_sprite.c - write the MC-style front-body sprite.csv for one
 * or more avatar dirs from their state.txt DNA (skin_index, hair_color,
 * shirt_color, pants_color).
 *
 * Same pixel layout avatar_window.c's synthesize_mc_front_sprite uses for
 * the desktop pet - this produces the identical sprite so the taskbar USER
 * cell shows the same full-body avatar (with its own skin/hair/clothes
 * colors) instead of a font emoji.
 *
 * Usage: make_avatar_sprite.+x <avatar_dir> [<avatar_dir> ...]
 * Each dir must contain state.txt; writes <dir>/sprite.csv
 * (# resolution=N, then N*N "r,g,b,a" rows).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MC_SPRITE_RES 64

static void mc_put(unsigned char *px, int res, int x, int y, int r, int g, int b, int a) {
    if (x < 0 || y < 0 || x >= res || y >= res) return;
    int i = (y * res + x) * 4;
    px[i+0] = (unsigned char)r; px[i+1] = (unsigned char)g;
    px[i+2] = (unsigned char)b; px[i+3] = (unsigned char)a;
}
static void mc_fill(unsigned char *px, int res, int x0, int y0, int x1, int y1,
                    int r, int g, int b) {
    if (x0 > x1) { int t=x0; x0=x1; x1=t; }
    if (y0 > y1) { int t=y0; y0=y1; y1=t; }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            mc_put(px, res, x, y, r, g, b, 255);
}
static void mc_skin_rgb(int idx, int *r, int *g, int *b) {
    switch (idx) {
    case 0: *r=245; *g=204; *b=176; break;
    case 1: *r=250; *g=222; *b=194; break;
    case 2: *r=230; *g=184; *b=148; break;
    case 3: *r=194; *g=140; *b=102; break;
    case 4: *r=140; *g=92;  *b=64;  break;
    case 5: *r=90;  *g=56;  *b=38;  break;
    default: *r=217; *g=166; *b=128; break;
    }
}
static void mc_name_rgb(const char *name, int *r, int *g, int *b) {
    *r = 80; *g = 80; *b = 80;
    if (!name || !name[0]) return;
    if (strstr(name, "black")) { *r=30;*g=28;*b=25; }
    else if (strstr(name, "brown")) { *r=110;*g=70;*b=30; }
    else if (strstr(name, "blonde") || strstr(name, "yellow")) { *r=230;*g=200;*b=70; }
    else if (strstr(name, "red")) { *r=190;*g=50;*b=40; }
    else if (strstr(name, "gray") || strstr(name, "grey")) { *r=140;*g=140;*b=140; }
    else if (strstr(name, "blue")) { *r=50;*g=90;*b=210; }
    else if (strstr(name, "pink")) { *r=230;*g=120;*b=160; }
    else if (strstr(name, "green")) { *r=50;*g=160;*b=70; }
    else if (strstr(name, "white")) { *r=235;*g=235;*b=240; }
    else if (strstr(name, "purple")) { *r=140;*g=70;*b=190; }
    else if (strstr(name, "orange")) { *r=230;*g=120;*b=30; }
    else if (strstr(name, "khaki")) { *r=180;*g=165;*b=100; }
}

static void read_kv_file(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* Build front-facing MC body into an RGBA buffer (res x res). Transparent outside. */
static int synthesize_mc_front_sprite(const char *state_path,
                                      unsigned char **out_pixels, int *out_res) {
    int res = MC_SPRITE_RES;
    unsigned char *px = (unsigned char *)calloc((size_t)res * (size_t)res * 4, 1);
    if (!px) return 0;

    char si[16] = "1", hair[32] = "brown", shirt[32] = "blue", pants[32] = "black";
    if (state_path && state_path[0]) {
        read_kv_file(state_path, "skin_index", si, sizeof(si));
        read_kv_file(state_path, "hair_color", hair, sizeof(hair));
        read_kv_file(state_path, "shirt_color", shirt, sizeof(shirt));
        read_kv_file(state_path, "pants_color", pants, sizeof(pants));
    }
    int sr, sg, sb, hr, hg, hb, tr, tg, tb, pr, pg, pb;
    mc_skin_rgb(atoi(si), &sr, &sg, &sb);
    mc_name_rgb(hair, &hr, &hg, &hb);
    mc_name_rgb(shirt, &tr, &tg, &tb);
    mc_name_rgb(pants, &pr, &pg, &pb);

    /* Pixel layout (y down): feet near bottom, head near top — front view. */
    /* legs */
    mc_fill(px, res, 22, 40, 30, 58, pr, pg, pb);
    mc_fill(px, res, 33, 40, 41, 58, (pr*9)/10, (pg*9)/10, (pb*9)/10);
    /* torso */
    mc_fill(px, res, 20, 22, 43, 39, tr, tg, tb);
    /* arms */
    mc_fill(px, res, 12, 22, 19, 39, sr, sg, sb);
    mc_fill(px, res, 44, 22, 51, 39, (sr*95)/100, (sg*95)/100, (sb*95)/100);
    /* sleeves */
    mc_fill(px, res, 12, 22, 19, 28, (tr*9)/10, (tg*9)/10, (tb*9)/10);
    mc_fill(px, res, 44, 22, 51, 28, (tr*9)/10, (tg*9)/10, (tb*9)/10);
    /* head */
    mc_fill(px, res, 22, 6, 41, 21, sr, sg, sb);
    /* hair top + bangs */
    mc_fill(px, res, 21, 4, 42, 10, hr, hg, hb);
    mc_fill(px, res, 21, 10, 42, 13, hr, hg, hb);
    /* eyes */
    mc_fill(px, res, 26, 12, 29, 14, 20, 20, 30);
    mc_fill(px, res, 34, 12, 37, 14, 20, 20, 30);
    /* mouth */
    mc_fill(px, res, 29, 17, 34, 18, 120, 70, 60);

    *out_pixels = px;
    *out_res = res;
    return 1;
}

static int write_sprite_csv(const char *dir, unsigned char *px, int res) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/sprite.csv", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# resolution=%d\n", res);
    for (int i = 0; i < res * res; i++) {
        fprintf(f, "%d,%d,%d,%d\n", px[i*4+0], px[i*4+1], px[i*4+2], px[i*4+3]);
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <avatar_dir> [<avatar_dir> ...]\n", argv[0]);
        return 1;
    }
    int ok = 0;
    for (int a = 1; a < argc; a++) {
        const char *dir = argv[a];
        char state_path[4352];
        snprintf(state_path, sizeof(state_path), "%s/state.txt", dir);
        unsigned char *px = NULL;
        int res = 0;
        if (!synthesize_mc_front_sprite(state_path, &px, &res)) {
            fprintf(stderr, "FAIL synth %s\n", dir);
            continue;
        }
        if (!write_sprite_csv(dir, px, res)) {
            fprintf(stderr, "FAIL write %s\n", dir);
            free(px);
            continue;
        }
        free(px);
        fprintf(stdout, "sprite: %s (%dx%d)\n", dir, res, res);
        ok++;
    }
    return ok > 0 ? 0 : 1;
}
