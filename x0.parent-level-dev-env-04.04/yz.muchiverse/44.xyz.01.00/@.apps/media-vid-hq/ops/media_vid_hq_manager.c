/* media_vid_hq_manager.c — HOW2_VIDEO iMovie shell.
 * Preview poster (clip color, no per-frame ffmpeg) + V1/V2/A1/A2 timeline. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define KH_PLAT_IMPL
#include "../../../&.widgits/_shared-lib/kh_plat.h"

#define PL 4096
#define CW 680
#define CH 360
#define MAX_CL 16
#define PW 320
#define PH 180
#define PX_PER_SEC 60
#define DUR 10.0f

static char pkg_dir[PL];
static char house_root[PL];
static char g_msg[160] = "";
static unsigned char *comp;
static unsigned char poster[PW * PH * 4];
static int poster_ok;
static float last_decode = -99;
static int dirty = 1, playing, sel;
static float playhead;
static const char *LANE[] = {"V1","V2","A1","A2"};

typedef struct {
    int used, lane; /* 0 V1 1 V2 2 A1 3 A2 */
    float t0, t1;
    unsigned char r,g,b;
    char name[24];
    char path[PL];
} Clip;
static Clip cl[MAX_CL];
static int n_cl;

static void bye(int s){ (void)s; free(comp); _exit(0); }

static void put(int x,int y, unsigned char r,unsigned char g,unsigned char b){
    if(x<0||y<0||x>=CW||y>=CH||!comp) return;
    size_t o=((size_t)y*CW+(size_t)x)*4;
    comp[o]=r; comp[o+1]=g; comp[o+2]=b; comp[o+3]=255;
}
static void fill(int x0,int y0,int w,int h, unsigned char r,unsigned char g,unsigned char b){
    for(int y=y0;y<y0+h;y++) for(int x=x0;x<x0+w;x++) put(x,y,r,g,b);
}
static void vline(int x,int y0,int y1, unsigned char r,unsigned char g,unsigned char b){
    if(y0>y1){ int t=y0;y0=y1;y1=t; }
    for(int y=y0;y<=y1;y++) put(x,y,r,g,b);
}
static void hline(int x0,int x1,int y, unsigned char r,unsigned char g,unsigned char b){
    if(x0>x1){ int t=x0;x0=x1;x1=t; }
    for(int x=x0;x<=x1;x++) put(x,y,r,g,b);
}

static Clip *under_playhead(void){
    Clip *best=NULL;
    for(int i=0;i<n_cl;i++){
        if(!cl[i].used) continue;
        if(playhead < cl[i].t0 || playhead >= cl[i].t1) continue;
        if(!best || cl[i].lane < best->lane) best=&cl[i];
    }
    return best;
}

static int decode_poster(Clip *c, float t, int force){
    if(!c || !c->path[0]) { poster_ok=0; return 0; }
    float src = t - c->t0;
    if(src < 0) src = 0;
    if(!force && poster_ok && src - last_decode < 0.45f && last_decode - src < 0.45f) return 1;
    char out[PL], cmd[PL*2];
    snprintf(out, sizeof(out), "%s/state/poster.rgba", pkg_dir);
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -v error -y -ss %.3f -i \"%s\" -frames:v 1 -an "
             "-f rawvideo -pix_fmt rgba -s %dx%d \"%s\"",
             src, c->path, PW, PH, out);
    if(system(cmd) != 0) { poster_ok=0; return 0; }
    FILE *f=fopen(out,"rb"); if(!f) { poster_ok=0; return 0; }
    size_t n=fread(poster,1,sizeof(poster),f); fclose(f);
    poster_ok = (n == sizeof(poster));
    if(poster_ok) last_decode = src;
    return poster_ok;
}

static void blit_poster(int dx, int dy){
    if(!poster_ok) return;
    for(int y=0;y<PH;y++) for(int x=0;x<PW;x++){
        size_t o=((size_t)y*PW+(size_t)x)*4;
        put(dx+x, dy+y, poster[o], poster[o+1], poster[o+2]);
    }
}

/* Path comes from VIDEO-ASSET-SOURCE-LOCATION.pdl SOURCE sample_mp4
 * (same pattern as palettes img_root). No hardcoded NNEST_ASSETS. */
static int pdl_source_value(const char *pdl_name, const char *key, char *out, size_t outsz){
    char pdl[PL];
    snprintf(pdl, sizeof(pdl), "%s/../#.#.calendar-dox/1.^V-hq/%s", house_root, pdl_name);
    FILE *f = fopen(pdl, "r");
    if(!f) return 0;
    char line[PL];
    int ok = 0;
    while(fgets(line, sizeof(line), f)){
        if(strncmp(line, "SOURCE", 6) != 0) continue;
        if(!strstr(line, key)) continue;
        char *bar = strrchr(line, '|');
        if(!bar) continue;
        char *v = bar + 1;
        while(*v == ' ' || *v == '\t') v++;
        size_t n = strlen(v);
        while(n > 0 && (v[n-1]=='\n' || v[n-1]=='\r' || v[n-1]==' ')) v[--n] = 0;
        if(n > 0){ snprintf(out, outsz, "%s", v); ok = 1; break; }
    }
    fclose(f);
    return ok;
}

