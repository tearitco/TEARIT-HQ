/*
 * p2p_heartbeat.c - P2P Chat Node Discovery & Status Sync (v2)
 * CPU-SAFE: non-destructive state update.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define STALE_THRESHOLD 15

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

static void update_gui_state(const char *gui_path, const char *peer_list, int peer_count, const char *messages) {
    char *keys[100];
    char *vals[100];
    int count = 0;

    FILE *f = fopen(gui_path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && count < 100) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                keys[count] = strdup(trim_str(line));
                vals[count] = strdup(trim_str(eq + 1));
                count++;
            }
        }
        fclose(f);
    }

    f = fopen(gui_path, "w");
    if (f) {
        int found_peers = 0, found_count = 0, found_status = 0, found_msgs = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(keys[i], "connected_peers_list") == 0) {
                fprintf(f, "connected_peers_list=%s\n", peer_list);
                found_peers = 1;
            } else if (strcmp(keys[i], "peer_count") == 0) {
                fprintf(f, "peer_count=%d\n", peer_count);
                found_count = 1;
            } else if (strcmp(keys[i], "network_status") == 0) {
                fprintf(f, "network_status=ONLINE\n");
                found_status = 1;
            } else if (strcmp(keys[i], "messages") == 0) {
                fprintf(f, "messages=%s\n", messages);
                found_msgs = 1;
            } else {
                fprintf(f, "%s=%s\n", keys[i], vals[i]);
            }
            free(keys[i]);
            free(vals[i]);
        }
        if (!found_peers) fprintf(f, "connected_peers_list=%s\n", peer_list);
        if (!found_count) fprintf(f, "peer_count=%d\n", peer_count);
        if (!found_status) fprintf(f, "network_status=ONLINE\n");
        if (!found_msgs) fprintf(f, "messages=%s\n", messages);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    char *root = argv[1], *hash = argv[2];
    int port = atoi(argv[3]);
    char *subnet = argv[4];
    char *mode = (argc > 5) ? argv[5] : "DEV";

    char *reg_dir; asprintf(&reg_dir, "%s/pieces/network/registry", root);
    mkdir(reg_dir, 0755);

    // Register self
    char *self_path; asprintf(&self_path, "%s/node_%s.txt", reg_dir, hash);
    FILE *sf = fopen(self_path, "w");
    if (sf) {
        fprintf(sf, "hash=%s\nport=%d\nsubnet=%s\nlast_seen=%ld\n", hash, port, subnet, time(NULL));
        fclose(sf);
    }

    // Discover
    DIR *dir = opendir(reg_dir);
    char peer_list[4096] = "";
    int peer_count = 0;
    time_t now = time(NULL);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char *path; asprintf(&path, "%s/%s", reg_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0) {
            FILE *f = fopen(path, "r");
            if (f) {
                char line[MAX_LINE], p_hash[64] = "", p_subnet[64] = "";
                int p_port = 0; long p_last = 0;
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "hash=", 5) == 0) strcpy(p_hash, trim_str(line + 5));
                    else if (strncmp(line, "port=", 5) == 0) p_port = atoi(line + 5);
                    else if (strncmp(line, "subnet=", 7) == 0) strcpy(p_subnet, trim_str(line + 7));
                    else if (strncmp(line, "last_seen=", 10) == 0) p_last = atol(line + 10);
                }
                fclose(f);
                if (now - p_last > STALE_THRESHOLD) {
                    unlink(path);
                } else {
                    char entry_str[256];
                    int is_self = (strcmp(p_hash, hash) == 0);
                    
                    if (strcmp(mode, "USER") == 0) {
                        char short_hash[9];
                        strncpy(short_hash, p_hash, 8);
                        short_hash[8] = '\0';
                        if (is_self) {
                            snprintf(entry_str, sizeof(entry_str), "User-%s (YOU)\\n", short_hash);
                        } else {
                            snprintf(entry_str, sizeof(entry_str), "User-%s\\n", short_hash);
                        }
                    } else {
                        if (is_self) {
                            snprintf(entry_str, sizeof(entry_str), "[%s] (YOU) | PORT:%d | SUB:%s\\n", p_hash, p_port, p_subnet);
                        } else {
                            snprintf(entry_str, sizeof(entry_str), "[%s] | PORT:%d | SUB:%s\\n", p_hash, p_port, p_subnet);
                        }
                    }

                    if (strlen(peer_list) + strlen(entry_str) < sizeof(peer_list) - 1) {
                        strcat(peer_list, entry_str);
                    }
                    peer_count++;
                }
            }
        }
        free(path);
    }
    closedir(dir);

    // Read Broadcast Messages
    char messages[8192] = "";
    char *msg_path; asprintf(&msg_path, "%s/pieces/network/broadcast/messages.txt", root);
    FILE *mf = fopen(msg_path, "r");
    if (mf) {
        char line[MAX_LINE];
        // Skip to end or read last N lines
        // For now, read all (simple POC)
        while (fgets(line, sizeof(line), mf)) {
            char *l = trim_str(line);
            if (strlen(messages) + strlen(l) + 2 < sizeof(messages)) {
                strcat(messages, l);
                strcat(messages, "\\n");
            }
        }
        fclose(mf);
    }
    free(msg_path);

    char *gui_path; asprintf(&gui_path, "%s/projects/chat-op/manager/gui_state.txt", root);
    update_gui_state(gui_path, peer_list, peer_count, messages);

    free(gui_path); free(self_path); free(reg_dir);
    return 0;
}
