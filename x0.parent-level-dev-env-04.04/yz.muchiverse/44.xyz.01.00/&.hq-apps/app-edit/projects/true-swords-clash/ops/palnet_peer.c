/* palnet_peer - ONE reusable, symmetric peer-to-peer companion process.
 * Read yz.muchiverse/2.muchi-verse/PAL-NET-STANDARD.txt sec. 2/3/4 in
 * full before touching this file - this file IS that doc's own
 * reference implementation, not a separate design.
 *
 * Modeled on projects/p2p-net/manager/p2p_manager.c's own real, proven
 * shape (files for discovery/presence, real TCP sockets for live
 * data) - but p2p-net's own topology (ring, leader election, an open
 * peer set) is deliberately NOT ported; this task has a small, known
 * number of local nodes, so discovery is a flat presence-directory
 * scan. Every instance of THIS binary is symmetric - there is no
 * client/server distinction anywhere in this file (a real, direct
 * user correction mid-session: an earlier draft wrongly split "zoo"
 * as a server and "pet" as a client before checking that p2p-net's
 * own real nodes are never split that way).
 *
 * Deliberately built as a STANDALONE OP, not embedded in any GUI
 * process (a second direct user correction: "id definately rather u
 * use ops and pals for this, since its very repeatable among other
 * projects... like mutaclysm later"). A GUI process (gl_mirror.c,
 * egg_window.c, zoo_window.c, or any future project's own equivalent)
 * never touches a socket - it only ever writes an outbox_file and
 * reads an inbox_file, exactly like every other file-based mechanism
 * already used throughout this family. This binary is launched as a
 * companion process (persistent for a "publish forever" node like a
 * zoo's own gl_mirror, or short-lived for the duration of one drag
 * gesture for a pet) and does 100% of the actual networking on that
 * GUI process's behalf.
 *
 * Usage:
 *   palnet_peer.+x <own_kind> <project_id> <piece_id_or_-> \
 *                  <outbox_file> <inbox_file> [seek_kind]
 *
 * Self-contained, no shared headers, matching this family's own
 * duplicate-rather-than-share-a-header convention. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifndef MSG_NOSIGNAL        /* macOS has no MSG_NOSIGNAL; SIGPIPE is ignored in main() instead */
#define MSG_NOSIGNAL 0
#endif
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_PEERS 32
#define HEARTBEAT_SEC 5
#define STALE_SEC 15
#define SELECT_TIMEOUT_USEC 100000 /* 100ms - responsive enough for a drag, not wasteful */

static char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_shutdown_requested = 0;

typedef struct {
    int fd;
    char node_id[128];
    int hello_sent;
    int hello_received;
    char host[64]; int port; char kind[64]; long since;   /* from HELLO; shown by peers_now.txt */
    int outbound;        /* 1 = we dialed this connection, 0 = we accepted it */
    int dup;             /* set when HELLO shows this node is already connected on another socket; the main loop closes it */
    char seed_key[96];   /* "host:port" of the PALNET_SEEDS entry this connection came from; HELLO never overwrites it */
} PeerConn;

static PeerConn g_peers[MAX_PEERS];
static int g_peer_count = 0;

static char g_own_kind[64];
static char g_project_id[128];
static char g_piece_id[128];
static char g_outbox_path[PATH_BUF];
static char g_inbox_path[PATH_BUF];
static char g_seek_kind[64] = "";
static char g_node_id[128];
static char g_presence_dir[PATH_BUF];
static char g_presence_path[PATH_BUF];
static int g_listen_fd = -1;
static int g_bound_port = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

/* PAL-NET-STANDARD.txt sec. 1 - mirrors shared-ops/pet_export.c's own
 * resolve_exchange_root() exactly: default is one level above this
 * project's own root, PRISC_NET_ROOT overrides it. */
static void resolve_presence_root(char *out, size_t out_sz) {
    const char *env = getenv("PRISC_NET_ROOT");
    if (env && env[0]) { snprintf(out, out_sz, "%s", env); return; }
    char parent[MAX_PATH];
    snprintf(parent, sizeof(parent), "%s", project_root);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s/net/presence", parent);
}

