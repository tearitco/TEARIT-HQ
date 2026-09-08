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
#define PX_PER_SEC 80
#define DUR 8.0f

static char pkg_dir[PL];
static char g_msg[160] = "";
static unsigned char *comp;
static int dirty = 1, playing, sel;
static float playhead;
static const char *LANE[] = {"V1","V2","A1","A2"};

typedef struct {
    int used, lane; /* 0 V1 1 V2 2 A1 3 A2 */
    float t0, t1;
    unsigned char r,g,b;
    char name[24];
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

static void demo(void){
    memset(cl,0,sizeof(cl)); n_cl=3; sel=0; playhead=0; playing=0;
    cl[0].used=1; cl[0].lane=0; cl[0].t0=0; cl[0].t1=3;
    cl[0].r=220; cl[0].g=120; cl[0].b=40; snprintf(cl[0].name,sizeof(cl[0].name),"orange");
    cl[1].used=1; cl[1].lane=0; cl[1].t0=3; cl[1].t1=6;
    cl[1].r=50; cl[1].g=90; cl[1].b=210; snprintf(cl[1].name,sizeof(cl[1].name),"blue");
    cl[2].used=1; cl[2].lane=1; cl[2].t0=1; cl[2].t1=4;
    cl[2].r=50; cl[2].g=170; cl[2].b=70; snprintf(cl[2].name,sizeof(cl[2].name),"green");
    snprintf(g_msg,sizeof(g_msg),"demo 3 clips V1/V2");
    dirty=1;
}

static void draw(void){
    if(!comp) return;
    memset(comp, 16, (size_t)CW*CH*4);
    for(int i=0;i<CW*CH;i++) comp[i*4+3]=255;
    int prev_h = 200, tl_top = prev_h + 8, lane_h = 32;
    /* preview poster: color of clip under playhead (HOW2: no per-frame decode) */
    Clip *u = under_playhead();
    if(u) fill(40, 16, CW-80, prev_h-24, u->r, u->g, u->b);
    else fill(40, 16, CW-80, prev_h-24, 24,24,28);
    /* letterbox */
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
    else if(!strcmp(cmd,"STEP:+")){ playhead+=1.0f/30.0f; if(playhead>DUR) playhead=DUR; dirty=1; }
    else if(!strcmp(cmd,"STEP:-")){ playhead-=1.0f/30.0f; if(playhead<0) playhead=0; dirty=1; }
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
