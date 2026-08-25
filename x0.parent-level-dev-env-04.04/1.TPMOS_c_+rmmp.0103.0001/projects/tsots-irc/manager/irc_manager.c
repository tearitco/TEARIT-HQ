#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_MSG 512
#define MAX_PACKET 131072
#define MAX_NODES 128
#define MAX_SEEN 4096
#define MAX_LEDGER_TEXT 32768
#define MAX_USERS_PAYLOAD 6144

typedef struct {
    char ip[64];
    int port;
    char name[64];
    int online;
    int is_leader;
    time_t last_seen;
} NodeEntry;

static volatile sig_atomic_t g_shutdown = 0;

static char project_root[MAX_PATH] = ".";
static char gui_state_path[MAX_PATH] = "";
static char history_path[MAX_PATH] = "";
static char ledger_path[MAX_PATH] = "";
static char node_cfg_path[MAX_PATH] = "";
static char known_hosts_path[MAX_PATH] = "";
static char users_path[MAX_PATH] = "";
static char debug_path[MAX_PATH] = "";

static char my_ip[64] = "127.0.0.1";
static int my_port = 8000;
static int default_peer_port = 8000;
static char node_name[64] = "node";
static int force_ip_from_cfg = 0;
static int force_port_from_cfg = 0;

static NodeEntry nodes[MAX_NODES];
static int node_count = 0;
static int my_node_idx = -1;

static int server_fd = -1;
static char seen_ids[MAX_SEEN][96];
static int seen_count = 0;

static char leader_endpoint[96] = "";
static char last_status[256] = "INIT";
static char last_key[32] = "None";
static char last_gui_blob[MAX_PACKET] = "";
static int gui_blob_init = 0;

static char known_host_eps[MAX_NODES][96];
static int known_host_count = 0;

static time_t last_probe_ts = 0;
static time_t last_snapshot_ts = 0;

static char* trim_str(char *s) {
    char *end;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static void sanitize_small(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '|' || *p == ',' || *p == ';' || *p == '\n' || *p == '\r') *p = '_';
    }
}

static void sanitize_text(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '|') *p = '/';
        if (*p == '\n' || *p == '\r') *p = ' ';
    }
}

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static void log_debug(const char *fmt, ...) {
    FILE *f = fopen(debug_path, "a");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "[%ld] ", (long)now);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void trigger_render(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fputs("M\n", f);
        fclose(f);
    }
}

static int is_active_layout(void) {
    char path[MAX_PATH];
    char line[MAX_LINE];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return strstr(line, "tsots-irc") != NULL;
}

static int find_location_kvp(char *out_path, size_t sz) {
    char current[MAX_PATH];
    if (!getcwd(current, sizeof(current))) return 0;
    while (strlen(current) > 1) {
        snprintf(out_path, sz, "%s/pieces/locations/location_kvp", current);
        if (access(out_path, R_OK) == 0) return 1;
        char *slash = strrchr(current, '/');
        if (!slash) break;
        *slash = '\0';
    }
    return 0;
}

static void resolve_paths(void) {
    char cwd[MAX_PATH] = ".";
    char kvp[MAX_PATH];

    if (getcwd(cwd, sizeof(cwd))) {
        char probe[MAX_PATH];
        snprintf(probe, sizeof(probe), "%s/projects/tsots-irc", cwd);
        if (access(probe, F_OK) == 0) {
            snprintf(project_root, sizeof(project_root), "%s", cwd);
        }
    }

    if (find_location_kvp(kvp, sizeof(kvp))) {
        FILE *f = fopen(kvp, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    char probe[MAX_PATH];
                    snprintf(probe, sizeof(probe), "%s/projects/tsots-irc", project_root);
                    if (access(probe, F_OK) != 0) {
                        snprintf(project_root, sizeof(project_root), "%s", v);
                    }
                }
            }
            fclose(f);
        }
    }

    snprintf(gui_state_path, sizeof(gui_state_path), "%s/projects/tsots-irc/manager/gui_state.txt", project_root);
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    snprintf(ledger_path, sizeof(ledger_path), "%s/projects/tsots-irc/chat_ledger.txt", project_root);
    snprintf(node_cfg_path, sizeof(node_cfg_path), "%s/projects/tsots-irc/node_config.txt", project_root);
    snprintf(known_hosts_path, sizeof(known_hosts_path), "%s/projects/tsots-irc/known_hosts.pdl", project_root);
    snprintf(users_path, sizeof(users_path), "%s/projects/tsots-irc/ring_users.txt", project_root);
    snprintf(debug_path, sizeof(debug_path), "%s/projects/tsots-irc/ring_debug.log", project_root);
}

static void ensure_defaults(void) {
    FILE *f;

    if (access(node_cfg_path, F_OK) != 0) {
        f = fopen(node_cfg_path, "w");
        if (f) {
            fprintf(f, "node_name=nodeA\n");
            fclose(f);
        }
    }

    if (access(known_hosts_path, F_OK) != 0) {
        f = fopen(known_hosts_path, "w");
        if (f) {
            fprintf(f, "SECTION      | KEY                | VALUE\n");
            fprintf(f, "----------------------------------------\n");
            fprintf(f, "IP_LIST      | node_0             | 127.0.0.1:19021\n");
            fprintf(f, "IP_LIST      | node_1             | 127.0.0.1:19022\n");
            fprintf(f, "IP_LIST      | node_2             | 127.0.0.1:19023\n");
            fprintf(f, "META         | last_updated       | 2026-05-25\n");
            fclose(f);
        }
    }

    if (access(ledger_path, F_OK) != 0) {
        f = fopen(ledger_path, "w");
        if (f) fclose(f);
    }

    if (access(history_path, F_OK) != 0) {
        f = fopen(history_path, "w");
        if (f) fclose(f);
    }
}

