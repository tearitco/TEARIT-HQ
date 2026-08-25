#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <ifaddrs.h>

#define MAX_PEERS 12
#define MAX_PATH 4096
#define MAX_LINE 1024
#define DEFAULT_PORT 8000
#define BUFFER_SIZE 8192

typedef struct {
    char hash[64];
    char ip[64];
    int port;
    int socket;
    int active;
} Peer;

static Peer peers[MAX_PEERS];
static int peer_count = 0;
static char my_hash[64];
static char my_ip[64] = "0.0.0.0"; // Will resolve
static char host_ip[64] = "";
static int is_host = 0;
static char project_root[MAX_PATH];

static volatile sig_atomic_t g_shutdown = 0;

/* --- Forward Declarations --- */
static void handle_sigint(int sig);
static char* trim_str(char *str);
static void log_msg(const char *msg);
static void append_to_chat_log(const char* msg);
static void update_gui_state(void);
static void broadcast_peer_list(void);
static void handle_client_data(int sock_idx);
static int try_connect(const char *ip);
static void discover_host(void);
static void resolve_my_ip(void);

/* --- Implementations --- */

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void log_msg(const char *msg) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/projects/chat-op/daemon_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s] %s\n", ts, msg);
        fclose(f);
    }
}

static void append_to_chat_log(const char* msg) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/projects/chat-op/chat_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s", msg);
        if (msg[strlen(msg)-1] != '\n') fprintf(f, "\n");
        fclose(f);
    }
}

static char ui_mode[32] = "DEV";

static void trigger_render(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

static void update_gui_state(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/projects/chat-op/manager/gui_state.txt", project_root);
    
    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/projects/chat-op/chat_log.txt", project_root);
    
    char messages[BUFFER_SIZE] = "";
    FILE *lf = fopen(log_path, "r");
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            char *trimmed = trim_str(line);
            if (strlen(messages) + strlen(trimmed) + 3 < sizeof(messages)) {
                if (strlen(messages) > 0) strcat(messages, "\\n");
                strcat(messages, trimmed);
            }
        }
        fclose(lf);
    }

    char peer_list[BUFFER_SIZE] = "";
    if (strcmp(ui_mode, "USER") == 0) {
        snprintf(peer_list, sizeof(peer_list), "User-%.4s (YOU)\\n", my_hash);
    } else {
        snprintf(peer_list, sizeof(peer_list), "[%s] (YOU) | IP:%s\\n", my_hash, my_ip);
    }

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            char entry[256];
            if (strcmp(ui_mode, "USER") == 0) {
                snprintf(entry, sizeof(entry), "User-%.4s\\n", peers[i].hash);
            } else {
                snprintf(entry, sizeof(entry), "[%s] | IP:%s\\n", peers[i].hash, peers[i].ip);
            }
            if (strlen(peer_list) + strlen(entry) < sizeof(peer_list) - 1) strcat(peer_list, entry);
        }
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "peer_count=%d\n", is_host ? peer_count + 1 : peer_count);
        fprintf(f, "network_status=%s\n", (peer_count > 0 || is_host) ? "ONLINE" : "DISCOVERING");
        fprintf(f, "net_role=%s\n", is_host ? "HOST" : "CLIENT");
        fprintf(f, "my_ip=%s\n", my_ip);
        fprintf(f, "host_ip=%s\n", is_host ? "SELF" : host_ip);
        fprintf(f, "connected_peers_list=%s\n", peer_list);
        fprintf(f, "messages=%s\n", messages);
        fclose(f);
    }
    trigger_render();
}

static void resolve_my_ip(void) {
    struct ifaddrs *ifaddr, *ifa;
    int s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        strncpy(my_ip, "127.0.0.1", 63);
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                           host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) {
                if (strcmp(host, "127.0.0.1") != 0) {
                    strncpy(my_ip, host, 63);
                    freeifaddrs(ifaddr);
                    return;
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    if (strlen(my_ip) == 0 || strcmp(my_ip, "0.0.0.0") == 0) {
        strncpy(my_ip, "127.0.0.1", 63);
    }
}

static void clear_ports(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "fuser -k %d/tcp > /dev/null 2>&1", DEFAULT_PORT);
    system(cmd);
}

