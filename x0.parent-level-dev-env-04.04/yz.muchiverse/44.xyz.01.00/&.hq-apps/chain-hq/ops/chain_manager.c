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
#include <sys/prctl.h>

/* chain_manager: X11-HQ wallet dashboard for 041.pal-chain⛓️.
 * <module> for chain-hq.xhtpm.  argv: <house> <pkg> [a3]
 *
 * ONE GLOBAL SHARED LEDGER (2026-09-06, direct: "its supposed to be
 * one global main ledger ... shared between both local user/accounts
 * and p2p").  So this manager does NOT session-isolate: every ops call
 * runs with PRISC_PROJECT_ROOT = the real 041.pal-chain⛓️/ dir, and
 * every chain-hq window (any local wallet) + the CLI app all read/write
 * the same data/blockchain.txt, data/pending_tx.txt and wallets/.
 *
 * Per-window state = the current wallet_id (held here, NOT the shared
 * net/session.txt).  Seeded from the house login name (current_login
 * .txt -> current_user_id, e.g. "jb"); auto chain_create_wallet.+x if
 * that wallet doesn't exist yet (password == wallet id, dev toy); a
 * real Login/Create affordance is on the Wallet tab.
 *
 * P2P: ONE palnet_peer.+x (chain_node) + ONE chain_inbox_watcher.+x
 * per NODE, not per window - the first chain-hq window to open starts
 * them against the shared root; later windows detect they're running
 * and just read the bound port.  (palnet_peer is CPU-hungry when it
 * has no peer; N-per-window stacked up badly - real, live-caught.)
 *
 * CPU: the 50ms loop only polls the action file.  The expensive
 * refresh - chain_balance.+x (scans the whole chain) + a full
 * blockchain re-read for History - runs only on a command, on the
 * ~10s slow tick, or at startup.  NO renderer C. */

#define CHAIN_APP_SUBDIR "041.pal-chain\xE2\x9B\x93\xEF\xB8\x8F"  /* 041.pal-chain⛓️ */
#define PROJECT_ID "pal-chain"
#define OWN_KIND   "chain_node"

#define PL       4096
#define ACT_BUF  4096
#define MAX_HIST 600
#define SLOW_TICKS 200   /* 200 * 50ms = 10s between idle full refreshes */

static char house_root[PL], pkg_dir[PL], chain_app[PL], net_root[PL];
static char piece_tag[64];

static char wallet_id[96] = "";
static int  bound_port = 0;
static char cur_tab[24] = "wallet";
static long balance = 0;

static pid_t peer_pid = -1, watcher_pid = -1, miner_pid = -1;
static int   i_own_node = 0;   /* did WE start the shared peer/watcher */

static char hist[MAX_HIST][512];
static int  n_hist = 0;
static long chain_len = 0;

static char m_running[8] = "0", m_blocks[24] = "0", m_lastidx[24] = "0",
            m_lasthash[80] = "-", m_supply[32] = "0";

/* ------------------------------------------------------------------ */

static void sanitize(char *s) {
    for (char *p = s; *p; p++)
        if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
}
static void mkdir_p(const char *path) {
    char t[PL]; snprintf(t, sizeof(t), "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = '\0'; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}
static void read_kv(const char *path, const char *key, char *out, size_t osz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[1024]; size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(out, osz, "%s", line + kl + 1); break;
        }
    fclose(f);
}

static void child_env(void) {
    setenv("PRISC_PROJECT_ROOT", chain_app, 1);   /* SHARED root */
    setenv("PRISC_PROJECT_ID", PROJECT_ID, 1);
    setenv("PRISC_NET_ROOT", net_root, 1);
}
static void op_bin(char *out, size_t osz, const char *name) {
    snprintf(out, osz, "%s/ops/+x/%s", chain_app, name);
}
static void run_op(char *const av[], const char *outfile) {
    struct stat st; if (stat(av[0], &st) != 0) return;
    pid_t pid = fork(); if (pid < 0) return;
    if (pid == 0) {
        child_env();
        int o = outfile ? open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644)
                        : open("/dev/null", O_WRONLY);
        int e = open("/dev/null", O_WRONLY);
        if (o >= 0) dup2(o, 1);
        if (e >= 0) dup2(e, 2);
        execv(av[0], av);
        _exit(127);
    }
    int status; waitpid(pid, &status, 0);
}
static pid_t spawn_daemon(char *const av[]) {
    struct stat st; if (stat(av[0], &st) != 0) return -1;
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) {
        child_env();
        prctl(PR_SET_PDEATHSIG, SIGTERM);   /* die if this manager dies, even on kill -9 */
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        execv(av[0], av);
        _exit(127);
    }
    return pid;
}

