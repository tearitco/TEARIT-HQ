/* media_daw_hq_manager.c - <module> backend for media-daw-hq.xhtpm.
 *
 * CONVERSION SKELETON 2026-09-08. Logic/GarageBand-shaped DAW: arrangement lanes on top, piano roll below, transport + BPM, mixer drawer.
 * Source app being converted: 44.xyz.01.00/103.media-studio/103.daw/
 *   (out-of-house-spec: own button.sh + ops/*_main.c GL binary).
 * Target: house x11-HQ toy - thin manager + static .xhtpm rendered by
 * khtpm_core_render.+x. Migration, not redesign - keep every feature
 * in 103.media-studio/103.daw/HOW2_DAW.md . See ../README.md for the plan.
 *
 * Forked as: media_daw_hq_manager.+x <house_root> <package_dir> [id]
 * Publishes <pkg>/state/media_daw_hq_ui.txt ; polls
 * <pkg>/state/media_daw_hq_action.txt (seq=N / cmd=...).
 *
 * TODO(grok): everything. This stub only proves the app launches,
 * shows in HQ toys, and round-trips one action. Build the real
 * layout/state next, one HOW2 feature at a time.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define KH_PLAT_IMPL
#include "../../../&.widgits/_shared-lib/kh_plat.h"
#define PL 4096
static char pkg_dir[PL];
static char g_msg[128] = "";
static long long g_ticks = 0;
static void bye(int s){ (void)s; _exit(0); }

static void write_ui(void){
    char dir[PL], tmp[PL], dst[PL];
    snprintf(dir,sizeof(dir),"%s/state",pkg_dir); kh_plat_mkdir_p(dir);
    snprintf(dst,sizeof(dst),"%s/media_daw_hq_ui.txt",dir);
    snprintf(tmp,sizeof(tmp),"%s/media_daw_hq_ui.txt.tmp",dir);
    FILE *f=fopen(tmp,"w"); if(!f) return;
    fprintf(f,"title=Muchi DAW\n");
    fprintf(f,"banner=CONVERSION IN PROGRESS - see @.apps/media-daw-hq/README.md\n");
    fprintf(f,"source=103.media-studio/103.daw/  (HOW2_DAW.md)\n");
    fprintf(f,"ticks=%lld\n",(long long)g_ticks);
    fprintf(f,"msg=%s\n",g_msg);
    fclose(f); rename(tmp,dst);
}
static void poll_action(int *last){
    char p[PL]; snprintf(p,sizeof(p),"%s/state/media_daw_hq_action.txt",pkg_dir);
    FILE *f=fopen(p,"r"); if(!f) return;
    char line[PL], cmd[128]=""; int seq=0;
    while(fgets(line,sizeof(line),f)){
        if(sscanf(line,"seq=%d",&seq)==1) continue;
        if(!strncmp(line,"cmd=",4)){ snprintf(cmd,sizeof(cmd),"%s",line+4); char *n=strchr(cmd,'\n'); if(n)*n=0; }
    }
    fclose(f);
    if(seq==*last||!cmd[0]) return;
    *last=seq;
    snprintf(g_msg,sizeof(g_msg),"got: %s (stub)",cmd);
}
int main(int argc,char**argv){
    if(argc<3){ fprintf(stderr,"usage: media_daw_hq_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(pkg_dir,sizeof(pkg_dir),"%s",argv[2]);
    kh_plat_on_terminate(bye);
    char sd[PL]; snprintf(sd,sizeof(sd),"%s/state",pkg_dir); kh_plat_mkdir_p(sd);
    { char ap[PL]; snprintf(ap,sizeof(ap),"%s/state/media_daw_hq_action.txt",pkg_dir);
      FILE *a=fopen(ap,"w"); if(a){ fputs("seq=0\ncmd=\n",a); fclose(a); } }
    write_ui();
    int last=0;
    for(;;){ kh_plat_sleep_ms(200); g_ticks++; poll_action(&last); write_ui(); }
    return 0;
}