static int try_connect(const char *ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    // Set non-blocking for timeout
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms probe

    if (select(sock + 1, NULL, &fdset, NULL, &tv) == 1) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
            // Restore blocking
            fcntl(sock, F_SETFL, flags);
            return sock;
        }
    }

    close(sock);
    return -1;
}

static void discover_host(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/projects/chat-op/known_hosts.pdl", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "node_")) {
            char *pipe = strrchr(line, '|');
            if (pipe) {
                char *ip = trim_str(pipe + 1);
                if (strcmp(ip, my_ip) == 0) continue;
                
                log_msg("Probing known host...");
                int sock = try_connect(ip);
                if (sock >= 0) {
                    strncpy(host_ip, ip, 63);
                    peers[0].socket = sock;
                    peers[0].active = 1;
                    peer_count = 1;
                    char hello[256];
                    snprintf(hello, sizeof(hello), "TCP_SYN|%s|%s\n", my_hash, my_ip);
                    send(sock, hello, strlen(hello), 0);
                    log_msg("Host found! Sent SYN.");
                    fclose(f);
                    return;
                }
            }
        }
    }
    fclose(f);
    is_host = 1;
    log_msg("No host found. Assuming ROLE:HOST.");
}

static void broadcast_peer_list(void) {
    if (!is_host) return;
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "PEER_TABLE");
    for (int i = 0; i < peer_count; i++) {
        if (!peers[i].active) continue;
        char entry[256];
        snprintf(entry, sizeof(entry), "|%s:%s", peers[i].hash, peers[i].ip);
        strcat(buffer, entry);
    }
    strcat(buffer, "\n");

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active && peers[i].socket > 0) {
            send(peers[i].socket, buffer, strlen(buffer), 0);
        }
    }
}

static void handle_client_data(int sock_idx) {
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(peers[sock_idx].socket, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) {
        peers[sock_idx].active = 0;
        close(peers[sock_idx].socket);
        peers[sock_idx].socket = -1;
        log_msg("Connection lost.");
        if (is_host) broadcast_peer_list();
        return;
    }
    buffer[n] = '\0';

    if (strncmp(buffer, "TCP_SYN|", 8) == 0) {
        char *hash = buffer + 8;
        char *ip = strchr(hash, '|');
        if (ip) {
            *ip = '\0'; ip = trim_str(ip + 1);
            strncpy(peers[sock_idx].hash, trim_str(hash), 63);
            strncpy(peers[sock_idx].ip, ip, 63);
            peers[sock_idx].active = 1;
            log_msg("Handshake: Received SYN.");
            
            if (is_host) {
                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "TCP_SYN_ACK|%s|OK\n", my_hash);
                send(peers[sock_idx].socket, ack, strlen(ack), 0);
                broadcast_peer_list();
                
                // Sync History
                char log_path[1024];
                snprintf(log_path, sizeof(log_path), "%s/projects/chat-op/chat_log.txt", project_root);
                FILE *hf = fopen(log_path, "r");
                if (hf) {
                    char hline[MAX_LINE];
                    while (fgets(hline, sizeof(hline), hf)) {
                        char sync[BUFFER_SIZE];
                        snprintf(sync, sizeof(sync), "LOG_SYNC|%s", hline);
                        send(peers[sock_idx].socket, sync, strlen(sync), 0);
                    }
                    fclose(hf);
                }
            }
        }
    } else if (strncmp(buffer, "TCP_SYN_ACK|", 12) == 0) {
        log_msg("Handshake: Received SYN-ACK.");
        send(peers[sock_idx].socket, "TCP_ACK|OK\n", 11, 0);
    } else if (strncmp(buffer, "MSG|", 4) == 0) {
        // buffer is MSG|[HASH] TEXT
        // If we are Host, we must log it (if not already logged) and relay to all OTHER peers.
        // If we are Client, we just log it.
        char *msg_content = buffer + 4;
        
        // Simple deduplication: only log if we didn't send it, or if we are the host
        // This is tricky. Let's just log if it doesn't start with our hash.
        char my_prefix[64];
        snprintf(my_prefix, sizeof(my_prefix), "[%s]", my_hash);
        if (strncmp(msg_content, my_prefix, strlen(my_prefix)) != 0) {
            append_to_chat_log(msg_content);
        }

        if (is_host) {
            log_msg("Relaying MSG to peers.");
            for (int i = 0; i < peer_count; i++) {
                if (peers[i].active && peers[i].socket != peers[sock_idx].socket) {
                    send(peers[i].socket, buffer, n, 0);
                }
            }
        }
    } else if (strncmp(buffer, "LOG_SYNC|", 9) == 0) {
        append_to_chat_log(buffer + 9);
    } else if (strncmp(buffer, "PEER_TABLE|", 11) == 0) {
        log_msg("Peer table updated.");
    }
}