/* ------------------------------------------------------------------ */
/* identity + shared node bring-up                                    */

static void resolve_login_user(char *out, size_t osz) {
    char p[PL];
    snprintf(p, sizeof(p),
             "%s/0.user-pal\xF0\x9F\x91\xA4\xEF\xB8\x8F/00.login-signup/current_login.txt", house_root);
    read_kv(p, "current_user_id", out, osz);
    for (char *c = out; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) *c = '_';
    if (!out[0]) snprintf(out, osz, "guest");
}

/* is a chain_node palnet_peer already running for this node? */
static int node_peer_running(void) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char cp[64];
        snprintf(cp, sizeof(cp), "/proc/%s/cmdline", e->d_name);
        FILE *f = fopen(cp, "r");
        if (!f) continue;
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = '\0';
        for (size_t i = 0; i + 1 < n; i++) if (buf[i] == '\0') buf[i] = ' ';
        if (strstr(buf, "palnet_peer.+x") && strstr(buf, "chain_node") && strstr(buf, "pal-chain"))
            found = 1;
    }
    closedir(d);
    return found;
}

static void read_bound_port(void) {
    if (bound_port > 0) return;
    DIR *d = opendir(net_root); if (!d) return;
    struct dirent *e;
    char pref[64]; snprintf(pref, sizeof(pref), "%s-", PROJECT_ID);   /* any pal-chain-* peer */
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, pref, strlen(pref)) != 0) continue;
        char p[PL]; snprintf(p, sizeof(p), "%s/%s", net_root, e->d_name);
        char v[32]; read_kv(p, "port", v, sizeof(v));
        if (v[0]) { bound_port = atoi(v); break; }
    }
    closedir(d);
}

static void node_setup(void) {
    long ts = (long)time(NULL);
    snprintf(piece_tag, sizeof(piece_tag), "hq%ld%d", ts, (int)getpid());
    mkdir_p(net_root);
    { char d[PL]; snprintf(d, sizeof(d), "%s/net", chain_app); mkdir_p(d);
      char f[PL];
      snprintf(f, sizeof(f), "%s/net/inbox.txt",  chain_app); fclose(fopen(f, "a"));
      snprintf(f, sizeof(f), "%s/net/outbox.txt", chain_app); fclose(fopen(f, "a")); }

    resolve_login_user(wallet_id, sizeof(wallet_id));

    /* auto create the login-name wallet in the SHARED wallets/ if new */
    char wdir[PL]; snprintf(wdir, sizeof(wdir), "%s/wallets/%s", chain_app, wallet_id);
    struct stat wst;
    if (stat(wdir, &wst) != 0) {
        char cb[PL]; op_bin(cb, sizeof(cb), "chain_create_wallet.+x");
        char *cav[] = { cb, wallet_id, wallet_id, NULL };
        run_op(cav, NULL);
    }

    if (node_peer_running()) {
        i_own_node = 0;   /* another chain-hq window already runs the node's P2P */
        return;
    }
    i_own_node = 1;

    char pbin[PL]; op_bin(pbin, sizeof(pbin), "palnet_peer.+x");
    char ob[PL], ib[PL];
    snprintf(ob, sizeof(ob), "%s/net/outbox.txt", chain_app);
    snprintf(ib, sizeof(ib), "%s/net/inbox.txt",  chain_app);
    char *pav[] = { pbin, (char *)OWN_KIND, (char *)PROJECT_ID, piece_tag, ob, ib, (char *)OWN_KIND, NULL };
    peer_pid = spawn_daemon(pav);

    char wbin[PL]; op_bin(wbin, sizeof(wbin), "chain_inbox_watcher.+x");
    char *wav[] = { wbin, NULL };
    watcher_pid = spawn_daemon(wav);
}

/* ------------------------------------------------------------------ */
/* data reads (EXPENSIVE - call sparingly)                            */

static void refresh_balance(void) {
    char bb[PL]; op_bin(bb, sizeof(bb), "chain_balance.+x");
    char tmp[PL]; snprintf(tmp, sizeof(tmp), "%s/.balance.%d.tmp", pkg_dir, (int)getpid());
    char *av[] = { bb, wallet_id, NULL };
    run_op(av, tmp);
    char v[64] = ""; FILE *f = fopen(tmp, "r");
    if (f) { if (fgets(v, sizeof(v), f)) balance = atol(v); fclose(f); }
    unlink(tmp);
}

