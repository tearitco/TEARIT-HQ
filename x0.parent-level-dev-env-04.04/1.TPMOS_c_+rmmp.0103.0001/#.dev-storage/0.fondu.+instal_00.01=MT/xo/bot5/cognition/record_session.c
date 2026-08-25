/*
 * record_session.c - XO Bot Imitation Learning Recorder (Clean Edition)
 * CPU-SAFE: Signal handling, fork/exec/waitpid, and stat-first polling.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_CMD 16384
#define MAX_LINE 1024

static volatile sig_atomic_t g_shutdown = 0;
static char *g_memory_path = NULL;
static int g_frame_count = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y%m%d_%H%M%S", t);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <project_root>\n", argv[0]);
        return 1;
    }

    char *project_root = argv[1];
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);

    // 1. Create Timestamped Memory Folder
    char ts[64];
    get_timestamp(ts, sizeof(ts));
    if (asprintf(&g_memory_path, "%s/xo/bot5/pieces/memories/rec_%s", project_root, ts) == -1) {
        return 1;
    }
    
    if (mkdir(g_memory_path, 0755) != 0) {
        perror("Failed to create memory directory");
        return 1;
    }

    printf("[XO-REC] Recording started at: %s\n", g_memory_path);

    // Paths to monitor
    char *frame_path, *history_path, *log_path;
    asprintf(&frame_path, "%s/pieces/display/current_frame.txt", project_root);
    asprintf(&history_path, "%s/pieces/keyboard/history.txt", project_root);
    asprintf(&log_path, "%s/input_log.txt", g_memory_path);

    struct stat st_frame, st_hist;
    long last_hist_pos = 0;
    off_t last_frame_size = -1;

    // Initialize history position to end of file
    if (stat(history_path, &st_hist) == 0) {
        last_hist_pos = st_hist.st_size;
    }

    while (!g_shutdown) {
        // Stream A: Monitor Frame (Vision)
        if (stat(frame_path, &st_frame) == 0) {
            if (st_frame.st_size != last_frame_size) {
                g_frame_count++;
                char *out_frame_path;
                asprintf(&out_frame_path, "%s/frame_%04d.txt", g_memory_path, g_frame_count);
                
                char *cmd;
                asprintf(&cmd, "cp %s %s", frame_path, out_frame_path);
                system(cmd);
                
                free(cmd);
                free(out_frame_path);
                last_frame_size = st_frame.st_size;
            }
        }

        // Stream B: Monitor History (Motor)
        if (stat(history_path, &st_hist) == 0 && st_hist.st_size > last_hist_pos) {
            FILE *hf = fopen(history_path, "r");
            FILE *lf = fopen(log_path, "a");
            if (hf && lf) {
                fseek(hf, last_hist_pos, SEEK_SET);
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), hf)) {
                    if (strstr(line, "KEY_PRESSED:")) {
                        fprintf(lf, "F:%04d | %s", g_frame_count, line);
                    }
                }
                last_hist_pos = ftell(hf);
            }
            if (hf) fclose(hf);
            if (lf) fclose(lf);
        }

        usleep(33333); // 30 FPS polling
    }

    // 2. Finalize Session
    char *marker_path;
    asprintf(&marker_path, "%s/session_complete.txt", g_memory_path);
    FILE *mf = fopen(marker_path, "w");
    if (mf) {
        fprintf(mf, "status=complete\nframes=%d\n", g_frame_count);
        fclose(mf);
    }

    printf("[XO-REC] Recording finalized. Memory saved to %s\n", g_memory_path);
    
    // Cleanup
    free(marker_path);
    free(log_path);
    free(history_path);
    free(frame_path);
    free(g_memory_path);

    return 0;
}
