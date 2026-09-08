/* media_daw_hq_manager.c — HOW2_DAW pass-2 shell: transport, arrangement
 * canvas (lanes + clips + playhead), piano roll, mixer drawer.
 * Software visual only this pass (no WAV/VST). */
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
#define MAX_TR 12
#define MAX_CL 32
#define BARS 8
#define PPB  80

static char pkg_dir[PL];
static char g_msg[160] = "";
static unsigned char *comp;
static int dirty = 1;
static int playing, rec, cycle, mixer_open;
static int bpm = 120;
static float playhead; /* bars, 0..BARS */
static int n_tr, sel;


typedef struct { char name[24]; int mute, solo, reca, vol; unsigned char r,g,b; } Trk;
typedef struct { int tr, bar0, bars, note; } Clip;
static Trk tr[MAX_TR];
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
static void hline(int x0,int x1,int y, unsigned char r,unsigned char g,unsigned char b){
    if(x0>x1){ int t=x0;x0=x1;x1=t; }
    for(int x=x0;x<=x1;x++) put(x,y,r,g,b);
}
static void vline(int x,int y0,int y1, unsigned char r,unsigned char g,unsigned char b){
    if(y0>y1){ int t=y0;y0=y1;y1=t; }
    for(int y=y0;y<=y1;y++) put(x,y,r,g,b);
}

static void demo(void){
    n_tr=4; sel=0; n_cl=0; playhead=0; playing=0; rec=0; cycle=0;
    snprintf(tr[0].name,sizeof(tr[0].name),"Drums"); tr[0].r=200; tr[0].g=70; tr[0].b=70; tr[0].vol=80;
    snprintf(tr[1].name,sizeof(tr[1].name),"Bass");  tr[1].r=70; tr[1].g=160; tr[1].b=90; tr[1].vol=70;
    snprintf(tr[2].name,sizeof(tr[2].name),"Keys");  tr[2].r=70; tr[2].g=120; tr[2].b=200; tr[2].vol=75;
    snprintf(tr[3].name,sizeof(tr[3].name),"Lead");  tr[3].r=200; tr[3].g=180; tr[3].b=60; tr[3].vol=65;
    for(int i=0;i<n_tr;i++){ tr[i].mute=0; tr[i].solo=0; tr[i].reca=0; }
    /* clips: bar0, length bars, piano note */
    Clip d[] = {
        {0,0,2,36},{0,2,2,36},{0,4,2,36},{0,6,2,36},
        {1,0,4,28},{1,4,4,31},
        {2,1,2,60},{2,3,2,64},{2,5,3,67},
        {3,2,2,72},{3,5,2,76}
    };
    n_cl = (int)(sizeof(d)/sizeof(d[0]));
    memcpy(cl, d, sizeof(d));
    snprintf(g_msg,sizeof(g_msg),"demo 4 tracks");
    dirty=1;
}

