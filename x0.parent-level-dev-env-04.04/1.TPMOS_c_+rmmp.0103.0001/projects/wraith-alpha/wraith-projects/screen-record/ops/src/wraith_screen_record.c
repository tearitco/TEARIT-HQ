#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024

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

static void path_join(char *out, size_t out_sz, const char *root, const char *rel) {
    snprintf(out, out_sz, "%s/%s", root && root[0] ? root : ".", rel);
}

static void write_status(const char *root, const char *state, const char *detail, const char *output_rel, const char *audio_mode) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/record.status");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "state=%s\n", state ? state : "unknown");
    fprintf(f, "detail=%s\n", detail ? detail : "");
    fprintf(f, "output=%s\n", output_rel ? output_rel : "");
    fprintf(f, "audio=%s\n", audio_mode ? audio_mode : "video_only");
    fclose(f);
}

static void pulse_marker(const char *root, const char *msg) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/fs_watch.marker");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "screen_record %s %ld\n", msg ? msg : "tick", (long)time(NULL));
    fclose(f);
}

static void kill_pid_file(const char *path, int sig) {
    FILE *f = fopen(path, "r");
    pid_t pid = -1;
    if (!f) return;
    if (fscanf(f, "%d", &pid) == 1 && pid > 1) {
        kill(pid, sig);
    }
    fclose(f);
}

static pid_t read_pid_file(const char *path) {
    FILE *f = fopen(path, "r");
    pid_t pid = -1;
    if (!f) return -1;
    if (fscanf(f, "%d", &pid) != 1) pid = -1;
    fclose(f);
    return pid;
}

static void kill_residual_ffmpeg(const char *root) {
    char recordings_dir[MAX_PATH_LEN];
    char quoted_dir[MAX_PATH_LEN * 2];
    char cmd[MAX_PATH_LEN * 4];

    path_join(recordings_dir, sizeof(recordings_dir), root, "session/recordings/");
    shell_quote(quoted_dir, sizeof(quoted_dir), recordings_dir);
    snprintf(cmd, sizeof(cmd),
        "pkill -TERM -f -- '%s' >/dev/null 2>&1; "
        "sleep 1; "
        "pkill -KILL -f -- '%s' >/dev/null 2>&1",
        quoted_dir, quoted_dir);
    run_cmd(cmd);
}

static void kill_previous(const char *root) {
    char daemon_pid[MAX_PATH_LEN];
    char ffmpeg_pid[MAX_PATH_LEN];
    path_join(daemon_pid, sizeof(daemon_pid), root, "session/record.pid");
    path_join(ffmpeg_pid, sizeof(ffmpeg_pid), root, "session/record.ffmpeg.pid");
    kill_pid_file(ffmpeg_pid, SIGINT);
    usleep(300000);
    kill_pid_file(ffmpeg_pid, SIGKILL);
    kill_pid_file(daemon_pid, SIGTERM);
    usleep(200000);
    kill_pid_file(daemon_pid, SIGKILL);
    remove(daemon_pid);
    remove(ffmpeg_pid);
    kill_residual_ffmpeg(root);
}

static void ensure_dirs(const char *root) {
    char cmd[MAX_PATH_LEN * 2];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/session/recordings'", root);
    run_cmd(cmd);
}

static void read_cmd_output(const char *cmd, char *out, size_t out_sz) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        out[0] = '\0';
        return;
    }
    if (!fgets(out, (int)out_sz, fp)) {
        out[0] = '\0';
    }
    pclose(fp);
    out[strcspn(out, "\r\n")] = '\0';
}

static void determine_dimensions(char *dims, size_t dims_sz) {
    read_cmd_output("xdpyinfo 2>/dev/null | awk '/dimensions:/{print $2; exit}'", dims, dims_sz);
    if (!dims[0]) snprintf(dims, dims_sz, "1280x720");
}

static void determine_audio_source(char *out, size_t out_sz) {
    char source[256];
    read_cmd_output("pactl get-default-sink 2>/dev/null", source, sizeof(source));
    if (source[0]) {
        char cmd[MAX_PATH_LEN];
        snprintf(cmd, sizeof(cmd), "pactl list sources short 2>/dev/null | awk '/%s.monitor/{print $2; exit}'", source);
        read_cmd_output(cmd, out, out_sz);
        if (out[0]) return;
    }
    read_cmd_output("pactl list sources short 2>/dev/null | awk '/monitor/{print $2; exit}'", out, out_sz);
}

