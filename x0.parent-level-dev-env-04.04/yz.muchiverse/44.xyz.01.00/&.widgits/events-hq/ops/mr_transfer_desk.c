/* mr_transfer_desk.c - real "transfer player" Common Event op.
 *
 * Direct instruction, resolved architecture decision (2026-09-14): this
 * gets its OWN small, self-contained "switch desk" implementation -
 * duplicating khtpm_taskbar_manager.c's own real livedesk_close_all() /
 * livedesk_write_active_desk() / livedesk_spawn_desk() mechanics
 * directly, matching this house's standing "duplicate rather than share
 * a header" convention - rather than any manager-polling/request-file
 * scheme. It's just a normal op, reached via the already-proven
 * play_event.sh -> prisc+x -> op path (same shape as castle's own
 * mr_show_text/cmd_1.sh pattern). Flagged for later consolidation into
 * the real, unified livedesk_switch_desk() once desk-switching is more
 * centrally event-driven - see CIV-TEST-DESK-AND-DOOR-TRANSFER-PLAN.md.
 *
 * Usage: mr_transfer_desk.+x <house_root> <session_id> <target_desk>
 *
 * Real mechanics (matching livedesk_switch_desk()'s own three real
 * steps, snapshot step intentionally omitted - a transfer-triggered
 * switch does not need to re-save the outgoing desk's live entity
 * positions, it's a teleport not a save):
 *   1. close_all  - real CLOSE relay to every live entity on the CURRENT
 *                    active desk (cursword exempt, same as the real fn).
 *   2. write_active_desk - STATE | active_desk | <target_desk> into the
 *                    session's own session.pdl (same real shape).
 *   3. spawn_desk  - read the target desk's own DESK rows and launch each
 *                    entity's pal via the real khtpm_core_render.+x
 *                    single-arg invocation (same real shape), skipping
 *                    cursword (it's always-open, outside the desk model)
 *                    and any pal already live (real /proc ground-truth
 *                    check, not a registry snapshot).
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

/* REAL FIX 2026-09-14, direct live report ("i just ran kill all bad
 * under proc mon and it killed castle and cursword. why did it think
 * those were bad? dont we have an entity/session id?") - yes, the house
 * DOES have one: <house_root>/#.desktop/livedesk_proc_list.txt (see
 * kh_proc_registry.h), and mon_scan.sh's "stray/bad" classification is
 * exactly "matches a house process pattern AND is unregistered in that
 * ledger AND orphaned/detached from the taskbar's own process-group."
 * This op's own spawn_desk() (below) launched every desk entity with a
 * raw `setsid nohup ... &` - a real, genuine gap, not a testing
 * artifact this time: unlike khtpm_taskbar_manager.c's own spawn path
 * (ktb_system_recorded(), which wraps every launch in
 * kh_proc_register_owned()), this op never registered anything it
 * spawned. A door-triggered transfer's own castle respawn was
 * therefore invisible to the ledger and looked exactly like a leaked
 * stray process to proc-mon - correctly killed by its own rules, given
 * what it could see. Fixed: use the house's own blessed spawn+register
 * helper (kh_spawn.h) instead of a raw shell background job, with this
 * op's own pid as master_pid (same "who reaps this on teardown" role
 * ktb_system_recorded() gives the manager for its own tb-launch rows -
 * a one-shot op doesn't stay alive to reap anything itself, but the
 * registry line still records real ownership/start-time for mon_scan's
 * PID-reuse-safe classification, which is the actual bug here). */
#define KH_PROC_REGISTRY_IMPL
#include "../../_shared-lib/kh_proc_registry.h"
#define KH_SPAWN_IMPL
#include "../../_shared-lib/kh_spawn.h"

#define PB 4096

static void read_pdl_kv(const char *path, const char *key, char *out, size_t sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, key);
        if (!p) continue;
        char *after = p + klen;
        while (*after == ' ') after++;
        if (*after != '|' && *after != '=') continue;
        after++;
        while (*after == ' ') after++;
        char *end = after + strlen(after);
        while (end > after && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) end--;
        size_t n = (size_t)(end - after);
        if (n >= sz) n = sz - 1;
        memcpy(out, after, n);
        out[n] = '\0';
        break;
    }
    fclose(f);
}

