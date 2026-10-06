#define _GNU_SOURCE
/* vm_rv_li_manager.c - X11-HQ manager for the "vm-rv-li" toy.
 *
 * Runs RVLV_EMUL's RV64GC guest (xv6 or Linux via --linux) and renders
 * the emulator's UART console as a terminal screen in its khtpm
 * window. Input is a pure relay - there is exactly one input path to
 * the guest: the emulator's stdin pipe. Both a live human and an
 * external agent feed it:
 *
 *   human  : the renderer's <cli_io> fires ops/vm_rv_li_input.sh
 *            'type' <pkg> <house> <typed_line>  ->  human_input.txt
 *   agent  : writes "type:<line>" / "paste:<line>" / "key:<byte>"
 *            lines to agent_input.txt
 *   this manager drains BOTH files each tick and writes the bytes to
 *   the emulator's stdin pipe. A human keypress and an agent-written
 *   line are byte-identical to the guest (house rule: relay, not xtest
 *   - see HQ-IQ-BOOK 00-compact §9).
 *
 * The emulator's stdout IS its UART TX (RVLV_EMUL uart.c fputc(stdout)
 * per received console byte, fflush'd); this manager reads that pipe
 * non-blocking into an 80x24 ANSI-stripped grid and publishes
 * state/ui.txt (row_N_text / rowcount / cursor_x / cursor_y / status).
 *
 * argv contract (renderer <module> launch, see launch_module() in
 * khtpm_core_render.c):  <house_root> <package_dir> [<id>]
 * env fallbacks: KHTPM_HOUSE / KHTPM_PKG (do not invent another order).
 *
 * Config is #.desktop/vm_rv_li/config.pdl, re-read each tick (house
 * rule §7: runtime-configurable, edit the .pdl, no rebuild). Seeds a
 * default on first run. See toy.pdl + vm-rv-li.xhtpm.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define PATH_BUF 4352
#define DEF_COLS 80
#define DEF_ROWS 24
#define POLL_US 200000  /* 200 ms, matches co-lab-hai cadence */
#define IN_CHUNK 64
#define LINE_BUF 16384

static char g_house[PATH_BUF];
static char g_package_dir[PATH_BUF];
static char g_desktop_dir[PATH_BUF];    /* <house>/#.desktop/vm_rv_li  */
static char g_config_path[PATH_BUF];
static char g_human_input[PATH_BUF];
static char g_agent_input[PATH_BUF];
static char g_ui_path[PATH_BUF];        /* <package_dir>/state/ui.txt  */
static char g_parent_pid_path[PATH_BUF];/* <package_dir>/module_parent.pid */
static char g_emu_log_path[PATH_BUF];

static char g_emu_root[PATH_BUF] = "";
static char g_emu_bin[PATH_BUF] = "build/main";
static char g_emu_args[LINE_BUF] =
    "--virtio-legacy -k images/xv6-kernel.bin -f images/xv6-fs.img";
static int g_cols = DEF_COLS;
static int g_rows = DEF_ROWS;

static char **g_grid = NULL;
static int g_cx = 0, g_cy = 0;
static int g_esc = 0;            /* non-zero while skipping an ESC sequence */
static int g_emu_running = 0;
static int g_emu_exited = 0;
static int g_emu_code = 0;

static pid_t g_child = -1;
static int g_stdin_wr = -1;      /* manager->child stdin  */
static int g_stdout_rd = -1;     /* child->manager stdout */

static volatile sig_atomic_t g_dying = 0;
static void on_signal(int s){ (void)s; g_dying = 1; }

