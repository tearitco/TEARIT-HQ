/* open_character_preview - fork system/character_preview <uuid>
 * Mutaclysm-style RGB manager view (orbit camera). Separate from
 * open_avatar_window (2D desktop pet).
 *
 * Tracks pieces/world_01/map_lobby/<uuid>/preview.pid for kill_all.
 * Usage: open_character_preview.+x <avatar_uuid>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
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

static long read_preview_pid(const char *uuid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/preview.pid", project_root, uuid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long pid = -1;
    if (fscanf(f, "%ld", &pid) != 1) pid = -1;
    fclose(f);
    return pid;
}

static void write_preview_pid(const char *uuid, long pid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/preview.pid", project_root, uuid);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", pid);
    fclose(f);
}

static void clear_preview_pid(const char *uuid) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/preview.pid", project_root, uuid);
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
    long old = read_preview_pid(uuid);
    if (pid_alive(old)) {
        kill((pid_t)old, SIGTERM);
        usleep(150000);
        if (pid_alive(old)) kill((pid_t)old, SIGKILL);
    }
    clear_preview_pid(uuid);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: open_character_preview.+x <avatar_uuid>\n");
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

    char bin[PATH_BUF];
    snprintf(bin, sizeof(bin), "%s/system/character_preview", project_root);
    if (stat(bin, &st) != 0) {
        printf("character_preview binary missing - run button.sh compile.\n");
        return 1;
    }

    kill_existing(uuid);

    pid_t pid = fork();
    if (pid < 0) {
        printf("Fork failed.\n");
        return 1;
    }
    if (pid == 0) {
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, 0);
            dup2(devnull, 1);
            dup2(devnull, 2);
            if (devnull > 2) close(devnull);
        }
        setenv("PRISC_PROJECT_ROOT", project_root, 1);
        execl(bin, bin, uuid, (char *)NULL);
        _exit(127);
    }

    write_preview_pid(uuid, (long)pid);
    track_pid((long)pid);
    printf("Opened RGB preview for %s (pid %d). q/e yaw r/t pitch w/s zoom f reset.\n",
           uuid, (int)pid);
    return 0;
}