/* real user-uuid resolution, ported from tp_place_desktop.c's own
 * resolve_user_uuid() - reads the real current_login.txt, not a
 * hardcoded uuid (this house may have more than one 0.user-pal dir). */
static int resolve_user_uuid(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    DIR *d = opendir(house_root);
    if (!d) return 0;
    struct dirent *e;
    char login_root[PB] = "";
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "0.user-pal", 10) == 0) {
            snprintf(login_root, sizeof(login_root), "%s/%s/00.login-signup", house_root, e->d_name);
            break;
        }
    }
    closedir(d);
    if (!login_root[0]) return 0;
    char p[PB];
    snprintf(p, sizeof(p), "%s/current_login.txt", login_root);
    read_pdl_kv(p, "current_user_uuid", out, sz);
    return out[0] != '\0';
}

/* real basename-of-pal-path helper, same convention as livedesk_base_name() */
static void base_name(const char *path, char *out, size_t sz) {
    const char *slash = strrchr(path, '/');
    snprintf(out, sz, "%s", slash ? slash + 1 : path);
    char *e = out + strlen(out);
    while (e > out && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' ')) *--e = '\0';
}

/* real /proc-cmdline ground-truth scan, same technique
 * ktb_find_live_pid_for_pal() already uses (no registry-staleness
 * window - see that function's own header comment for the full story). */
static int find_live_pid_for_pal(const char *pal_dir) {
    DIR *d = opendir("/proc");
    if (!d) return -1;
    struct dirent *e;
    int found = -1;
    while ((e = readdir(d))) {
        int pid = atoi(e->d_name);
        if (pid <= 0) continue;
        char cmdpath[PB];
        snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", pid);
        FILE *f = fopen(cmdpath, "r");
        if (!f) continue;
        char buf[PB * 2];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = '\0';
        /* argv entries are NUL-separated within buf */
        int matched = 0;
        for (size_t i = 0; i < n; ) {
            if (strcmp(buf + i, pal_dir) == 0) { matched = 1; break; }
            i += strlen(buf + i) + 1;
        }
        if (matched) { found = pid; break; }
    }
    closedir(d);
    return found;
}

static void close_all_on_desk(const char *house_root, const char *desk_pdl) {
    FILE *f = fopen(desk_pdl, "r");
    if (!f) return;
    char line[PB];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        strtok(p, "|");                 /* name (unused) */
        char *path = strtok(NULL, "|"); /* rel pal path */
        if (!path) continue;
        while (*path == ' ') path++;
        char *pe = path + strlen(path);
        while (pe > path && (pe[-1] == ' ' || pe[-1] == '\r' || pe[-1] == '\n')) *--pe = '\0';

        char base[64];
        base_name(path, base, sizeof(base));
        if (strcmp(base, "cursword") == 0) continue; /* always-open, exempt - matches livedesk_close_all() */

        char full[PB];
        snprintf(full, sizeof(full), "%s/%s", house_root, path);
        char relay[PB];
        snprintf(relay, sizeof(relay), "%s/interact_relay.txt", full);
        FILE *rf = fopen(relay, "w");
        if (rf) { fprintf(rf, "CLOSE\n"); fclose(rf); }
    }
    fclose(f);
    struct timespec ts = {0, 450 * 1000 * 1000};
    nanosleep(&ts, NULL);
}

static void write_active_desk(const char *sess_dir, const char *target_desk) {
    char sp[PB];
    snprintf(sp, sizeof(sp), "%s/session.pdl", sess_dir);
    char name[256] = "";
    read_pdl_kv(sp, "name", name, sizeof(name));
    FILE *f = fopen(sp, "w");
    if (!f) return;
    if (name[0]) fprintf(f, "STATE | name | %s\n", name);
    fprintf(f, "STATE | active_desk | %s\n", target_desk);
    fclose(f);
}

