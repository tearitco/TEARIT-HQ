#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* irc_chat_manager: X11-HQ window backend for 044.pal-chat-irc👥️+2.
 * <module> launched by the shared khtpm_core_render.+x against
 * irc-chat-hq.xhtpm.  Invocation: <house_root> <package_dir> [arg3]
 *
 * Identity + networking are REAL (2026-09-06, direct: "it should have
 * my user name and port number. since we can test with same user
 * multiports"):
 *  - username: seeded from the house login
 *    (0.user-pal👤️/00.login-signup/current_login.txt -> current_user_id),
 *    written into this instance's own net/session.txt exactly like the
 *    CLI app does; live-switchable in-window (chat_switch_user.+x).
 *  - per-instance SESSION dir under
 *    044.pal-chat-irc👥️+2/pieces/sessions/hq-<ts>-<pid>/ is this
 *    instance's PRISC_PROJECT_ROOT (so two windows as the same user
 *    have independent ledgers and really exercise P2P).
 *  - starts palnet_peer.+x (own_kind irc_node, full mesh) + a
 *    chat_inbox_watcher.+x as children (SIGTERM'd on window close).
 *    palnet_peer auto-allocates a port from base 9950 and writes it to
 *    the SHARED net/presence/ dir; this manager reads it back and
 *    publishes it, so the header shows "<user>  :<port>  #<room>".
 *
 * Reuses the real ops verbatim (chat_post_message / chat_switch_user /
 * chat_create_user / palnet_peer / chat_inbox_watcher). NO renderer C.
 * See 08-roadmap/design-docs/IRC-FORUM-CHAIN-HQ-WINDOWS.md. */

#define IRC_APP_SUBDIR "044.pal-chat-irc👥️+2"   /* literal UTF-8 */
#define PROJECT_ID     "pal-chat-irc"
#define OWN_KIND       "irc_node"

#define MAX_MSGS   200
#define MAX_ROOMS  128
#define PL         4096
#define ACT_BUF    4096

static char house_root[PL];
static char pkg_dir[PL];
static char irc_app[PL];      /* .../044.pal-chat-irc👥️+2               */
static char session_root[PL]; /* .../pieces/sessions/hq-<ts>-<pid>       */
static char net_root[PL];     /* .../44.xyz.01.00/net/presence (shared)  */
static char piece_tag[64];    /* hq<ts><pid> - unique presence-file key  */

static char cur_room[128] = "lobby";
static char cur_user[96]  = "guest";
static int  bound_port    = 0;

static pid_t peer_pid = -1, watcher_pid = -1;

typedef struct { char user[96]; char text[1600]; int self; } Msg;
static Msg  msgs[MAX_MSGS];
static int  n_msgs = 0;
static char rooms[MAX_ROOMS][128];
static int  n_rooms = 0;

/* ------------------------------------------------------------------ */

static void sanitize(char *s) {
    for (char *p = s; *p; p++)
        if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
}

static void mkdir_p(const char *path) {
    char t[PL];
    snprintf(t, sizeof(t), "%s", path);
    for (char *p = t + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}

static void read_kv(const char *path, const char *key, char *out, size_t osz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(out, osz, "%s", line + kl + 1);
            break;
        }
    }
    fclose(f);
}

static void copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
}

/* ------------------------------------------------------------------ */
/* child ops                                                          */

static void child_env(void) {
    setenv("PRISC_PROJECT_ROOT", session_root, 1);
    setenv("PRISC_PROJECT_ID", PROJECT_ID, 1);
    setenv("PRISC_NET_ROOT", net_root, 1);
}

/* fork+exec, wait for completion (one-shot ops) */
static void run_op(const char *bin, char *const av[]) {
    struct stat st;
    if (stat(bin, &st) != 0) return;
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        child_env();
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); }
        execv(bin, av);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
}

/* fork+exec a daemon, return its pid (no wait) */
static pid_t spawn_daemon(char *const av[]) {
    struct stat st;
    if (stat(av[0], &st) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        child_env();
        setsid();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        execv(av[0], av);
        _exit(127);
    }
    return pid;
}

static void op_bin(char *out, size_t osz, const char *name) {
    snprintf(out, osz, "%s/ops/+x/%s", irc_app, name);
}

