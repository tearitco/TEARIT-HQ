/* test_proc_registry_master.c — the master-ledger additions
 * (PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md §5 step 4):
 *   kh_proc_register_owned / kh_proc_reap_subtree /
 *   kh_proc_self_register / kh_proc_self_unregister, and 4+5-field
 *   ledger-line coexistence.
 * Desktop-safe: spawns its own detached /tmp children.
 *
 *   cc -std=c11 -Wall -Wextra -O2 test_proc_registry_master.c -o /tmp/tprm && /tmp/tprm
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

static long spawn_child(void){
    pid_t p=fork();
    if(p==0){ setsid(); for(;;) pause(); _exit(0); }
    return (long)p;
}
static long ledger_lines(const char *hr){
    char p[2048]; kh_proc_registry_path(hr,p,sizeof(p));
    FILE *f=fopen(p,"r"); if(!f) return -1;
    long n=0; int c,prev='\n';
    while((c=fgetc(f))!=EOF){ if(c=='\n') n++; prev=c; }
    if(prev!='\n') n++;
    fclose(f); return n;
}
static int ledger_has_pid(const char *hr, long pid){
    char p[2048]; kh_proc_registry_path(hr,p,sizeof(p));
    FILE *f=fopen(p,"r"); if(!f) return 0;
    char line[512]; long a; int hit=0;
    while(fgets(line,sizeof(line),f)) if(sscanf(line,"%ld",&a)==1 && a==pid){ hit=1; break; }
    fclose(f); return hit;
}

int main(void){
    char tmpl[]="/tmp/tprm_XXXXXX"; char *dir=mkdtemp(tmpl);
    if(!dir){ perror("mkdtemp"); return 2; }
    char desk[2200]; snprintf(desk,sizeof(desk),"%s/#.desktop",dir);
    if(mkdir(desk,0755)){ perror("mkdir"); return 2; }
    printf("fake house: %s\n\n",dir);

    /* ---- case 1: reap_subtree — master A dies, master B lives ---- */
    printf("[case 1] kh_proc_reap_subtree(A): only A's children die\n");
    long A=100001, B=100002;              /* synthetic owner pids */
    long a1=spawn_child(), a2=spawn_child(), b1=spawn_child(), b2=spawn_child();
    msleep(120);
    kh_proc_register_owned(dir,a1,a1,A,"A-child-1");
    kh_proc_register_owned(dir,a2,a2,A,"A-child-2");
    kh_proc_register_owned(dir,b1,b1,B,"B-child-1");
    kh_proc_register_owned(dir,b2,b2,B,"B-child-2");
    CHECK(ledger_lines(dir)==4, "ledger has 4 owned entries");
    int sig=kh_proc_reap_subtree(dir,A,150,1);
    for(int i=0;i<50 && (alive(a1)||alive(a2));i++) msleep(20);
    waitpid((pid_t)a1,NULL,WNOHANG); waitpid((pid_t)a2,NULL,WNOHANG);
    CHECK(sig==2, "reap_subtree signalled 2 (A's children)");
    CHECK(!alive(a1) && !alive(a2), "A's children dead");
    CHECK(alive(b1) && alive(b2), "B's children untouched");
    CHECK(ledger_lines(dir)==2, "ledger keeps B's 2 lines");
    CHECK(ledger_has_pid(dir,b1) && ledger_has_pid(dir,b2), "the 2 kept lines are B's");
    /* cleanup B */
    kh_proc_registry_reset(dir);
    kh_proc_register_owned(dir,b1,b1,B,"b"); kh_proc_register_owned(dir,b2,b2,B,"b");
    kh_proc_reap_all(dir,150,0);
    for(int i=0;i<50 && (alive(b1)||alive(b2));i++) msleep(20);
    waitpid((pid_t)b1,NULL,WNOHANG); waitpid((pid_t)b2,NULL,WNOHANG);

    /* ---- case 2: reap the app-root line itself via its own pid ---- */
    printf("\n[case 2] reap_subtree(pid) also removes the row FOR that pid\n");
    kh_proc_registry_reset(dir);
    long root=spawn_child(); long kid=spawn_child();
    msleep(120);
    kh_proc_register_owned(dir,root,root,0,"app-root");   /* master 0: it's a root */
    kh_proc_register_owned(dir,kid,kid,root,"app-kid");   /* owned by root        */
    int s2=kh_proc_reap_subtree(dir,root,150,1);
    for(int i=0;i<50 && (alive(root)||alive(kid));i++) msleep(20);
    waitpid((pid_t)root,NULL,WNOHANG); waitpid((pid_t)kid,NULL,WNOHANG);
    CHECK(s2==2, "reap_subtree(root) signalled root + kid");
    CHECK(!alive(root) && !alive(kid), "root and kid both dead");
    CHECK(ledger_lines(dir)<=0, "ledger empty after");

    /* ---- case 3: self-register / self-unregister from a child ---- */
    printf("\n[case 3] kh_proc_self_register / _unregister\n");
    kh_proc_registry_reset(dir);
    int up[2], dn[2];                 /* up = child->parent, dn = parent->child */
    if(pipe(up)!=0 || pipe(dn)!=0){ perror("pipe"); return 2; }
    pid_t sc=fork();
    if(sc==0){
        close(up[0]); close(dn[1]);
        kh_proc_self_register(dir,"self-kid");
        char x=1; ssize_t w=write(up[1],&x,1); (void)w;      /* "registered" */
        char buf; ssize_t r=read(dn[0],&buf,1); (void)r;     /* wait for "go" */
        kh_proc_self_unregister(dir);
        _exit(0);
    }
    close(up[1]); close(dn[0]);
    { char b; ssize_t r=read(up[0],&b,1); (void)r; }
    CHECK(ledger_has_pid(dir,(long)sc), "child's self-registered line is present");
    { long ll=ledger_lines(dir); CHECK(ll==1, "exactly 1 line while child lives"); }
    { char go=1; ssize_t w=write(dn[1],&go,1); (void)w; }
    waitpid(sc,NULL,0);
    msleep(50);
    close(up[0]); close(dn[1]);
    CHECK(!ledger_has_pid(dir,(long)sc), "child's line gone after self-unregister");

    /* ---- case 4: 4-field (v1) + 5-field lines coexist ---- */
    printf("\n[case 4] v1 4-field + master 5-field lines both reaped by reap_all\n");
    kh_proc_registry_reset(dir);
    long f4=spawn_child(), f5=spawn_child();
    msleep(120);
    { /* write a v1 4-field line by hand for f4 */
      char p[2048]; kh_proc_registry_path(dir,p,sizeof(p));
      FILE *f=fopen(p,"a"); fprintf(f,"%ld %ld 0 v1-4field\n", f4, f4); fclose(f);
    }
    kh_proc_register_owned(dir,f5,f5,777,"v2-5field");
    int s4=kh_proc_reap_all(dir,150,1);
    for(int i=0;i<50 && (alive(f4)||alive(f5));i++) msleep(20);
    waitpid((pid_t)f4,NULL,WNOHANG); waitpid((pid_t)f5,NULL,WNOHANG);
    CHECK(s4==2, "reap_all signalled both the 4-field and 5-field entries");
    CHECK(!alive(f4) && !alive(f5), "both dead");

    char rm[2300]; snprintf(rm,sizeof(rm),"rm -rf '%s'",dir);
    if(system(rm)) fprintf(stderr,"warn: cleanup failed\n");
    printf("\n%s (%d failure%s)\n", failures?"FAILED":"ALL PASS", failures, failures==1?"":"s");
    return failures?1:0;
}