static void extract_poster(const char *root, const char *output_abs) {
    char poster[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 4];
    path_join(poster, sizeof(poster), root, "session/last_capture.png");
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -i '%s' -vf \"thumbnail,scale=320:240\" -frames:v 1 '%s' >/dev/null 2>&1",
        output_abs, poster);
    run_cmd(cmd);
}

static void read_first_log_line(const char *root, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN];
    FILE *f;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    path_join(path, sizeof(path), root, "session/record.ffmpeg.log");
    f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)out_sz, f)) {
        out[strcspn(out, "\r\n")] = '\0';
    }
    fclose(f);
}

static int validate_mp4(const char *path) {
    char cmd[MAX_PATH_LEN * 2];
    snprintf(cmd, sizeof(cmd),
        "ffprobe -v error -show_entries format=duration,size -of default=noprint_wrappers=1 '%s' >/dev/null 2>&1",
        path);
    return run_cmd(cmd) == 0;
}

static int daemon_main(const char *root) {
    char pid_path[MAX_PATH_LEN];
    char ffmpeg_pid_path[MAX_PATH_LEN];
    char rel_output[MAX_PATH_LEN];
    char abs_output[MAX_PATH_LEN];
    char dims[64];
    char audio_source[256];
    char audio_mode[32];
    char ffmpeg_log[MAX_PATH_LEN];
    char failure_detail[256];
    char ts[64];
    FILE *pidf;
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    struct stat output_st;

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);

    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tmv ? tmv : &(struct tm){0});
    snprintf(rel_output, sizeof(rel_output), "session/recordings/gl-session-%s.mp4", ts);
    path_join(abs_output, sizeof(abs_output), root, rel_output);
    determine_dimensions(dims, sizeof(dims));
    determine_audio_source(audio_source, sizeof(audio_source));
    snprintf(audio_mode, sizeof(audio_mode), "%s", audio_source[0] ? "pulse_monitor" : "video_only");
    path_join(ffmpeg_log, sizeof(ffmpeg_log), root, "session/record.ffmpeg.log");
    remove(ffmpeg_log);

    path_join(pid_path, sizeof(pid_path), root, "session/record.pid");
    path_join(ffmpeg_pid_path, sizeof(ffmpeg_pid_path), root, "session/record.ffmpeg.pid");
    pidf = fopen(pid_path, "w");
    if (pidf) {
        fprintf(pidf, "%d\n", getpid());
        fclose(pidf);
    }

    g_child_pid = fork();
    if (g_child_pid == 0) {
        freopen(ffmpeg_log, "w", stdout);
        freopen(ffmpeg_log, "a", stderr);
        if (audio_source[0]) {
            execlp("ffmpeg", "ffmpeg",
                "-loglevel", "error",
                "-nostdin",
                "-y",
                "-video_size", dims,
                "-framerate", "15",
                "-f", "x11grab",
                "-i", getenv("DISPLAY") ? getenv("DISPLAY") : ":0",
                "-f", "pulse",
                "-i", audio_source,
                "-c:v", "libx264",
                "-preset", "ultrafast",
                "-pix_fmt", "yuv420p",
                "-c:a", "aac",
                "-b:a", "128k",
                abs_output,
                (char *)NULL);
        } else {
            execlp("ffmpeg", "ffmpeg",
                "-loglevel", "error",
                "-nostdin",
                "-y",
                "-video_size", dims,
                "-framerate", "15",
                "-f", "x11grab",
                "-i", getenv("DISPLAY") ? getenv("DISPLAY") : ":0",
                "-c:v", "libx264",
                "-preset", "ultrafast",
                "-pix_fmt", "yuv420p",
                abs_output,
                (char *)NULL);
        }
        _exit(127);
    }
    if (g_child_pid < 0) {
        write_status(root, "error", "ffmpeg_fork_failed", rel_output, audio_mode);
        remove(pid_path);
        return 1;
    }
    pidf = fopen(ffmpeg_pid_path, "w");
    if (pidf) {
        fprintf(pidf, "%d\n", g_child_pid);
        fclose(pidf);
    }

    write_status(root, "recording", dims, rel_output, audio_mode);
    pulse_marker(root, "started");

    while (!g_stop) {
        pid_t child_state = waitpid(g_child_pid, NULL, WNOHANG);
        if (child_state == g_child_pid) {
            g_child_pid = -1;
            write_status(root, "stopped", "ffmpeg_exited", rel_output, audio_mode);
            break;
        }
        pulse_marker(root, "recording");
        usleep(1000000);
    }

    if (g_child_pid > 1) {
        kill(g_child_pid, SIGINT);
        waitpid(g_child_pid, NULL, 0);
        g_child_pid = -1;
    }
    if (stat(abs_output, &output_st) == 0 && output_st.st_size > 0) {
        extract_poster(root, abs_output);
        write_status(root, "stopped", "recording_saved", rel_output, audio_mode);
    } else {
        read_first_log_line(root, failure_detail, sizeof(failure_detail));
        write_status(root, "error", failure_detail[0] ? failure_detail : "recording_missing_output", rel_output, audio_mode);
    }
    pulse_marker(root, "stopped");
    remove(pid_path);
    remove(ffmpeg_pid_path);
    return 0;
}