static void op_create_user(const char *name) {
    char bin[PL]; op_bin(bin, sizeof(bin), "chat_create_user.+x");
    char *av[] = { bin, (char *)name, (char *)name, NULL };
    run_op(bin, av);
}
static void op_switch_user(const char *name) {
    /* switch refuses if users/<name> is absent - make it first */
    char udir[PL];
    snprintf(udir, sizeof(udir), "%s/users/%s", session_root, name);
    mkdir_p(udir);
    op_create_user(name);
    char bin[PL]; op_bin(bin, sizeof(bin), "chat_switch_user.+x");
    char *av[] = { bin, (char *)name, NULL };
    run_op(bin, av);
}
static void op_post(const char *room, const char *user, const char *text) {
    char bin[PL]; op_bin(bin, sizeof(bin), "chat_post_message.+x");
    char *av[] = { bin, (char *)room, (char *)user, (char *)text, NULL };
    run_op(bin, av);
}

/* ------------------------------------------------------------------ */
/* session bring-up                                                   */

static void resolve_login_user(char *out, size_t osz) {
    char p[PL];
    snprintf(p, sizeof(p), "%s/0.user-pal\xF0\x9F\x91\xA4\xEF\xB8\x8F/00.login-signup/current_login.txt", house_root);
    read_kv(p, "current_user_id", out, osz);
    if (!out[0]) snprintf(out, osz, "guest");
    sanitize(out);
}

static void session_setup(void) {
    long ts = (long)time(NULL);
    snprintf(session_root, sizeof(session_root),
             "%s/pieces/sessions/hq-%ld-%d", irc_app, ts, (int)getpid());
    snprintf(piece_tag, sizeof(piece_tag), "hq%ld%d", ts, (int)getpid());

    char d[PL];
    snprintf(d, sizeof(d), "%s/net",   session_root); mkdir_p(d);
    snprintf(d, sizeof(d), "%s/data",  session_root); mkdir_p(d);
    snprintf(d, sizeof(d), "%s/rooms", session_root); mkdir_p(d);
    snprintf(d, sizeof(d), "%s/users", session_root); mkdir_p(d);
    { char f[PL]; snprintf(f, sizeof(f), "%s/net/inbox.txt",  session_root); fclose(fopen(f, "a"));
      snprintf(f, sizeof(f), "%s/net/outbox.txt", session_root); fclose(fopen(f, "a")); }

    resolve_login_user(cur_user, sizeof(cur_user));

    /* users/<cur_user>/ so chat_switch_user accepts it */
    char udir[PL]; snprintf(udir, sizeof(udir), "%s/users/%s", session_root, cur_user); mkdir_p(udir);
    op_create_user(cur_user);

    /* net/session.txt current_user_id=<cur_user> (same as chat_switch_user writes) */
    char sp[PL]; snprintf(sp, sizeof(sp), "%s/net/session.txt", session_root);
    FILE *f = fopen(sp, "w");
    if (f) { fprintf(f, "current_user_id=%s\n", cur_user); fclose(f); }

    /* seed this session's ledger from the real project history */
    char real_ledger[PL], sess_ledger[PL];
    snprintf(real_ledger, sizeof(real_ledger), "%s/data/master_ledger.txt", irc_app);
    snprintf(sess_ledger, sizeof(sess_ledger), "%s/data/master_ledger.txt", session_root);
    copy_file(real_ledger, sess_ledger);

    mkdir_p(net_root);

    /* palnet_peer.+x <own_kind> <project_id> <piece_id> <outbox> <inbox> [seek_kind] */
    char pbin[PL]; op_bin(pbin, sizeof(pbin), "palnet_peer.+x");
    char ob[PL], ib[PL];
    snprintf(ob, sizeof(ob), "%s/net/outbox.txt", session_root);
    snprintf(ib, sizeof(ib), "%s/net/inbox.txt",  session_root);
    char *pav[] = { pbin, (char *)OWN_KIND, (char *)PROJECT_ID, piece_tag, ob, ib, (char *)OWN_KIND, NULL };
    peer_pid = spawn_daemon(pav);

    /* chat_inbox_watcher.+x (no args) - merges net/inbox.txt -> ledger */
    char wbin[PL]; op_bin(wbin, sizeof(wbin), "chat_inbox_watcher.+x");
    char *wav[] = { wbin, NULL };
    watcher_pid = spawn_daemon(wav);
}