static void mkdir_p(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* PAL-NET-STANDARD.txt sec. 3 - REAL, NEW retry-scan logic, NOT a port
 * of p2p-net's own bind_server_socket() (that binds once to a single
 * fixed, human-pre-configured port and just fails otherwise - checked
 * directly, see PAL-NET-STANDARD.txt sec. 3 for why that doesn't fit
 * this task's own unpredictable process lifecycle). */
/* Addresses come from the environment, never from this file (owner rule: no hardcoded addresses, so the same binary runs on this
 * machine, debil and the Mac with each one's own address):
 *   PALNET_BIND       address to listen on (default 127.0.0.1 = this machine only; a LAN address or 0.0.0.0 to accept other machines).
 *   PALNET_ADVERTISE  host written to the presence file for peers to dial (default: PALNET_BIND; required when binding 0.0.0.0).
 *   PALNET_SEEDS      comma list of host:port to dial when the presence directory has nobody (cross-machine discovery:
 *                     the presence directory is a local folder, so a remote peer is only reachable by a seed). Used with seek_kind.
 * NOTE: a non-loopback bind accepts any connection; there is no authentication yet (PAL-NET-STANDARD sec. 7). LAN test use only. */
static const char *bind_host(void) { const char *e = getenv("PALNET_BIND"); return (e && e[0]) ? e : "127.0.0.1"; }
static const char *advertise_host(void) {
    const char *e = getenv("PALNET_ADVERTISE"); if (e && e[0]) return e;
    const char *b = bind_host(); return strcmp(b, "0.0.0.0") == 0 ? "127.0.0.1" : b;
}

static int bind_with_retry(int base_port, int *out_port) {
    for (int attempt = 0; attempt < 200; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = strcmp(bind_host(), "0.0.0.0") == 0 ? htonl(INADDR_ANY) : inet_addr(bind_host());
        if (addr.sin_addr.s_addr == INADDR_NONE && strcmp(bind_host(), "255.255.255.255") != 0) { close(fd); fprintf(stderr, "palnet_peer: bad PALNET_BIND '%s'\n", bind_host()); return -1; }
        addr.sin_port = htons((uint16_t)(base_port + attempt));

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0 && listen(fd, 8) == 0) {
            set_nonblocking(fd);
            *out_port = base_port + attempt;
            return fd;
        }
        close(fd);
    }
    return -1;
}

/* Base port per kind, so two different kinds never collide on their
 * first-choice port (PAL-NET-STANDARD.txt sec. 3) - not a closed list,
 * any new kind not named here just gets a shared fallback base. */
static int base_port_for_kind(const char *kind) {
    if (strcmp(kind, "zoo") == 0) return 9900;
    if (strcmp(kind, "pet") == 0) return 9901;
    return 9950;
}

static void write_presence_file(void) {
    FILE *f = fopen(g_presence_path, "w");
    if (!f) return;
    fprintf(f, "kind=%s\n", g_own_kind);
    fprintf(f, "project_id=%s\n", g_project_id);
    fprintf(f, "piece_id=%s\n", g_piece_id);
    fprintf(f, "host=%s\n", advertise_host());
    fprintf(f, "port=%d\n", g_bound_port);
    fprintf(f, "pid=%d\n", (int)getpid());
    fprintf(f, "last_seen=%ld\n", (long)time(NULL));
    fclose(f);
}

static int read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    if (!read_kv_str(path, key, buf, sizeof(buf))) return def;
    return atoi(buf);
}

/* PAL-NET-STANDARD.txt sec. 2/4 - only used when seek_kind is set.
 * Scans the presence directory for a live (non-stale), not-already-
 * connected node of the wanted kind, excluding this node's own
 * presence file. Returns 1 and fills host/port/node_id on success. */
