/* media_img3d_hq_manager.c — combined 2D image + 3D viewport toy.
 * Forked as: media_img3d_hq_manager.+x <house_root> <package_dir> [id]
 * Publishes state/media_img3d_hq_ui.txt ; polls state/media_img3d_hq_action.txt
 * Writes state/canvas.raw + canvas.receipt.txt for <canvas sprite=>.
 * No renderer C. HOW2_IMAGE + HOW2_BLEND features that fit file-backed
 * actions live here; canvas drag-paint still needs generic canvas click
 * (not invented this pass — STROKE is the file-backed stand-in). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#define KH_PLAT_IMPL
#include "../../../&.widgits/_shared-lib/kh_plat.h"

#define PL 4096
#define CW 320
#define CH 240
#define MAX_LAY 6
#define MAX_OBJ 9

static char pkg_dir[PL];
static char g_msg[160] = "";
static int mode3d = 0; /* 0=2D 1=3D */
static char tool2d = 'B';
static char tool3d[8] = "SEL";
static int brush = 8;
static unsigned char fg[4] = {220, 60, 60, 255};
static unsigned char bg[4] = {40, 40, 48, 255};
static int active_lay = 0;
static int n_lay = 3;
static unsigned char *layer[MAX_LAY];
static int vis[MAX_LAY] = {1, 1, 1, 1, 1, 1};
static unsigned char *comp;

typedef struct { char name[24]; float x, y, z, s; int kind; } Obj; /* 0 cube 1 sph 2 ground */
static Obj objs[MAX_OBJ];
static int n_obj = 0;
static int sel_obj = 0;
static float cam_yaw = 0.6f, cam_pit = 0.35f, cam_dist = 8.0f, cam_panx = 0, cam_pany = 0;
static int dirty = 1;
static int zoom2d = 100;
static int panx = 0, pany = 0;

static void bye(int s){ (void)s; for (int i=0;i<MAX_LAY;i++) free(layer[i]); free(comp); _exit(0); }

static unsigned char *alloc_layer(void) {
    unsigned char *p = calloc((size_t)CW * CH * 4, 1);
    return p;
}

static void clear_layer(int i) {
    if (i < 0 || i >= MAX_LAY || !layer[i]) return;
    memset(layer[i], 0, (size_t)CW * CH * 4);
}

static void put_px(unsigned char *L, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (x < 0 || y < 0 || x >= CW || y >= CH || !L) return;
    size_t o = ((size_t)y * CW + (size_t)x) * 4;
    L[o]=r; L[o+1]=g; L[o+2]=b; L[o+3]=a;
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
    if (!L) return;
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

static void demo_2d(void) {
    n_lay = 3; active_lay = 0;
    for (int i = 0; i < MAX_LAY; i++) { clear_layer(i); vis[i] = (i < 3); }
    unsigned char *A = layer[0], *B = layer[1], *C = layer[2];
    for (int y = 30; y < 120; y++)
        for (int x = 30; x < 160; x++) put_px(A, x, y, 200, 70, 70, 255);
    for (int y = 80; y < 180; y++)
        for (int x = 100; x < 230; x++) put_px(B, x, y, 70, 160, 90, 200);
    stamp(220, 70, 28, 0);
    (void)C;
}

static void demo_3d(void) {
    n_obj = 3; sel_obj = 0;
    snprintf(objs[0].name, sizeof(objs[0].name), "Cube");
    objs[0].x=0; objs[0].y=0.5f; objs[0].z=0; objs[0].s=1; objs[0].kind=0;
    snprintf(objs[1].name, sizeof(objs[1].name), "Sphere");
    objs[1].x=2.2f; objs[1].y=0.6f; objs[1].z=0.4f; objs[1].s=0.7f; objs[1].kind=1;
    snprintf(objs[2].name, sizeof(objs[2].name), "Ground");
    objs[2].x=0; objs[2].y=0; objs[2].z=0; objs[2].s=4; objs[2].kind=2;
    cam_yaw=0.6f; cam_pit=0.35f; cam_dist=8.0f; cam_panx=0; cam_pany=0;
}

static void project(float x, float y, float z, int *ox, int *oy) {
    float cy = cosf(cam_yaw), sy = sinf(cam_yaw);
    float cp = cosf(cam_pit), sp = sinf(cam_pit);
    float x1 = x * cy + z * sy;
    float z1 = -x * sy + z * cy;
    float y1 = y * cp - z1 * sp;
    float z2 = y * sp + z1 * cp;
    z2 += cam_dist;
    if (z2 < 0.2f) z2 = 0.2f;
    float s = 140.0f / z2;
    *ox = (int)(CW/2 + (x1 + cam_panx) * s);
    *oy = (int)(CH/2 - (y1 + cam_pany) * s);
}

static void line3(unsigned char *D, float x0,float y0,float z0, float x1,float y1,float z1, unsigned char r,unsigned char g,unsigned char b) {
    int ax,ay,bx,by; project(x0,y0,z0,&ax,&ay); project(x1,y1,z1,&bx,&by);
    int dx = abs(bx-ax), sx = ax<bx?1:-1;
    int dy = -abs(by-ay), sy = ay<by?1:-1;
    int err = dx+dy;
    for (;;) {
        put_px(D, ax, ay, r, g, b, 255);
        if (ax==bx && ay==by) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; ax += sx; }
        if (e2 <= dx) { err += dx; ay += sy; }
    }
}

