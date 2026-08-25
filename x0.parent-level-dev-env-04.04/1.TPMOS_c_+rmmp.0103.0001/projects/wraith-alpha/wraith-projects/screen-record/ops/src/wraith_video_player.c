#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024
#define FRAME_RATE 8

static int run_cmd(const char *cmd) {
    int status = system(cmd);
    if (status < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static void build_pid_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/video.pid", root && root[0] ? root : ".");
}

static void build_frames_dir(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/video_frames", root && root[0] ? root : ".");
}

static void build_current_frame_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/current_frame.png", root && root[0] ? root : ".");
}

static void build_control_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/video.control", root && root[0] ? root : ".");
}

static void build_status_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/video.playback", root && root[0] ? root : ".");
}

static void build_marker_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/fs_watch.marker", root && root[0] ? root : ".");
}

static void build_source_stamp_path(char *out, size_t out_sz, const char *root) {
    snprintf(out, out_sz, "%s/session/video.source.txt", root && root[0] ? root : ".");
}

static void clear_cached_frames(const char *root) {
    char frames_dir[MAX_PATH_LEN];
    char current_frame[MAX_PATH_LEN];
    char poster[MAX_PATH_LEN];
    char playback[MAX_PATH_LEN];
    char source_stamp[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 4];

    build_frames_dir(frames_dir, sizeof(frames_dir), root);
    build_current_frame_path(current_frame, sizeof(current_frame), root);
    snprintf(poster, sizeof(poster), "%s/session/poster.png", root && root[0] ? root : ".");
    build_status_path(playback, sizeof(playback), root);
    build_source_stamp_path(source_stamp, sizeof(source_stamp), root);

    snprintf(cmd, sizeof(cmd), "rm -f '%s'/frame_*.png '%s' '%s' '%s' '%s'",
        frames_dir, current_frame, poster, playback, source_stamp);
    run_cmd(cmd);
}

static int source_changed(const char *root, const char *video_path) {
    char stamp_path[MAX_PATH_LEN];
    char current[256];
    char saved[256];
    struct stat st;
    FILE *f;

    build_source_stamp_path(stamp_path, sizeof(stamp_path), root);
    if (stat(video_path, &st) != 0) {
        return 0;
    }

    snprintf(current, sizeof(current), "%s|%ld|%ld", video_path, (long) st.st_mtime, (long) st.st_size);
    f = fopen(stamp_path, "r");
    if (!f) {
        return 1;
    }
    if (!fgets(saved, sizeof(saved), f)) {
        fclose(f);
        return 1;
    }
    fclose(f);
    saved[strcspn(saved, "\r\n")] = '\0';
    return strcmp(current, saved) != 0;
}

static void write_source_stamp(const char *root, const char *video_path) {
    char stamp_path[MAX_PATH_LEN];
    struct stat st;
    FILE *f;

    if (stat(video_path, &st) != 0) {
        return;
    }
    build_source_stamp_path(stamp_path, sizeof(stamp_path), root);
    f = fopen(stamp_path, "w");
    if (!f) {
        return;
    }
    fprintf(f, "%s|%ld|%ld\n", video_path, (long) st.st_mtime, (long) st.st_size);
    fclose(f);
}

static void ensure_frames(const char *root, const char *video_path) {
    char frames_dir[MAX_PATH_LEN];
    char frame_glob[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 4];
    struct stat st;

    build_frames_dir(frames_dir, sizeof(frames_dir), root);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", frames_dir);
    run_cmd(cmd);

    if (source_changed(root, video_path)) {
        clear_cached_frames(root);
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", frames_dir);
        run_cmd(cmd);
    }

    snprintf(frame_glob, sizeof(frame_glob), "%s/frame_0001.png", frames_dir);
    if (stat(frame_glob, &st) == 0) {
        return;
    }

    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -i '%s' -vf fps=%d,scale=320:240 '%s/frame_%%04d.png' >/dev/null 2>&1",
        video_path,
        FRAME_RATE,
        frames_dir);
    if (run_cmd(cmd) == 0) {
        write_source_stamp(root, video_path);
    }
}

