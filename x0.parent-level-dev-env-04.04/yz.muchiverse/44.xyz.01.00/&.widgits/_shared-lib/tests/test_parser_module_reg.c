/* test_parser_module_reg.c — PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md §3.
 * Reproduces chtpm_parser_pal.c's prisc-VM ledger hooks VERBATIM:
 *   launch_module()  parent branch  -> kh_pal_register_module(pid,"prisc")
 *   cleanup_module()                -> waitpid + kh_pal_reap_module(pid)
 *   handle_sigint(): cleanup_extra_modules() -> SIGTERM + reap each extra
 * plus: a house-wide kh_proc_reap_all() (taskbar-quit path) also gets a
 * still-registered module whose parser was hard-killed.
 * Desktop-safe: only /tmp children.
 *
 *   cc -std=c11 -Wall -Wextra -O2 test_parser_module_reg.c -o /tmp/tpmr && /tmp/tpmr
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

static const char *G_HR = "";
/* mirror of chtpm_parser_pal.c's thin wrappers (house root is fixed here) */
static void kh_pal_register_module(pid_t p, const char *tag){
    if (G_HR[0] && p > 1)
        kh_proc_register_owned(G_HR,(long)p,(long)p,(long)getpid(),
                               (tag && tag[0]) ? tag : "prisc");
}
static void kh_pal_reap_module(pid_t p){
    if (G_HR[0] && p > 1) kh_proc_reap_one(G_HR,(long)p,1);
}

static long spawn_sleeper(void){
    pid_t p=fork();
    if(p==0){ /* NOT setsid: a <module> child stays in the parser's group */
        for(;;) pause();
        _exit(0);
    }
    return (long)p;
}
static int ledger_has_pid(long pid){
    char path[2048]; kh_proc_registry_path(G_HR,path,sizeof(path));
    FILE *f=fopen(path,"r"); if(!f) return 0;
    char line[512]; long a; int hit=0;
    while(fgets(line,sizeof(line),f)) if(sscanf(line,"%ld",&a)==1 && a==pid){ hit=1; break; }
    fclose(f); return hit;
}

int main(void){
    char tmpl[]="/tmp/tpmr_XXXXXX"; char *dir=mkdtemp(tmpl);
    if(!dir){ perror("mkdtemp"); return 2; }
    char desk[2200]; snprintf(desk,sizeof(desk),"%s/#.desktop",dir);
    if(mkdir(desk,0755)){ perror("mkdir"); return 2; }
    G_HR = dir;
    printf("fake house: %s\n\n",dir);

    /* ── 1. launch_module(): register on fork ─────────────────────── */
    long m1 = spawn_sleeper();
    kh_pal_register_module((pid_t)m1,"prisc");
    CHECK(ledger_has_pid(m1), "launch_module: module row added to ledger");
    CHECK(alive(m1),          "launch_module: module still running");

    /* ── 2. cleanup_module(): kill + reap drops the row ───────────── */
    kill((pid_t)m1, SIGTERM);
    waitpid((pid_t)m1, NULL, 0);
    kh_pal_reap_module((pid_t)m1);
    CHECK(!ledger_has_pid(m1), "cleanup_module: module row removed");
    CHECK(!alive(m1),          "cleanup_module: module reaped");

    /* ── 3. taskbar-quit path: reap_all gets an orphaned module ───── */
    long m2 = spawn_sleeper();
    kh_pal_register_module((pid_t)m2,"prisc");
    CHECK(ledger_has_pid(m2), "orphan case: module registered");
    /* parser hard-killed => never ran cleanup_module; simulate a
     * house-wide quit calling kh_proc_reap_all (ktb_reap_launched). */
    kh_proc_reap_all(G_HR, 200, 0);
    msleep(50);
    CHECK(!alive(m2),          "reap_all: orphaned module killed");
    CHECK(!ledger_has_pid(m2), "reap_all: orphaned module row cleared");

    /* ── 4. cleanup_extra_modules(): SIGTERM + reap each extra ────── */
    long e1 = spawn_sleeper(), e2 = spawn_sleeper();
    kh_pal_register_module((pid_t)e1,"prisc-x");
    kh_pal_register_module((pid_t)e2,"prisc-x");
    CHECK(ledger_has_pid(e1) && ledger_has_pid(e2), "extra modules registered");
    /* verbatim cleanup_extra_modules() body */
    long ex[2] = { e1, e2 };
    for (int i=0;i<2;i++){
        pid_t p=(pid_t)ex[i];
        if (p>0){ kill(p,SIGTERM); waitpid(p,NULL,WNOHANG); kh_pal_reap_module(p); }
    }
    msleep(50);
    CHECK(!alive(e1) && !alive(e2), "cleanup_extra_modules: both killed");
    CHECK(!ledger_has_pid(e1) && !ledger_has_pid(e2), "cleanup_extra_modules: rows cleared");

    /* ── 5. no house root => every hook is a no-op (standalone build) */
    const char *save=G_HR; G_HR="";
    long s1 = spawn_sleeper();
    kh_pal_register_module((pid_t)s1,"prisc");   /* must not crash / write */
    kh_pal_reap_module((pid_t)s1);               /* must not kill it */
    CHECK(alive(s1), "empty house root: register/reap are no-ops");
    kill((pid_t)s1,SIGKILL); waitpid((pid_t)s1,NULL,0);
    G_HR=save;

    printf("\n%s (%d failure%s)\n", failures?"FAILURES":"ALL PASS",
           failures, failures==1?"":"s");
    return failures ? 1 : 0;
}