static void discover_my_ip(void) {
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    char host[NI_MAXHOST];

    if (force_ip_from_cfg) return;

    snprintf(my_ip, sizeof(my_ip), "127.0.0.1");

    if (getifaddrs(&ifaddr) == -1) {
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (s != 0) continue;
        if (strcmp(host, "127.0.0.1") == 0) continue;
        snprintf(my_ip, sizeof(my_ip), "%s", host);
        break;
    }

    freeifaddrs(ifaddr);
}

static void load_node_config(void) {
    log_debug("DEBUG: Attempting to load config from: %s", node_cfg_path);
    FILE *f = fopen(node_cfg_path, "r");
    if (!f) {
        log_debug("DEBUG: Failed to open config file!");
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        log_debug("DEBUG: Parsing config line: %s", line);
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = trim_str(line);
        char *v = trim_str(eq + 1);

        if (strcmp(k, "node_name") == 0) {
            snprintf(node_name, sizeof(node_name), "%s", v);
            sanitize_small(node_name);
        } else if (strcmp(k, "my_port") == 0) {
            int p = atoi(v);
            if (p > 0) {
                my_port = p;
                force_port_from_cfg = 1;
                log_debug("DEBUG: Port set to %d", my_port);
            }
        } else if (strcmp(k, "default_peer_port") == 0) {
            int p = atoi(v);
            if (p > 0) {
                default_peer_port = p;
                if (!force_port_from_cfg) my_port = p;
            }
        } else if (strcmp(k, "my_ip") == 0) {
            if (*v) {
                snprintf(my_ip, sizeof(my_ip), "%s", v);
                force_ip_from_cfg = 1;
            }
        }
    }

    fclose(f);
}

static int endpoint_compare(const char *ip_a, int port_a, const char *ip_b, int port_b) {
    int c = strcmp(ip_a, ip_b);
    if (c != 0) return c;
    if (port_a < port_b) return -1;
    if (port_a > port_b) return 1;
    return 0;
}

static int find_node(const char *ip, int port) {
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].port == port && strcmp(nodes[i].ip, ip) == 0) return i;
    }
    return -1;
}

static int ensure_node(const char *ip, int port, const char *name) {
    if (!ip || !*ip || port <= 0) return -1;

    int idx = find_node(ip, port);
    if (idx >= 0) {
        if (name && *name) {
            snprintf(nodes[idx].name, sizeof(nodes[idx].name), "%s", name);
            sanitize_small(nodes[idx].name);
        }
        return idx;
    }

    if (node_count >= MAX_NODES) return -1;

    idx = node_count++;
    memset(&nodes[idx], 0, sizeof(nodes[idx]));
    snprintf(nodes[idx].ip, sizeof(nodes[idx].ip), "%s", ip);
    nodes[idx].port = port;
    if (name && *name) {
        snprintf(nodes[idx].name, sizeof(nodes[idx].name), "%s", name);
    } else {
        snprintf(nodes[idx].name, sizeof(nodes[idx].name), "node_%d", idx);
    }
    sanitize_small(nodes[idx].name);
    nodes[idx].online = 0;
    nodes[idx].is_leader = 0;
    nodes[idx].last_seen = 0;

    return idx;
}

static void mark_online_idx(int idx, int online, time_t seen_at) {
    if (idx < 0 || idx >= node_count) return;
    nodes[idx].online = online;
    if (online) {
        nodes[idx].last_seen = seen_at > 0 ? seen_at : time(NULL);
    }
}

static void clear_all_leaders(void) {
    for (int i = 0; i < node_count; i++) {
        nodes[i].is_leader = 0;
    }
    leader_endpoint[0] = '\0';
}

static int elect_leader(void) {
    int best = -1;
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].online) continue;
        if (best < 0) best = i;
        else if (endpoint_compare(nodes[i].ip, nodes[i].port, nodes[best].ip, nodes[best].port) < 0) best = i;
    }

    clear_all_leaders();
    if (best >= 0) {
        nodes[best].is_leader = 1;
        snprintf(leader_endpoint, sizeof(leader_endpoint), "%s:%d", nodes[best].ip, nodes[best].port);
    }
    return best;
}

static int get_online_count(void) {
    int n = 0;
    for (int i = 0; i < node_count; i++) if (nodes[i].online) n++;
    return n;
}

static int node_is_self_idx(int idx) {
    if (idx < 0 || idx >= node_count) return 0;
    return (nodes[idx].port == my_port && strcmp(nodes[idx].ip, my_ip) == 0);
}

static void refresh_my_node_idx(void) {
    my_node_idx = ensure_node(my_ip, my_port, node_name);
    if (my_node_idx >= 0) {
        mark_online_idx(my_node_idx, 1, time(NULL));
    }
}

static int parse_ip_port_token(const char *token, char *out_ip, size_t out_ip_sz, int *out_port) {
    if (!token || !*token) return -1;
    char local[MAX_LINE];
    snprintf(local, sizeof(local), "%s", token);
    char *t = trim_str(local);
    if (!*t) return -1;

    char *colon = strrchr(t, ':');
    if (colon) {
        *colon = '\0';
        int p = atoi(trim_str(colon + 1));
        if (p <= 0) p = default_peer_port;
        snprintf(out_ip, out_ip_sz, "%s", trim_str(t));
        *out_port = p;
        return 0;
    }

    snprintf(out_ip, out_ip_sz, "%s", t);
    *out_port = default_peer_port;
    return 0;
}

static int is_valid_ipv4(const char *ip) {
    struct sockaddr_in sa;
    if (!ip || !*ip) return 0;
    return inet_pton(AF_INET, ip, &sa.sin_addr) == 1;
}

