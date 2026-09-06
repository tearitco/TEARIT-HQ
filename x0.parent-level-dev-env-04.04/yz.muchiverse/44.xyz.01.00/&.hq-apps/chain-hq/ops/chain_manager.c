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

/* chain_manager: X11-HQ window backend for 041.pal-chain⛓️ (wallet
 * dashboard).  <module> for chain-hq.xhtpm.  argv: <house> <pkg> [a3]
 *
 * Same bring-up as irc_chat_manager (IRC-FORUM-CHAIN-HQ-WINDOWS.md):
 * per-instance session dir under 041.pal-chain⛓️/pieces/sessions/
 * hq-<ts>-<pid>/ (PRISC_PROJECT_ROOT), blockchain seeded from the real
 * one, palnet_peer.+x (own_kind chain_node) + chain_inbox_watcher.+x
 * as children.  Identity: a wallet, seeded from the house login name
 * (current_login.txt -> current_user_id, e.g. "jb") - if that wallet
 * doesn't exist in this fresh session it is auto-created with a
 * password equal to the wallet id (dev toy; a real Login/Create
 * affordance is on the Wallet tab for anything else) and logged in via
 * chain_login.+x (writes net/session.txt wallet_id=).
 *
 * Reuses chain_balance / chain_send / chain_miner / chain_create_wallet
 * / chain_login / palnet_peer / chain_inbox_watcher verbatim. NO
 * renderer C - class "chain-window" rides the generic path. */

#define CHAIN_APP_SUBDIR "041.pal-chain\xE2\x9B\x93\xEF\xB8\x8F"  /* 041.pal-chain⛓️ */
#define PROJECT_ID "pal-chain"
#define OWN_KIND   "chain_node"

#define PL      4096
#define ACT_BUF 4096
#define MAX_HIST 600

static char house_root[PL], pkg_dir[PL], chain_app[PL], session_root[PL], net_root[PL];
static char piece_tag[64];

static char wallet_id[96] = "";
static int  bound_port = 0;
static char cur_tab[24] = "wallet";
static long balance = 0;

static pid_t peer_pid = -1, watcher_pid = -1, miner_pid = -1;

static char hist[MAX_HIST][512];
static int  n_hist = 0;
static long chain_len = 0;   /* total BLOCK lines on disk */

/* miner_status.txt fields */
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
static void copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb"); if (!in) return;
    FILE *out = fopen(dst, "wb"); if (!out) { fclose(in); return; }
    char b[8192]; size_t n;
    while ((n = fread(b, 1, sizeof(b), in)) > 0) fwrite(b, 1, n, out);
    fclose(in); fclose(out);
}

static void child_env(void) {
    setenv("PRISC_PROJECT_ROOT", session_root, 1);
    setenv("PRISC_PROJECT_ID", PROJECT_ID, 1);
    setenv("PRISC_NET_ROOT", net_root, 1);
}
static void op_bin(char *out, size_t osz, const char *name) {
    snprintf(out, osz, "%s/ops/+x/%s", chain_app, name);
}
/* one-shot op; if outfile != NULL, child stdout is redirected there */
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
        child_env(); setsid();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        execv(av[0], av);
        _exit(127);
    }
    return pid;
}

/* ------------------------------------------------------------------ */

static void resolve_login_user(char *out, size_t osz) {
    char p[PL];
    snprintf(p, sizeof(p),
             "%s/0.user-pal\xF0\x9F\x91\xA4\xEF\xB8\x8F/00.login-signup/current_login.txt", house_root);
    read_kv(p, "current_user_id", out, osz);
    /* wallet_id charset: letters/digits/_/- only */
    for (char *c = out; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) *c = '_';
    if (!out[0]) snprintf(out, osz, "guest");
}