static void demo(void){
    memset(cl,0,sizeof(cl)); n_cl=1; sel=0; playhead=0; playing=0;
    poster_ok=0; last_decode=-99;
    cl[0].used=1; cl[0].lane=0; cl[0].t0=0; cl[0].t1=DUR;
    cl[0].r=80; cl[0].g=140; cl[0].b=200;
    snprintf(cl[0].name,sizeof(cl[0].name),"sample-10s");
    cl[0].path[0]=0;
    if(pdl_source_value("VIDEO-ASSET-SOURCE-LOCATION.pdl", "sample_mp4",
                        cl[0].path, sizeof(cl[0].path))){
        if(access(cl[0].path, R_OK) != 0) cl[0].path[0] = 0;
    }
    decode_poster(&cl[0], 0, 1);
    snprintf(g_msg,sizeof(g_msg), cl[0].path[0]
             ? (poster_ok ? "loaded sample-10s-vp9.mp4" : "ffmpeg poster failed")
             : "VIDEO-ASSET-SOURCE-LOCATION.pdl sample_mp4 missing");
    dirty=1;
}

static void draw(void){
    if(!comp) return;
    memset(comp, 16, (size_t)CW*CH*4);
    for(int i=0;i<CW*CH;i++) comp[i*4+3]=255;
    int prev_h = 200, tl_top = prev_h + 8, lane_h = 32;
    Clip *u = under_playhead();
    fill(40, 16, CW-80, prev_h-24, 24,24,28);
    if(u && u->path[0]){
        decode_poster(u, playhead, 0);
        blit_poster(40 + (CW-80-PW)/2, 16 + (prev_h-24-PH)/2);
    } else if(u) fill(40, 16, CW-80, prev_h-24, u->r, u->g, u->b);
    hline(40, CW-41, 16, 80,80,90);
    hline(40, CW-41, prev_h-9, 80,80,90);
    /* timeline */
    fill(0, tl_top, CW, CH-tl_top, 20,20,24);
    for(int L=0;L<4;L++){
        int y = tl_top + L*lane_h;
        fill(0,y,36,lane_h-2, 32,32,38);
        /* lane label as 2px ticks — V1=1 mark etc */
        for(int k=0;k<L+1;k++) put(8+k*6, y+lane_h/2, 180,180,190);
        hline(0,CW-1,y+lane_h-1, 40,40,48);
    }
    for(int i=0;i<n_cl;i++){
        if(!cl[i].used) continue;
        int x = 40 + (int)(cl[i].t0 * PX_PER_SEC);
        int w = (int)((cl[i].t1-cl[i].t0) * PX_PER_SEC) - 2;
        int y = tl_top + cl[i].lane * lane_h + 4;
        fill(x,y,w,lane_h-10, cl[i].r, cl[i].g, cl[i].b);
        if(i==sel){
            hline(x,x+w,y, 255,255,255);
            hline(x,x+w,y+lane_h-11, 255,255,255);
        }
    }
    int px = 40 + (int)(playhead * PX_PER_SEC);
    vline(px, 16, CH-1, 220, 40, 40);
}

static void write_canvas(void){
    draw();
    char raw[PL], tmp[PL], rec[PL];
    snprintf(raw,sizeof(raw),"%s/state/canvas.raw",pkg_dir);
    snprintf(tmp,sizeof(tmp),"%s/state/canvas.raw.tmp",pkg_dir);
    snprintf(rec,sizeof(rec),"%s/state/canvas.receipt.txt",pkg_dir);
    FILE *f=fopen(tmp,"wb");
    if(f){ fwrite(comp,1,(size_t)CW*CH*4,f); fclose(f); rename(tmp,raw); }
    FILE *r=fopen(rec,"w");
    if(r){ fprintf(r,"overlay_w=%d\noverlay_h=%d\n",CW,CH); fclose(r); }
}

