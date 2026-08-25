#include <signal.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../../../libraries/stb_image_write.h"

#include "../../../../../../pieces/chtpm/ops/lib/tpmos_share_kvp_runtime.c"
#include "../../../../../../pieces/chtpm/ops/lib/tpmos_live_frame_cache.c"

#define MAX_PATH_LEN 1024
#define WEBCAM_FRAME_CACHE_KEY "projects/wraith-alpha/wraith-projects/web-cam/session/current_frame"
#define WEBCAM_SNAPSHOT_INTERVAL_MS 500

static volatile sig_atomic_t g_stop = 0;
static pid_t g_child_pid = -1;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static int run_cmd(const char *cmd) {
    int status = system(cmd);
    if (status < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static void shell_quote(char *out, size_t out_sz, const char *in) {
    size_t oi = 0;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!in) return;
    while (*in && oi + 1 < out_sz) {
        if (*in == '\'') {
            if (oi + 4 >= out_sz) break;
            out[oi++] = '\'';
            out[oi++] = '\\';
            out[oi++] = '\'';
            out[oi++] = '\'';
        } else {
            out[oi++] = *in;
        }
        in++;
    }
    out[oi] = '\0';
}

static pid_t read_pid_file(const char *path) {
    FILE *f = fopen(path, "r");
    pid_t pid = -1;
    if (!f) return -1;
    if (fscanf(f, "%d", &pid) != 1) {
        pid = -1;
    }
    fclose(f);
    return pid;
}

static void resolve_device(char *out, size_t out_sz, const char *requested) {
    struct stat st;
    int i;
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "%s", requested && requested[0] ? requested : "/dev/video0");
    if (stat(out, &st) == 0) {
        return;
    }
    for (i = 0; i < 10; i++) {
        snprintf(out, out_sz, "/dev/video%d", i);
        if (stat(out, &st) == 0) {
            return;
        }
    }
    snprintf(out, out_sz, "%s", requested && requested[0] ? requested : "/dev/video0");
}

static void resolve_profile(char *out, size_t out_sz, const char *requested) {
    if (!out || out_sz == 0) return;
    if (requested && strcmp(requested, "debug") == 0) {
        snprintf(out, out_sz, "debug");
        return;
    }
    snprintf(out, out_sz, "fast");
}

static void path_join(char *out, size_t out_sz, const char *root, const char *rel) {
    snprintf(out, out_sz, "%s/%s", root && root[0] ? root : ".", rel);
}

static void write_status(const char *root, const char *state, const char *detail, long frame_epoch) {
    char payload[2048];
    snprintf(payload, sizeof(payload),
        "state=%s\n"
        "detail=%s\n"
        "frame_epoch=%ld\n",
        state ? state : "unknown",
        detail ? detail : "",
        frame_epoch);
    tpmos_share_kvp_write_text(root, "session/webcam.status", payload);
}

static void pulse_marker(const char *root, const char *msg) {
    char line[256];
    snprintf(line, sizeof(line), "webcam %s %ld", msg ? msg : "tick", (long)time(NULL));
    tpmos_share_kvp_append_text(root, "session/fs_watch.marker", line);
}

static void kill_pid_file(const char *path) {
    pid_t pid = read_pid_file(path);
    if (pid > 1) {
        kill(pid, SIGTERM);
        usleep(300000);
        if (kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
        }
    }
    remove(path);
}

static void kill_residual_ffmpeg(const char *root) {
    char current_frame[MAX_PATH_LEN];
    char quoted_frame[MAX_PATH_LEN * 2];
    char cmd[MAX_PATH_LEN * 4];

    path_join(current_frame, sizeof(current_frame), root, "session/current_frame.png");
    shell_quote(quoted_frame, sizeof(quoted_frame), current_frame);

    snprintf(cmd, sizeof(cmd),
        "pkill -TERM -f -- '%s' >/dev/null 2>&1; "
        "sleep 1; "
        "pkill -KILL -f -- '%s' >/dev/null 2>&1",
        quoted_frame, quoted_frame);
    run_cmd(cmd);
}

static void kill_previous(const char *root) {
    char daemon_pid[MAX_PATH_LEN];
    char ffmpeg_pid[MAX_PATH_LEN];
    path_join(daemon_pid, sizeof(daemon_pid), root, "session/webcam.pid");
    path_join(ffmpeg_pid, sizeof(ffmpeg_pid), root, "session/webcam.ffmpeg.pid");
    kill_pid_file(ffmpeg_pid);
    kill_pid_file(daemon_pid);
    kill_residual_ffmpeg(root);
}