static void finalize_stop(const char *root) {
    char status_path[MAX_PATH_LEN];
    char rel_output[MAX_PATH_LEN] = "";
    char abs_output[MAX_PATH_LEN];
    char audio_mode[64] = "video_only";
    char ffmpeg_pid_path[MAX_PATH_LEN];
    char daemon_pid_path[MAX_PATH_LEN];
    char line[512];
    char failure_detail[256];
    struct stat output_st;
    FILE *f;
    pid_t ffmpeg_pid;
    int wait_ticks = 0;

    path_join(status_path, sizeof(status_path), root, "session/record.status");
    f = fopen(status_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strncmp(line, "output=", 7) == 0) {
                snprintf(rel_output, sizeof(rel_output), "%s", line + 7);
            } else if (strncmp(line, "audio=", 6) == 0) {
                snprintf(audio_mode, sizeof(audio_mode), "%s", line + 6);
            }
        }
        fclose(f);
    }

    path_join(ffmpeg_pid_path, sizeof(ffmpeg_pid_path), root, "session/record.ffmpeg.pid");
    ffmpeg_pid = read_pid_file(ffmpeg_pid_path);
    if (ffmpeg_pid > 1) {
        kill(ffmpeg_pid, SIGINT);
        for (wait_ticks = 0; wait_ticks < 25; wait_ticks++) {
            if (kill(ffmpeg_pid, 0) != 0) {
                break;
            }
            usleep(200000);
        }
        if (kill(ffmpeg_pid, 0) == 0) {
            kill(ffmpeg_pid, SIGKILL);
        }
    }
    kill_residual_ffmpeg(root);

    if (rel_output[0]) {
        path_join(abs_output, sizeof(abs_output), root, rel_output);
    } else {
        abs_output[0] = '\0';
    }

    if (abs_output[0] &&
        stat(abs_output, &output_st) == 0 &&
        output_st.st_size > 0 &&
        validate_mp4(abs_output)) {
        extract_poster(root, abs_output);
        write_status(root, "stopped", "recording_saved", rel_output, audio_mode);
    } else {
        read_first_log_line(root, failure_detail, sizeof(failure_detail));
        write_status(root, "error", failure_detail[0] ? failure_detail : "recording_missing_output", rel_output, audio_mode);
    }

    path_join(daemon_pid_path, sizeof(daemon_pid_path), root, "session/record.pid");
    kill_pid_file(daemon_pid_path, SIGKILL);
    remove(daemon_pid_path);
    remove(ffmpeg_pid_path);
    pulse_marker(root, "stop_finalized");
}

int main(int argc, char **argv) {
    const char *mode;
    const char *root;

    if (argc < 3) {
        fprintf(stderr, "usage: wraith_screen_record <--start|--stop> <project_root>\n");
        return 1;
    }
    mode = argv[1];
    root = argv[2];
    ensure_dirs(root);

    if (strcmp(mode, "--stop") == 0) {
        finalize_stop(root);
        return 0;
    }
    if (strcmp(mode, "--start") != 0) {
        return 1;
    }

    kill_previous(root);

    {
        pid_t pid = fork();
        if (pid == 0) {
            if (setsid() < 0) _exit(1);
            freopen("/dev/null", "r", stdin);
            freopen("/dev/null", "a", stdout);
            freopen("/dev/null", "a", stderr);
            _exit(daemon_main(root));
        }
        if (pid < 0) {
            write_status(root, "error", "daemon_fork_failed", "", "video_only");
            return 1;
        }
    }

    write_status(root, "starting", "start_requested", "", "video_only");
    pulse_marker(root, "starting");
    return 0;
}