static void write_ui(void){
    char dir[PL], tmp[PL], dst[PL], canvas[PL];
    snprintf(dir,sizeof(dir),"%s/state",pkg_dir); kh_plat_mkdir_p(dir);
    snprintf(dst,sizeof(dst),"%s/media_vid_hq_ui.txt",dir);
    snprintf(tmp,sizeof(tmp),"%s/media_vid_hq_ui.txt.tmp",dir);
    snprintf(canvas,sizeof(canvas),"%s/state/canvas.raw",pkg_dir);
    int sec=(int)playhead, fr=(int)((playhead-sec)*30);
    FILE *f=fopen(tmp,"w"); if(!f) return;
    fprintf(f,"tc=00:%02d:%02d\n", sec, fr);
    fprintf(f,"play_label=%s\n", playing?"||":">");
    fprintf(f,"play_cls=%s\n", playing?"active":"");
    fprintf(f,"canvas_raw=%s\n", canvas);
    int n=0;
    for(int i=0;i<n_cl;i++) if(cl[i].used) n++;
    fprintf(f,"n_clips=%d\n", n);
    int k=0;
    for(int i=0;i<n_cl;i++){
        if(!cl[i].used) continue;
        fprintf(f,"c_%d_hdr=%s %s  %.1f-%.1f  dur=%.1f\n", k, LANE[cl[i].lane], cl[i].name, cl[i].t0, cl[i].t1, cl[i].t1-cl[i].t0);
        fprintf(f,"c_%d_cls=%s\n", k, i==sel?"active":"");
        k++;
    }
    Clip *u=under_playhead();
    const char *ins = "(no clip)";
    char ibuf[80];
    if(sel>=0 && sel<n_cl && cl[sel].used){
        snprintf(ibuf,sizeof(ibuf),"insp %s in=%.1f out=%.1f dur=%.1f",
                 cl[sel].name, cl[sel].t0, cl[sel].t1, cl[sel].t1-cl[sel].t0);
        ins=ibuf;
    }
    fprintf(f,"gutter=space play  ,/. frame  C split  Del  lcd 00:%02d:%02d  preview=%s  %s\n",
            sec, fr, u?u->name:"black", ins);
    fprintf(f,"msg=%s\n", g_msg);
    fclose(f); rename(tmp,dst);
}

static int clip_at_k(int k){
    int n=0;
    for(int i=0;i<n_cl;i++) if(cl[i].used){ if(n==k) return i; n++; }
    return -1;
}

static void handle(const char *cmd){
    if(!cmd[0]) return;
    snprintf(g_msg,sizeof(g_msg),"%s",cmd);
    if(!strcmp(cmd,"NEW")){ memset(cl,0,sizeof(cl)); n_cl=0; sel=-1; playhead=0; playing=0; dirty=1; }
    else if(!strcmp(cmd,"DEMO")) demo();
    else if(!strcmp(cmd,"PLAY")){ playing=!playing; dirty=1; }
    else if(!strcmp(cmd,"STOP")){ playing=0; dirty=1; }
    else if(!strcmp(cmd,"REW")){ playhead=0; dirty=1; }
    else if(!strcmp(cmd,"END")){ playhead=DUR; playing=0; dirty=1; }
    else if(!strcmp(cmd,"STEP:+")){ playhead+=0.2f; if(playhead>DUR) playhead=DUR; last_decode=-99; dirty=1; }
    else if(!strcmp(cmd,"STEP:-")){ playhead-=0.2f; if(playhead<0) playhead=0; last_decode=-99; dirty=1; }
    else if(!strncmp(cmd,"SEL:",4)){ int i=clip_at_k(atoi(cmd+4)); if(i>=0){ sel=i; dirty=1; } }
    else if(!strcmp(cmd,"DEL") && sel>=0 && sel<n_cl){ cl[sel].used=0; sel=-1; dirty=1; }
    else if(!strcmp(cmd,"SPLIT") && sel>=0 && sel<n_cl && cl[sel].used){
        if(playhead>cl[sel].t0+0.15f && playhead<cl[sel].t1-0.15f && n_cl<MAX_CL){
            Clip *a=&cl[sel], *b=&cl[n_cl];
            *b=*a; b->t0=playhead; a->t1=playhead; n_cl++; dirty=1;
        }
    }
}

static void poll_action(int *last){
    char p[PL]; snprintf(p,sizeof(p),"%s/state/media_vid_hq_action.txt",pkg_dir);
    FILE *f=fopen(p,"r"); if(!f) return;
    char line[PL], cmd[256]=""; int seq=0;
    while(fgets(line,sizeof(line),f)){
        if(sscanf(line,"seq=%d",&seq)==1) continue;
        if(!strncmp(line,"cmd=",4)){ snprintf(cmd,sizeof(cmd),"%s",line+4); char *n=strchr(cmd,'\n'); if(n)*n=0; }
    }
    fclose(f);
    if(seq==*last||!cmd[0]) return;
    *last=seq; handle(cmd);
}

int main(int argc,char**argv){
    if(argc<3){ fprintf(stderr,"usage: media_vid_hq_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(house_root,sizeof(house_root),"%s",argv[1]);
    snprintf(pkg_dir,sizeof(pkg_dir),"%s",argv[2]);
    kh_plat_on_terminate(bye);
    comp=calloc((size_t)CW*CH*4,1);
    char sd[PL]; snprintf(sd,sizeof(sd),"%s/state",pkg_dir); kh_plat_mkdir_p(sd);
    { char ap[PL]; snprintf(ap,sizeof(ap),"%s/state/media_vid_hq_action.txt",pkg_dir);
      FILE *a=fopen(ap,"w"); if(a){ fputs("seq=0\ncmd=\n",a); fclose(a);} }
    demo(); write_canvas(); write_ui();
    int last=0;
    for(;;){
        kh_plat_sleep_ms(playing?50:200);
        poll_action(&last);
        if(playing){
            playhead += 0.05f;
            if(playhead >= DUR){ playhead=DUR; playing=0; }
            dirty=1;
        }
        if(dirty){ write_canvas(); dirty=0; }
        write_ui();
    }
    return 0;
}
