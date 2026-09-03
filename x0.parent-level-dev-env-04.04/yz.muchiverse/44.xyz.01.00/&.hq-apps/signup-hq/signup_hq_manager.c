#define _POSIX_C_SOURCE 200809L
/* signup_hq_manager.c - real manager for "signup-hq", a proper x11-hq
 * window for creating an account (replaces the cramped one-line cli_io
 * strip modal the USER cell used to pop).
 *
 * Same proven contract as co-lab-hai / network-browser / open-hai:
 *   - launched as a <module> child of the shared khtpm_core_render.+x
 *     (button.sh -> renderer -> generic launch_module())
 *   - argv[1] = house_root, everything else derived
 *   - every ~150ms: read one line from #.desktop/signup_hq/request.txt
 *     (truncate it), act, then write plain key=value lines to
 *     <pkg>/state/ui.txt - the STATIC signup-hq.chtpm template does the
 *     layout (${var} + show=), this manager never emits markup
 *     (CHTPM-ARCHITECTURE-FIX.md)
 *   - exit when the parent renderer's module_parent.pid is gone
 *
 * The actual account work is NOT reinvented here: on submit it shells
 * out to the SAME binaries the taskbar already used -
 *   cd <login_root> && ./ops/+x/userpal_create_account.+x <id> <name>
 *                    && ./ops/+x/userpal_login.+x <id>
 * (the `cd` is required - see khtpm_taskbar_manager.c ktb_cliio_submit).
 *
 * Stages:  0 enter-username  ->  1 enter-display-name  ->  2 creating
 *          ->  3 done (welcome + "open the person icon for an avatar")
 * If someone is already signed in at startup, jumps straight to a
 * "you're already signed in" screen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

#define PB 1024

static char g_house[PB];
static char g_pkg[PB];          /* <house>/&.hq-apps/signup-hq            */
static char g_login[PB];        /* <house>/<0.user-pal*>/00.login-signup  */
static char g_state[PB];        /* <house>/#.desktop/signup_hq            */
static char g_req[PB];          /* <state>/request.txt                    */
static char g_out[PB];          /* <pkg>/state/ui.txt  (key=value state)  */
static char g_curlogin[PB];     /* <login>/current_login.txt             */

static int   g_stage = 0;
static char  g_id[64]  = "";
static char  g_name[96] = "";
static char  g_err[200] = "";
static long  g_submit_at = 0;
static char  g_already[64] = "";   /* set if already signed in at startup */

/* ---------- tiny helpers -------------------------------------------- */

static void chomp(char *s) {
    size_t l = strlen(s);
    while (l && (s[l-1]=='\n'||s[l-1]=='\r')) s[--l]=0;
}
static long now_s(void) { return (long)time(NULL); }

static size_t slurp(const char *p, char *b, size_t n) {
    FILE *f = fopen(p, "r");
    if (!f) { b[0]=0; return 0; }
    size_t r = fread(b, 1, n-1, f);
    fclose(f); b[r]=0; return r;
}

static void xesc(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o < n-7; p++) {
        switch (*p) {
            case '&':  memcpy(out+o,"&amp;",5);  o+=5; break;
            case '<':  memcpy(out+o,"&lt;",4);   o+=4; break;
            case '>':  memcpy(out+o,"&gt;",4);   o+=4; break;
            case '"':  memcpy(out+o,"&quot;",6); o+=6; break;
            case '\'': memcpy(out+o,"&#39;",5);  o+=5; break;
            default: out[o++] = *p;
        }
    }
    out[o] = 0;
}

/* find a child dir of `parent` whose name begins with `prefix` */
static int find_child_prefixed(const char *parent, const char *prefix, char *out, size_t n) {
    DIR *d = opendir(parent);
    if (!d) return 0;
    struct dirent *e; int found = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0]=='.') continue;
        if (strncmp(e->d_name, prefix, strlen(prefix)) == 0) {
            snprintf(out, n, "%s/%s", parent, e->d_name); found = 1; break;
        }
    }
    closedir(d);
    return found;
}

/* read current_user_id=... from current_login.txt (empty if guest) */
static void read_current_user(char *out, size_t n) {
    char buf[512];
    out[0] = 0;
    if (slurp(g_curlogin, buf, sizeof(buf)) == 0) return;
    char *p = strstr(buf, "current_user_id=");
    if (!p) return;
    p += strlen("current_user_id=");
    size_t i = 0;
    while (*p && *p != '\n' && *p != '\r' && i < n-1) out[i++] = *p++;
    out[i] = 0;
}