static void read_bound_port(void) {
    if (bound_port > 0) return;
    DIR *d = opendir(net_root);
    if (!d) return;
    struct dirent *e;
    char want[80];
    snprintf(want, sizeof(want), "%s-%s-", PROJECT_ID, piece_tag);
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, want, strlen(want)) != 0) continue;
        char p[PL]; snprintf(p, sizeof(p), "%s/%s", net_root, e->d_name);
        char v[32]; read_kv(p, "port", v, sizeof(v));
        if (v[0]) { bound_port = atoi(v); break; }
    }
    closedir(d);
}

/* ------------------------------------------------------------------ */

/* Auto-generated test-harness rooms clutter the list (direct: "can we
 * get rid of some of the old rooms"). Hide them from the sidebar - a
 * pure DISPLAY filter, the ledger is untouched, and cur_room is always
 * shown even if it matches (so a deliberate visit still works). */
static int room_is_junk(const char *r) {
    return strncmp(r, "harness_room_", 13) == 0
        || strncmp(r, "p2p_test_room_", 14) == 0
        || strncmp(r, "uxtest_", 7) == 0;
}

static void add_room(const char *r) {
    if (!r || !r[0]) return;
    if (room_is_junk(r) && strcmp(r, cur_room) != 0) return;
    for (int i = 0; i < n_rooms; i++) if (strcmp(rooms[i], r) == 0) return;
    if (n_rooms < MAX_ROOMS) { snprintf(rooms[n_rooms], 128, "%s", r); n_rooms++; }
}

static void read_ledger(void) {
    n_rooms = 0;
    n_msgs = 0;
    add_room("lobby");        /* always-present defaults, listed first */
    add_room("general");
    char p[PL];
    snprintf(p, sizeof(p), "%s/data/master_ledger.txt", session_root);
    FILE *f = fopen(p, "r");
    if (!f) { add_room(cur_room); return; }
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "MSG|", 4) != 0) continue;
        char *save = NULL;
        strtok_r(line, "|", &save);              /* MSG  */
        strtok_r(NULL, "|", &save);              /* id   */
        char *room = strtok_r(NULL, "|", &save);
        char *user = strtok_r(NULL, "|", &save);
        strtok_r(NULL, "|", &save);              /* ts   */
        char *text = strtok_r(NULL, "", &save);
        if (!room || !user) continue;
        add_room(room);
        if (strcmp(room, cur_room) != 0) continue;
        if (n_msgs >= MAX_MSGS) {
            memmove(&msgs[0], &msgs[1], sizeof(Msg) * (MAX_MSGS - 1));
            n_msgs--;
        }
        Msg *m = &msgs[n_msgs++];
        snprintf(m->user, sizeof(m->user), "%s", user);
        snprintf(m->text, sizeof(m->text), "%s", text ? text : "");
        sanitize(m->user);
        sanitize(m->text);
        m->self = (strcmp(user, cur_user) == 0);
    }
    fclose(f);
    add_room(cur_room);
}