static void path_join(char *out, size_t n, const char *a, const char *b){
    snprintf(out, n, "%s/%s", a, b);
}
static void mkdir_p_local(const char *path){
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++){
        if (*p == '/'){ *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}
static void xml_escape(const char *in, char *out, size_t n){
    size_t o = 0;
    for (const char *p = in; *p && o + 6 < n; p++){
        switch(*p){
            case '&':  o += (size_t)snprintf(out+o, n-o, "&amp;");  break;
            case '<':  o += (size_t)snprintf(out+o, n-o, "&lt;");    break;
            case '>':  o += (size_t)snprintf(out+o, n-o, "&gt;");    break;
            case '"':  o += (size_t)snprintf(out+o, n-o, "&quot;");  break;
            default:   out[o++] = *p;
        }
    }
    out[o] = '\0';
}

/* ---- config.pdl (KEY | VALUE), re-read each tick so edits apply live ---- */
static void write_default_config(void){
    char home[PATH_BUF];
    snprintf(home, sizeof(home), "%s", getenv("HOME") ? getenv("HOME") : "/home/debil");
    char default_emu[PATH_BUF];
    snprintf(default_emu, sizeof(default_emu), "%s/Desktop/github/RVLV_EMUL", home);
    FILE *f = fopen(g_config_path, "w");
    if (!f) return;
    fprintf(f, "# vm-rv-li emulator configuration. House rule §7: edit this\n"
               "# .pdl, reopen the window, no rebuild. emu_root points at the\n"
               "# RVLV_EMUL checkout; emu_args are passed straight to build/main.\n"
               "# xv6 (shipped images/) boots now; Linux needs Image + rootfs.img\n"
               "# under shared/ assets (see RVLV-ASSET-SOURCE-LOCATION.pdl).\n");
    fprintf(f, "emu_root | %s\n", default_emu);
    fprintf(f, "emu_bin  | build/main\n");
    fprintf(f, "emu_args | --virtio-legacy -k images/xv6-kernel.bin -f images/xv6-fs.img\n");
    fprintf(f, "cols     | 80\n");
    fprintf(f, "rows     | 24\n");
    fclose(f);
}
static void resolve_emu_root(void){
    /* house §7: asset pointer file for flexible RVLV_EMUL location
       if emu_root is a placeholder like "RVLV_EMUL_PATH", read the
       actual path from shared/RVLV-ASSET-SOURCE.pdl */
    if (strcmp(g_emu_root,"RVLV_EMUL_PATH")!=0) return;  /* not a pointer, keep as-is */

    char ptr_path[PATH_BUF];
    snprintf(ptr_path,sizeof ptr_path,"%s/shared/RVLV-ASSET-SOURCE.pdl",g_house);
    FILE *f=fopen(ptr_path,"r");
    if (!f) return;  /* no pointer file, leave as placeholder */

    char line[LINE_BUF];
    while (fgets(line,sizeof(line),f)){
        char *nl=strchr(line,'\n'); if(nl)*nl='\0';
        char *p=line;
        while(*p==' '||*p=='\t') p++;
        if (*p=='#'||*p=='\0') continue;
        char *k=p; char *pipe=strchr(p,'|');
        if (!pipe) continue;
        *pipe='\0'; char *v=pipe+1;
        while (*v==' '||*v=='\t') v++;
        char *e=v+strlen(v)-1;
        while (e>v && (*e==' '||*e=='\t'||*e=='\r')) *e--='\0';
        if (strcmp(k,"RVLV_EMUL_PATH")==0){
            /* resolve relative path against house root */
            char resolved[PATH_BUF];
            if (v[0]=='/'){
                snprintf(resolved,sizeof resolved,"%s",v);
            } else {
                snprintf(resolved,sizeof resolved,"%s/%s",g_house,v);
            }
            snprintf(g_emu_root,sizeof g_emu_root,"%s",resolved);
            break;
        }
    }
    fclose(f);
}

static void read_config(void){
    FILE *f = fopen(g_config_path, "r");
    if (!f){
        /* first run: seed, then re-open */
        write_default_config();
        f = fopen(g_config_path, "r");
        if (!f) return;
    }
    char line[LINE_BUF];
    while (fgets(line, sizeof(line), f)){
        char *nl = strchr(line,'\n'); if (nl)*nl='\0';
        char *p = line;
        while (*p==' '||*p=='\t') p++;
        if (*p=='#'||*p=='\0') continue;
        char *k=p; char *pipe=strchr(p,'|');
        if (!pipe) continue;
        *pipe='\0'; char *v=pipe+1;
        while (*v==' '||*v=='\t') v++;
        char *e=v+strlen(v)-1;
        while (e>v && (*e==' '||*e=='\t'||*e=='\r')) *e--='\0';
        while (pipe-1>k && (*(pipe-1)==' '||*(pipe-1)=='\t')) *(--pipe)='\0';
        if (strcmp(k,"emu_root")==0) snprintf(g_emu_root,sizeof g_emu_root,"%s",v);
        else if (strcmp(k,"emu_bin")==0) snprintf(g_emu_bin,sizeof g_emu_bin,"%s",v);
        else if (strcmp(k,"emu_args")==0) snprintf(g_emu_args,sizeof g_emu_args,"%s",v);
        else if (strcmp(k,"cols")==0)    { g_cols=atoi(v); if(g_cols<10)g_cols=DEF_COLS; if(g_cols>400)g_cols=400; }
        else if (strcmp(k,"rows")==0)   { g_rows=atoi(v); if(g_rows<5)g_rows=DEF_ROWS; if(g_rows>200)g_rows=200; }
    }
    fclose(f);
    resolve_emu_root();  /* §7: resolve any asset pointers */
}

static void grid_alloc(void){
    if (g_grid){ for(int i=0;i<g_rows;i++) free(g_grid[i]); free(g_grid); }
    g_grid = calloc(g_rows, sizeof(char*));
    for (int i=0;i<g_rows;i++){
        g_grid[i]=calloc(g_cols+1,1);
    }
    g_cx=0;g_cy=0;g_esc=0;
}
static void grid_scroll_up(void){
    for (int y=1;y<g_rows;y++) memmove(g_grid[y-1],g_grid[y],g_cols);
    memset(g_grid[g_rows-1],' ',g_cols);
    if (g_cy>0) g_cy--;
}
static void grid_putc(unsigned char c){
    if (g_esc){
        /* strip CSI/OSC/DSC: ESC [ ...final(0x40-0x7e), or ESC + one byte
           for non-[ private modes. OSC (ESC ]) and DCS (ESC P) also fall
           here and are skipped to their ST (ESC \). */
        if (g_esc==1){
            if (c=='['){ g_esc=2; return; }
            if (c=='P'||c==']'||c=='^'||c=='_'){ g_esc=3; return; }
            if (c>=0x40 && c<=0x7e){ g_esc=0; return; }   /* final of simple ESC */
            return;
        }
        if (g_esc==2){ if (c>=0x40 && c<=0x7e) g_esc=0; return; }
        if (g_esc==3){ if (c==0x1b) g_esc=4; return; }  /* ST = ESC \  (ESC then \) */
        if (g_esc==4){ if (c=='\\') g_esc=0; return; }  /* could also be BEL */
        return;
    }
    if (c==0x1b){ g_esc=1; return; }
    switch(c){
      case '\r': g_cx=0; return;
      case '\n':
        g_cy++; if(g_cy>=g_rows){grid_scroll_up();} else { if(g_cy>=g_rows)g_cy=g_rows-1; }
        g_cx=0; return;
      case '\b': if(g_cx>0)g_cx--; return;
      case '\t': g_cx=(g_cx+8)&~7; break;
      default:
        if (c<0x20) return;
    }
    if (g_cx>=g_cols){ g_cx=0; if(g_cy+1>=g_rows)grid_scroll_up(); else g_cy++; }
    if (g_cy>=g_rows){grid_scroll_up();g_cy=g_rows-1;}
    if (g_cy<0)g_cy=0;
    g_grid[g_cy][g_cx]=c;
    if(g_cx+1<g_cols) g_grid[g_cy][g_cx+1]=0;
    g_cx++;
    if(g_cx>=g_cols){ g_cx=0; if(g_cy+1>=g_rows)grid_scroll_up(); else g_cy++; }
}
static void grid_reset(void){ for(int y=0;y<g_rows;y++)memset(g_grid[y],' ',g_cols); g_cx=0;g_cy=0;g_esc=0; }

static int set_nonblock(int fd){
    int f=fcntl(fd,F_GETFL,0); if(f<0)return -1; return fcntl(fd,F_SETFL,f|O_NONBLOCK);
}

static int parent_still_alive(void){
    FILE *f=fopen(g_parent_pid_path,"r");
    if(!f) return 1;                 /* no file yet = don't assume dead */
    long pid=0;
    if(fscanf(f,"%ld",&pid)!=1){fclose(f);return 1;}
    fclose(f);
    if(pid<=0) return 1;
    return kill((int)pid,0)==0 || errno==EPERM;
}

static void drain_input_to_emu(void){
    const char *files[2] = { g_human_input, g_agent_input };
    for (int i=0;i<2;i++){
        FILE *f=fopen(files[i],"r");
        if(!f) continue;
        char *line=NULL; size_t lln=0; ssize_t n;
        int wrote=0;
        while ((n=getline(&line,&lln,f))>=0){
            char *s=line; while(*s=='\r'||*s=='\n') s++;  /* nothing */
            /* operate on the line content */
            char *src=line;
            char *nl=strchr(src,'\n'); if(nl)*nl='\0';
            nl=strchr(src,'\r'); if(nl)*nl='\0';
            if (strncmp(src,"type:",5)==0){
                const char *t=src+5;
                size_t tl=strlen(t);
                /* send the line verbatim then a newline */
                ssize_t w=write(g_stdin_wr,t,tl); (void)w;
                if (g_stdin_wr>=0) write(g_stdin_wr,"\n",1);
                wrote=1;
            } else if (strncmp(src,"paste:",6)==0){
                const char *t=src+6; size_t tl=strlen(t);
                write(g_stdin_wr,t,tl); wrote=1;
            } else if (strncmp(src,"key:",4)==0){
                int code=(int)strtol(src+4,NULL,10);
                if(code>=0 && code<=255){ unsigned char b=(unsigned char)code; write(g_stdin_wr,&b,1); wrote=1; }
            } else if (src[0]){  /* bare line = treat as typed text (no prefix) */
                size_t tl=strlen(src);
                write(g_stdin_wr,src,tl);
                write(g_stdin_wr,"\n",1);
                wrote=1;
            }
            free(line); line=NULL; lln=0;
        }
        free(line);
        if (g_stdin_wr>=0 && wrote){ /* nothing */ }
        fclose(f);
        /* truncate consumed file */
        FILE *w=fopen(files[i],"w"); if(w) fclose(w);
    }
}

static void read_emu_output(void){
    if (!g_emu_running) return;
    char b[256];
    for (;;){
        ssize_t n = read(g_stdout_rd, b, sizeof(b));
        if (n>0){ for(ssize_t i=0;i<n;i++) grid_putc((unsigned char)b[i]); }
        else if (n<0){ if(errno==EAGAIN||errno==EWOULDBLOCK) break; break; }   /* n==0 EOF => child closed stdout */
        else { g_emu_running=0; break; }
    }
}
static void reap_child(void){
    if (g_child<0) return;
    int st=0;
    pid_t r=waitpid(g_child,&st,WNOHANG);
    if (r==g_child){ g_emu_exited=1; g_emu_code=WIFEXITED(st)?WEXITSTATUS(st):0; g_emu_running=0; }
}

static void spawn_emulator(void){
    char full[PATH_BUF];
    snprintf(full,sizeof full,"%s/%s", g_emu_root, g_emu_bin);
    char log[PATH_BUF]; snprintf(log,sizeof log,"%s/emu.log", g_desktop_dir);

    int in_pipe[2];   /* child stdin  */
    int out_pipe[2];  /* child stdout */
    if (pipe(in_pipe)<0 || pipe(out_pipe)<0){ g_emu_running=0; return; }
    set_nonblock(out_pipe[0]); set_nonblock(in_pipe[1]);

    pid_t pid=fork();
    if (pid<0){ g_emu_running=0; return; }
    if (pid==0){
        /* child */
        setpgid(0,0);                 /* own process group, survives parent */
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        /* stderr -> log file */
        int lf=open(log,O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(lf>=0){ dup2(lf,STDERR_FILENO); close(lf); }
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        /* house rule §13: relative emu_args (images/xv6-kernel.bin) resolve
           against emu_root, NOT the inherited CWD - chdir first. */
        if (g_emu_root && g_emu_root[0]){ chdir(g_emu_root); }
        /* build argv from g_emu_args (tokenized on spaces) */
        char *argv[32]; int ac=0;
        argv[ac++]=strdup(full);
        char *save=NULL;
        char *t=strtok_r(g_emu_args," ",&save);
        while(t && ac<31){ argv[ac++]=strdup(t); t=strtok_r(NULL," ",&save); }
        argv[ac]=NULL;
        execv(full,argv);
        _exit(127);
    }
    /* parent */
    close(in_pipe[0]); close(out_pipe[1]);
    g_stdin_wr=in_pipe[1]; g_stdout_rd=out_pipe[0];
    g_child=pid; g_emu_running=1; g_emu_exited=0;
}

static int publish_ui(void){
    /* atomic tmp+rename (house §8) */
    char tmp[PATH_BUF];
    FILE *wf;
    snprintf(tmp,sizeof tmp,"%s.tmp", g_ui_path);
    wf=fopen(tmp,"w"); if(!wf) return -1;
    char esc[LINE_BUF];
    fprintf(wf, "app=vm-rv-li\n");
    fprintf(wf, "cols=%d\n", g_cols);
    fprintf(wf, "rows=%d\n", g_rows);
    fprintf(wf, "rowcount=%d\n", g_rows);
    /* emit only as many rows as the grid has; pad with blank lines so the
       repeat block always sees `rows` items. */
    for (int y=0;y<g_rows;y++){
        char line[PATH_BUF]; 
        snprintf(line,sizeof line,"%.*s", g_cols, g_grid[y]);
        xml_escape(line, esc, sizeof esc);
        fprintf(wf, "row_%d_text=%s\n", y, esc);
    }
    fprintf(wf, "cursor_x=%d\n", g_cx);
    fprintf(wf, "cursor_y=%d\n", g_cy);
    char st[128]="running";
    if (g_emu_exited) snprintf(st,sizeof st,"emulator exited (%d)", g_emu_code);
    else if (!g_emu_running && !g_emu_exited) {
        if (g_child<0) snprintf(st,sizeof st,"not started");
        else snprintf(st,sizeof st,"starting...");
    }
    fprintf(wf, "status=%s\n", st);
    fprintf(wf, "interact_armed=1\n");
    fprintf(wf, "input_action='%s/ops/vm_rv_li_input.sh' 'type'\n", g_package_dir);
    fclose(wf);
    rename(tmp, g_ui_path);
    return 0;
}

static void cleanup_emulator(void){
    if (g_child>=0){
        kill(-g_child, SIGTERM); sleep(0);
        kill(-g_child, SIGKILL);
    }
    if (g_stdin_wr>=0) close(g_stdin_wr);
    if (g_stdout_rd>=0) close(g_stdout_rd);
    g_child=-1; g_stdin_wr=-1; g_stdout_rd=-1;
}

int main(int argc, char **argv){
    if (argc < 2){
        fprintf(stderr,"usage: %s <house_root> <package_dir> [<id>]\n", argv[0]);
        return 1;
    }
    snprintf(g_house, sizeof g_house, "%s", argv[1]);
    if (argc>=3 && argv[2][0]){
        snprintf(g_package_dir, sizeof g_package_dir, "%s", argv[2]);
    } else {
        snprintf(g_package_dir, sizeof g_package_dir, "%s/@.apps/vm-rv-li", g_house);
    }
    /* env fallbacks (house §6) */
    const char *h=getenv("KHTPM_HOUSE"); if(h&&h[0]) snprintf(g_house,sizeof g_house,"%s",h);
    const char *p=getenv("KHTPM_PKG");   if(p&&p[0]) snprintf(g_package_dir,sizeof g_package_dir,"%s",p);

    path_join(g_desktop_dir, sizeof g_desktop_dir, g_house, "#.desktop/vm_rv_li");
    path_join(g_config_path, sizeof g_config_path, g_desktop_dir, "config.pdl");
    path_join(g_human_input, sizeof g_human_input, g_desktop_dir, "human_input.txt");
    path_join(g_agent_input, sizeof g_agent_input, g_desktop_dir, "agent_input.txt");
    path_join(g_emu_log_path, sizeof g_emu_log_path, g_desktop_dir, "emu.log");
    mkdir_p_local(g_desktop_dir);
    { char sd[PATH_BUF]; snprintf(sd,sizeof sd,"%s/state", g_package_dir); mkdir_p_local(sd); }
    path_join(g_ui_path, sizeof g_ui_path, g_package_dir, "state/ui.txt");
    path_join(g_parent_pid_path, sizeof g_parent_pid_path, g_package_dir, "module_parent.pid");

    read_config();
    grid_alloc(); grid_reset();

    struct sigaction sa; memset(&sa,0,sizeof sa);
    sa.sa_handler=on_signal; sigemptyset(&sa.sa_mask); sa.sa_flags=0;
    sigaction(SIGTERM,&sa,NULL); sigaction(SIGINT,&sa,NULL);

    /* first tick: seed ui so the window paints before the guest boots */
    publish_ui();

    /* spawn the emulator once (config edits apply on a window reopen) */
    spawn_emulator();
    publish_ui();

    for (;;){
        if (g_dying) break;
        reap_child();
        read_emu_output();
        drain_input_to_emu();
        read_config();                 /* §7: live-editable config */
        publish_ui();
        if (!parent_still_alive()){
            break;
        }
        if (g_dying) break;   /* SIGTERM/SIGINT from button.sh quit/kill */
        usleep(POLL_US);
    }

    cleanup_emulator();
    publish_ui();   /* final state: emulator stopped */
    return 0;
}
