/* media_3d_hq_manager.c — 3D viewport toy (HOW2_BLEND.md).
 * media_3d_hq_manager.+x <house_root> <package_dir> [id]
 * Software wireframe into canvas.raw. Camera lives here, not in the 2D editor. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>
#define KH_PLAT_IMPL
#include "../../../&.widgits/_shared-lib/kh_plat.h"

#define PL 4096
#define CW 640
#define CH 480
#define MAX_OBJ 9
#define MAX_VOX 2048
#define MAX_MV  4096
#define MAX_ME  8192

static char pkg_dir[PL];
static char g_msg[160] = "";
static char g_loaded[256] = "";
static char tool[8] = "SEL";
/* kind: 0 cube 1 sph 2 ground 3 voxel-cloud 4 mesh */
typedef struct { char name[24]; float x, y, z, s; int kind; int vox0, nvox; int e0, ne; } Obj;
static Obj objs[MAX_OBJ];
static int n_obj = 0, sel_obj = 0, dirty = 1;
static float cam_yaw = 0.6f, cam_pit = 0.35f, cam_dist = 8.0f, cam_panx = 0, cam_pany = 0;
static unsigned char *comp;
typedef struct { float x,y,z; unsigned char r,g,b; } Vox;
static Vox voxs[MAX_VOX];
static int n_vox = 0;
static float mv[MAX_MV][3];
static int n_mv = 0;
static int me[MAX_ME][2];
static int n_me = 0;

static void bye(int s){ (void)s; free(comp); _exit(0); }

static void put_px(unsigned char *D, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= CW || y >= CH || !D) return;
    size_t o = ((size_t)y * CW + (size_t)x) * 4;
    D[o]=r; D[o+1]=g; D[o+2]=b; D[o+3]=255;
}

static int load_voxels_csv(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[256];
    int x,y,z,r,g,b;
    int v0 = n_vox;
    float xmin=1e9,xmax=-1e9,ymin=1e9,ymax=-1e9,zmin=1e9,zmax=-1e9;
    while (fgets(line, sizeof(line), f) && n_vox < MAX_VOX) {
        if (line[0]=='#' || !strncmp(line,"x,y,z",5)) continue;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d", &x,&y,&z,&r,&g,&b) != 6) continue;
        voxs[n_vox].x=(float)x; voxs[n_vox].y=(float)y; voxs[n_vox].z=(float)z;
        voxs[n_vox].r=(unsigned char)r; voxs[n_vox].g=(unsigned char)g; voxs[n_vox].b=(unsigned char)b;
        if (x<(int)xmin) xmin=(float)x;
        if (x>(int)xmax) xmax=(float)x;
        if (y<(int)ymin) ymin=(float)y;
        if (y>(int)ymax) ymax=(float)y;
        if (z<(int)zmin) zmin=(float)z;
        if (z>(int)zmax) zmax=(float)z;
        n_vox++;
    }
    fclose(f);
    int nv = n_vox - v0;
    if (nv < 1) return 0;
    float cx=(xmin+xmax)*0.5f, cy=(ymin+ymax)*0.5f, cz=(zmin+zmax)*0.5f;
    float span = xmax-xmin; if (ymax-ymin>span) span=ymax-ymin; if (zmax-zmin>span) span=zmax-zmin;
    if (span < 1) span = 1;
    float sc = 3.2f / span;
    for (int i=v0;i<n_vox;i++) {
        voxs[i].x = (voxs[i].x - cx) * sc;
        voxs[i].y = (voxs[i].y - cy) * sc;
        voxs[i].z = (voxs[i].z - cz) * sc;
    }
    if (n_obj >= MAX_OBJ) return 0;
    Obj *o = &objs[n_obj];
    memset(o, 0, sizeof(*o));
    o->kind=3; o->s=1; o->vox0=v0; o->nvox=nv;
    snprintf(o->name, sizeof(o->name), "voxel");
    sel_obj = n_obj; n_obj++;
    return 1;
}