static int find_seek_candidate(char *out_host, size_t host_sz, int *out_port, char *out_node_id, size_t node_id_sz) {
    DIR *d = opendir(g_presence_dir);
    if (!d) return 0;
    time_t now = time(NULL);
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(full, sizeof(full), "%s/%s", g_presence_dir, ent->d_name);
#pragma GCC diagnostic pop

        char kind[64];
        if (!read_kv_str(full, "kind", kind, sizeof(kind))) continue;
        if (strcmp(kind, g_seek_kind) != 0) continue;

        char node_id[128];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(node_id, sizeof(node_id), "%s", ent->d_name);
#pragma GCC diagnostic pop
        char *dot = strstr(node_id, ".txt");
        if (dot) *dot = '\0';
        if (strcmp(node_id, g_node_id) == 0) continue; /* never seek self */

        int already = 0;
        for (int i = 0; i < g_peer_count; i++) {
            if (strcmp(g_peers[i].node_id, node_id) == 0) { already = 1; break; }
        }
        if (already) continue;

        int last_seen = read_kv_int(full, "last_seen", 0);
        if (now - last_seen > STALE_SEC) continue; /* stale - sec. 4 */

        char host[64];
        if (!read_kv_str(full, "host", host, sizeof(host))) continue;
        int port = read_kv_int(full, "port", 0);
        if (port <= 0) continue;

        snprintf(out_host, host_sz, "%s", host);
        *out_port = port;
        snprintf(out_node_id, node_id_sz, "%s", node_id);
        found = 1;
        break;
    }
    closedir(d);
    return found;
}

/* Address book: every peer that told us its listening address in HELLO is remembered in <project root>/known_peers.txt
 * (`host|port|node_id|kind|last_seen`, one row per host:port, rewritten small), so after a restart peers find each other again
 * without PALNET_SEEDS. Dialed in the seek step below after the presence directory and the seeds. */
static char g_known_path[PATH_BUF];
static time_t g_known_tried[64];
static void known_path(void) { if (!g_known_path[0]) snprintf(g_known_path, sizeof g_known_path, "%s/known_peers.txt", project_root); }
static void remember_peer(const char *host, int port, const char *node_id, const char *kind) {
    if (!host[0] || port <= 0 || !strcmp(node_id, g_node_id)) return;
    known_path();
    char rows[64][384]; int n = 0, found = 0; char key[96]; snprintf(key, sizeof key, "%s|%d|", host, port);
    FILE *f = fopen(g_known_path, "r");
    if (f) { char ln[384]; while (n < 64 && fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue;
        if (!strncmp(ln, key, strlen(key))) { snprintf(rows[n++], 384, "%s|%d|%s|%s|%ld", host, port, node_id, kind, (long)time(NULL)); found = 1; } else snprintf(rows[n++], 384, "%s", ln); } fclose(f); }
    if (!found && n < 64) snprintf(rows[n++], 384, "%s|%d|%s|%s|%ld", host, port, node_id, kind, (long)time(NULL));
    f = fopen(g_known_path, "w"); if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", rows[i]);
    fclose(f);
}
static int next_known(char *out_host, size_t host_sz, int *out_port, char *out_key, size_t key_sz) {
    known_path(); FILE *f = fopen(g_known_path, "r"); if (!f) return 0;
    char ln[384]; int idx = 0, got = 0; time_t now = time(NULL);
    while (!got && idx < 64 && fgets(ln, sizeof ln, f)) {
        ln[strcspn(ln, "\r\n")] = 0; char *host = strtok(ln, "|"), *ps = strtok(NULL, "|"); int my = idx++;
        if (!host || !ps) continue;
        int port = atoi(ps);
        if (port <= 0) continue;
        char key[96]; snprintf(key, sizeof key, "%s:%d", host, port);
        if (!strcmp(host, advertise_host()) && port == g_bound_port) continue;   /* never dial self */
        int connected = 0; for (int i = 0; i < g_peer_count; i++) if (!strcmp(g_peers[i].seed_key, key)) { connected = 1; break; }
        if (connected || now - g_known_tried[my] < STALE_SEC) continue;
        g_known_tried[my] = now; snprintf(out_host, host_sz, "%s", host); *out_port = port; snprintf(out_key, key_sz, "%s", key); got = 1;
    }
    fclose(f); return got;
}

/* Next PALNET_SEEDS entry that is not currently connected and has not been tried in the last STALE_SEC seconds. */
static time_t g_seed_tried[16];
static int next_seed(char *out_host, size_t host_sz, int *out_port, char *out_key, size_t key_sz);
static void add_peer(int fd) {
    if (g_peer_count >= MAX_PEERS) { close(fd); return; }
    set_nonblocking(fd);
    g_peers[g_peer_count].fd = fd;
    g_peers[g_peer_count].node_id[0] = '\0';
    g_peers[g_peer_count].hello_sent = 0;
    g_peers[g_peer_count].hello_received = 0;
    g_peers[g_peer_count].seed_key[0] = '\0';
    g_peers[g_peer_count].dup = 0;
    g_peers[g_peer_count].outbound = 0;
    g_peers[g_peer_count].host[0] = '\0'; g_peers[g_peer_count].port = 0; g_peers[g_peer_count].kind[0] = '\0'; g_peers[g_peer_count].since = 0;
    g_peer_count++;
}