static void draw(void){
    if(!comp) return;
    memset(comp, 18, (size_t)CW*CH*4);
    for(int i=0;i<CW*CH;i++) comp[i*4+3]=255;
    int lane_h = 28, arr_top = 22, arr_h = lane_h * (n_tr>0?n_tr:1);
    if(arr_h > 140) arr_h = 140;
    int roll_top = arr_top + arr_h + 8;
    /* ruler */
    fill(0,0,CW,arr_top, 28,28,32);
    for(int b=0;b<=BARS;b++){
        int x = 8 + b * PPB;
        vline(x, 0, arr_top-1, 80,80,90);
        /* tick digit as 2-px blocks — skip font; bar index as dots */
        if(b<BARS) for(int k=0;k<b+1 && k<8;k++) put(x+2+k*3, 4, 180,180,190);
    }
    /* lanes + clips */
    for(int t=0;t<n_tr;t++){
        int y = arr_top + t * lane_h;
        unsigned char bg = (t==sel)? 36: 24;
        fill(0,y,CW,lane_h-1, bg,bg,bg+4);
        hline(0,CW-1,y+lane_h-1, 40,40,48);
        /* header color chip */
        fill(2,y+4,10,lane_h-8, tr[t].r, tr[t].g, tr[t].b);
    }
    for(int i=0;i<n_cl;i++){
        Clip *c=&cl[i];
        if(c->tr<0||c->tr>=n_tr) continue;
        int x = 8 + c->bar0 * PPB;
        int w = c->bars * PPB - 4;
        int y = arr_top + c->tr * lane_h + 4;
        fill(x,y,w,lane_h-10, tr[c->tr].r, tr[c->tr].g, tr[c->tr].b);
    }
    /* playhead */
    int px = 8 + (int)(playhead * PPB);
    vline(px, 0, arr_top+arr_h, 255, 210, 80);
    /* piano roll for selected track */
    fill(0, roll_top, CW, CH-roll_top, 16,16,20);
    int notes = 16, nh = (CH-roll_top-4)/notes;
    if(nh<6) nh=6;
    for(int n=0;n<notes;n++){
        int y = roll_top + n*nh;
        int black = (n%12==1||n%12==3||n%12==6||n%12==8||n%12==10);
        fill(0,y,24,nh-1, black?40:70, black?40:70, black?48:78);
        hline(24,CW-1,y, 30,30,36);
    }
    for(int i=0;i<n_cl;i++){
        if(cl[i].tr!=sel) continue;
        int x = 8 + cl[i].bar0 * PPB;
        int w = cl[i].bars * PPB - 4;
        int row = 15 - (cl[i].note % 16);
        if(row<0) row=0;
        int y = roll_top + row*nh + 1;
        fill(x,y,w,nh-3, tr[sel].r, tr[sel].g, tr[sel].b);
    }
    vline(px, roll_top, CH-1, 255, 210, 80);
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
    snprintf(dst,sizeof(dst),"%s/media_daw_hq_ui.txt",dir);
    snprintf(tmp,sizeof(tmp),"%s/media_daw_hq_ui.txt.tmp",dir);
    snprintf(canvas,sizeof(canvas),"%s/state/canvas.raw",pkg_dir);
    int bar=(int)playhead, beat=(int)((playhead-bar)*4);
    FILE *f=fopen(tmp,"w"); if(!f) return;
    fprintf(f,"lcd=%d.%d.00\n", bar+1, beat+1);
    fprintf(f,"bpm=%d\n", bpm);
    fprintf(f,"play_label=%s\n", playing?"||":">");
    fprintf(f,"play_cls=%s\n", playing?"active":"");
    fprintf(f,"rec_cls=%s\n", rec?"active":"");
    fprintf(f,"cycle_cls=%s\n", cycle?"active":"");
    fprintf(f,"canvas_raw=%s\n", canvas);
    fprintf(f,"n_tracks=%d\n", n_tr);
    for(int i=0;i<n_tr;i++){
        fprintf(f,"t_%d_hdr=%c %s  vol=%d%s%s%s\n", i,
                (i==sel)?'>':' ', tr[i].name, tr[i].vol,
                tr[i].mute?" M":"", tr[i].solo?" S":"", tr[i].reca?" R":"");
        fprintf(f,"t_%d_cls=%s\n", i, i==sel?"active":"");
    }
    fprintf(f,"n_mix=%d\n", mixer_open?n_tr:0);
    if(mixer_open){
        for(int i=0;i<n_tr;i++)
            fprintf(f,"m_%d_fader=%s %d\n", i, tr[i].name, tr[i].vol);
    }
    fprintf(f,"gutter=|< stop  > play  (o) rec  cyc  +Track  Mixer     lcd %d.%d.00  %d BPM  now: %s\n",
            bar+1, beat+1, bpm, playing?"play":"stop");
    fprintf(f,"msg=%s\n", g_msg);
    fclose(f); rename(tmp,dst);
}