static int parse_obj_file(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    int v0 = n_mv, e0 = n_me;
    char line[512];
    float xmin=1e9,xmax=-1e9,ymin=1e9,ymax=-1e9,zmin=1e9,zmax=-1e9;
    while (fgets(line, sizeof(line), f)) {
        if (line[0]=='v' && line[1]==' ' && n_mv < MAX_MV) {
            float x,y,z;
            if (sscanf(line+2, "%f %f %f", &x,&y,&z)!=3) continue;
            mv[n_mv][0]=x; mv[n_mv][1]=y; mv[n_mv][2]=z;
            if (x<xmin) xmin=x;
            if (x>xmax) xmax=x;
            if (y<ymin) ymin=y;
            if (y>ymax) ymax=y;
            if (z<zmin) zmin=z;
            if (z>zmax) zmax=z;
            n_mv++;
        } else if (line[0]=='f' && line[1]==' ') {
            int idx[8], ni=0;
            char *p = line+2;
            while (*p && ni<8) {
                int id=0;
                if (sscanf(p, "%d", &id)!=1) break;
                idx[ni++] = v0 + id - 1;
                while (*p && *p!=' ') p++;
                while (*p==' ') p++;
            }
            for (int k=0;k<ni;k++) {
                int a=idx[k], b=idx[(k+1)%ni];
                if (a<v0||b<v0||a>=n_mv||b>=n_mv || n_me>=MAX_ME) continue;
                me[n_me][0]=a; me[n_me][1]=b; n_me++;
            }
        }
    }
    fclose(f);
    int nv = n_mv - v0, ne = n_me - e0;
    if (nv < 1) return 0;
    float cx=(xmin+xmax)*0.5f, cy=(ymin+ymax)*0.5f, cz=(zmin+zmax)*0.5f;
    float span = xmax-xmin; if (ymax-ymin>span) span=ymax-ymin; if (zmax-zmin>span) span=zmax-zmin;
    if (span < 0.0001f) span = 1;
    float sc = 3.2f / span;
    for (int i=v0;i<n_mv;i++) {
        mv[i][0]=(mv[i][0]-cx)*sc; mv[i][1]=(mv[i][1]-cy)*sc; mv[i][2]=(mv[i][2]-cz)*sc;
    }
    if (n_obj >= MAX_OBJ) return 0;
    Obj *o = &objs[n_obj];
    memset(o, 0, sizeof(*o));
    o->kind=4; o->s=1; o->e0=e0; o->ne=ne;
    snprintf(o->name, sizeof(o->name), "mesh");
    sel_obj = n_obj; n_obj++;
    return 1;
}

static int load_mesh(const char *path) {
    const char *dot = strrchr(path, '.');
    char objp[PL];
    snprintf(objp, sizeof(objp), "%s", path);
    if (dot && strcasecmp(dot, ".obj") != 0) {
        snprintf(objp, sizeof(objp), "%s/state/import.obj", pkg_dir);
        char cmd[PL*2];
        snprintf(cmd, sizeof(cmd), "assimp export \"%s\" \"%s\" 2>/dev/null", path, objp);
        if (system(cmd) != 0) return 0;
    }
    return parse_obj_file(objp);
}

static int load_3d_path(const char *path) {
    const char *dot = strrchr(path, '.');
    int ok = 0;
    if (dot && (!strcasecmp(dot, ".csv") || strstr(path, "voxels")))
        ok = load_voxels_csv(path);
    else
        ok = load_mesh(path);
    if (ok) {
        const char *base = strrchr(path, '/');
        snprintf(g_loaded, sizeof(g_loaded), "%s", base ? base+1 : path);
        snprintf(g_msg, sizeof(g_msg), "loaded %s", g_loaded);
        cam_dist = 6.0f;
    } else snprintf(g_msg, sizeof(g_msg), "load failed: %s", path);
    return ok;
}