/* peers_now.txt: who is connected RIGHT NOW (peers that said HELLO), `host|port|node_id|kind|since`, rewritten whenever the set changes and on every
 * heartbeat. Windows read it for an "Online" list; known_peers.txt (remembered) minus this file = offline friends. */
static void write_peers_now(void) {
    char path[PATH_BUF], tmp[PATH_BUF];
    snprintf(path, sizeof path, "%s/peers_now.txt", project_root); snprintf(tmp, sizeof tmp, "%s/peers_now.txt.tmp", project_root);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    for (int i = 0; i < g_peer_count; i++)
        if (g_peers[i].hello_received && g_peers[i].host[0] && g_peers[i].port > 0 && !g_peers[i].dup)
            fprintf(f, "%s|%d|%s|%s|%ld\n", g_peers[i].host, g_peers[i].port, g_peers[i].node_id, g_peers[i].kind, g_peers[i].since);
    fclose(f); rename(tmp, path);
}

static int next_seed(char *out_host, size_t host_sz, int *out_port, char *out_key, size_t key_sz) {
    const char *e = getenv("PALNET_SEEDS"); if (!e || !e[0]) return 0;
    char copy[512]; snprintf(copy, sizeof copy, "%s", e);
    time_t now = time(NULL); int idx = 0; char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok && idx < 16; tok = strtok_r(NULL, ",", &save), idx++) {
        char *colon = strrchr(tok, ':'); if (!colon) continue;
        *colon = '\0'; int port = atoi(colon + 1); if (port <= 0 || !tok[0]) continue;
        char key[96]; snprintf(key, sizeof key, "%s:%d", tok, port);
        int connected = 0;
        for (int i = 0; i < g_peer_count; i++) if (strcmp(g_peers[i].seed_key, key) == 0) { connected = 1; break; }
        if (connected || now - g_seed_tried[idx] < STALE_SEC) continue;
        g_seed_tried[idx] = now;
        snprintf(out_host, host_sz, "%s", tok); *out_port = port; snprintf(out_key, key_sz, "%s", key); return 1;
    }
    return 0;
}

static void write_peers_now(void);
static void remove_peer(int idx) {
    close(g_peers[idx].fd);
    for (int i = idx; i < g_peer_count - 1; i++) g_peers[i] = g_peers[i + 1];
    g_peer_count--;
    write_peers_now();
}

static void send_line(int fd, const char *line) {
    size_t len = strlen(line);
    ssize_t sent = send(fd, line, len, MSG_NOSIGNAL);
    (void)sent; /* best-effort - a dead peer is reaped on its own next recv()==0 */
}

static void send_hello_if_needed(int idx) {
    if (g_peers[idx].hello_sent) return;
    char line[256];
    snprintf(line, sizeof(line), "HELLO|%s|%s|%s|%d\n", g_node_id, g_own_kind, advertise_host(), g_bound_port);
    send_line(g_peers[idx].fd, line);
    g_peers[idx].hello_sent = 1;
}

/* REVISED, live-caught during pal-chain's own 2-node mining test: the
 * original version of this function only ever read the outbox file's
 * FIRST line and compared it to the last-sent value - correct for a
 * "mirror the current single value" consumer (e.g. a geometry file
 * overwritten in place), but silently dropped every message after the
 * first for an APPEND-ONLY consumer (chain_send.c/chain_miner.c append
 * a new TX/BLOCK line per event, never touching earlier lines - the
 * first line never "changes" again, so nothing after it was ever
 * broadcast). pal-forum's own posts/follows/likes/DMs need the exact
 * same append-only relay (PAL-FORUM-STANDARD.txt sec. 0), so this is
 * fixed at the canonical, reusable level rather than worked around in
 * each consumer.
 *
 * g_outbox_offset tracks the byte position already broadcast - each
 * call reads and returns every NEW complete line since that offset (a
 * partial trailing line, if the writer is mid-append, is left for the
 * next call, matching handle_peer_data()'s own tolerance for a partial
 * recv()). A one-shot "mirror" consumer that always writes exactly one
 * line still works correctly under this scheme AS LONG AS it APPENDS a
 * new line per change rather than truncating in place - the one
 * existing convention this changes (no real consumer relies on
 * truncate-in-place today - gl_mirror.c's own geometry file is a
 * separate, non-networked mechanism, confirmed by direct read).
 *
 * Returns the number of new lines found (0 if none), each copied into
 * out_lines[i] (up to max_lines). */
