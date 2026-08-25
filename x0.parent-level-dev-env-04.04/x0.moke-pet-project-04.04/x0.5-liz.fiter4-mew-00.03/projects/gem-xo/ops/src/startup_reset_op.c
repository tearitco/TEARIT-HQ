#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void truncate_file(const char *path) {
    if (!path || !*path) return;
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

static void ensure_dir(const char *path) {
    if (!path || !*path) return;
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0755);
}

int main(void) {
    ensure_dir("manager");
    ensure_dir("state");
    ensure_dir("pieces/display");
    ensure_dir("pieces/chtpm/frame_buffer");
    ensure_dir("pieces/apps/player_app");

    truncate_file("pieces/apps/player_app/history.txt");
    truncate_file("pieces/apps/player_app/cli_buffers.txt");
    truncate_file("manager/gui_state.txt");
    truncate_file("pieces/display/current_frame.txt");
    truncate_file("pieces/display/frame_changed.txt");
    truncate_file("pieces/display/active_gui_index.txt");
    truncate_file("pieces/chtpm/frame_buffer/current_frame.txt");
    truncate_file("pieces/chtpm/frame_buffer/frame_changed.txt");
    truncate_file("state/context.json");
    truncate_file("state/prompt.json");
    truncate_file("state/llm_response.json");
    truncate_file("state/last_response.txt");
    truncate_file("state/curl_debug.log");
    truncate_file("state/args.tmp");
    unlink("state/function_call.tmp");
    return 0;
}
