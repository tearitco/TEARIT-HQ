// ops/tts_speak.c - self-contained text-to-speech playing op
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_PATH 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <text>\n", argv[0]);
        return 1;
    }

    const char *text = argv[1];
    if (strlen(text) == 0) {
        printf("Empty text, nothing to speak.\n");
        return 0;
    }

    const char *env = getenv("PRISC_PROJECT_ROOT");
    const char *root = (env && env[0]) ? env : ".";

    char mp3_path[MAX_PATH];
    snprintf(mp3_path, sizeof(mp3_path), "%s/pieces/world_01/session_01/chat/tts_temp.mp3", root);

    // Step 1: Run edge-tts to generate mp3
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        return 1;
    }
    if (pid1 == 0) {
        execlp("edge-tts", "edge-tts", "--text", text, "--write-media", mp3_path, (char *)NULL);
        perror("execlp edge-tts");
        _exit(127);
    }

    int status1;
    if (waitpid(pid1, &status1, 0) == -1) {
        perror("waitpid edge-tts");
        remove(mp3_path);
        return 1;
    }

    if (!WIFEXITED(status1) || WEXITSTATUS(status1) != 0) {
        fprintf(stderr, "edge-tts generation failed or timed out.\n");
        remove(mp3_path);
        return 1;
    }

    // Step 2: Run mpg123 to play mp3
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork mpg123");
        remove(mp3_path);
        return 1;
    }
    if (pid2 == 0) {
        execlp("mpg123", "mpg123", "-q", mp3_path, (char *)NULL);
        perror("execlp mpg123");
        _exit(127);
    }

    int status2;
    if (waitpid(pid2, &status2, 0) == -1) {
        perror("waitpid mpg123");
        remove(mp3_path);
        return 1;
    }

    // Cleanup mp3 file
    remove(mp3_path);

    printf("Spoken: %s\n", text);
    return 0;
}