static void handle(const char *cmd){
    if(!cmd[0]) return;
    snprintf(g_msg,sizeof(g_msg),"%s",cmd);
    if(!strcmp(cmd,"NEW")){ n_tr=1; sel=0; n_cl=0; playhead=0; playing=0;
        snprintf(tr[0].name,sizeof(tr[0].name),"Track1"); tr[0].r=120;tr[0].g=120;tr[0].b=140; tr[0].vol=70; tr[0].mute=tr[0].solo=tr[0].reca=0; dirty=1; }
    else if(!strcmp(cmd,"DEMO")) demo();
    else if(!strcmp(cmd,"PLAY")){ playing=!playing; dirty=1; }
    else if(!strcmp(cmd,"STOP")){ playing=0; rec=0; dirty=1; }
    else if(!strcmp(cmd,"REW")){ playhead=0; dirty=1; }
    else if(!strcmp(cmd,"REC")){ rec=!rec; if(rec) playing=1; dirty=1; }
    else if(!strcmp(cmd,"CYCLE")){ cycle=!cycle; dirty=1; }
    else if(!strcmp(cmd,"BPM:+")){ bpm+=5; if(bpm>240) bpm=240; }
    else if(!strcmp(cmd,"BPM:-")){ bpm-=5; if(bpm<40) bpm=40; }
    else if(!strcmp(cmd,"ADD_TRACK") && n_tr<MAX_TR){
        snprintf(tr[n_tr].name,sizeof(tr[n_tr].name),"Track%d", n_tr+1);
        tr[n_tr].r=100+(n_tr*40)%120; tr[n_tr].g=80+(n_tr*25)%140; tr[n_tr].b=140;
        tr[n_tr].vol=70; tr[n_tr].mute=tr[n_tr].solo=tr[n_tr].reca=0;
        sel=n_tr; n_tr++; dirty=1;
    } else if(!strcmp(cmd,"MIX_TOGGLE")) mixer_open=!mixer_open;
    else if(!strncmp(cmd,"SEL_TRACK:",10)){ int i=atoi(cmd+10); if(i>=0&&i<n_tr){ sel=i; dirty=1; } }
    else if(!strncmp(cmd,"MUTE:",5)){ int i=atoi(cmd+5); if(i>=0&&i<n_tr){ tr[i].mute=!tr[i].mute; dirty=1; } }
    else if(!strncmp(cmd,"FADER:",6)){ int i=atoi(cmd+6); if(i>=0&&i<n_tr){ tr[i].vol+=5; if(tr[i].vol>100) tr[i].vol=40; dirty=1; } }
}

static void poll_action(int *last){
    char p[PL]; snprintf(p,sizeof(p),"%s/state/media_daw_hq_action.txt",pkg_dir);
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
    if(argc<3){ fprintf(stderr,"usage: media_daw_hq_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(pkg_dir,sizeof(pkg_dir),"%s",argv[2]);
    kh_plat_on_terminate(bye);
    comp=calloc((size_t)CW*CH*4,1);
    char sd[PL]; snprintf(sd,sizeof(sd),"%s/state",pkg_dir); kh_plat_mkdir_p(sd);
    { char ap[PL]; snprintf(ap,sizeof(ap),"%s/state/media_daw_hq_action.txt",pkg_dir);
      FILE *a=fopen(ap,"w"); if(a){ fputs("seq=0\ncmd=\n",a); fclose(a);} }
    demo(); write_canvas(); write_ui();
    int last=0;
    for(;;){
        kh_plat_sleep_ms(playing?50:200);
        poll_action(&last);
        if(playing){
            playhead += (bpm/60.0f) * (playing?0.05f:0) / 1.0f * 0.25f;
            /* ~0.05s * bpm/60 beats... keep simple: advance ~0.02 bar/tick */
            playhead += 0.02f * (bpm/120.0f);
            if(playhead >= BARS){ if(cycle) playhead=0; else { playhead=BARS; playing=0; } }
            dirty=1;
        }
        if(dirty){ write_canvas(); dirty=0; }
        write_ui();
    }
    return 0;
}