static int count_frames(const char *root) {
    char frames_dir[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    int index = 1;
    struct stat st;

    build_frames_dir(frames_dir, sizeof(frames_dir), root);
    for (;;) {
        snprintf(path, sizeof(path), "%s/frame_%04d.png", frames_dir, index);
        if (stat(path, &st) != 0) {
            break;
        }
        index++;
    }
    return index - 1;
}

static void write_status(const char *root, const char *state, int frame_idx, int frame_total) {
    char path[MAX_PATH_LEN];
    char marker_path[MAX_PATH_LEN];
    FILE *f;
    FILE *marker;

    build_status_path(path, sizeof(path), root);
    f = fopen(path, "w");
    if (!f) {
        return;
    }
    fprintf(f, "state=%s\n", state);
    fprintf(f, "frame_index=%d\n", frame_idx);
    fprintf(f, "frame_total=%d\n", frame_total);
    fclose(f);

    build_marker_path(marker_path, sizeof(marker_path), root);
    marker = fopen(marker_path, "a");
    if (!marker) {
        return;
    }
    fprintf(marker, "video state=%s frame=%d/%d\n", state, frame_idx, frame_total);
    fclose(marker);
}

static int read_control(const char *root, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN];
    FILE *f;
    char line[64];

    out[0] = '\0';
    build_control_path(path, sizeof(path), root);
    f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    snprintf(out, out_sz, "%s", line);
    return 1;
}

static void write_control(const char *root, const char *value) {
    char path[MAX_PATH_LEN];
    FILE *f;

    build_control_path(path, sizeof(path), root);
    f = fopen(path, "w");
    if (!f) {
        return;
    }
    fprintf(f, "%s\n", value);
    fclose(f);
}

static void copy_frame(const char *root, int frame_idx) {
    char frames_dir[MAX_PATH_LEN];
    char src[MAX_PATH_LEN];
    char dst[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 3];

    build_frames_dir(frames_dir, sizeof(frames_dir), root);
    build_current_frame_path(dst, sizeof(dst), root);
    snprintf(src, sizeof(src), "%s/frame_%04d.png", frames_dir, frame_idx);
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", src, dst);
    run_cmd(cmd);
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
        usleep(150000);
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
        fprintf(stderr, "usage: wraith_video_player <video_path|--stop|--pause|--resume> [project_root]\n");
        return 1;
    }

    if (argc > 2 && argv[2][0]) {
        root = argv[2];
    }

    if (strcmp(argv[1], "--stop") == 0) {
        write_control(root, "stop");
        kill_previous(root);
        write_status(root, "stopped", 0, 0);
        return 0;
    }
    if (strcmp(argv[1], "--pause") == 0) {
        write_control(root, "pause");
        signal_process(root, SIGSTOP);
        write_status(root, "paused", 0, 0);
        return 0;
    }
    if (strcmp(argv[1], "--resume") == 0) {
        write_control(root, "play");
        signal_process(root, SIGCONT);
        return 0;
    }

    target = argv[1];
    kill_previous(root);
    ensure_frames(root, target);
    write_control(root, "play");

    {
        pid_t pid = fork();
        if (pid == 0) {
            int frame_total = count_frames(root);
            int frame_idx = 1;
            char control[64];

            if (frame_total <= 0) {
                _exit(1);
            }

            while (frame_idx <= frame_total) {
                copy_frame(root, frame_idx);
                write_status(root, "playing", frame_idx, frame_total);
                if (read_control(root, control, sizeof(control)) && strcmp(control, "stop") == 0) {
                    write_status(root, "stopped", frame_idx, frame_total);
                    _exit(0);
                }
                usleep(1000000 / FRAME_RATE);
                frame_idx++;
            }

            copy_frame(root, frame_total);
            write_status(root, "stopped", frame_total, frame_total);
            _exit(0);
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
