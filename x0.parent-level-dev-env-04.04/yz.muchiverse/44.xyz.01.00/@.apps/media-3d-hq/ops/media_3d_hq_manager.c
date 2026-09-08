/* media_3d_hq_manager.c — 3D viewport toy (HOW2_BLEND.md).
 * media_3d_hq_manager.+x <house_root> <package_dir> [id]
 * Software wireframe into canvas.raw. Camera lives here, not in the 2D editor. */
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
#define MAX_OBJ 9

static char pkg_dir[PL];
static char g_msg[160] = "";
static char tool[8] = "SEL";
typedef struct { char name[24]; float x, y, z, s; int kind; } Obj;
static Obj objs[MAX_OBJ];
static int n_obj = 0, sel_obj = 0, dirty = 1;
static float cam_yaw = 0.6f, cam_pit = 0.35f, cam_dist = 8.0f, cam_panx = 0, cam_pany = 0;
static unsigned char *comp;

static void bye(int s){ (void)s; free(comp); _exit(0); }

static void put_px(unsigned char *D, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= CW || y >= CH || !D) return;
    size_t o = ((size_t)y * CW + (size_t)x) * 4;
    D[o]=r; D[o+1]=g; D[o+2]=b; D[o+3]=255;
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
    float z2 = y * sp + z1 * cp + cam_dist;
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
        put_px(D, ax, ay, r, g, b);
        if (ax==bx && ay==by) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; ax += sx; }
        if (e2 <= dx) { err += dx; ay += sy; }
    }
}

static void render_3d(unsigned char *D) {
    memset(D, 18, (size_t)CW*CH*4);
    for (int i=0;i<CW*CH;i++) D[i*4+3]=255;
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

static void write_canvas(void) {
    if (!comp) return;
    render_3d(comp);
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
    snprintf(dst, sizeof(dst), "%s/media_3d_hq_ui.txt", dir);
    snprintf(tmp, sizeof(tmp), "%s/media_3d_hq_ui.txt.tmp", dir);
    snprintf(canvas, sizeof(canvas), "%s/state/canvas.raw", pkg_dir);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "tSEL=%s\ntGRAB=%s\ntROT=%s\ntSCL=%s\n",
            strcmp(tool,"SEL")==0?"active":"", strcmp(tool,"GRAB")==0?"active":"",
            strcmp(tool,"ROT")==0?"active":"", strcmp(tool,"SCL")==0?"active":"");
    fprintf(f, "canvas_raw=%s\n", canvas);
    fprintf(f, "n_items=%d\n", n_obj);
    for (int i=0;i<n_obj;i++) {
        fprintf(f, "it_%d_name=%s  %.1f,%.1f,%.1f s=%.1f\n", i, objs[i].name, objs[i].x, objs[i].y, objs[i].z, objs[i].s);
        fprintf(f, "it_%d_cls=%s\n", i, i==sel_obj ? "active" : "");
    }
    fprintf(f, "status_line=3D  tool=%s  sel=%s  yaw=%.2f pit=%.2f dist=%.1f\n",
            tool, n_obj?objs[sel_obj].name:"-", cam_yaw, cam_pit, cam_dist);
    fprintf(f, "msg=%s\n", g_msg);
    fclose(f); rename(tmp, dst);
}

static void nudge_sel(void) {
    if (sel_obj < 0 || sel_obj >= n_obj) return;
    if (!strcmp(tool,"GRAB")) { objs[sel_obj].x += 0.2f; }
    else if (!strcmp(tool,"SCL")) { objs[sel_obj].s += 0.15f; }
    else if (!strcmp(tool,"ROT")) { objs[sel_obj].z += 0.2f; }
    dirty = 1;
}

static void handle(const char *cmd) {
    if (!cmd[0]) return;
    snprintf(g_msg, sizeof(g_msg), "%s", cmd);
    if (!strcmp(cmd, "NEW")) { n_obj=0; sel_obj=0; dirty=1; }
    else if (!strcmp(cmd, "DEMO")) { demo_3d(); dirty=1; }
    else if (!strncmp(cmd, "TOOL:", 5)) snprintf(tool, sizeof(tool), "%s", cmd+5);
    else if (!strncmp(cmd, "CAM:", 4)) {
        const char *c = cmd+4;
        if (!strcmp(c,"L")) cam_yaw -= 0.12f;
        else if (!strcmp(c,"R")) cam_yaw += 0.12f;
        else if (!strcmp(c,"U")) cam_pit += 0.08f;
        else if (!strcmp(c,"D")) cam_pit -= 0.08f;
        else if (!strcmp(c,"IN")) { cam_dist -= 0.6f; if (cam_dist<2) cam_dist=2; }
        else if (!strcmp(c,"OUT")) { cam_dist += 0.6f; if (cam_dist>24) cam_dist=24; }
        else if (!strcmp(c,"FRAME")) { cam_yaw=0.6f; cam_pit=0.35f; cam_dist=8; cam_panx=0; cam_pany=0; }
        dirty=1;
    } else if (!strcmp(cmd, "NUDGE")) nudge_sel();
    else if (!strncmp(cmd, "PICK:", 5)) { int i=atoi(cmd+5); if (i>=0&&i<n_obj) sel_obj=i; }
    else if (!strncmp(cmd, "DEL:", 4)) {
        int i=atoi(cmd+4);
        if (i>=0 && i<n_obj) {
            for (int j=i;j<n_obj-1;j++) objs[j]=objs[j+1];
            n_obj--; if (sel_obj>=n_obj) sel_obj=n_obj-1; if (sel_obj<0) sel_obj=0; dirty=1;
        }
    } else if (!strncmp(cmd, "ADD:", 4) && n_obj < MAX_OBJ) {
        Obj *o = &objs[n_obj];
        o->x = 0.4f * n_obj; o->y = 0.5f; o->z = 0.3f * n_obj; o->s = 0.8f;
        if (!strcmp(cmd+4, "CUBE")) { o->kind=0; snprintf(o->name,sizeof(o->name),"Cube%d", n_obj+1); }
        else { o->kind=1; snprintf(o->name,sizeof(o->name),"Sph%d", n_obj+1); }
        sel_obj = n_obj; n_obj++; dirty=1;
    }
}

static void poll_action(int *last) {
    char p[PL]; snprintf(p, sizeof(p), "%s/state/media_3d_hq_action.txt", pkg_dir);
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
    if (argc < 3) { fprintf(stderr, "usage: media_3d_hq_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(pkg_dir, sizeof(pkg_dir), "%s", argv[2]);
    kh_plat_on_terminate(bye);
    comp = calloc((size_t)CW*CH*4, 1);
    char sd[PL]; snprintf(sd, sizeof(sd), "%s/state", pkg_dir); kh_plat_mkdir_p(sd);
    { char ap[PL]; snprintf(ap, sizeof(ap), "%s/state/media_3d_hq_action.txt", pkg_dir);
      FILE *a=fopen(ap,"w"); if(a){ fputs("seq=0\ncmd=\n",a); fclose(a); } }
    demo_3d(); write_canvas(); write_ui();
    int last=0;
    for (;;) {
        kh_plat_sleep_ms(dirty ? 80 : 200);
        poll_action(&last);
        if (dirty) { write_canvas(); dirty=0; }
        write_ui();
    }
    return 0;
}