static void ensure_dirs(const char *root) {
    char cmd[MAX_PATH_LEN * 2];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/session'", root);
    run_cmd(cmd);
}

static void cleanup_child(void) {
    if (g_child_pid > 1) {
        kill(g_child_pid, SIGTERM);
        usleep(150000);
        kill(g_child_pid, SIGKILL);
        waitpid(g_child_pid, NULL, 0);
        g_child_pid = -1;
    }
}

static int read_full_frame(int fd, unsigned char *buf, size_t need) {
    size_t got = 0;
    while (got < need) {
        ssize_t n = read(fd, buf + got, need - got);
        if (n == 0) return 0;
        if (n < 0) return -1;
        got += (size_t)n;
    }
    return 1;
}

static void resolve_profile_dims(const char *profile, int *out_w, int *out_h, int *in_fps, int *out_fps) {
    if (!out_w || !out_h || !in_fps || !out_fps) return;
    if (profile && strcmp(profile, "debug") == 0) {
        *out_w = 320;
        *out_h = 240;
        *in_fps = 2;
        *out_fps = 1;
        return;
    }
    *out_w = 240;
    *out_h = 180;
    *in_fps = 15;
    *out_fps = 10;
}

static unsigned long webcam_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

static int daemon_main(const char *root, const char *device, const char *profile) {
    char current_frame[MAX_PATH_LEN];
    char daemon_pid_path[MAX_PATH_LEN];
    char ffmpeg_pid_path[MAX_PATH_LEN];
    char ffmpeg_log[MAX_PATH_LEN];
    char resolved_device[64];
    char resolved_profile[16];
    char filter_complex[256];
    struct stat st;
    FILE *pidf;
    int raw_pipe[2];
    int out_w = 0;
    int out_h = 0;
    int in_fps = 0;
    int out_fps = 0;
    unsigned char *frame_buf = NULL;
    size_t frame_bytes = 0;
    unsigned long last_snapshot_ms = 0;
    unsigned long last_frame_ms = 0;

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);

    resolve_device(resolved_device, sizeof(resolved_device), device);
    resolve_profile(resolved_profile, sizeof(resolved_profile), profile);
    resolve_profile_dims(resolved_profile, &out_w, &out_h, &in_fps, &out_fps);
    frame_bytes = (size_t)out_w * (size_t)out_h * 4;
    frame_buf = (unsigned char *)malloc(frame_bytes);
    if (!frame_buf) {
        write_status(root, "error", "frame_buffer_alloc_failed", 0);
        return 1;
    }

    if (stat(resolved_device, &st) != 0) {
        write_status(root, "missing_device", resolved_device, 0);
        pulse_marker(root, "missing_device");
        free(frame_buf);
        return 1;
    }

    path_join(current_frame, sizeof(current_frame), root, "session/current_frame.png");
    path_join(daemon_pid_path, sizeof(daemon_pid_path), root, "session/webcam.pid");
    path_join(ffmpeg_pid_path, sizeof(ffmpeg_pid_path), root, "session/webcam.ffmpeg.pid");
    path_join(ffmpeg_log, sizeof(ffmpeg_log), root, "session/webcam.ffmpeg.log");
    remove(ffmpeg_log);
    pidf = fopen(daemon_pid_path, "w");
    if (pidf) {
        fprintf(pidf, "%d\n", getpid());
        fclose(pidf);
    }

    if (pipe(raw_pipe) != 0) {
        write_status(root, "error", "raw_pipe_failed", 0);
        free(frame_buf);
        remove(daemon_pid_path);
        return 1;
    }

    g_child_pid = fork();
    if (g_child_pid == 0) {
        close(raw_pipe[0]);
        dup2(raw_pipe[1], STDOUT_FILENO);
        close(raw_pipe[1]);
        freopen(ffmpeg_log, "a", stderr);
        execlp("ffmpeg", "ffmpeg",
            "-loglevel", "error",
            "-nostdin",
            "-y",
            "-f", "video4linux2",
            "-framerate", resolved_profile[0] && strcmp(resolved_profile, "debug") == 0 ? "2" : "15",
            "-video_size", "320x240",
            "-i", resolved_device,
            "-vf", resolved_profile[0] && strcmp(resolved_profile, "debug") == 0 ? "fps=1,scale=320:240,format=rgba" : "fps=10,scale=240:180,format=rgba",
            "-f", "rawvideo",
            "-pix_fmt", "rgba",
            "pipe:1",
            (char *)NULL);
        _exit(127);
    }
    if (g_child_pid < 0) {
        write_status(root, "error", "ffmpeg_fork_failed", 0);
        remove(daemon_pid_path);
        close(raw_pipe[0]);
        close(raw_pipe[1]);
        free(frame_buf);
        return 1;
    }
    close(raw_pipe[1]);
    pidf = fopen(ffmpeg_pid_path, "w");
    if (pidf) {
        fprintf(pidf, "%d\n", g_child_pid);
        fclose(pidf);
    }

    write_status(root, "streaming", resolved_profile, 0);
    pulse_marker(root, "started");

    while (!g_stop) {
        pid_t child_state = waitpid(g_child_pid, NULL, WNOHANG);
        fd_set rfds;
        struct timeval tv;
        if (child_state == g_child_pid) {
            g_child_pid = -1;
            write_status(root, "stopped", "ffmpeg_exited", 0);
            break;
        }
        FD_ZERO(&rfds);
        FD_SET(raw_pipe[0], &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        if (select(raw_pipe[0] + 1, &rfds, NULL, NULL, &tv) > 0 && FD_ISSET(raw_pipe[0], &rfds)) {
            int read_rc = read_full_frame(raw_pipe[0], frame_buf, frame_bytes);
            if (read_rc == 1) {
                unsigned long now_ms = webcam_now_ms();
                tpmos_live_frame_cache_write_rgba(WEBCAM_FRAME_CACHE_KEY, frame_buf, frame_bytes, out_w, out_h, 4);
                last_frame_ms = now_ms;
                write_status(root, "streaming", resolved_profile, (long)now_ms);
                pulse_marker(root, "frame");
                if (last_snapshot_ms == 0 || now_ms - last_snapshot_ms >= WEBCAM_SNAPSHOT_INTERVAL_MS) {
                    stbi_write_png(current_frame, out_w, out_h, 4, frame_buf, out_w * 4);
                    last_snapshot_ms = now_ms;
                }
            } else if (read_rc <= 0) {
                write_status(root, "stopped", "raw_stream_closed", 0);
                break;
            }
        }
    }

    cleanup_child();
    close(raw_pipe[0]);
    tpmos_live_frame_cache_clear(WEBCAM_FRAME_CACHE_KEY);
    free(frame_buf);
    write_status(root, "stopped", resolved_profile, (long)last_frame_ms);
    pulse_marker(root, "stopped");
    remove(daemon_pid_path);
    remove(ffmpeg_pid_path);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode;
    const char *root;
    const char *device = "/dev/video0";
    const char *profile = "fast";

    if (argc < 3) {
        fprintf(stderr, "usage: wraith_webcam_capture <--start|--stop> <project_root> [device]\n");
        return 1;
    }
    mode = argv[1];
    root = argv[2];
    if (argc > 3 && argv[3][0]) {
        device = argv[3];
    }
    if (argc > 4 && argv[4][0]) {
        profile = argv[4];
    }

    ensure_dirs(root);

    if (strcmp(mode, "--stop") == 0) {
        kill_previous(root);
        tpmos_live_frame_cache_clear(WEBCAM_FRAME_CACHE_KEY);
        write_status(root, "stopped", "stop_requested", 0);
        pulse_marker(root, "stop_requested");
        return 0;
    }
    if (strcmp(mode, "--start") != 0) {
        return 1;
    }

    kill_previous(root);

    {
        pid_t pid = fork();
        if (pid == 0) {
            if (setsid() < 0) {
                _exit(1);
            }
            freopen("/dev/null", "r", stdin);
            freopen("/dev/null", "a", stdout);
            freopen("/dev/null", "a", stderr);
            _exit(daemon_main(root, device, profile));
        }
        if (pid < 0) {
            write_status(root, "error", profile, 0);
            return 1;
        }
    }

    write_status(root, "starting", profile, 0);
    pulse_marker(root, "starting");
    return 0;
}