static void refresh_miner_status(void) {
    char p[PL]; snprintf(p, sizeof(p), "%s/net/miner_status.txt", chain_app);
    read_kv(p, "running",                   m_running,  sizeof(m_running));
    read_kv(p, "blocks_mined_this_session", m_blocks,   sizeof(m_blocks));
    read_kv(p, "last_block_index",          m_lastidx,  sizeof(m_lastidx));
    read_kv(p, "last_block_hash",           m_lasthash, sizeof(m_lasthash));
    read_kv(p, "total_supply_minted",       m_supply,   sizeof(m_supply));
    if (!m_running[0]) snprintf(m_running, sizeof(m_running), "0");
    if (miner_pid > 0 && waitpid(miner_pid, NULL, WNOHANG) == 0)
        snprintf(m_running, sizeof(m_running), "1");
    else if (miner_pid > 0) miner_pid = -1;
}

static void read_history(void) {
    n_hist = 0; chain_len = 0;
    static char ring[MAX_HIST][512];
    int rn = 0, rs = 0;
    char line[512];

    char pp[PL]; snprintf(pp, sizeof(pp), "%s/data/pending_tx.txt", chain_app);
    FILE *pf = fopen(pp, "r");
    if (pf) {
        while (fgets(line, sizeof(line), pf)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!line[0]) continue;
            if (n_hist < MAX_HIST) {
                snprintf(hist[n_hist], 512, "PENDING  %s", line);
                sanitize(hist[n_hist]); n_hist++;
            }
        }
        fclose(pf);
    }

    char p[PL]; snprintf(p, sizeof(p), "%s/data/blockchain.txt", chain_app);
    FILE *f = fopen(p, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strncmp(line, "BLOCK|", 6) != 0 && strncmp(line, "TX|", 3) != 0) continue;
            chain_len++;
            snprintf(ring[(rs + rn) % MAX_HIST], 512, "%s", line);
            if (rn < MAX_HIST) rn++; else rs = (rs + 1) % MAX_HIST;
        }
        fclose(f);
    }
    for (int i = 0; i < rn && n_hist < MAX_HIST; i++) {
        snprintf(hist[n_hist], 512, "%s", ring[(rs + rn - 1 - i) % MAX_HIST]);
        sanitize(hist[n_hist]);
        n_hist++;
    }
}

/* pull everything that needs an op/full-scan */
static void refresh_all(void) {
    read_bound_port();
    refresh_balance();
    refresh_miner_status();
    if (strcmp(cur_tab, "history") == 0) read_history();
}

/* ------------------------------------------------------------------ */

static void write_ui(void) {
    char tmp[PL], dst[PL];
    snprintf(dst, sizeof(dst), "%s/chain_ui.txt", pkg_dir);
    snprintf(tmp, sizeof(tmp), "%s/chain_ui.txt.tmp", pkg_dir);
    FILE *f = fopen(tmp, "w"); if (!f) return;

    char portstr[24];
    if (bound_port > 0) snprintf(portstr, sizeof(portstr), "%d", bound_port);
    else                snprintf(portstr, sizeof(portstr), "…");

    fprintf(f, "wallet_id=%s\n", wallet_id);
    fprintf(f, "port=%s\n", portstr);
    fprintf(f, "balance=%ld\n", balance);
    fprintf(f, "cur_tab=%s\n", cur_tab);
    fprintf(f, "status=%s   ·   node :%s   ·   %ld mc   ·   shared ledger\n", wallet_id, portstr, balance);

    fprintf(f, "tab_wallet=%s\n",  strcmp(cur_tab, "wallet")  == 0 ? "1" : "");
    fprintf(f, "tab_send=%s\n",    strcmp(cur_tab, "send")    == 0 ? "1" : "");
    fprintf(f, "tab_mine=%s\n",    strcmp(cur_tab, "mine")    == 0 ? "1" : "");
    fprintf(f, "tab_history=%s\n", strcmp(cur_tab, "history") == 0 ? "1" : "");
    fprintf(f, "cls_wallet=%s\n",  strcmp(cur_tab, "wallet")  == 0 ? "tab-active" : "");
    fprintf(f, "cls_send=%s\n",    strcmp(cur_tab, "send")    == 0 ? "tab-active" : "");
    fprintf(f, "cls_mine=%s\n",    strcmp(cur_tab, "mine")    == 0 ? "tab-active" : "");
    fprintf(f, "cls_history=%s\n", strcmp(cur_tab, "history") == 0 ? "tab-active" : "");

    int mining = (strcmp(m_running, "1") == 0);
    fprintf(f, "mine_running=%s\n", mining ? "1" : "");
    fprintf(f, "mine_toggle=%s\n", mining ? "Stop mining" : "Start mining");
    fprintf(f, "mine_blocks=%s\n", m_blocks);
    fprintf(f, "mine_lastidx=%s\n", m_lastidx);
    fprintf(f, "mine_supply=%s\n", m_supply);

    int show_hist = (strcmp(cur_tab, "history") == 0);
    int nh = show_hist ? n_hist : 0;
    fprintf(f, "chain_len=%ld\n", chain_len);
    fprintf(f, "hist_hdr=shared chain: %ld blocks/tx   ·   showing latest %d\n", chain_len, nh);
    fprintf(f, "n_hist=%d\n", nh);
    for (int i = 0; i < nh; i++)
        fprintf(f, "h_%d_text=%s\n", i, hist[i]);
    fprintf(f, "hist_empty=%s\n",
            (show_hist && n_hist == 0) ? "Chain is empty - mine some blocks on the Mine tab." : "");

    fclose(f);
    rename(tmp, dst);
}