static void add_known_endpoint(const char *ip, int port) {
    if (!ip || !*ip || port <= 0) return;
    for (int i = 0; i < known_host_count; i++) {
        char existing_ip[64] = "";
        int existing_port = 0;
        if (parse_ip_port_token(known_host_eps[i], existing_ip, sizeof(existing_ip), &existing_port) == 0) {
            if (strcmp(existing_ip, ip) == 0 && existing_port == port) return;
        }
    }
    if (known_host_count < MAX_NODES) {
        snprintf(known_host_eps[known_host_count], sizeof(known_host_eps[known_host_count]), "%s:%d", ip, port);
        known_host_count++;
    }
}

static int endpoint_allowed(const char *ip, int port) {
    if (!ip || !*ip || port <= 0) return 0;
    if (strcmp(ip, my_ip) == 0 && port == my_port) return 1;
    for (int i = 0; i < known_host_count; i++) {
        char allowed_ip[64] = "";
        int allowed_port = 0;
        if (parse_ip_port_token(known_host_eps[i], allowed_ip, sizeof(allowed_ip), &allowed_port) == 0) {
            if (strcmp(allowed_ip, ip) == 0 && allowed_port == port) return 1;
        }
    }
    return 0;
}

static void load_known_hosts(void) {
    FILE *f = fopen(known_hosts_path, "r");
    char fallback_path[MAX_PATH];
    known_host_count = 0;

    if (!f) {
        snprintf(fallback_path, sizeof(fallback_path), "%s/projects/chat-op/known_hosts.pdl", project_root);
        f = fopen(fallback_path, "r");
    }
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char ip[64] = "";
        int port = default_peer_port;

        if (strstr(line, "protocol") && strstr(line, "TCP_PORT_")) {
            char *p = strstr(line, "TCP_PORT_");
            if (p) {
                p += strlen("TCP_PORT_");
                int inferred = atoi(p);
                if (inferred > 0) {
                    default_peer_port = inferred;
                    if (!force_port_from_cfg) my_port = inferred;
                }
            }
            continue;
        }

        if (strstr(line, "IP_LIST") && strchr(line, '|')) {
            char *last = strrchr(line, '|');
            if (!last) continue;
            char *value = trim_str(last + 1);
            if (parse_ip_port_token(value, ip, sizeof(ip), &port) == 0) {
                if (!is_valid_ipv4(ip)) continue;
                ensure_node(ip, port, NULL);
                add_known_endpoint(ip, port);
            }
            continue;
        }

        char raw[MAX_LINE];
        snprintf(raw, sizeof(raw), "%s", line);
        char *t = trim_str(raw);
        if (!*t || *t == '#') continue;
        if (strstr(t, "SECTION") || strstr(t, "META") || strstr(t, "KEY")) continue;
        if (parse_ip_port_token(t, ip, sizeof(ip), &port) == 0) {
            if (!is_valid_ipv4(ip)) continue;
            ensure_node(ip, port, NULL);
            add_known_endpoint(ip, port);
        }
    }

    fclose(f);
}

static void save_users_file(void) {
    FILE *f = fopen(users_path, "w");
    if (!f) return;

    fprintf(f, "# ip:port|online|leader|last_seen|name\n");
    for (int i = 0; i < node_count; i++) {
        fprintf(f, "%s:%d|%d|%d|%ld|%s\n",
                nodes[i].ip,
                nodes[i].port,
                nodes[i].online,
                nodes[i].is_leader,
                (long)nodes[i].last_seen,
                nodes[i].name);
    }

    fclose(f);
}

static void load_users_file(void) {
    FILE *f = fopen(users_path, "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *t = trim_str(line);
        if (!*t || *t == '#') continue;

        char *pipe1 = strchr(t, '|');
        if (!pipe1) continue;
        *pipe1 = '\0';
        char endpoint[128];
        snprintf(endpoint, sizeof(endpoint), "%s", t);

        char ip[64] = "";
        int port = default_peer_port;
        if (parse_ip_port_token(endpoint, ip, sizeof(ip), &port) != 0) continue;
        if (!is_valid_ipv4(ip)) continue;
        if (!endpoint_allowed(ip, port)) continue;

        int idx = ensure_node(ip, port, NULL);
        if (idx < 0) continue;

        char *rest = pipe1 + 1;
        char *pipe2 = strchr(rest, '|');
        if (!pipe2) continue;
        *pipe2 = '\0';
        nodes[idx].online = atoi(rest) ? 1 : 0;

        rest = pipe2 + 1;
        char *pipe3 = strchr(rest, '|');
        if (!pipe3) continue;
        *pipe3 = '\0';
        nodes[idx].is_leader = atoi(rest) ? 1 : 0;

        rest = pipe3 + 1;
        char *pipe4 = strchr(rest, '|');
        if (!pipe4) continue;
        *pipe4 = '\0';
        nodes[idx].last_seen = atol(rest);

        rest = pipe4 + 1;
        snprintf(nodes[idx].name, sizeof(nodes[idx].name), "%s", trim_str(rest));
        sanitize_small(nodes[idx].name);
    }

    fclose(f);
}

static int seen_contains(const char *id) {
    for (int i = 0; i < seen_count; i++) {
        if (strcmp(seen_ids[i], id) == 0) return 1;
    }
    return 0;
}