static void session_setup(void) {
    long ts = (long)time(NULL);
    snprintf(session_root, sizeof(session_root),
             "%s/pieces/sessions/hq-%ld-%d", chain_app, ts, (int)getpid());
    snprintf(piece_tag, sizeof(piece_tag), "hq%ld%d", ts, (int)getpid());

    char d[PL];
    snprintf(d, sizeof(d), "%s/net",     session_root); mkdir_p(d);
    snprintf(d, sizeof(d), "%s/data",    session_root); mkdir_p(d);
    snprintf(d, sizeof(d), "%s/wallets", session_root); mkdir_p(d);
    { char f[PL];
      snprintf(f, sizeof(f), "%s/net/inbox.txt",  session_root); fclose(fopen(f, "a"));
      snprintf(f, sizeof(f), "%s/net/outbox.txt", session_root); fclose(fopen(f, "a")); }

    /* seed blockchain from the real project history. The live
     * data/blockchain.txt is often 0 bytes (a fresh project) - fall
     * back to the newest data/blockchain.txt.pre-harness-run-* snapshot
     * so the History / full-chain view isn't empty on first open. */
    char rb[PL], sb[PL];
    snprintf(rb, sizeof(rb), "%s/data/blockchain.txt", chain_app);
    snprintf(sb, sizeof(sb), "%s/data/blockchain.txt", session_root);
    struct stat rst;
    if (stat(rb, &rst) != 0 || rst.st_size == 0) {
        char ddir[PL]; snprintf(ddir, sizeof(ddir), "%s/data", chain_app);
        DIR *dd = opendir(ddir);
        char best[PL] = ""; long best_sz = 0;
        if (dd) {
            struct dirent *e;
            while ((e = readdir(dd))) {
                if (strncmp(e->d_name, "blockchain.txt.pre-harness-run-", 30) != 0) continue;
                char fp[PL]; snprintf(fp, sizeof(fp), "%s/%s", ddir, e->d_name);
                struct stat fs;
                if (stat(fp, &fs) == 0 && fs.st_size > best_sz) {
                    best_sz = fs.st_size; snprintf(best, sizeof(best), "%s", fp);
                }
            }
            closedir(dd);
        }
        if (best[0]) snprintf(rb, sizeof(rb), "%s", best);
    }
    copy_file(rb, sb);
    { char f[PL]; snprintf(f, sizeof(f), "%s/data/pending_tx.txt", session_root); fclose(fopen(f, "a")); }

    resolve_login_user(wallet_id, sizeof(wallet_id));

    /* auto create+login the login-name wallet in this fresh session */
    char wdir[PL];
    snprintf(wdir, sizeof(wdir), "%s/wallets/%s", session_root, wallet_id);
    struct stat wst;
    if (stat(wdir, &wst) != 0) {
        char cb[PL]; op_bin(cb, sizeof(cb), "chain_create_wallet.+x");
        char *cav[] = { cb, wallet_id, wallet_id, NULL };
        run_op(cav, NULL);
    }
    { char lb[PL]; op_bin(lb, sizeof(lb), "chain_login.+x");
      char *lav[] = { lb, wallet_id, wallet_id, NULL };
      run_op(lav, NULL); }

    mkdir_p(net_root);

    /* palnet_peer.+x <own_kind> <project_id> <piece_tag> <outbox> <inbox> [seek_kind] */
    char pbin[PL]; op_bin(pbin, sizeof(pbin), "palnet_peer.+x");
    char ob[PL], ib[PL];
    snprintf(ob, sizeof(ob), "%s/net/outbox.txt", session_root);
    snprintf(ib, sizeof(ib), "%s/net/inbox.txt",  session_root);
    char *pav[] = { pbin, (char *)OWN_KIND, (char *)PROJECT_ID, piece_tag, ob, ib, (char *)OWN_KIND, NULL };
    peer_pid = spawn_daemon(pav);

    char wbin[PL]; op_bin(wbin, sizeof(wbin), "chain_inbox_watcher.+x");
    char *wav[] = { wbin, NULL };
    watcher_pid = spawn_daemon(wav);
}

static void read_bound_port(void) {
    if (bound_port > 0) return;
    DIR *d = opendir(net_root); if (!d) return;
    struct dirent *e; char want[80];
    snprintf(want, sizeof(want), "%s-%s-", PROJECT_ID, piece_tag);
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, want, strlen(want)) != 0) continue;
        char p[PL]; snprintf(p, sizeof(p), "%s/%s", net_root, e->d_name);
        char v[32]; read_kv(p, "port", v, sizeof(v));
        if (v[0]) { bound_port = atoi(v); break; }
    }
    closedir(d);
}