static int valid_id(const char *s, char *why, size_t wn) {
    size_t l = strlen(s);
    if (l < 1)  { snprintf(why, wn, "Pick a username."); return 0; }
    if (l > 32) { snprintf(why, wn, "Too long (32 max)."); return 0; }
    if (s[0] == '-') { snprintf(why, wn, "Can't start with a dash."); return 0; }
    for (const char *p = s; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
            snprintf(why, wn, "Letters, numbers, _ and - only (no spaces).");
            return 0;
        }
    return 1;
}

/* ---------- account creation (shell out, same as the taskbar) ------ */

static void kick_off_create(void) {
    char cmd[PB*3];
    snprintf(cmd, sizeof(cmd),
        "setsid nohup sh -c 'cd \"%s\" && \"./ops/+x/userpal_create_account.+x\" \"%s\" \"%s\" "
        "&& \"./ops/+x/userpal_login.+x\" \"%s\"' >/dev/null 2>&1 &",
        g_login, g_id, g_name, g_id);
    int rc = system(cmd);
    (void)rc;
    g_submit_at = now_s();
}

/* ---------- request handling -------------------------------------- */

static void handle_request(void) {
    char line[1024];
    FILE *f = fopen(g_req, "r");
    if (!f) return;
    int got = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    { FILE *c = fopen(g_req, "w"); if (c) fclose(c); }   /* consume */
    if (!got) return;
    chomp(line);
    if (!line[0]) return;

    if (strncmp(line, "setid:", 6) == 0) {
        if (g_stage != 0) return;
        char why[200];
        const char *v = line + 6;
        if (!valid_id(v, why, sizeof(why))) { snprintf(g_err, sizeof(g_err), "%s", why); return; }
        snprintf(g_id, sizeof(g_id), "%s", v);
        g_err[0] = 0;
        g_stage = 1;
    } else if (strncmp(line, "setname:", 8) == 0) {
        if (g_stage != 1) return;
        const char *v = line + 8;
        if (!v[0]) { snprintf(g_err, sizeof(g_err), "Enter a display name."); return; }
        snprintf(g_name, sizeof(g_name), "%.95s", v);
        g_err[0] = 0;
        g_stage = 2;
        kick_off_create();
    } else if (strcmp(line, "restart:") == 0) {
        g_stage = 0; g_id[0] = 0; g_name[0] = 0; g_err[0] = 0;
    } else if (strcmp(line, "back:") == 0) {
        if (g_stage == 1) { g_stage = 0; g_err[0] = 0; }
    }
}

/* ---------- state projection ------------------------------------- *
 * Write plain key=value lines to <pkg>/state/ui.txt. The static
 * signup-hq.chtpm template turns these into the actual UI:
 *   ${title} ${hint} ${id_shown} ${field_label} ${field_verb}
 *   ${field_tid} ${err} ${btn_label} ${btn_verb}
 *   show_hint / show_id_line / show_field / show_err / show_rules /
 *   show_btn  ("1" or "0", consumed by the template's show="${...}")
 * Values that can carry user text (id / name / already) are XML-attr
 * escaped here exactly as the old markup path did - the renderer runs
 * decode_entities() on label= after substitution, so it round-trips. */

static char *g_last = NULL;

