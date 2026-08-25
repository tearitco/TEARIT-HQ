#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024

static void build_pid_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/audio.pid", root && root[0] ? root : ".");
}

static void kill_previous(const char *root) {
    char pid_path[MAX_PATH_LEN];
    FILE *fp;
    pid_t pid;

    build_pid_path(pid_path, sizeof(pid_path), root);
    fp = fopen(pid_path, "r");
    if (!fp) {
        return;
    }

    if (fscanf(fp, "%d", &pid) == 1) {
        kill(pid, SIGTERM);
        usleep(100000);
        kill(pid, SIGKILL);
    }
    fclose(fp);
    remove(pid_path);
}

static void signal_process(const char *root, int sig) {
    char pid_path[MAX_PATH_LEN];
    FILE *fp;
    pid_t pid;

    build_pid_path(pid_path, sizeof(pid_path), root);
    fp = fopen(pid_path, "r");
    if (!fp) {
        return;
    }

    if (fscanf(fp, "%d", &pid) == 1) {
        kill(pid, sig);
    }
    fclose(fp);
}

int main(int argc, char **argv) {
    const char *root = ".";
    const char *target = NULL;

    if (argc < 2) {
        fprintf(stderr, "usage: wraith_audio <audio_path|--stop|--pause|--resume> [project_root]\n");
        return 1;
    }

    if (argc > 2 && argv[2][0]) {
        root = argv[2];
    }

    if (strcmp(argv[1], "--stop") == 0) {
        kill_previous(root);
        return 0;
    }
    if (strcmp(argv[1], "--pause") == 0) {
        signal_process(root, SIGSTOP);
        return 0;
    }
    if (strcmp(argv[1], "--resume") == 0) {
        signal_process(root, SIGCONT);
        return 0;
    }

    target = argv[1];
    kill_previous(root);

    {
        pid_t pid = fork();
        if (pid == 0) {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            execlp("mpg123", "mpg123", "-q", target, (char *)NULL);
            _exit(1);
        }
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        {
            char pid_path[MAX_PATH_LEN];
            FILE *fp;
            build_pid_path(pid_path, sizeof(pid_path), root);
            fp = fopen(pid_path, "w");
            if (fp) {
                fprintf(fp, "%d\n", pid);
                fclose(fp);
            }
        }
    }

    return 0;
}