static void render_3d(unsigned char *D) {
    memset(D, 18, (size_t)CW*CH*4);
    for (int i=0;i<CW*CH;i++) D[i*4+3]=255;
    /* grid */
    for (int g = -4; g <= 4; g++) {
        line3(D, (float)g, 0, -4, (float)g, 0, 4, 50, 50, 58);
        line3(D, -4, 0, (float)g, 4, 0, (float)g, 50, 50, 58);
    }
    line3(D, 0,0,0, 2,0,0, 180,40,40);
    line3(D, 0,0,0, 0,2,0, 40,180,40);
    line3(D, 0,0,0, 0,0,2, 40,80,180);
    for (int i = 0; i < n_obj; i++) {
        Obj *o = &objs[i];
        unsigned char r=180,g=180,b=190;
        if (i==sel_obj) { r=255; g=210; b=80; }
        if (o->kind==2) {
            float s=o->s;
            line3(D, o->x-s, o->y, o->z-s, o->x+s, o->y, o->z-s, r,g,b);
            line3(D, o->x+s, o->y, o->z-s, o->x+s, o->y, o->z+s, r,g,b);
            line3(D, o->x+s, o->y, o->z+s, o->x-s, o->y, o->z+s, r,g,b);
            line3(D, o->x-s, o->y, o->z+s, o->x-s, o->y, o->z-s, r,g,b);
        } else if (o->kind==1) {
            float s=o->s;
            for (int k=0;k<12;k++) {
                float a0 = (float)k * 6.28318f / 12.0f;
                float a1 = (float)(k+1) * 6.28318f / 12.0f;
                line3(D, o->x+cosf(a0)*s, o->y, o->z+sinf(a0)*s,
                         o->x+cosf(a1)*s, o->y, o->z+sinf(a1)*s, r,g,b);
                line3(D, o->x+cosf(a0)*s, o->y+sinf(a0)*s, o->z,
                         o->x+cosf(a1)*s, o->y+sinf(a1)*s, o->z, r,g,b);
            }
        } else {
            float s=o->s*0.5f;
            float xs[2]={o->x-s,o->x+s}, ys[2]={o->y-s,o->y+s}, zs[2]={o->z-s,o->z+s};
            for (int a=0;a<2;a++) for (int b=0;b<2;b++) {
                line3(D, xs[0], ys[a], zs[b], xs[1], ys[a], zs[b], r,g,b);
                line3(D, xs[a], ys[0], zs[b], xs[a], ys[1], zs[b], r,g,b);
                line3(D, xs[a], ys[b], zs[0], xs[a], ys[b], zs[1], r,g,b);
            }
        }
    }
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
    if (mode3d) render_3d(comp); else composite_2d(comp);
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
    snprintf(dst, sizeof(dst), "%s/media_img3d_hq_ui.txt", dir);
    snprintf(tmp, sizeof(tmp), "%s/media_img3d_hq_ui.txt.tmp", dir);
    snprintf(canvas, sizeof(canvas), "%s/state/canvas.raw", pkg_dir);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "mode_label=%s\n", mode3d ? "3D" : "2D");
    fprintf(f, "mode2d_cls=%s\n", mode3d ? "" : "active");
    fprintf(f, "mode3d_cls=%s\n", mode3d ? "active" : "");
    fprintf(f, "show_2d=%s\n", mode3d ? "" : "1");
    fprintf(f, "show_3d=%s\n", mode3d ? "1" : "");
    fprintf(f, "tB=%s\ntE=%s\ntG=%s\ntR=%s\ntI=%s\ntH=%s\n",
            tool2d=='B'?"active":"", tool2d=='E'?"active":"", tool2d=='G'?"active":"",
            tool2d=='R'?"active":"", tool2d=='I'?"active":"", tool2d=='H'?"active":"");
    fprintf(f, "tSEL=%s\ntGRAB=%s\ntROT=%s\ntSCL=%s\n",
            strcmp(tool3d,"SEL")==0?"active":"", strcmp(tool3d,"GRAB")==0?"active":"",
            strcmp(tool3d,"ROT")==0?"active":"", strcmp(tool3d,"SCL")==0?"active":"");
    fprintf(f, "canvas_raw=%s\n", canvas);
    if (mode3d) {
        fprintf(f, "n_items=%d\n", n_obj);
        for (int i=0;i<n_obj;i++) {
            fprintf(f, "it_%d_name=%c %s  %.1f,%.1f,%.1f\n", i, vis[i]?'*':'-', objs[i].name, objs[i].x, objs[i].y, objs[i].z);
            fprintf(f, "it_%d_cls=%s\n", i, i==sel_obj ? "active" : "");
        }
        fprintf(f, "status_line=3D  tool=%s  sel=%s  yaw=%.2f pit=%.2f dist=%.1f\n",
                tool3d, n_obj?objs[sel_obj].name:"-", cam_yaw, cam_pit, cam_dist);
    } else {
        fprintf(f, "n_items=%d\n", n_lay);
        for (int i=0;i<n_lay;i++) {
            fprintf(f, "it_%d_name=%c L%d%s\n", i, vis[i]?'*':'-', i+1, i==active_lay?"  (active)":"");
            fprintf(f, "it_%d_cls=%s\n", i, i==active_lay ? "active" : "");
        }
        fprintf(f, "status_line=2D  tool=%c  brush=%d  zoom=%d%%  pan=%d,%d  fg=%d,%d,%d\n",
                tool2d, brush, zoom2d, panx, pany, fg[0], fg[1], fg[2]);
    }
    fprintf(f, "msg=%s\n", g_msg);
    fclose(f); rename(tmp, dst);
}