static void write_state(void) {
    char buf[8192];
    size_t o = 0;
    #define K(...) do { o += snprintf(buf+o, sizeof(buf)-o, __VA_ARGS__); } while (0)

    char title[256] = "", hint[256] = "", id_shown[128] = "";
    char field_label[64] = "", field_verb[16] = "", field_tid[16] = "";
    char err_esc[256] = "";
    char btn_label[32] = "", btn_verb[16] = "";
    int show_hint = 0, show_id_line = 0, show_field = 0;
    int show_err = 0, show_rules = 0, show_btn = 0;

    if (g_err[0]) { xesc(g_err, err_esc, sizeof(err_esc)); show_err = 1; }

    if (g_already[0]) {
        char e[128]; xesc(g_already, e, sizeof(e));
        snprintf(title, sizeof(title), "You're already signed in as %s.", e);
    } else if (g_stage == 0) {
        snprintf(title, sizeof(title), "Create your account");
        snprintf(hint, sizeof(hint), "Step 1 of 2  -  choose a username."); show_hint = 1;
        snprintf(field_label, sizeof(field_label), "username: ");
        snprintf(field_verb, sizeof(field_verb), "setid");
        snprintf(field_tid, sizeof(field_tid), "idf");
        show_field = 1;
        show_rules = 1;
    } else if (g_stage == 1) {
        xesc(g_id, id_shown, sizeof(id_shown)); show_id_line = 1;
        snprintf(title, sizeof(title), "Create your account");
        snprintf(hint, sizeof(hint), "Step 2 of 2  -  a display name (what other people see)."); show_hint = 1;
        snprintf(field_label, sizeof(field_label), "display name: ");
        snprintf(field_verb, sizeof(field_verb), "setname");
        snprintf(field_tid, sizeof(field_tid), "nmf");
        show_field = 1;
        snprintf(btn_label, sizeof(btn_label), "back");
        snprintf(btn_verb, sizeof(btn_verb), "back");
        show_btn = 1;
    } else if (g_stage == 2) {
        snprintf(title, sizeof(title), "Creating your account...");
        snprintf(btn_label, sizeof(btn_label), "start over");
        snprintf(btn_verb, sizeof(btn_verb), "restart");
        show_btn = 1;
    } else { /* stage 3 done */
        char nm[128]; xesc(g_name[0] ? g_name : g_id, nm, sizeof(nm));
        snprintf(title, sizeof(title), "Welcome, %s!", nm);
        snprintf(hint, sizeof(hint),
                 "You're signed in. Next: open the person icon up top to build an avatar.");
        show_hint = 1;
    }

    K("title=%s\n", title);
    K("hint=%s\n", hint);
    K("id_shown=%s\n", id_shown);
    K("field_label=%s\n", field_label);
    K("field_verb=%s\n", field_verb);
    K("field_tid=%s\n", field_tid);
    K("err=%s\n", err_esc);
    K("btn_label=%s\n", btn_label);
    K("btn_verb=%s\n", btn_verb);
    K("show_hint=%d\n", show_hint);
    K("show_id_line=%d\n", show_id_line);
    K("show_field=%d\n", show_field);
    K("show_err=%d\n", show_err);
    K("show_rules=%d\n", show_rules);
    K("show_btn=%d\n", show_btn);
    #undef K

    if (g_last && strcmp(g_last, buf) == 0) return;
    free(g_last);
    g_last = strdup(buf);

    char tmp[PB];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_out);
    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    fputs(buf, wf);
    fclose(wf);
    rename(tmp, g_out);
}

/* ---------- parent-alive ---------------------------------------- */

static int parent_alive(void) {
    char p[PB], b[64];
    snprintf(p, sizeof(p), "%s/module_parent.pid", g_pkg);
    if (slurp(p, b, sizeof(b)) == 0) return 1;      /* not written yet */
    long pid = atol(b);
    if (pid <= 0) return 1;
    return kill((int)pid, 0) == 0 || errno == EPERM;
}

/* ---------- main ---------------------------------------------- */

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root>\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    { size_t l = strlen(g_house); if (l>1 && g_house[l-1]=='/') g_house[l-1]=0; }

    snprintf(g_pkg, sizeof(g_pkg), "%s/&.hq-apps/signup-hq", g_house);
    { char sd[PB]; snprintf(sd, sizeof(sd), "%s/state", g_pkg); mkdir(sd, 0777); }
    snprintf(g_out, sizeof(g_out), "%s/state/ui.txt", g_pkg);

    char userpal[PB];
    if (!find_child_prefixed(g_house, "0.user-pal", userpal, sizeof(userpal))) {
        fprintf(stderr, "signup_hq_manager: no 0.user-pal* dir under house\n");
        return 1;
    }
    snprintf(g_login, sizeof(g_login), "%s/00.login-signup", userpal);
    snprintf(g_curlogin, sizeof(g_curlogin), "%s/current_login.txt", g_login);

    snprintf(g_state, sizeof(g_state), "%s/#.desktop/signup_hq", g_house);
    mkdir(g_state, 0777);
    snprintf(g_req, sizeof(g_req), "%s/request.txt", g_state);
    { FILE *c = fopen(g_req, "w"); if (c) fclose(c); }

    /* already signed in?  -> show the "already signed in" screen */
    { char u[64]; read_current_user(u, sizeof(u)); if (u[0]) snprintf(g_already, sizeof(g_already), "%s", u); }

    write_state();

    for (;;) {
        handle_request();

        if (g_stage == 2 && !g_already[0]) {
            char u[64]; read_current_user(u, sizeof(u));
            if (u[0] && strcmp(u, g_id) == 0) {
                g_stage = 3;
            } else if (now_s() - g_submit_at > 20) {
                g_stage = 1;
                snprintf(g_err, sizeof(g_err), "That didn't work - the name may be taken. Try again.");
            }
        }

        write_state();
        if (!parent_alive()) break;
        { struct timespec ts = { 0, 150 * 1000 * 1000 }; nanosleep(&ts, NULL); }
    }
    return 0;
}