int main(int argc, char *argv[]) {
    // Immediate debug for startup
    FILE *init_debug = fopen("projects/chat-op/daemon_init.txt", "w");
    if (init_debug) { fprintf(init_debug, "Daemon starting...\n"); fclose(init_debug); }

    if (argc < 3) return 1;
    strncpy(project_root, argv[1], MAX_PATH-1);
    strncpy(my_hash, argv[2], 63);
    if (argc > 4) strncpy(ui_mode, argv[4], 31);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    clear_ports();
    resolve_my_ip();
    update_gui_state(); // Initial state for UI

    discover_host();
    update_gui_state(); // State after discovery

    int server_fd = -1;
    if (is_host) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(DEFAULT_PORT);
        
        if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            log_msg("CRITICAL: Failed to bind as HOST (Port 8000 taken). Switching to CLIENT.");
            is_host = 0;
            // In a production system, we would trigger a re-scan. For now, exit to avoid confusion.
            return 1;
        }
        listen(server_fd, 5);
        log_msg("Successfully bound as HOST on 8000.");
    }

    while (!g_shutdown) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;

        if (is_host) {
            FD_SET(server_fd, &readfds);
            max_fd = server_fd;
        }

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].socket > 0) {
                FD_SET(peers[i].socket, &readfds);
                if (peers[i].socket > max_fd) max_fd = peers[i].socket;
            }
        }

        struct timeval tv = {1, 0};
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR) continue;

        if (is_host && FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
            if (new_sock >= 0 && peer_count < MAX_PEERS) {
                peers[peer_count].socket = new_sock;
                peers[peer_count].active = 0;
                peer_count++;
                log_msg("Accepted new connection.");
            }
        }

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].socket > 0 && FD_ISSET(peers[i].socket, &readfds)) {
                handle_client_data(i);
            }
        }

        // Poll outbox
        char outbox_path[1024];
        snprintf(outbox_path, sizeof(outbox_path), "%s/pieces/network/outbox.txt", project_root);
        struct stat out_st;
        if (stat(outbox_path, &out_st) == 0 && out_st.st_size > 0) {
            FILE *f = fopen(outbox_path, "r");
            if (f) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), f)) {
                    char msg[BUFFER_SIZE];
                    snprintf(msg, sizeof(msg), "MSG|[%s] %s", my_hash, line);
                    if (is_host) {
                        append_to_chat_log(msg + 4);
                        for (int j = 0; j < peer_count; j++) if (peers[j].active) send(peers[j].socket, msg, strlen(msg), 0);
                    } else if (peers[0].active) {
                        send(peers[0].socket, msg, strlen(msg), 0);
                    }
                }
                fclose(f);
                truncate(outbox_path, 0);
            }
        }

        update_gui_state();
        usleep(100000);
    }

    if (server_fd >= 0) close(server_fd);
    for (int i = 0; i < peer_count; i++) if (peers[i].socket >= 0) close(peers[i].socket);

    return 0;
}