static void apply_tool2d_click(void) {
    int cx = CW/2 + panx, cy = CH/2 + pany;
    if (tool2d=='B') stamp(cx, cy, brush, 0);
    else if (tool2d=='E') stamp(cx, cy, brush, 1);
    else if (tool2d=='G') fill_at(cx, cy);
    else if (tool2d=='R') rect_demo();
    else if (tool2d=='I') {
        size_t o = ((size_t)cy*CW+(size_t)cx)*4;
        if (comp) { fg[0]=comp[o]; fg[1]=comp[o+1]; fg[2]=comp[o+2]; }
    }
    dirty = 1;
}

static void nudge_sel(float dx, float dy, float dz) {
    if (sel_obj < 0 || sel_obj >= n_obj) return;
    if (!strcmp(tool3d,"GRAB")) { objs[sel_obj].x += dx; objs[sel_obj].z += dz; objs[sel_obj].y += dy; }
    else if (!strcmp(tool3d,"SCL")) { objs[sel_obj].s += dx; if (objs[sel_obj].s < 0.1f) objs[sel_obj].s = 0.1f; }
    else if (!strcmp(tool3d,"ROT")) { /* orbit the object around Y by using yaw as stand-in */ objs[sel_obj].x += dx; objs[sel_obj].z += dz; }
    dirty = 1;
}