static void seen_add(const char *id) {
    if (!id || !*id) return;
    if (seen_contains(id)) return;

    if (seen_count < MAX_SEEN) {
        snprintf(seen_ids[seen_count], sizeof(seen_ids[seen_count]), "%s", id);
        seen_count++;
        return;
    }

    for (int i = 1; i < MAX_SEEN; i++) {
        snprintf(seen_ids[i - 1], sizeof(seen_ids[i - 1]), "%s", seen_ids[i]);
    }
    snprintf(seen_ids[MAX_SEEN - 1], sizeof(seen_ids[MAX_SEEN - 1]), "%s", id);
}

static int connect_with_timeout(const char *ip, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        close(sock);
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    int c = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (c < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int s = select(sock + 1, NULL, &wfds, NULL, &tv);
    if (s <= 0) {
        close(sock);
        return -1;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) {
        close(sock);
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags) < 0) {
        close(sock);
        return -1;
    }

    struct timeval io_tv;
    io_tv.tv_sec = 0;
    io_tv.tv_usec = timeout_ms * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &io_tv, sizeof(io_tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &io_tv, sizeof(io_tv));

    return sock;
}

static int send_line_raw(const char *ip, int port, const char *line) {
    int sock = connect_with_timeout(ip, port, 600);
    if (sock < 0) return -1;

    size_t n = strlen(line);
    ssize_t w = send(sock, line, n, 0);
    close(sock);
    return (w == (ssize_t)n) ? 0 : -1;
}

static int send_and_recv_line(const char *ip, int port, const char *line, char *resp, size_t resp_sz) {
    int sock = connect_with_timeout(ip, port, 700);
    if (sock < 0) return -1;

    size_t n = strlen(line);
    ssize_t w = send(sock, line, n, 0);
    if (w != (ssize_t)n) {
        close(sock);
        return -1;
    }

    ssize_t r = recv(sock, resp, resp_sz - 1, 0);
    close(sock);
    if (r <= 0) return -1;
    resp[r] = '\0';
    return 0;
}

