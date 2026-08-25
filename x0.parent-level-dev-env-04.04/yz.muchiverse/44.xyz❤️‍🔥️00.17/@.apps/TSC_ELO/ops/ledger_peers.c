/* ledger_peers.c — Query house runtime ledger for active peers by type
 * Usage: ledger_peers <type>
 *   type  editor | widget | app | daemon
 *
 * Scans ledger for latest ONLINE entry of each project_id matching type,
 * checks /proc/<pid> for aliveness. Returns active peers.
 *
 * Output: pipe-delimited lines (one per active peer):
 *   session_root|inbox_path|display_name|project_id|pid
 *
 * Returns 0 if any peer found, 1 if none.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 4096
#define MAX_PATH 4096
#define MAX_PEERS 64

typedef struct {
    char project_id[128];
    char session_root[MAX_PATH];
    char inbox_path[MAX_PATH];
    char display_name[256];
    char pid_str[64];
    char timestamp[64];
} PeerEntry;

static char house_root[MAX_PATH] = "";

static void resolve_house_root(void) {
    const char *prisc_root = getenv("PRISC_PROJECT_ROOT");
    if (!prisc_root || !prisc_root[0]) {
        fprintf(stderr, "Error: PRISC_PROJECT_ROOT not set\n");
        exit(1);
    }
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", prisc_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (!fgets(house_root, sizeof(house_root), f)) { fclose(f); return; }
    fclose(f);
    size_t ln = strlen(house_root);
    while (ln > 0 && (house_root[ln-1] == '\n' || house_root[ln-1] == '\r')) house_root[--ln] = '\0';
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static int ledger_path(char *out, size_t out_sz) {
    if (!house_root[0]) resolve_house_root();
    if (!house_root[0]) return -1;

    char login_path[MAX_PATH];
    snprintf(login_path, sizeof(login_path), "%s/0.user-pal👤️/00.login-signup/current_login.txt", house_root);
    char xyzfs[MAX_PATH];
    read_kv(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));
    if (!xyzfs[0]) return -1;

    snprintf(out, out_sz, "%s/%s/home/runtime/ledger.txt", house_root, xyzfs);
    return 0;
}

static int pid_alive(const char *pid_str) {
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%s", pid_str);
    return (access(proc_path, F_OK) == 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ledger_peers <type>\n");
        return 1;
    }

    const char *filter_type = argv[1];
    char lpath[MAX_PATH];
    if (ledger_path(lpath, sizeof(lpath)) != 0) return 1;

    FILE *f = fopen(lpath, "r");
    if (!f) return 1;

    PeerEntry latest[MAX_PEERS];
    char line[MAX_LINE];

    /* Read in forward order: for each project_id, latest ONLINE wins,
     * OFFLINE removes entry, dead PID drops entry */
    int found = 0;
    f = fopen(lpath, "r");
    if (!f) return 1;

    /* Clear latest entries */
    for (int i = 0; i < MAX_PEERS; i++) {
        latest[i].project_id[0] = '\0';
    }

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;

        char *tok[8];
        int nt = 0;
        char *p = line;
        while (p && nt < 8) {
            tok[nt++] = p;
            p = strchr(p, '|');
            if (p) { *p = '\0'; p++; }
        }
        if (nt < 8) continue;

        const char *event       = tok[1];
        const char *type_val    = tok[2];
        const char *project_id  = tok[3];

        if (strcmp(type_val, filter_type) != 0) continue;

        if (strcmp(event, "OFFLINE") == 0) {
            /* Remove from latest */
            for (int i = 0; i < MAX_PEERS; i++) {
                if (strcmp(latest[i].project_id, project_id) == 0) {
                    latest[i].project_id[0] = '\0';
                    break;
                }
            }
            continue;
        }

        if (strcmp(event, "ONLINE") != 0) continue;

        /* Check aliveness */
        const char *pid_val = tok[5];
        if (!pid_alive(pid_val)) continue;

        /* Upsert: replace existing entry for this project_id */
        int replaced = 0;
        for (int i = 0; i < MAX_PEERS; i++) {
            if (strcmp(latest[i].project_id, project_id) == 0) {
                snprintf(latest[i].session_root, sizeof(latest[i].session_root), "%s", tok[4]);
                snprintf(latest[i].inbox_path, sizeof(latest[i].inbox_path), "%s", tok[7]);
                snprintf(latest[i].display_name, sizeof(latest[i].display_name), "%s", tok[6]);
                snprintf(latest[i].pid_str, sizeof(latest[i].pid_str), "%s", pid_val);
                snprintf(latest[i].timestamp, sizeof(latest[i].timestamp), "%s", tok[0]);
                replaced = 1;
                break;
            }
        }
        if (!replaced) {
            for (int i = 0; i < MAX_PEERS; i++) {
                if (!latest[i].project_id[0]) {
                    snprintf(latest[i].project_id, sizeof(latest[i].project_id), "%s", project_id);
                    snprintf(latest[i].session_root, sizeof(latest[i].session_root), "%s", tok[4]);
                    snprintf(latest[i].inbox_path, sizeof(latest[i].inbox_path), "%s", tok[7]);
                    snprintf(latest[i].display_name, sizeof(latest[i].display_name), "%s", tok[6]);
                    snprintf(latest[i].pid_str, sizeof(latest[i].pid_str), "%s", pid_val);
                    snprintf(latest[i].timestamp, sizeof(latest[i].timestamp), "%s", tok[0]);
                    break;
                }
            }
        }
    }
    fclose(f);

    /* Output active peers */
    for (int i = 0; i < MAX_PEERS; i++) {
        if (latest[i].project_id[0]) {
            printf("%s|%s|%s|%s|%s\n",
                   latest[i].session_root,
                   latest[i].inbox_path,
                   latest[i].display_name,
                   latest[i].project_id,
                   latest[i].pid_str);
            found = 1;
        }
    }

    return found ? 0 : 1;
}
