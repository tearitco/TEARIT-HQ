#include <stdio.h>
#include <stdlib.h>  // For popen, pclose
#include <string.h>  // For strcspn
#include <unistd.h>  // For sleep

#define MAX_PATH_LENGTH 1024

// Helper to get path from path_utils executable
static int get_path_from_path_utils(const char* command, char* result_path) {
    char cmd_buffer[MAX_PATH_LENGTH * 2];
    FILE *fp;
    
    snprintf(cmd_buffer, sizeof(cmd_buffer),
             "./pieces/locations/+x/path_utils.+x %s", command);
    
    fp = popen(cmd_buffer, "r");
    if (fp == NULL) {
        fprintf(stderr, "[gl_renderer] Failed to run path_utils command: %s\n", cmd_buffer);
        return -1;
    }
    
    if (fgets(result_path, MAX_PATH_LENGTH, fp) != NULL) {
        result_path[strcspn(result_path, "\n")] = 0; // Remove newline
    } else {
        fprintf(stderr, "[gl_renderer] path_utils command returned no output for %s\n", command);
        pclose(fp);
        return -1;
    }
    
    pclose(fp);
    return 0;
}


/*
 * Stub for gl_renderer.c
 * In a real implementation, this would use OpenGL (e.g., via glut, SDL, or glfw)
 * to create a window and render graphics based on primitives in current_frame.txt.
 * It would also monitor frame_changed.txt for updates.
 *
 * It will use path_utils.+x to resolve the paths.
 *
 * Compilation requires linking against OpenGL libraries, e.g.:
 * gcc -o gl_renderer.+x gl_renderer.c -lGL -lglut
 */
int main(int argc, char *argv[]) {
    char current_frame_path[MAX_PATH_LENGTH];
    char frame_changed_path[MAX_PATH_LENGTH];
    
    if (get_path_from_path_utils("current_frame", current_frame_path) != 0) {
        fprintf(stderr, "[gl_renderer] Error: Could not resolve current_frame path.\n");
        return 1;
    }
    if (get_path_from_path_utils("frame_changed", frame_changed_path) != 0) {
        fprintf(stderr, "[gl_renderer] Error: Could not resolve frame_changed path.\n");
        return 1;
    }
    
    printf("[gl_renderer.c] Stub executed. This piece will render frames to an OpenGL window.\n");
    printf("[gl_renderer.c] Will read frames from: %s\n", current_frame_path);
    printf("[gl_renderer.c] Will monitor: %s\n", frame_changed_path);
    
    // This process would typically run as a long-running daemon.
    // For now, just sleep to simulate running.
    sleep(1);
    return 0;
}