static void append_ledger(const char *line) {
    FILE *f = fopen(ledger_path, "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

static void read_ledger_joined(char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(ledger_path, "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *t = trim_str(line);
        if (!*t) continue;
        if (strlen(out) + strlen(t) + 3 >= out_sz) break;
        if (out[0]) strncat(out, "\\n", out_sz - strlen(out) - 1);
        strncat(out, t, out_sz - strlen(out) - 1);
    }

    fclose(f);
}

static int ledger_line_exists(const char *candidate) {
    FILE *f = fopen(ledger_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *t = trim_str(line);
        if (strcmp(t, candidate) == 0) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static int read_ledger_raw(char *out, size_t out_sz) {
    FILE *f = fopen(ledger_path, "r");
    if (!f) {
        out[0] = '\0';
        return 0;
    }
    size_t used = 0;
    out[0] = '\0';
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (used + 1 >= out_sz) break;
        out[used++] = (char)c;
    }
    out[used] = '\0';
    fclose(f);
    return (int)used;
}

static void merge_ledger_raw(const char *raw) {
    if (!raw || !*raw) return;
    char *copy = strdup(raw);
    if (!copy) return;
    char *save = NULL;
    char *line = strtok_r(copy, "\n", &save);
    while (line) {
        char *t = trim_str(line);
        if (*t && !ledger_line_exists(t)) {
            append_ledger(t);
        }
        line = strtok_r(NULL, "\n", &save);
    }
    free(copy);
}

static size_t hex_encode(const unsigned char *in, size_t in_len, char *out, size_t out_sz) {
    static const char *hex = "0123456789ABCDEF";
    if (out_sz < (in_len * 2 + 1)) return 0;
    for (size_t i = 0; i < in_len; i++) {
        out[i * 2] = hex[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[in[i] & 0xF];
    }
    out[in_len * 2] = '\0';
    return in_len * 2;
}

static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static size_t hex_decode(const char *in, unsigned char *out, size_t out_sz) {
    size_t in_len = strlen(in);
    if ((in_len % 2) != 0) return 0;
    size_t out_len = in_len / 2;
    if (out_len + 1 > out_sz) return 0;
    for (size_t i = 0; i < out_len; i++) {
        int hi = hex_val(in[i * 2]);
        int lo = hex_val(in[i * 2 + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    out[out_len] = '\0';
    return out_len;
}

static int build_online_order(int *order, int max_order) {
    int count = 0;
    for (int i = 0; i < node_count && count < max_order; i++) {
        if (!nodes[i].online) continue;
        order[count++] = i;
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            int a = order[j];
            int b = order[j + 1];
            if (endpoint_compare(nodes[a].ip, nodes[a].port, nodes[b].ip, nodes[b].port) > 0) {
                int t = order[j];
                order[j] = order[j + 1];
                order[j + 1] = t;
            }
        }
    }

    return count;
}

static int find_self_pos_in_order(const int *order, int count) {
    for (int i = 0; i < count; i++) {
        int idx = order[i];
        if (node_is_self_idx(idx)) return i;
    }
    return -1;
}

static void serialize_users(char *out, size_t out_sz) {
    out[0] = '\0';

    for (int i = 0; i < node_count; i++) {
        char name[64];
        snprintf(name, sizeof(name), "%s", nodes[i].name);
        sanitize_small(name);

        char entry[256];
        snprintf(entry, sizeof(entry), "%s,%d,%d,%d,%ld,%s;",
                 nodes[i].ip,
                 nodes[i].port,
                 nodes[i].online,
                 nodes[i].is_leader,
                 (long)nodes[i].last_seen,
                 name);

        if (strlen(out) + strlen(entry) < out_sz - 1) {
            strcat(out, entry);
        }
    }
}

static void merge_users_payload(const char *payload) {
    if (!payload || !*payload) return;

    char local[MAX_USERS_PAYLOAD + 64];
    snprintf(local, sizeof(local), "%s", payload);

    char *save = NULL;
    char *entry = strtok_r(local, ";", &save);
    while (entry) {
        char *fields[6] = {0};
        int fcount = 0;

        char *save2 = NULL;
        char *tok = strtok_r(entry, ",", &save2);
        while (tok && fcount < 6) {
            fields[fcount++] = tok;
            tok = strtok_r(NULL, ",", &save2);
        }

        if (fcount == 6) {
            char *ip = trim_str(fields[0]);
            int port = atoi(trim_str(fields[1]));
            int online = atoi(trim_str(fields[2])) ? 1 : 0;
            int leader = atoi(trim_str(fields[3])) ? 1 : 0;
            time_t last = (time_t)atol(trim_str(fields[4]));
            char name[64];
            snprintf(name, sizeof(name), "%s", trim_str(fields[5]));
            sanitize_small(name);
            if (!is_valid_ipv4(ip)) {
                entry = strtok_r(NULL, ";", &save);
                continue;
            }
            if (!endpoint_allowed(ip, port)) {
                entry = strtok_r(NULL, ";", &save);
                continue;
            }

            int idx = ensure_node(ip, port, name);
            if (idx >= 0) {
                if (last >= nodes[idx].last_seen) {
                    nodes[idx].online = online;
                    nodes[idx].last_seen = last;
                    if (*name) snprintf(nodes[idx].name, sizeof(nodes[idx].name), "%s", name);
                }
                if (leader && online) {
                    clear_all_leaders();
                    nodes[idx].is_leader = 1;
                    snprintf(leader_endpoint, sizeof(leader_endpoint), "%s:%d", nodes[idx].ip, nodes[idx].port);
                }
            }
        }

        entry = strtok_r(NULL, ";", &save);
    }
}

static void broadcast_snapshot_ring(const char *snap_id);

static void mark_node_offline(const char *ip, int port, const char *reason) {
    int idx = find_node(ip, port);
    if (idx < 0) return;
    if (!nodes[idx].online) return;

    nodes[idx].online = 0;
    nodes[idx].is_leader = 0;
    log_debug("mark offline %s:%d (%s)", ip, port, reason ? reason : "n/a");

    elect_leader();
    save_users_file();

    char snap_id[96];
    snprintf(snap_id, sizeof(snap_id), "snap-offline-%ld-%d", (long)time(NULL), rand() % 100000);
    broadcast_snapshot_ring(snap_id);
}

static int send_to_next_online(const char *packet, char *sent_endpoint, size_t sent_sz) {
    int order[MAX_NODES];
    int count = build_online_order(order, MAX_NODES);
    int self_pos = find_self_pos_in_order(order, count);

    if (count < 2 || self_pos < 0) return -1;

    for (int step = 1; step < count; step++) {
        int pos = (self_pos + step) % count;
        int idx = order[pos];
        if (node_is_self_idx(idx)) continue;

        if (send_line_raw(nodes[idx].ip, nodes[idx].port, packet) == 0) {
            if (sent_endpoint && sent_sz > 0) {
                snprintf(sent_endpoint, sent_sz, "%s:%d", nodes[idx].ip, nodes[idx].port);
            }
            return 0;
        }

        mark_node_offline(nodes[idx].ip, nodes[idx].port, "send failure");
    }

    return -1;
}

static void forward_ring_message(const char *msg_id, const char *origin, int hops_left, const char *text) {
    if (hops_left <= 0) return;

    char packet[MAX_PACKET];
    snprintf(packet, sizeof(packet), "RINGMSG|%s|%s|%d|%s\n", msg_id, origin, hops_left, text);

    char endpoint[96] = "";
    if (send_to_next_online(packet, endpoint, sizeof(endpoint)) == 0) {
        snprintf(last_status, sizeof(last_status), "Forwarded to %s", endpoint);
    } else {
        snprintf(last_status, sizeof(last_status), "No reachable next hop");
    }
}

static void broadcast_snapshot_ring(const char *snap_id) {
    char payload[MAX_USERS_PAYLOAD];
    serialize_users(payload, sizeof(payload));

    int online_count = get_online_count();
    if (online_count < 2) return;

    char packet[MAX_PACKET];
    snprintf(packet, sizeof(packet), "RINGSNAP|%s|%s:%d|%d|%s\n",
             snap_id,
             my_ip,
             my_port,
             online_count - 1,
             payload);

    send_to_next_online(packet, NULL, 0);
}

static void broadcast_ledger_ring(const char *ledger_id) {
    char raw[MAX_LEDGER_TEXT];
    char hex[MAX_PACKET];
    int raw_len = read_ledger_raw(raw, sizeof(raw));
    if (raw_len <= 0) return;
    if (hex_encode((const unsigned char*)raw, (size_t)raw_len, hex, sizeof(hex)) == 0) return;

    int online_count = get_online_count();
    if (online_count < 2) return;

    char packet[MAX_PACKET];
    snprintf(packet, sizeof(packet), "LEDGER_SYNC|%s|%s:%d|%d|%s\n",
             ledger_id,
             my_ip,
             my_port,
             online_count - 1,
             hex);
    send_to_next_online(packet, NULL, 0);
}

static void submit_local_message(const char *input_text) {
    if (!input_text || !*input_text) return;

    char text[MAX_MSG];
    snprintf(text, sizeof(text), "%s", input_text);
    sanitize_text(text);

    char msg_id[96];
    snprintf(msg_id, sizeof(msg_id), "msg-%ld-%d", (long)time(NULL), rand() % 100000);
    char origin[128];
    snprintf(origin, sizeof(origin), "%s:%d", my_ip, my_port);

    char ledger_line[MAX_MSG + 128];
    snprintf(ledger_line, sizeof(ledger_line), "[%s] %s", origin, text);
    append_ledger(ledger_line);
    seen_add(msg_id);

    int online_count = get_online_count();
    if (online_count > 1) {
        forward_ring_message(msg_id, origin, online_count - 1, text);
        char ledger_id[96];
        snprintf(ledger_id, sizeof(ledger_id), "ledger-%ld-%d", (long)time(NULL), rand() % 100000);
        broadcast_ledger_ring(ledger_id);
    } else {
        snprintf(last_status, sizeof(last_status), "Local-only (no peers online)");
    }
}

static int bind_server_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(my_port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 32) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void handle_packet_line(char *line, const char *remote_ip) {
    char *kind = strtok(line, "|");
    if (!kind) return;

    if (strcmp(kind, "HELLO") == 0) {
        char *ip = strtok(NULL, "|");
        char *port_s = strtok(NULL, "|");
        char *name = strtok(NULL, "|");
        char *ts_s = strtok(NULL, "|");
        (void)ts_s;
        if (!ip || !port_s || !name) return;

        int port = atoi(port_s);
        if (port <= 0) return;

        sanitize_small(name);
        int idx = ensure_node(trim_str(ip), port, trim_str(name));
        if (idx >= 0) {
            mark_online_idx(idx, 1, time(NULL));
        }

        int my_is_leader = 0;
        if (my_node_idx >= 0 && nodes[my_node_idx].is_leader) my_is_leader = 1;

        char ack[MAX_PACKET];
        snprintf(ack, sizeof(ack), "HELLO_ACK|%s|%d|%s|%s|%ld\n",
                 my_ip,
                 my_port,
                 node_name,
                 my_is_leader ? "LEADER" : "NODE",
                 (long)time(NULL));
        send_line_raw(trim_str(ip), port, ack);

        char snap_id[96];
        snprintf(snap_id, sizeof(snap_id), "snap-hello-%ld-%d", (long)time(NULL), rand() % 100000);
        broadcast_snapshot_ring(snap_id);

    } else if (strcmp(kind, "HELLO_ACK") == 0) {
        char *ip = strtok(NULL, "|");
        char *port_s = strtok(NULL, "|");
        char *name = strtok(NULL, "|");
        char *role = strtok(NULL, "|");
        char *ts_s = strtok(NULL, "|");
        (void)ts_s;
        if (!ip || !port_s || !name || !role) return;

        int port = atoi(port_s);
        if (port <= 0) return;

        sanitize_small(name);
        int idx = ensure_node(trim_str(ip), port, trim_str(name));
        if (idx >= 0) {
            mark_online_idx(idx, 1, time(NULL));
            if (strcmp(trim_str(role), "LEADER") == 0) {
                clear_all_leaders();
                nodes[idx].is_leader = 1;
                snprintf(leader_endpoint, sizeof(leader_endpoint), "%s:%d", nodes[idx].ip, nodes[idx].port);
            }
        }

    } else if (strcmp(kind, "PING") == 0) {
        char *ip = strtok(NULL, "|");
        char *port_s = strtok(NULL, "|");
        char *ts_s = strtok(NULL, "|");
        (void)ts_s;
        if (!ip || !port_s) return;

        int port = atoi(port_s);
        int idx = ensure_node(trim_str(ip), port, NULL);
        if (idx >= 0) mark_online_idx(idx, 1, time(NULL));

        char pong[MAX_PACKET];
        snprintf(pong, sizeof(pong), "PONG|%s|%d|%ld\n", my_ip, my_port, (long)time(NULL));
        send_line_raw(trim_str(ip), port, pong);

    } else if (strcmp(kind, "PONG") == 0) {
        char *ip = strtok(NULL, "|");
        char *port_s = strtok(NULL, "|");
        char *ts_s = strtok(NULL, "|");
        (void)ts_s;
        if (!ip || !port_s) return;

        int port = atoi(port_s);
        int idx = ensure_node(trim_str(ip), port, NULL);
        if (idx >= 0) mark_online_idx(idx, 1, time(NULL));

    } else if (strcmp(kind, "RINGMSG") == 0) {
        char *msg_id = strtok(NULL, "|");
        char *origin = strtok(NULL, "|");
        char *hops_s = strtok(NULL, "|");
        char *text = strtok(NULL, "");
        if (!msg_id || !origin || !hops_s || !text) return;

        text = trim_str(text);
        int hops_left = atoi(hops_s);

        if (!seen_contains(msg_id)) {
            char ledger_line[MAX_MSG + 128];
            snprintf(ledger_line, sizeof(ledger_line), "[%s] %s", origin, text);
            append_ledger(ledger_line);
            seen_add(msg_id);
        }

        if (hops_left > 1) {
            forward_ring_message(msg_id, origin, hops_left - 1, text);
        } else {
            snprintf(last_status, sizeof(last_status), "Ring complete for %s", msg_id);
        }

    } else if (strcmp(kind, "LEDGER_SYNC") == 0) {
        char *ledger_id = strtok(NULL, "|");
        char *origin = strtok(NULL, "|");
        char *hops_s = strtok(NULL, "|");
        char *hex_payload = strtok(NULL, "");
        if (!ledger_id || !origin || !hops_s || !hex_payload) return;

        int hops_left = atoi(hops_s);
        if (!seen_contains(ledger_id)) {
            unsigned char decoded[MAX_LEDGER_TEXT];
            size_t dec_len = hex_decode(hex_payload, decoded, sizeof(decoded));
            if (dec_len > 0) {
                merge_ledger_raw((const char*)decoded);
            }
            seen_add(ledger_id);
        }

        if (hops_left > 1) {
            char packet[MAX_PACKET];
            snprintf(packet, sizeof(packet), "LEDGER_SYNC|%s|%s|%d|%s\n", ledger_id, origin, hops_left - 1, hex_payload);
            send_to_next_online(packet, NULL, 0);
        }

    } else if (strcmp(kind, "RINGSNAP") == 0) {
        char *snap_id = strtok(NULL, "|");
        char *origin = strtok(NULL, "|");
        char *hops_s = strtok(NULL, "|");
        char *payload = strtok(NULL, "");
        if (!snap_id || !origin || !hops_s || !payload) return;

        int hops_left = atoi(hops_s);
        if (!seen_contains(snap_id)) {
            merge_users_payload(payload);
            seen_add(snap_id);
        }

        if (hops_left > 1) {
            char packet[MAX_PACKET];
            snprintf(packet, sizeof(packet), "RINGSNAP|%s|%s|%d|%s\n", snap_id, origin, hops_left - 1, payload);
            send_to_next_online(packet, NULL, 0);
        }
    }

    (void)remote_ip;
}

static void poll_server_once(void) {
    if (server_fd < 0) return;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(server_fd, &rfds);

    struct timeval tv = {0, 0};
    int r = select(server_fd + 1, &rfds, NULL, NULL, &tv);
    if (r <= 0 || !FD_ISSET(server_fd, &rfds)) return;

    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int cfd = accept(server_fd, (struct sockaddr*)&caddr, &clen);
    if (cfd < 0) return;

    char remote_ip[64] = "";
    inet_ntop(AF_INET, &caddr.sin_addr, remote_ip, sizeof(remote_ip));

    char buf[MAX_PACKET];
    ssize_t n = recv(cfd, buf, sizeof(buf) - 1, 0);
    close(cfd);
    if (n <= 0) return;

    buf[n] = '\0';

    char *save = NULL;
    char *line = strtok_r(buf, "\n", &save);
    while (line) {
        char tmp[MAX_PACKET];
        snprintf(tmp, sizeof(tmp), "%s", line);
        handle_packet_line(tmp, remote_ip);
        line = strtok_r(NULL, "\n", &save);
    }
}

static void read_input_text(char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(gui_state_path, "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = trim_str(line);
        char *v = trim_str(eq + 1);
        if (strcmp(k, "input_text") == 0) {
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }

    fclose(f);
}

static void clear_input_text(void) {
    FILE *f = fopen(gui_state_path, "a");
    if (f) {
        fputs("input_text=\n", f);
        fclose(f);
    }
}

static void process_key(int key) {
    snprintf(last_key, sizeof(last_key), "KEY:%d", key);

    if (key == 10 || key == 13) {
        char input[MAX_MSG] = "";
        read_input_text(input, sizeof(input));
        if (strlen(trim_str(input)) > 0) {
            submit_local_message(input);
            clear_input_text();
        }
    }
}

static int write_gui_state(void) {
    char messages[MAX_LEDGER_TEXT];
    char current_input[MAX_MSG] = "";
    char users_list[MAX_LEDGER_TEXT] = "";

    read_ledger_joined(messages, sizeof(messages));
    read_input_text(current_input, sizeof(current_input));

    int order[MAX_NODES];
    int online_count = build_online_order(order, MAX_NODES);
    int self_pos = find_self_pos_in_order(order, online_count);

    char next_hop[96] = "N/A";
    char prev_hop[96] = "N/A";

    if (online_count > 1 && self_pos >= 0) {
        int next_idx = order[(self_pos + 1) % online_count];
        int prev_idx = order[(self_pos - 1 + online_count) % online_count];
        snprintf(next_hop, sizeof(next_hop), "%s:%d", nodes[next_idx].ip, nodes[next_idx].port);
        snprintf(prev_hop, sizeof(prev_hop), "%s:%d", nodes[prev_idx].ip, nodes[prev_idx].port);
    }

    for (int i = 0; i < node_count; i++) {
        char row[256];
        char marker = '-';
        if (node_is_self_idx(i)) marker = '*';
        if (!nodes[i].online) marker = 'x';

        snprintf(row, sizeof(row), "%c %s:%d %s %s\\n",
                 marker,
                 nodes[i].ip,
                 nodes[i].port,
                 nodes[i].is_leader ? "LEADER" : "NODE",
                 nodes[i].online ? "ONLINE" : "OFFLINE");
        if (strlen(users_list) + strlen(row) < sizeof(users_list) - 1) {
            strcat(users_list, row);
        }
    }

    char blob[MAX_PACKET];
    int written = snprintf(blob, sizeof(blob),
        "network_status=%s\n"
        "node_name=%s\n"
        "my_endpoint=%s:%d\n"
        "leader_endpoint=%s\n"
        "peer_count=%d\n"
        "ring_size=%d\n"
        "ring_index=%d\n"
        "next_hop=%s\n"
        "prev_hop=%s\n"
        "ring_hosts=%s\n"
        "messages=%s\n"
        "manager_status=%s\n"
        "last_key=%s\n"
        "input_text=%s\n",
        (server_fd >= 0) ? "ONLINE" : "BIND_FAIL",
        node_name,
        my_ip, my_port,
        leader_endpoint[0] ? leader_endpoint : "NONE",
        online_count > 0 ? online_count - 1 : 0,
        online_count,
        self_pos,
        next_hop,
        prev_hop,
        users_list,
        messages,
        last_status,
        last_key,
        current_input
    );
    if (written <= 0) return 0;
    if ((size_t)written >= sizeof(blob)) {
        blob[sizeof(blob) - 1] = '\0';
    }

    if (gui_blob_init && strcmp(last_gui_blob, blob) == 0) {
        return 0;
    }

    FILE *f = fopen(gui_state_path, "w");
    if (!f) return 0;
    fputs(blob, f);
    fclose(f);

    snprintf(last_gui_blob, sizeof(last_gui_blob), "%s", blob);
    gui_blob_init = 1;
    return 1;
}

static void process_history_events(long *last_pos) {
    struct stat st;
    if (stat(history_path, &st) != 0) return;
    if (st.st_size < *last_pos) *last_pos = 0;
    if (st.st_size == *last_pos) return;

    FILE *f = fopen(history_path, "r");
    if (!f) return;
    fseek(f, *last_pos, SEEK_SET);

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *t = trim_str(line);
        if (!*t) continue;
        if (isdigit((unsigned char)t[0])) {
            process_key(atoi(t));
        }
    }

    *last_pos = ftell(f);
    fclose(f);
}

static void send_hello_to_node(int idx) {
    if (idx < 0 || idx >= node_count) return;
    if (node_is_self_idx(idx)) return;

    char packet[MAX_PACKET];
    snprintf(packet, sizeof(packet), "HELLO|%s|%d|%s|%ld\n", my_ip, my_port, node_name, (long)time(NULL));

    char resp[MAX_PACKET];
    if (send_and_recv_line(nodes[idx].ip, nodes[idx].port, packet, resp, sizeof(resp)) == 0) {
        mark_online_idx(idx, 1, time(NULL));
        char *save = NULL;
        char *line = strtok_r(resp, "\n", &save);
        while (line) {
            char tmp[MAX_PACKET];
            snprintf(tmp, sizeof(tmp), "%s", line);
            handle_packet_line(tmp, nodes[idx].ip);
            line = strtok_r(NULL, "\n", &save);
        }
    } else {
        mark_node_offline(nodes[idx].ip, nodes[idx].port, "hello timeout");
    }
}

static void bootstrap_known_hosts(void) {
    int handshakes = 0;

    for (int i = 0; i < node_count; i++) {
        if (node_is_self_idx(i)) continue;
        send_hello_to_node(i);
        if (nodes[i].online) handshakes++;
    }

    if (handshakes == 0) {
        elect_leader();
        if (my_node_idx >= 0 && nodes[my_node_idx].is_leader) {
            snprintf(last_status, sizeof(last_status), "No host handshake; became ring leader");
        }
    } else {
        if (!leader_endpoint[0]) elect_leader();
        snprintf(last_status, sizeof(last_status), "Connected to known host(s)");
    }

    save_users_file();

    char snap_id[96];
    snprintf(snap_id, sizeof(snap_id), "snap-bootstrap-%ld-%d", (long)time(NULL), rand() % 100000);
    broadcast_snapshot_ring(snap_id);
}

static void periodic_probe_hosts(void) {
    time_t now = time(NULL);
    if (now - last_probe_ts < 3) return;
    last_probe_ts = now;

    for (int i = 0; i < node_count; i++) {
        if (node_is_self_idx(i)) continue;

        char ping[MAX_PACKET];
        snprintf(ping, sizeof(ping), "PING|%s|%d|%ld\n", my_ip, my_port, (long)now);
        if (send_line_raw(nodes[i].ip, nodes[i].port, ping) == 0) {
            mark_online_idx(i, 1, now);
        } else {
            mark_node_offline(nodes[i].ip, nodes[i].port, "ping fail");
        }
    }

    if (!leader_endpoint[0] || get_online_count() <= 0) {
        elect_leader();
    } else {
        int leader_ok = 0;
        for (int i = 0; i < node_count; i++) {
            if (nodes[i].is_leader && nodes[i].online) {
                leader_ok = 1;
                break;
            }
        }
        if (!leader_ok) elect_leader();
    }

    save_users_file();
}

static void periodic_snapshot(void) {
    time_t now = time(NULL);
    if (now - last_snapshot_ts < 5) return;
    last_snapshot_ts = now;

    char snap_id[96];
    snprintf(snap_id, sizeof(snap_id), "snap-periodic-%ld-%d", (long)now, rand() % 100000);
    broadcast_snapshot_ring(snap_id);
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    srand((unsigned int)(time(NULL) ^ getpid()));

    resolve_paths();
    ensure_defaults();
    load_node_config();
    discover_my_ip();

    load_users_file();
    refresh_my_node_idx();
    load_known_hosts();
    refresh_my_node_idx();

    if (my_node_idx >= 0) {
        nodes[my_node_idx].online = 1;
        nodes[my_node_idx].last_seen = time(NULL);
        snprintf(nodes[my_node_idx].name, sizeof(nodes[my_node_idx].name), "%s", node_name);
    }

    server_fd = bind_server_socket();
    if (server_fd < 0) {
        snprintf(last_status, sizeof(last_status), "FATAL: Could not bind port %d", my_port);
        log_debug("FATAL: bind failed on %d (%s)", my_port, strerror(errno));
        // Exit to force user/orchestrator to address the collision
        exit(1);
    } else {
        snprintf(last_status, sizeof(last_status), "Listening on %s:%d", my_ip, my_port);
        log_debug("listening on %s:%d", my_ip, my_port);
    }

    if (server_fd >= 0) {
        bootstrap_known_hosts();
    }

    long last_hist_pos = 0;
    struct stat st;
    if (stat(history_path, &st) == 0) last_hist_pos = st.st_size;

    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }

        process_history_events(&last_hist_pos);
        poll_server_once();
        periodic_probe_hosts();
        periodic_snapshot();

        if (!leader_endpoint[0]) {
            elect_leader();
        }

        int gui_changed = write_gui_state();
        save_users_file();
        if (gui_changed) {
            trigger_render();
        }

        usleep(80000);
    }

    if (server_fd >= 0) close(server_fd);
    return 0;
}
