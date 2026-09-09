/* test_proc_registry_tb.c — integration test for the taskbar-manager
 * wiring in PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md.
 *
 * It reproduces, verbatim, the two code paths added to
 * khtpm_taskbar_manager.c:
 *   - ktb_system_recorded(): run `<cmd> & echo $! > .livedesk_last_launch.pid`,
 *     then read that PID back and kh_proc_register_owned() it into the one
 *     proc-ledger (the legacy livedesk_launched_pids.txt is gone — DROP,
 *     PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md §2).
 *   - ktb_reap_launched(): kh_proc_reap_all(house_root, 200, 0).
 * against its own detached /tmp children — NO taskbar, NO house process.
 *
 *   cc -std=c11 -Wall -Wextra -O2 test_proc_registry_tb.c -o /tmp/tprtb && /tmp/tprtb
 */
#define _GNU_SOURCE
#define KH_PROC_REGISTRY_IMPL
#include "../kh_proc_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

static int failures = 0;
#define CHECK(c,m) do{ if(c) printf("  ok   %s\n",m); else {printf("  FAIL %s\n",m); failures++;} }while(0)
static int alive(long p){ return kill((pid_t)p,0)==0 || errno==EPERM; }
static void msleep(int ms){ struct timespec t={ms/1000,(long)(ms%1000)*1000000L}; nanosleep(&t,NULL); }

/* verbatim shape of ktb_system_recorded() from khtpm_taskbar_manager.c */
static void tb_system_recorded(const char *house_root, const char *cmd) {
    char pidfile[2048];
    snprintf(pidfile, sizeof(pidfile),
             "%s/#.desktop/.livedesk_last_launch.pid", house_root);
    char wrapped[4096];
    snprintf(wrapped, sizeof(wrapped),
             "%s echo $! > \"%s\"", cmd, pidfile);
    int rc = system(wrapped); (void)rc;
    FILE *pf = fopen(pidfile, "r");
    if (pf) {
        char ln[64]; long last = 0;
        while (fgets(ln, sizeof(ln), pf)) { long v = strtol(ln, NULL, 10); if (v > 1) last = v; }
        fclose(pf);
        if (last > 1)
            kh_proc_register_owned(house_root, last, last,
                                   (long)getpid(), "tb-launch");
    }
}
/* verbatim shape of ktb_reap_launched() */
static void tb_reap_launched(const char *house_root) { kh_proc_reap_all(house_root, 200, 0); }

static long file_line_count(const char *p) {
    FILE *f = fopen(p, "r"); if (!f) return -1;
    long n = 0; int c; while ((c = fgetc(f)) != EOF) if (c == '\n') n++;
    fclose(f); return n;
}

int main(void) {
    char tmpl[] = "/tmp/tprtb_XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) { perror("mkdtemp"); return 2; }
    char desk[2200]; snprintf(desk, sizeof(desk), "%s/#.desktop", dir);
    if (mkdir(desk, 0755)) { perror("mkdir"); return 2; }
    printf("fake house: %s\n\n", dir);

    /* 3 "launches" through the exact recorded-launch path. Use the same
     * setsid + trailing-& shape every real ktb launch site uses. */
    printf("[launch] 3 detached children via tb_system_recorded()\n");
    for (int i = 0; i < 3; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "setsid nohup sleep 600 >/dev/null 2>&1 &");
        tb_system_recorded(dir, cmd);
    }
    msleep(150);

    /* the one proc-ledger now has all 3 entries; the scratch pidfile
     * holds only the most-recent launch (single line, overwritten). */
    char lastpid[2200]; snprintf(lastpid,sizeof(lastpid),"%s/#.desktop/.livedesk_last_launch.pid",dir);
    char rp[2048]; kh_proc_registry_path(dir, rp, sizeof(rp));
    long reg_lines = 0; { FILE *f = fopen(rp, "r"); if (f){ int c; while((c=fgetc(f))!=EOF) if(c=='\n') reg_lines++; fclose(f);} }
    CHECK(file_line_count(lastpid) == 1, "scratch .livedesk_last_launch.pid is single-line (overwritten)");
    CHECK(reg_lines == 3, "canonical livedesk_proc_list.txt has 3 lines");

    /* collect the 3 pids from the proc-ledger (field 1) to check liveness */
    long pids[3] = {0,0,0};
    { FILE *f = fopen(rp,"r"); char ln[128]; int k=0;
      while (f && k<3 && fgets(ln,sizeof(ln),f)) pids[k++] = strtol(ln,NULL,10);
      if (f) fclose(f); }
    CHECK(alive(pids[0]) && alive(pids[1]) && alive(pids[2]), "all 3 launched children alive");

    /* the quit-path reap */
    printf("\n[quit] tb_reap_launched()\n");
    tb_reap_launched(dir);
    for (int i=0;i<60 && (alive(pids[0])||alive(pids[1])||alive(pids[2]));i++) msleep(20);
    for (int i=0;i<3;i++) waitpid((pid_t)pids[i], NULL, WNOHANG);
    CHECK(!alive(pids[0]) && !alive(pids[1]) && !alive(pids[2]), "all 3 dead after reap");
    { FILE *f=fopen(rp,"r"); long n=0; int c; if(f){while((c=fgetc(f))!=EOF) if(c=='\n') n++; fclose(f);}
      CHECK(n==0, "canonical registry truncated after reap"); }

    /* prune on next 'startup' with one live + one stale line */
    printf("\n[startup] kh_proc_registry_prune keeps live, drops stale\n");
    long live = -1; { pid_t p=fork(); if(p==0){ setsid(); for(;;) pause(); } live=p; }
    msleep(100);
    kh_proc_register(dir, live, live, "survivor");
    { FILE *f=fopen(rp,"a"); fprintf(f,"%d %d 999999 stale-line\n", 999001, 999001); fclose(f); }
    int kept = kh_proc_registry_prune(dir);
    CHECK(kept == 1, "prune kept exactly the 1 live entry");
    CHECK(alive(live), "the live 'survivor' was not touched by prune");
    kh_proc_registry_reset(dir);
    kill((pid_t)live, SIGKILL); waitpid((pid_t)live, NULL, WNOHANG);

    char rm[2300]; snprintf(rm,sizeof(rm),"rm -rf '%s'",dir);
    if (system(rm)) fprintf(stderr,"warn: cleanup failed\n");
    printf("\n%s (%d failure%s)\n", failures?"FAILED":"ALL PASS", failures, failures==1?"":"s");
    return failures ? 1 : 0;
}