static void write_ui(void) {
    read_bound_port();
    char cu[96];
    { char sp[PL]; snprintf(sp, sizeof(sp), "%s/net/session.txt", session_root);
      read_kv(sp, "current_user_id", cu, sizeof(cu)); }
    if (cu[0]) { snprintf(cur_user, sizeof(cur_user), "%s", cu); sanitize(cur_user); }

    char tmp[PL], dst[PL];
    snprintf(dst, sizeof(dst), "%s/irc_chat_ui.txt", pkg_dir);
    snprintf(tmp, sizeof(tmp), "%s/irc_chat_ui.txt.tmp", pkg_dir);
    FILE *f = fopen(tmp, "w");
    if (!f) return;

    fprintf(f, "n_rooms=%d\n", n_rooms);
    for (int i = 0; i < n_rooms; i++) {
        char rn[128]; snprintf(rn, sizeof(rn), "%s", rooms[i]); sanitize(rn);
        fprintf(f, "r_%d_name=%s\n", i, rn);
        fprintf(f, "r_%d_cls=%s\n", i, (strcmp(rooms[i], cur_room) == 0) ? "room-active" : "");
    }
    fprintf(f, "cur_room=%s\n", cur_room);
    fprintf(f, "cur_user=%s\n", cur_user);

    char portstr[24];
    if (bound_port > 0) snprintf(portstr, sizeof(portstr), "%d", bound_port);
    else                snprintf(portstr, sizeof(portstr), "…");
    fprintf(f, "port=%s\n", portstr);
    fprintf(f, "status=%s   ·   :%s   ·   #%s\n", cur_user, portstr, cur_room);

    fprintf(f, "n_msgs=%d\n", n_msgs);
    for (int i = 0; i < n_msgs; i++) {
        fprintf(f, "m_%d_text=%s:  %s\n", i, msgs[i].user, msgs[i].text);
        fprintf(f, "m_%d_cls=%s\n", i, msgs[i].self ? "msg-self" : "msg-other");
    }
    fprintf(f, "empty_hint=%s\n",
            n_msgs ? "" : "No messages in this room yet - type below to say hi.");
    fclose(f);
    rename(tmp, dst);
}

static void do_cmd(const char *cmd) {
    if (strncmp(cmd, "ROOM:", 5) == 0) {
        snprintf(cur_room, sizeof(cur_room), "%s", cmd + 5);
        sanitize(cur_room);
        if (!cur_room[0]) snprintf(cur_room, sizeof(cur_room), "lobby");
    } else if (strncmp(cmd, "SEND:", 5) == 0) {
        const char *t = cmd + 5;
        if (*t) op_post(cur_room, cur_user, t);
    } else if (strncmp(cmd, "USER:", 5) == 0) {
        char nm[96]; snprintf(nm, sizeof(nm), "%s", cmd + 5); sanitize(nm);
        if (nm[0]) { op_switch_user(nm); snprintf(cur_user, sizeof(cur_user), "%s", nm); }
    }
    read_ledger();
    write_ui();
}

static void clear_action_file(void) {
    char p[PL];
    snprintf(p, sizeof(p), "%s/irc_chat_action.txt", pkg_dir);
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); }
}

static void poll_action(int *last_seq) {
    char p[PL];
    snprintf(p, sizeof(p), "%s/irc_chat_action.txt", pkg_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    char buf[ACT_BUF];
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nr] = '\0';
    int seq = 0;
    char cmd[2048]; cmd[0] = '\0';
    char *ls = buf;
    while (*ls) {
        char *le = strchr(ls, '\n');
        size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (strncmp(ls, "seq=", 4) == 0) seq = atoi(ls + 4);
        else if (strncmp(ls, "cmd=", 4) == 0) {
            size_t cl = ll - 4;
            if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1;
            memcpy(cmd, ls + 4, cl); cmd[cl] = '\0';
        }
        if (!le) break;
        ls = le + 1;
    }
    if (seq > *last_seq && cmd[0]) { *last_seq = seq; do_cmd(cmd); }
}

static void cleanup_and_exit(int sig) {
    (void)sig;
    if (peer_pid > 0)    kill(peer_pid, SIGTERM);
    if (watcher_pid > 0) kill(watcher_pid, SIGTERM);
    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> [arg3]\n", argv[0]);
        return 1;
    }
    snprintf(house_root, sizeof(house_root), "%s", argv[1]);
    snprintf(pkg_dir,    sizeof(pkg_dir),    "%s", argv[2]);
    snprintf(irc_app,    sizeof(irc_app),    "%s/%s", house_root, IRC_APP_SUBDIR);
    snprintf(net_root,   sizeof(net_root),   "%s/net/presence", house_root);

    signal(SIGTERM, cleanup_and_exit);
    signal(SIGINT,  cleanup_and_exit);
    signal(SIGHUP,  cleanup_and_exit);

    session_setup();
    clear_action_file();
    read_ledger();
    write_ui();

    int last_seq = 0, tick = 0;
    for (;;) {
        usleep(50000);
        poll_action(&last_seq);
        if (++tick >= 8) {            /* ~0.4s: pick up watcher/peer writes */
            tick = 0;
            read_ledger();
            write_ui();
        }
    }
    return 0;
}
