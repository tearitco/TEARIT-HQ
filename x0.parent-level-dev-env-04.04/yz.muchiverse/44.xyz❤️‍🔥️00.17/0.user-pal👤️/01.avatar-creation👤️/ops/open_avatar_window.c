/* open_avatar_window - fork system/avatar_window <uuid>.
 * Writes window.pid + appends to pieces/system/avatar_window_pids.txt
 * so kill_all.sh / button.sh quit can reap desktop windows (they keep
 * running at ~60fps if orphaned).
 *
 * Usage: open_avatar_window.+x <avatar_uuid> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int pid_alive(long pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

static long read_window_pid(const char *uuid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/window.pid", project_root, uuid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long pid = -1;
    if (fscanf(f, "%ld", &pid) != 1) pid = -1;
    fclose(f);
    return pid;
}

static void write_window_pid(const char *uuid, long pid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/window.pid", project_root, uuid);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", pid);
    fclose(f);
}

static void clear_window_pid(const char *uuid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/window.pid", project_root, uuid);
    unlink(path);
}

static void track_pid(long pid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/avatar_window_pids.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%ld\n", pid);
    fclose(f);
}

static void kill_existing(const char *uuid) {
    long old = read_window_pid(uuid);
    if (pid_alive(old)) {
        kill((pid_t)old, SIGTERM);
        usleep(150000);
        if (pid_alive(old)) kill((pid_t)old, SIGKILL);
    }
    clear_window_pid(uuid);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: open_avatar_window.+x <avatar_uuid>\n");
        return 1;
    }
    resolve_root();
    const char *uuid = argv[1];

    char state[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    struct stat st;
    if (stat(state, &st) != 0) {
        printf("No local avatar piece for %s.\n", uuid);
        return 1;
    }

    char win[PATH_BUF];
    snprintf(win, sizeof(win), "%s/system/avatar_window", project_root);
    if (stat(win, &st) != 0) {
        printf("avatar_window binary missing - run button.sh compile.\n");
        return 1;
    }

    /* One window per clone - replace previous if still spinning. */
    kill_existing(uuid);

    pid_t pid = fork();
    if (pid < 0) {
        printf("Fork failed.\n");
        return 1;
    }
    if (pid == 0) {
        /* Detach from parent session so UI can die without double-free
         * races, but we still track pid for kill_all on quit.
         * MUST close/reopen stdio: if parent was launched under a pipe
         * (harness OUT=$(open_avatar_window ...)), an inherited open
         * pipe keeps command-substitution hanging forever. */
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, 0);
            dup2(devnull, 1);
            dup2(devnull, 2);
            if (devnull > 2) close(devnull);
        }
        setenv("PRISC_PROJECT_ROOT", project_root, 1);
        execl(win, win, uuid, (char *)NULL);
        _exit(127);
    }

    write_window_pid(uuid, (long)pid);
    track_pid((long)pid);

    /* Update active_avatar_* only — never wipe identity (session.pdl first). */
    {
        const char *login_env = getenv("USERPAL_LOGIN_ROOT");
        char login_root[MAX_PATH];
        if (login_env && login_env[0]) snprintf(login_root, sizeof(login_root), "%s", login_env);
        else {
            char cand[PATH_BUF], real[MAX_PATH];
            snprintf(cand, sizeof(cand), "%s/../00.login-signup", project_root);
            if (!realpath(cand, real)) {
                snprintf(cand, sizeof(cand), "%s/../../../00.login-signup", project_root);
                if (!realpath(cand, real)) snprintf(real, sizeof(real), "%s", project_root);
            }
            snprintf(login_root, sizeof(login_root), "%s", real);
        }
        char sess[PATH_BUF];
        snprintf(sess, sizeof(sess), "%s/xyzfs/session.pdl", login_root);
        char uid[128] = "", uuuid[128] = "", xyz[512] = "", dname[128] = "Guest", mode[32] = "guest";
        /* Prefer session.pdl (authoritative) */
        FILE *sf_in = fopen(sess, "r");
        if (sf_in) {
            char line[512];
            while (fgets(line, sizeof(line), sf_in)) {
                if (strncmp(line, "STATE", 5) != 0) continue;
                char *p = strrchr(line, '|');
                if (!p) continue;
                p++;
                while (*p == ' ' || *p == '\t') p++;
                p[strcspn(p, "\r\n")] = '\0';
                size_t n = strlen(p);
                while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) p[--n] = '\0';
                if (strstr(line, "mode") && !strstr(line, "active")) snprintf(mode, sizeof(mode), "%s", p);
                else if (strstr(line, "user_id")) snprintf(uid, sizeof(uid), "%s", p);
                else if (strstr(line, "user_uuid")) snprintf(uuuid, sizeof(uuuid), "%s", p);
                else if (strstr(line, "display_name")) snprintf(dname, sizeof(dname), "%s", p);
                else if (strstr(line, "xyzfs_path")) snprintf(xyz, sizeof(xyz), "%s", p);
            }
            fclose(sf_in);
        }
        if (!uuuid[0] || !xyz[0]) {
            char cpath[PATH_BUF];
            snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
            FILE *cf = fopen(cpath, "r");
            if (cf) {
                char line[512];
                while (fgets(line, sizeof(line), cf)) {
                    if (!strncmp(line, "current_user_id=", 16)) {
                        snprintf(uid, sizeof(uid), "%s", line + 16);
                        uid[strcspn(uid, "\r\n")] = '\0';
                    } else if (!strncmp(line, "current_user_uuid=", 18)) {
                        snprintf(uuuid, sizeof(uuuid), "%s", line + 18);
                        uuuid[strcspn(uuuid, "\r\n")] = '\0';
                    } else if (!strncmp(line, "current_xyzfs=", 14)) {
                        snprintf(xyz, sizeof(xyz), "%s", line + 14);
                        xyz[strcspn(xyz, "\r\n")] = '\0';
                    }
                }
                fclose(cf);
            }
        }
        if (!dname[0]) snprintf(dname, sizeof(dname), "%s", uid[0] ? uid : "Guest");
        char av_path[PATH_BUF];
        if (xyz[0])
            snprintf(av_path, sizeof(av_path), "%s/home/avatars/%s", xyz, uuid);
        else
            snprintf(av_path, sizeof(av_path), "pieces/world_01/map_lobby/%s", uuid);
        FILE *sf = fopen(sess, "w");
        if (sf) {
            fprintf(sf, "SECTION      | KEY                | VALUE\n");
            fprintf(sf, "----------------------------------------\n");
            fprintf(sf, "META         | piece_id           | xyzfs_session\n");
            fprintf(sf, "META         | version            | 1.0\n\n");
            fprintf(sf, "STATE        | mode                 | %s\n", mode[0] ? mode : "guest");
            fprintf(sf, "STATE        | user_id              | %s\n", uid);
            fprintf(sf, "STATE        | user_uuid            | %s\n", uuuid);
            fprintf(sf, "STATE        | display_name         | %s\n", dname);
            fprintf(sf, "STATE        | xyzfs_path           | %s\n", xyz);
            fprintf(sf, "STATE        | logged_in_at         | %ld\n", (long)time(NULL));
            fprintf(sf, "STATE        | active_avatar_uuid   | %s\n", uuid);
            fprintf(sf, "STATE        | active_avatar_path   | %s\n", av_path);
            fclose(sf);
        }
    }

    printf("Opened chara window for %s (pid %d).\n", uuid, (int)pid);
    return 0;
}