/* ------------------------------------------------------------------ */

static void miner_toggle(void) {
    if (miner_pid > 0 && waitpid(miner_pid, NULL, WNOHANG) == 0) {
        kill(miner_pid, SIGTERM);
        miner_pid = -1;
    } else {
        char mb[PL]; op_bin(mb, sizeof(mb), "chain_miner.+x");
        char *av[] = { mb, wallet_id, NULL };
        miner_pid = spawn_daemon(av);
    }
}

static void do_send(const char *arg) {
    char to[96] = "", amt[32] = "";
    const char *bar = strchr(arg, '|');
    if (!bar) bar = strchr(arg, ' ');
    if (!bar) return;
    size_t tl = (size_t)(bar - arg);
    if (tl >= sizeof(to)) tl = sizeof(to) - 1;
    memcpy(to, arg, tl); to[tl] = '\0';
    snprintf(amt, sizeof(amt), "%s", bar + 1);
    for (char *c = to; *c; c++) if (*c == ' ') *c = '\0';
    if (!to[0] || atol(amt) <= 0) return;
    char sb[PL]; op_bin(sb, sizeof(sb), "chain_send.+x");
    char *av[] = { sb, wallet_id, to, amt, NULL };
    run_op(av, NULL);
}

static void do_cmd(const char *cmd) {
    if (strncmp(cmd, "TAB:", 4) == 0) {
        snprintf(cur_tab, sizeof(cur_tab), "%s", cmd + 4);
        sanitize(cur_tab);
    } else if (strncmp(cmd, "SEND:", 5) == 0) {
        do_send(cmd + 5);
    } else if (strcmp(cmd, "MINE_TOGGLE") == 0) {
        miner_toggle();
    }
    /* every command triggers a full refresh + publish */
    refresh_all();
    write_ui();
}

static void clear_action_file(void) {
    char p[PL]; snprintf(p, sizeof(p), "%s/chain_action.txt", pkg_dir);
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); }
}
static void poll_action(int *last_seq) {
    char p[PL]; snprintf(p, sizeof(p), "%s/chain_action.txt", pkg_dir);
    FILE *f = fopen(p, "r"); if (!f) return;
    char buf[ACT_BUF];
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f); buf[nr] = '\0';
    int seq = 0; char cmd[1024]; cmd[0] = '\0';
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
    if (miner_pid > 0) kill(miner_pid, SIGTERM);
    if (i_own_node) {
        if (peer_pid > 0)    kill(peer_pid, SIGTERM);
        if (watcher_pid > 0) kill(watcher_pid, SIGTERM);
    }
    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> [arg3]\n", argv[0]);
        return 1;
    }
    snprintf(house_root, sizeof(house_root), "%s", argv[1]);
    snprintf(pkg_dir,    sizeof(pkg_dir),    "%s", argv[2]);
    snprintf(chain_app,  sizeof(chain_app),  "%s/%s", house_root, CHAIN_APP_SUBDIR);
    snprintf(net_root,   sizeof(net_root),   "%s/net/presence", house_root);

    signal(SIGTERM, cleanup_and_exit);
    signal(SIGINT,  cleanup_and_exit);
    signal(SIGHUP,  cleanup_and_exit);

    node_setup();
    clear_action_file();
    refresh_all();
    write_ui();

    int last_seq = 0, slow = 0;
    for (;;) {
        usleep(50000);                 /* 50 ms - action poll only */
        poll_action(&last_seq);
        if (++slow >= SLOW_TICKS) {     /* ~10 s idle refresh */
            slow = 0;
            refresh_all();
            write_ui();
        }
    }
    return 0;
}