static void demo_3d(void) {
    n_obj = 0; sel_obj = 0; n_vox = 0; n_mv = 0; n_me = 0;
    cam_yaw=0.7f; cam_pit=0.4f; cam_dist=6.0f; cam_panx=0; cam_pany=0;
    char p[PL];
    snprintf(p, sizeof(p), "%s/media/cursword.voxels.csv", pkg_dir);
    if (load_voxels_csv(p)) {
        snprintf(objs[0].name, sizeof(objs[0].name), "cursword");
        snprintf(g_loaded, sizeof(g_loaded), "cursword.voxels.csv");
        return;
    }
    snprintf(g_loaded, sizeof(g_loaded), "(empty)");
}

static void project(float x, float y, float z, int *ox, int *oy) {
    float cy = cosf(cam_yaw), sy = sinf(cam_yaw);
    float cp = cosf(cam_pit), sp = sinf(cam_pit);
    float x1 = x * cy + z * sy;
    float z1 = -x * sy + z * cy;
    float y1 = y * cp - z1 * sp;
    float z2 = y * sp + z1 * cp + cam_dist;
    if (z2 < 0.2f) z2 = 0.2f;
    float s = 220.0f / z2;
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
        if (o->kind==3) {
            for (int k=0;k<o->nvox;k++) {
                Vox *v = &voxs[o->vox0 + k];
                float px = o->x + v->x * o->s, py = o->y + v->y * o->s, pz = o->z + v->z * o->s;
                int sx, sy; project(px, py, pz, &sx, &sy);
                unsigned char vr=v->r, vg=v->g, vb=v->b;
                if (i==sel_obj) { vr = (unsigned char)((vr+255)/2); vg = (unsigned char)((vg+210)/2); }
                for (int dy=-3; dy<=3; dy++)
                    for (int dx=-3; dx<=3; dx++)
                        if (dx*dx+dy*dy <= 10) put_px(D, sx+dx, sy+dy, vr, vg, vb);
            }
        } else if (o->kind==4) {
            unsigned char vr=r, vg=g, vb=b;
            for (int k=0;k<o->ne;k++) {
                int a=me[o->e0+k][0], b2=me[o->e0+k][1];
                line3(D, o->x+mv[a][0]*o->s, o->y+mv[a][1]*o->s, o->z+mv[a][2]*o->s,
                         o->x+mv[b2][0]*o->s, o->y+mv[b2][1]*o->s, o->z+mv[b2][2]*o->s, vr,vg,vb);
            }
        } else if (o->kind==2) {
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
    fprintf(f, "status_line=3D  %s  tool=%s  sel=%s  yaw=%.2f pit=%.2f dist=%.1f\n",
            g_loaded, tool, n_obj?objs[sel_obj].name:"-", cam_yaw, cam_pit, cam_dist);
    {
        const char *now = "select";
        if (!strcmp(tool,"GRAB")) now = "grab";
        else if (!strcmp(tool,"ROT")) now = "rotate";
        else if (!strcmp(tool,"SCL")) now = "scale";
        fprintf(f, "gutter=Sel select   Grab grab   Rot rotate   Scl scale     now: %s\n", now);
    }
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
    if (!strcmp(cmd, "NEW")) { n_obj=0; sel_obj=0; n_vox=0; n_mv=0; n_me=0; g_loaded[0]=0; dirty=1; }
    else if (!strcmp(cmd, "DEMO")) { demo_3d(); dirty=1; }
    else if (!strncmp(cmd, "LOAD:", 5)) { load_3d_path(cmd+5); dirty=1; }
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
        memset(o, 0, sizeof(*o));
        o->x = 0.4f * n_obj; o->y = 0.5f; o->z = 0.3f * n_obj; o->s = 0.8f;
        if (!strcmp(cmd+4, "CUBE")) { o->kind=0; snprintf(o->name,sizeof(o->name),"Cube%d", n_obj+1); }
        else { o->kind=1; snprintf(o->name,sizeof(o->name),"Sph%d", n_obj+1); }
        sel_obj = n_obj; n_obj++; dirty=1;
    }
}

static void poll_action(int *last) {
    char p[PL]; snprintf(p, sizeof(p), "%s/state/media_3d_hq_action.txt", pkg_dir);
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