static long g_outbox_offset = 0;
static int read_outbox_new_lines(char out_lines[][MAX_LINE], int max_lines) {
    FILE *f = fopen(g_outbox_path, "r");
    if (!f) return 0;
    if (fseek(f, g_outbox_offset, SEEK_SET) != 0) {
        /* outbox was likely truncated (size guard rotation) - reset and reread from start */
        g_outbox_offset = 0;
        if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    }

    int n = 0;
    long consumed = g_outbox_offset;
    char line[MAX_LINE];
    while (n < max_lines && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n') break; /* partial trailing line - wait for the rest */
        line[strcspn(line, "\r\n")] = '\0';
        consumed += (long)len;
        if (line[0] == '\0') continue; /* blank line - skip, don't count as a message */
        snprintf(out_lines[n], MAX_LINE, "%s", line);
        n++;
    }
    g_outbox_offset = consumed;
    fclose(f);
    return n;
}

/* Replays every line already consumed from the outbox (0..g_outbox_offset)
 * to ONE newly-connected peer, so it catches up on the full backlog
 * instead of only whatever the mirror-style "last value" used to be -
 * required for a peer that connects after several TX/BLOCK lines have
 * already been mined/sent, exactly the "mine against other headless
 * nodes" scenario this was built for. */
static void replay_backlog_to_peer(int fd) {
    FILE *f = fopen(g_outbox_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    long pos = 0;
    while (pos < g_outbox_offset && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        pos += (long)len;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        char out[MAX_LINE];
        snprintf(out, sizeof(out), "DATA|%s|%s\n", g_node_id, line);
        send_line(fd, out);
    }
    fclose(f);
}

/* APPENDS a received DATA line (never overwrites) - each message is a
 * distinct event (a TX, a BLOCK, a post, a like...), not a value to be
 * mirrored - see read_outbox_new_lines()'s own header comment for the
 * live-caught bug this fixes at the same time. */
/* Receiver-side dedup: a DATA line already delivered (same sender and content) is not written to the inbox again, so a backlog replayed
 * on a reconnect cannot duplicate it. In-memory ring of 64-bit hashes, per process (consumers still dedup by their own ids across restarts). */
static unsigned long long g_seen[8192]; static unsigned g_seen_n;
static int seen_before(const char *sender, const char *content) {
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = sender; *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
    h ^= '|'; h *= 1099511628211ULL;
    for (const char *p = content; *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
    unsigned n = g_seen_n < 8192 ? g_seen_n : 8192;
    for (unsigned i = 0; i < n; i++) if (g_seen[i] == h) return 1;
    g_seen[g_seen_n++ % 8192] = h; return 0;
}
static void write_inbox(const char *sender_node_id, const char *content) {
    FILE *f = fopen(g_inbox_path, "a");
    if (!f) return;
    fprintf(f, "%s|%s\n", sender_node_id, content);
    fclose(f);
}

/* Parses whatever raw bytes just arrived on one peer's own connection -
 * PAL-NET-STANDARD.txt sec. 3's own pipe-delimited HELLO/DATA lines.
 * A partial line at the end of a recv() is simply dropped (best-effort,
 * matching this family's own tolerance elsewhere for a single missed
 * tick - the NEXT outbox-changed broadcast or heartbeat will resend
 * current state anyway, nothing here is a one-shot event that can't
 * be recovered from a later message). */
static void handle_peer_data(int idx, const char *buf, ssize_t n) {
    char copy[MAX_LINE];
    size_t len = (size_t)n < sizeof(copy) - 1 ? (size_t)n : sizeof(copy) - 1;
    memcpy(copy, buf, len);
    copy[len] = '\0';

    char *line = strtok(copy, "\n");
    while (line) {
        if (strncmp(line, "HELLO|", 6) == 0) {
            char *rest = line + 6;
            char *bar = strchr(rest, '|');
            if (bar) {
                *bar = '\0';
                snprintf(g_peers[idx].node_id, sizeof(g_peers[idx].node_id), "%s", rest);
                g_peers[idx].hello_received = 1;
                char *kind = bar + 1, *host = NULL; int port = 0;
                char *b2 = strchr(kind, '|'); if (b2) { *b2 = '\0'; host = b2 + 1; char *b3 = strchr(host, '|'); if (b3) { *b3 = '\0'; port = atoi(b3 + 1); } }
                if (host && port > 0) {
                    snprintf(g_peers[idx].host, sizeof g_peers[idx].host, "%s", host); g_peers[idx].port = port;
                    snprintf(g_peers[idx].kind, sizeof g_peers[idx].kind, "%s", kind); if (!g_peers[idx].since) g_peers[idx].since = (long)time(NULL);
                    remember_peer(host, port, g_peers[idx].node_id, kind);
                    if (!g_peers[idx].seed_key[0]) snprintf(g_peers[idx].seed_key, sizeof g_peers[idx].seed_key, "%s:%d", host, port);   /* inbound: now known by its listening address */
                }
                /* Two sockets to the same node (both sides dialed): both ends keep the SAME one, the connection dialed by the node with the smaller
                 * node_id, so they cannot close different ones and leave no link. */
                for (int j = 0; j < g_peer_count; j++) {
                    if (j == idx || !g_peers[j].hello_received || strcmp(g_peers[j].node_id, g_peers[idx].node_id)) continue;
                    const char *init_idx = g_peers[idx].outbound ? g_node_id : g_peers[idx].node_id;
                    const char *init_j   = g_peers[j].outbound   ? g_node_id : g_peers[j].node_id;
                    if (strcmp(init_idx, init_j) <= 0) g_peers[j].dup = 1; else g_peers[idx].dup = 1;
                }
            }
        } else if (strncmp(line, "DATA|", 5) == 0) {
            char *rest = line + 5;
            char *bar = strchr(rest, '|');
            if (bar) {
                *bar = '\0';
                if (!seen_before(rest, bar + 1)) write_inbox(rest, bar + 1);
            }
        }
        line = strtok(NULL, "\n");
    }
}

static void broadcast_data(const char *content) {
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "DATA|%s|%s\n", g_node_id, content);
    for (int i = 0; i < g_peer_count; i++) send_line(g_peers[i].fd, line);
}

static void cleanup_and_exit(void) {
    for (int i = 0; i < g_peer_count; i++) close(g_peers[i].fd);
    if (g_listen_fd >= 0) close(g_listen_fd);
    unlink(g_presence_path);
    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <own_kind> <project_id> <piece_id_or_-> <outbox_file> <inbox_file> [seek_kind]\n", argv[0]);
        return 1;
    }
    snprintf(g_own_kind, sizeof(g_own_kind), "%s", argv[1]);
    snprintf(g_project_id, sizeof(g_project_id), "%s", argv[2]);
    snprintf(g_piece_id, sizeof(g_piece_id), "%s", strcmp(argv[3], "-") == 0 ? "" : argv[3]);
    snprintf(g_outbox_path, sizeof(g_outbox_path), "%s", argv[4]);
    snprintf(g_inbox_path, sizeof(g_inbox_path), "%s", argv[5]);
    if (argc >= 7) snprintf(g_seek_kind, sizeof(g_seek_kind), "%s", argv[6]);

    resolve_root();
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    resolve_presence_root(g_presence_dir, sizeof(g_presence_dir));
    mkdir_p(g_presence_dir);

    g_listen_fd = bind_with_retry(base_port_for_kind(g_own_kind), &g_bound_port);
    if (g_listen_fd < 0) {
        fprintf(stderr, "palnet_peer: bind failed after 200 attempts - exiting\n");
        return 1;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(g_node_id, sizeof(g_node_id), "%s-%s-%d", g_project_id, g_piece_id[0] ? g_piece_id : g_own_kind, (int)getpid());
    snprintf(g_presence_path, sizeof(g_presence_path), "%s/%s.txt", g_presence_dir, g_node_id);
#pragma GCC diagnostic pop
    write_presence_file();
    write_peers_now();

    time_t last_heartbeat = time(NULL);

    while (!g_shutdown_requested) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_listen_fd, &rfds);
        int maxfd = g_listen_fd;
        for (int i = 0; i < g_peer_count; i++) {
            FD_SET(g_peers[i].fd, &rfds);
            if (g_peers[i].fd > maxfd) maxfd = g_peers[i].fd;
        }
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = SELECT_TIMEOUT_USEC;
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (ready > 0) {
            if (FD_ISSET(g_listen_fd, &rfds)) {
                for (;;) {
                    int fd = accept(g_listen_fd, NULL, NULL);
                    if (fd < 0) break;
                    add_peer(fd);
                    send_hello_if_needed(g_peer_count - 1);
                    /* Catches this new peer up on the FULL backlog
                     * (every outbox line already consumed), not just a
                     * single "last value" - see replay_backlog_to_peer()'s
                     * own header comment for why (a peer connecting
                     * after several blocks/tx's were already mined
                     * needs every one of them, not just the latest). */
                    replay_backlog_to_peer(fd);
                }
            }
            for (int i = 0; i < g_peer_count; i++) {
                if (!FD_ISSET(g_peers[i].fd, &rfds)) continue;
                char buf[MAX_LINE];
                ssize_t n = recv(g_peers[i].fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
                    remove_peer(i);
                    i--;
                    continue;
                }
                buf[n] = '\0';
                handle_peer_data(i, buf, n);
                write_peers_now();
                { int removed = 0;
                  for (int j = g_peer_count - 1; j >= 0; j--) if (g_peers[j].dup) { remove_peer(j); removed = 1; if (j <= i) i--; }
                  if (removed) continue; }
            }
        }

        /* Outbox -> broadcast, only whatever's genuinely NEW since last
         * check (PAL-NET-STANDARD.txt sec. 3's own "never send
         * unconditionally" rule - still honored, just per-line instead
         * of per-whole-file now that the outbox is append-only). */
        {
            char new_lines[16][MAX_LINE];
            int n = read_outbox_new_lines(new_lines, 16);
            for (int li = 0; li < n; li++) broadcast_data(new_lines[li]);
        }

        /* seek_kind: actively find and connect to a peer of the wanted
         * kind (PAL-NET-STANDARD.txt sec. 2) - this is what makes this
         * node genuinely peer-to-peer rather than only ever accepting. */
        if (g_seek_kind[0] && g_peer_count < MAX_PEERS) {
            char host[64], node_id[128];
            int port;
            char seed_key[96]; seed_key[0] = '\0';
            int have = find_seek_candidate(host, sizeof(host), &port, node_id, sizeof(node_id));
            if (!have && next_seed(host, sizeof(host), &port, seed_key, sizeof(seed_key))) { have = 1; snprintf(node_id, sizeof node_id, "seed-%s", seed_key); }
            if (!have && next_known(host, sizeof(host), &port, seed_key, sizeof(seed_key))) { have = 1; snprintf(node_id, sizeof node_id, "known-%s", seed_key); }
            if (have) {
                int fd = socket(AF_INET, SOCK_STREAM, 0);
                if (fd >= 0) {
                    struct sockaddr_in addr;
                    memset(&addr, 0, sizeof(addr));
                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = inet_addr(host);
                    addr.sin_port = htons((uint16_t)port);
                    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                        add_peer(fd);
                        g_peers[g_peer_count - 1].outbound = 1;
                        snprintf(g_peers[g_peer_count - 1].node_id, sizeof(g_peers[0].node_id), "%s", node_id);
                        snprintf(g_peers[g_peer_count - 1].seed_key, sizeof(g_peers[0].seed_key), "%s", seed_key);
                        send_hello_if_needed(g_peer_count - 1);
                        replay_backlog_to_peer(fd);
                    } else {
                        close(fd);
                    }
                }
            }
        }

        time_t now = time(NULL);
        if (now - last_heartbeat >= HEARTBEAT_SEC) {
            last_heartbeat = now;
            write_presence_file();
            write_peers_now();
        }
    }

    cleanup_and_exit();
    return 0;
}