static void spawn_desk(const char *house_root, const char *desk_pdl) {
    char exe[PB];
    snprintf(exe, sizeof(exe), "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_entity.+x", house_root);
    if (access(exe, F_OK) != 0) return;

    FILE *f = fopen(desk_pdl, "r");
    if (!f) return;
    char spawned[64][64];
    int n_spawned = 0;
    char line[PB];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        strtok(p, "|");                 /* name (unused) */
        char *path = strtok(NULL, "|");
        char *xs = strtok(NULL, "|");
        char *ys = strtok(NULL, "|");
        if (!path) continue;
        while (*path == ' ') path++;
        char *pe = path + strlen(path);
        while (pe > path && (pe[-1] == ' ' || pe[-1] == '\r' || pe[-1] == '\n')) *--pe = '\0';

        char base[64];
        base_name(path, base, sizeof(base));
        if (strcmp(base, "cursword") == 0) continue; /* always-open, outside the desk model */

        int dup = 0;
        for (int i = 0; i < n_spawned; i++) if (strcmp(spawned[i], base) == 0) { dup = 1; break; }
        if (dup) continue;
        if (n_spawned < 64) snprintf(spawned[n_spawned++], sizeof(spawned[0]), "%s", base);

        char full[PB];
        snprintf(full, sizeof(full), "%s/%s", house_root, path);
        if (access(full, F_OK) != 0) continue;

        if (find_live_pid_for_pal(full) > 0) continue; /* already live - real ground truth, never double-spawn */

        {
            char relay[PB];
            snprintf(relay, sizeof(relay), "%s/interact_relay.txt", full);
            FILE *rf = fopen(relay, "w");
            if (rf) fclose(rf);
        }
        int x = xs ? atoi(xs) : -1, y = ys ? atoi(ys) : -1;
        if (x >= 0 && y >= 0) {
            char posp[PB];
            snprintf(posp, sizeof(posp), "%s/desktop_pos.txt", full);
            FILE *pw = fopen(posp, "w");
            if (pw) { fprintf(pw, "x=%d\ny=%d\n", x, y); fclose(pw); }
        }
        char *argv2[3];
        argv2[0] = (char *)exe;
        argv2[1] = full;
        argv2[2] = NULL;
        kh_spawn(house_root, (long)getpid(), base, argv2,
                 KH_SPAWN_SETSID | KH_SPAWN_QUIET);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: mr_transfer_desk.+x <house_root> <session_id> <target_desk>\n");
        return 1;
    }
    const char *house_root = argv[1];
    const char *session_id = argv[2];
    const char *target_desk = argv[3];

    char uuid[128];
    if (!resolve_user_uuid(house_root, uuid, sizeof(uuid))) {
        fprintf(stderr, "mr_transfer_desk: could not resolve user uuid\n");
        return 1;
    }
    char sroot[PB];
    snprintf(sroot, sizeof(sroot), "%s/xyzfs/users/%s/home/livedesk/sessions", house_root, uuid);
    char sess_dir[PB];
    snprintf(sess_dir, sizeof(sess_dir), "%s/%s", sroot, session_id);

    char cur_active[64] = "";
    {
        char sp[PB];
        snprintf(sp, sizeof(sp), "%s/session.pdl", sess_dir);
        read_pdl_kv(sp, "active_desk", cur_active, sizeof(cur_active));
    }
    if (cur_active[0] && strcmp(cur_active, target_desk) == 0) return 0; /* already there - no-op */

    if (cur_active[0]) {
        char cur_desk_pdl[PB];
        snprintf(cur_desk_pdl, sizeof(cur_desk_pdl), "%s/desks/%s.pdl", sess_dir, cur_active);
        close_all_on_desk(house_root, cur_desk_pdl);
    }

    write_active_desk(sess_dir, target_desk);

    char target_desk_pdl[PB];
    snprintf(target_desk_pdl, sizeof(target_desk_pdl), "%s/desks/%s.pdl", sess_dir, target_desk);
    spawn_desk(house_root, target_desk_pdl);

    return 0;
}
