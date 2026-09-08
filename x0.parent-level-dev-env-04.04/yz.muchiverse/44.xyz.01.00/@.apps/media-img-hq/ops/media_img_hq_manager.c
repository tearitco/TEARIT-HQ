/* media_img_hq_manager.c — 2D image editor toy (HOW2_IMAGE.md).
 * media_img_hq_manager.+x <house_root> <package_dir> [id]
 * Publishes state/media_img_hq_ui.txt ; canvas.raw + receipt.
 * STROKE is the file-backed stand-in for canvas drag-paint. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#define KH_PLAT_IMPL
#include "../../../&.widgits/_shared-lib/kh_plat.h"

#define PL 4096
#define CW 320
#define CH 240
#define MAX_LAY 6

static char pkg_dir[PL];
static char g_msg[160] = "";
static char g_loaded[256] = "";
static char tool = 'B';
static int brush = 8;
static unsigned char fg[4] = {220, 60, 60, 255};
static unsigned char bg[4] = {40, 40, 48, 255};
static int active_lay = 0, n_lay = 3;
static unsigned char *layer[MAX_LAY];
static int vis[MAX_LAY] = {1, 1, 1, 1, 1, 1};
static unsigned char *comp;
static int dirty = 1, zoom = 100, panx = 0, pany = 0;

static void bye(int s){ (void)s; for (int i=0;i<MAX_LAY;i++) free(layer[i]); free(comp); _exit(0); }

static void put_px(unsigned char *L, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (x < 0 || y < 0 || x >= CW || y >= CH || !L) return;
    size_t o = ((size_t)y * CW + (size_t)x) * 4;
    L[o]=r; L[o+1]=g; L[o+2]=b; L[o+3]=a;
}

static void clear_layer(int i) {
    if (i < 0 || i >= MAX_LAY || !layer[i]) return;
    memset(layer[i], 0, (size_t)CW * CH * 4);
}

static void stamp(int cx, int cy, int rad, int erase) {
    unsigned char *L = layer[active_lay];
    if (!L) return;
    int r2 = rad * rad;
    for (int y = cy - rad; y <= cy + rad; y++)
        for (int x = cx - rad; x <= cx + rad; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy > r2) continue;
            if (erase) put_px(L, x, y, 0, 0, 0, 0);
            else put_px(L, x, y, fg[0], fg[1], fg[2], 255);
        }
}

static void fill_at(int sx, int sy) {
    unsigned char *L = layer[active_lay];
    if (!L || sx<0||sy<0||sx>=CW||sy>=CH) return;
    size_t o0 = ((size_t)sy * CW + (size_t)sx) * 4;
    unsigned char tr=L[o0], tg=L[o0+1], tb=L[o0+2], ta=L[o0+3];
    if (tr==fg[0] && tg==fg[1] && tb==fg[2] && ta==255) return;
    int *stk = malloc((size_t)CW * CH * sizeof(int) * 2);
    if (!stk) return;
    int sp = 0;
    stk[sp++]=sx; stk[sp++]=sy;
    while (sp > 0) {
        int y = stk[--sp], x = stk[--sp];
        if (x<0||y<0||x>=CW||y>=CH) continue;
        size_t o = ((size_t)y*CW+(size_t)x)*4;
        if (L[o]!=tr||L[o+1]!=tg||L[o+2]!=tb||L[o+3]!=ta) continue;
        put_px(L, x, y, fg[0], fg[1], fg[2], 255);
        stk[sp++]=x+1; stk[sp++]=y;
        stk[sp++]=x-1; stk[sp++]=y;
        stk[sp++]=x; stk[sp++]=y+1;
        stk[sp++]=x; stk[sp++]=y-1;
        if (sp > CW*CH*2 - 8) break;
    }
    free(stk);
}

static void rect_demo(void) {
    unsigned char *L = layer[active_lay];
    int x0 = 40 + (active_lay * 12), y0 = 40 + (active_lay * 8);
    for (int y = y0; y < y0 + 50; y++)
        for (int x = x0; x < x0 + 80; x++)
            put_px(L, x, y, fg[0], fg[1], fg[2], 220);
}

static void blit_rgba(const unsigned char *src, int sw, int sh, int scale) {
    if (scale < 1) scale = 1;
    int dw = sw * scale, dh = sh * scale;
    int ox = (CW - dw) / 2, oy = (CH - dh) / 2;
    unsigned char *L = layer[active_lay];
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        int sx = x / scale, sy = y / scale;
        const unsigned char *p = src + ((size_t)sy * sw + (size_t)sx) * 4;
        put_px(L, ox + x, oy + y, p[0], p[1], p[2], p[3]);
    }
}

static int load_sprite_csv(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    int res = 64;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') {
            if (sscanf(line, "# resolution=%d", &res) == 1 && res > 0 && res <= 512) continue;
            continue;
        }
        if (!strncmp(line, "r,g,b,a", 7) || !strncmp(line, "r,g,b", 5)) break;
        break;
    }
    size_t n = (size_t)res * (size_t)res;
    unsigned char *buf = calloc(n * 4, 1);
    if (!buf) { fclose(f); return 0; }
    int i = 0, r,g,b,a;
    while (i < (int)n && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        a = 255;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) < 3) continue;
        buf[i*4]=(unsigned char)r; buf[i*4+1]=(unsigned char)g;
        buf[i*4+2]=(unsigned char)b; buf[i*4+3]=(unsigned char)a;
        i++;
    }
    fclose(f);
    if (i < 8) { free(buf); return 0; }
    int scale = 3; if (res * scale > CW) scale = CW / res; if (scale < 1) scale = 1;
    blit_rgba(buf, res, res, scale);
    free(buf);
    return 1;
}

static int load_png(const char *path) {
    char cmd[PL], rawp[PL], dim[64];
    snprintf(rawp, sizeof(rawp), "%s/state/import.rgba", pkg_dir);
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 \"%s\"", path);
    FILE *p = popen(cmd, "r"); if (!p) return 0;
    if (!fgets(dim, sizeof(dim), p)) { pclose(p); return 0; }
    pclose(p);
    int w=0,h=0; if (sscanf(dim, "%d,%d", &w, &h) != 2 || w<=0 || h<=0 || w>2048 || h>2048) return 0;
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -v error -y -i \"%s\" -f rawvideo -pix_fmt rgba \"%s\"", path, rawp);
    if (system(cmd) != 0) return 0;
    FILE *f = fopen(rawp, "rb"); if (!f) return 0;
    size_t n = (size_t)w * (size_t)h * 4;
    unsigned char *buf = malloc(n); if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, n, f) != n) { free(buf); fclose(f); return 0; }
    fclose(f);
    int scale = 1;
    while (w * (scale+1) <= CW && h * (scale+1) <= CH) scale++;
    blit_rgba(buf, w, h, scale);
    free(buf);
    return 1;
}

static int load_image(const char *path) {
    const char *dot = strrchr(path, '.');
    int ok = 0;
    if (dot && (!strcasecmp(dot, ".csv") || !strcasecmp(dot, ".sprite.csv")))
        ok = load_sprite_csv(path);
    else
        ok = load_png(path);
    if (ok) {
        const char *base = strrchr(path, '/');
        snprintf(g_loaded, sizeof(g_loaded), "%s", base ? base+1 : path);
        snprintf(g_msg, sizeof(g_msg), "loaded %s", g_loaded);
    } else snprintf(g_msg, sizeof(g_msg), "load failed: %s", path);
    return ok;
}

static void demo_2d(void) {
    n_lay = 1; active_lay = 0;
    for (int i = 0; i < MAX_LAY; i++) { clear_layer(i); vis[i] = (i == 0); }
    char p[PL];
    snprintf(p, sizeof(p), "%s/media/cursword.sprite.csv", pkg_dir);
    if (load_sprite_csv(p)) { snprintf(g_loaded, sizeof(g_loaded), "cursword.sprite.csv"); return; }
    snprintf(p, sizeof(p), "%s/media/cursword.png", pkg_dir);
    if (load_png(p)) { snprintf(g_loaded, sizeof(g_loaded), "cursword.png"); return; }
    snprintf(g_loaded, sizeof(g_loaded), "(empty)");
}

static void composite_2d(unsigned char *D) {
    for (int y=0;y<CH;y++) for (int x=0;x<CW;x++) {
        int chk = ((x/8)+(y/8)) & 1;
        unsigned char cr = chk ? 48 : 36, cg = chk ? 48 : 36, cb = chk ? 52 : 40;
        for (int L=0; L<n_lay; L++) {
            if (!vis[L] || !layer[L]) continue;
            size_t o = ((size_t)y*CW+(size_t)x)*4;
            unsigned char a = layer[L][o+3];
            if (!a) continue;
            float t = a / 255.0f;
            cr = (unsigned char)(cr*(1-t) + layer[L][o]*t);
            cg = (unsigned char)(cg*(1-t) + layer[L][o+1]*t);
            cb = (unsigned char)(cb*(1-t) + layer[L][o+2]*t);
        }
        size_t o = ((size_t)y*CW+(size_t)x)*4;
        D[o]=cr; D[o+1]=cg; D[o+2]=cb; D[o+3]=255;
    }
}

static void write_canvas(void) {
    if (!comp) return;
    composite_2d(comp);
    char raw[PL], tmp[PL], rec[PL];
    snprintf(raw, sizeof(raw), "%s/state/canvas.raw", pkg_dir);
    snprintf(tmp, sizeof(tmp), "%s/state/canvas.raw.tmp", pkg_dir);
    snprintf(rec, sizeof(rec), "%s/state/canvas.receipt.txt", pkg_dir);
    FILE *f = fopen(tmp, "wb");
    if (f) { fwrite(comp, 1, (size_t)CW*CH*4, f); fclose(f); rename(tmp, raw); }
    FILE *r = fopen(rec, "w");
    if (r) { fprintf(r, "overlay_w=%d\noverlay_h=%d\n", CW, CH); fclose(r); }
}

static void write_ui(void) {
    char dir[PL], tmp[PL], dst[PL], canvas[PL];
    snprintf(dir, sizeof(dir), "%s/state", pkg_dir); kh_plat_mkdir_p(dir);
    snprintf(dst, sizeof(dst), "%s/media_img_hq_ui.txt", dir);
    snprintf(tmp, sizeof(tmp), "%s/media_img_hq_ui.txt.tmp", dir);
    snprintf(canvas, sizeof(canvas), "%s/state/canvas.raw", pkg_dir);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "tB=%s\ntE=%s\ntG=%s\ntR=%s\ntI=%s\ntH=%s\n",
            tool=='B'?"active":"", tool=='E'?"active":"", tool=='G'?"active":"",
            tool=='R'?"active":"", tool=='I'?"active":"", tool=='H'?"active":"");
    fprintf(f, "canvas_raw=%s\n", canvas);
    fprintf(f, "n_items=%d\n", n_lay);
    for (int i=0;i<n_lay;i++) {
        fprintf(f, "it_%d_name=%c L%d%s\n", i, vis[i]?'*':'-', i+1, i==active_lay?"  (active)":"");
        fprintf(f, "it_%d_cls=%s\n", i, i==active_lay ? "active" : "");
    }
    fprintf(f, "status_line=2D  %s  tool=%c  brush=%d  zoom=%d%%  pan=%d,%d  fg=%d,%d,%d\n",
            g_loaded, tool, brush, zoom, panx, pany, fg[0], fg[1], fg[2]);
    {
        const char *now = "brush";
        if (tool=='E') now = "eraser";
        else if (tool=='G') now = "fill";
        else if (tool=='R') now = "rect";
        else if (tool=='I') now = "eyedrop";
        else if (tool=='H') now = "hand";
        fprintf(f, "gutter=B brush   E eraser   G fill   R rect   I eyedrop   H hand     now: %s\n", now);
    }
    fprintf(f, "msg=%s\n", g_msg);
    fclose(f); rename(tmp, dst);
}

static void apply_stroke(void) {
    int cx = CW/2 + panx, cy = CH/2 + pany;
    if (tool=='B') stamp(cx, cy, brush, 0);
    else if (tool=='E') stamp(cx, cy, brush, 1);
    else if (tool=='G') fill_at(cx, cy);
    else if (tool=='R') rect_demo();
    else if (tool=='I' && comp) {
        size_t o = ((size_t)cy*CW+(size_t)cx)*4;
        fg[0]=comp[o]; fg[1]=comp[o+1]; fg[2]=comp[o+2];
    }
    dirty = 1;
}

static void handle(const char *cmd) {
    if (!cmd[0]) return;
    snprintf(g_msg, sizeof(g_msg), "%s", cmd);
    if (!strcmp(cmd, "NEW")) { n_lay=1; active_lay=0; clear_layer(0); vis[0]=1; g_loaded[0]=0; dirty=1; }
    else if (!strcmp(cmd, "DEMO")) { demo_2d(); dirty=1; }
    else if (!strncmp(cmd, "LOAD:", 5)) {
        if (n_lay < 1) { n_lay=1; active_lay=0; vis[0]=1; }
        load_image(cmd+5); dirty=1;
    }
    else if (!strcmp(cmd, "EXPORT"))
        snprintf(g_msg, sizeof(g_msg), "export: state/canvas.raw (ffmpeg png later)");
    else if (!strncmp(cmd, "TOOL:", 5) && cmd[5] && !cmd[6]) tool = cmd[5];
    else if (!strcmp(cmd, "BRUSH:+")) { brush += 2; if (brush>48) brush=48; }
    else if (!strcmp(cmd, "BRUSH:-")) { brush -= 2; if (brush<1) brush=1; }
    else if (!strcmp(cmd, "SWAP_FG")) { unsigned char t[4]; memcpy(t,fg,4); memcpy(fg,bg,4); memcpy(bg,t,4); }
    else if (!strcmp(cmd, "STROKE")) apply_stroke();
    else if (!strcmp(cmd, "PAN:L")) { panx -= 8; }
    else if (!strcmp(cmd, "PAN:R")) { panx += 8; }
    else if (!strcmp(cmd, "ZOOM:+")) { zoom += 10; if (zoom>400) zoom=400; }
    else if (!strcmp(cmd, "ZOOM:-")) { zoom -= 10; if (zoom<25) zoom=25; }
    else if (!strncmp(cmd, "PICK:", 5)) { int i=atoi(cmd+5); if (i>=0&&i<n_lay) active_lay=i; }
    else if (!strncmp(cmd, "VIS:", 5)) { int i=atoi(cmd+5); if (i>=0&&i<n_lay) vis[i]=!vis[i]; dirty=1; }
    else if (!strcmp(cmd, "NEW_LAYER") && n_lay < MAX_LAY) {
        clear_layer(n_lay); vis[n_lay]=1; active_lay=n_lay; n_lay++; dirty=1;
    } else if (!strcmp(cmd, "CLEAR_LAYER")) { clear_layer(active_lay); dirty=1; }
}

static void poll_action(int *last) {
    char p[PL]; snprintf(p, sizeof(p), "%s/state/media_img_hq_action.txt", pkg_dir);
    FILE *f = fopen(p, "r"); if (!f) return;
    char line[PL], cmd[1024]=""; int seq=0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "seq=%d", &seq)==1) continue;
        if (!strncmp(line, "cmd=", 4)) { snprintf(cmd, sizeof(cmd), "%s", line+4); char *n=strchr(cmd,'\n'); if(n)*n=0; }
    }
    fclose(f);
    if (seq==*last || !cmd[0]) return;
    *last = seq;
    handle(cmd);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: media_img_hq_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(pkg_dir, sizeof(pkg_dir), "%s", argv[2]);
    kh_plat_on_terminate(bye);
    for (int i=0;i<MAX_LAY;i++) layer[i] = calloc((size_t)CW*CH*4, 1);
    comp = calloc((size_t)CW*CH*4, 1);
    char sd[PL]; snprintf(sd, sizeof(sd), "%s/state", pkg_dir); kh_plat_mkdir_p(sd);
    { char ap[PL]; snprintf(ap, sizeof(ap), "%s/state/media_img_hq_action.txt", pkg_dir);
      FILE *a=fopen(ap,"w"); if(a){ fputs("seq=0\ncmd=\n",a); fclose(a); } }
    demo_2d(); write_canvas(); write_ui();
    int last=0;
    for (;;) {
        kh_plat_sleep_ms(dirty ? 80 : 200);
        poll_action(&last);
        if (dirty) { write_canvas(); dirty=0; }
        write_ui();
    }
    return 0;
}