static void handle(const char *cmd) {
    if (!cmd[0]) return;
    snprintf(g_msg, sizeof(g_msg), "%s", cmd);
    if (!strcmp(cmd, "NEW")) {
        if (mode3d) { n_obj=0; sel_obj=0; } else { n_lay=1; active_lay=0; clear_layer(0); vis[0]=1; }
        dirty=1;
    } else if (!strcmp(cmd, "DEMO")) {
        if (mode3d) demo_3d(); else demo_2d(); dirty=1;
    } else if (!strcmp(cmd, "EXPORT")) {
        snprintf(g_msg, sizeof(g_msg), "export: state/canvas.raw is the live frame (png via ffmpeg later)");
    } else if (!strncmp(cmd, "MODE:", 5)) {
        mode3d = (cmd[5]=='3'); dirty=1;
    } else if (!strncmp(cmd, "TOOL:", 5)) {
        const char *t = cmd+5;
        if (!mode3d && t[0] && !t[1]) tool2d = t[0];
        else snprintf(tool3d, sizeof(tool3d), "%s", t);
    } else if (!strcmp(cmd, "BRUSH:+")) { brush += 2; if (brush>48) brush=48; }
    else if (!strcmp(cmd, "BRUSH:-")) { brush -= 2; if (brush<1) brush=1; }
    else if (!strcmp(cmd, "SWAP_FG")) { unsigned char t[4]; memcpy(t,fg,4); memcpy(fg,bg,4); memcpy(bg,t,4); }
    else if (!strcmp(cmd, "STROKE")) { apply_tool2d_click(); }
    else if (!strncmp(cmd, "CAM:", 4)) {
        const char *c = cmd+4;
        if (!strcmp(c,"L")) { if (mode3d) cam_yaw -= 0.12f; else panx -= 8; }
        else if (!strcmp(c,"R")) { if (mode3d) cam_yaw += 0.12f; else panx += 8; }
        else if (!strcmp(c,"U")) { if (mode3d) cam_pit += 0.08f; else pany -= 8; }
        else if (!strcmp(c,"D")) { if (mode3d) cam_pit -= 0.08f; else pany += 8; }
        else if (!strcmp(c,"IN")) { if (mode3d) { cam_dist -= 0.6f; if (cam_dist<2) cam_dist=2; } else { zoom2d += 10; if (zoom2d>400) zoom2d=400; } }
        else if (!strcmp(c,"OUT")) { if (mode3d) { cam_dist += 0.6f; if (cam_dist>24) cam_dist=24; } else { zoom2d -= 10; if (zoom2d<25) zoom2d=25; } }
        else if (!strcmp(c,"FRAME")) { cam_yaw=0.6f; cam_pit=0.35f; cam_dist=8; panx=pany=0; zoom2d=100; }
        if (mode3d && !strcmp(tool3d,"GRAB")) nudge_sel((c[0]=='R')?0.2f:(c[0]=='L')?-0.2f:0, (c[0]=='U')?0.2f:(c[0]=='D')?-0.2f:0, 0);
        dirty=1;
    } else if (!strncmp(cmd, "PICK:", 5)) {
        int i = atoi(cmd+5);
        if (mode3d) { if (i>=0 && i<n_obj) sel_obj=i; }
        else { if (i>=0 && i<n_lay) active_lay=i; }
    } else if (!strncmp(cmd, "VIS:", 5)) {
        int i = atoi(cmd+5);
        if (!mode3d && i>=0 && i<n_lay) vis[i] = !vis[i];
        dirty=1;
    } else if (!strcmp(cmd, "NEW_LAYER")) {
        if (!mode3d && n_lay < MAX_LAY) { clear_layer(n_lay); vis[n_lay]=1; active_lay=n_lay; n_lay++; dirty=1; }
    } else if (!strcmp(cmd, "CLEAR_LAYER")) {
        if (!mode3d) { clear_layer(active_lay); dirty=1; }
        else if (sel_obj>=0 && sel_obj<n_obj) {
            for (int j=sel_obj;j<n_obj-1;j++) objs[j]=objs[j+1];
            n_obj--; if (sel_obj>=n_obj) sel_obj=n_obj-1; if (sel_obj<0) sel_obj=0; dirty=1;
        }
    } else if (!strncmp(cmd, "ADD:", 4)) {
        if (n_obj >= MAX_OBJ) return;
        Obj *o = &objs[n_obj];
        o->x = 0.4f * n_obj; o->y = 0.5f; o->z = 0.3f * n_obj; o->s = 0.8f;
        if (!strcmp(cmd+4, "CUBE")) { o->kind=0; snprintf(o->name,sizeof(o->name),"Cube%d", n_obj+1); }
        else { o->kind=1; snprintf(o->name,sizeof(o->name),"Sph%d", n_obj+1); }
        sel_obj = n_obj; n_obj++; dirty=1;
    }
}

static void poll_action(int *last) {
    char p[PL]; snprintf(p, sizeof(p), "%s/state/media_img3d_hq_action.txt", pkg_dir);
    FILE *f = fopen(p, "r"); if (!f) return;
    char line[PL], cmd[160]=""; int seq=0;
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
    if (argc < 3) { fprintf(stderr, "usage: media_img3d_hq_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(pkg_dir, sizeof(pkg_dir), "%s", argv[2]);
    kh_plat_on_terminate(bye);
    for (int i=0;i<MAX_LAY;i++) layer[i] = alloc_layer();
    comp = alloc_layer();
    char sd[PL]; snprintf(sd, sizeof(sd), "%s/state", pkg_dir); kh_plat_mkdir_p(sd);
    { char ap[PL]; snprintf(ap, sizeof(ap), "%s/state/media_img3d_hq_action.txt", pkg_dir);
      FILE *a=fopen(ap,"w"); if(a){ fputs("seq=0\ncmd=\n",a); fclose(a); } }
    demo_2d(); demo_3d();
    write_canvas(); write_ui();
    int last=0;
    for (;;) {
        kh_plat_sleep_ms(dirty ? 80 : 200);
        poll_action(&last);
        if (dirty) { write_canvas(); dirty=0; }
        write_ui();
    }
    return 0;
}