static void refresh_balance(void) {
    char bb[PL]; op_bin(bb, sizeof(bb), "chain_balance.+x");
    char tmp[PL]; snprintf(tmp, sizeof(tmp), "%s/.bal.tmp", session_root);
    char *av[] = { bb, wallet_id, NULL };
    run_op(av, tmp);
    char v[64] = ""; FILE *f = fopen(tmp, "r");
    if (f) { if (fgets(v, sizeof(v), f)) balance = atol(v); fclose(f); }
}

static void refresh_miner_status(void) {
    char p[PL]; snprintf(p, sizeof(p), "%s/net/miner_status.txt", session_root);
    read_kv(p, "running",                   m_running,  sizeof(m_running));
    read_kv(p, "blocks_mined_this_session", m_blocks,   sizeof(m_blocks));
    read_kv(p, "last_block_index",          m_lastidx,  sizeof(m_lastidx));
    read_kv(p, "last_block_hash",           m_lasthash, sizeof(m_lasthash));
    read_kv(p, "total_supply_minted",       m_supply,   sizeof(m_supply));
    if (!m_running[0])  snprintf(m_running, sizeof(m_running), "0");
    /* a live miner child overrides the file's own running flag */
    if (miner_pid > 0 && waitpid(miner_pid, NULL, WNOHANG) == 0)
        snprintf(m_running, sizeof(m_running), "1");
    else if (miner_pid > 0) miner_pid = -1;
}

static void read_history(void) {
    n_hist = 0;
    chain_len = 0;
    static char ring[MAX_HIST][512];   /* static: too big for the stack */
    int rn = 0, rs = 0;
    char line[512];

    /* pending TX first (not yet mined) - shown at the very top */
    char pp[PL]; snprintf(pp, sizeof(pp), "%s/data/pending_tx.txt", session_root);
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

    /* chain: newest-last on disk; show newest-first, ring-buffered */
    char p[PL]; snprintf(p, sizeof(p), "%s/data/blockchain.txt", session_root);
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

/* ------------------------------------------------------------------ */

static void write_ui(void) {
    read_bound_port();
    refresh_balance();
    refresh_miner_status();
    read_history();

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
    fprintf(f, "status=%s   ·   :%s   ·   %ld mc\n", wallet_id, portstr, balance);

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

    /* only emit history rows on the History tab - data-driven gating,
     * doesn't rely on show= working on a <scrolllist> */
    int show_hist = (strcmp(cur_tab, "history") == 0);
    int nh = show_hist ? n_hist : 0;
    fprintf(f, "chain_len=%ld\n", chain_len);
    fprintf(f, "hist_hdr=chain: %ld blocks/tx   ·   showing latest %d\n", chain_len, nh);
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
    /* arg = "<to>|<amount>" */
    char to[96] = "", amt[32] = "";
    const char *bar = strchr(arg, '|');
    if (!bar) { /* also accept "to amount" */
        bar = strchr(arg, ' ');
        if (!bar) return;
    }
    size_t tl = (size_t)(bar - arg);
    if (tl >= sizeof(to)) tl = sizeof(to) - 1;
    memcpy(to, arg, tl); to[tl] = '\0';
    snprintf(amt, sizeof(amt), "%s", bar + 1);
    for (char *c = to; *c; c++) if (*c == ' ') *c = '\0';   /* trim */
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
    /* REFRESH / unknown: write_ui() below refreshes everything */
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
    if (miner_pid > 0)   kill(miner_pid, SIGTERM);
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
    snprintf(chain_app,  sizeof(chain_app),  "%s/%s", house_root, CHAIN_APP_SUBDIR);
    snprintf(net_root,   sizeof(net_root),   "%s/net/presence", house_root);

    signal(SIGTERM, cleanup_and_exit);
    signal(SIGINT,  cleanup_and_exit);
    signal(SIGHUP,  cleanup_and_exit);

    session_setup();
    clear_action_file();
    write_ui();

    int last_seq = 0, tick = 0;
    for (;;) {
        usleep(50000);
        poll_action(&last_seq);
        if (++tick >= 12) {   /* ~0.6s: balance/miner/chain refresh */
            tick = 0;
            write_ui();
        }
    }
    return 0;
}
