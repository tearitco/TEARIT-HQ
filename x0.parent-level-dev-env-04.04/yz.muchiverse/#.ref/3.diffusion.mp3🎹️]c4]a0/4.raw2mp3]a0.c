#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 256

// Convert raw PCM to MP3 using FFmpeg
int convert_to_mp3(const char *input_raw, const char *output_mp3, int sample_rate, int channels, int bitrate, FILE *log_fp) {
    char cmd[512];
    // Escape input/output paths (basic escaping for spaces and special chars)
    char escaped_input[MAX_PATH], escaped_output[MAX_PATH];
    snprintf(escaped_input, MAX_PATH, "\"%s\"", input_raw);
    snprintf(escaped_output, MAX_PATH, "\"%s\"", output_mp3);

    snprintf(cmd, sizeof(cmd), 
             "ffmpeg -y -f f32le -ar %d -ac %d -i %s -c:a mp3 -b:a %dk %s 2> ffmpeg_convert_err.txt",
             sample_rate, channels, escaped_input, bitrate, escaped_output);
    fprintf(log_fp, "Executing FFmpeg: %s\n", cmd);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(log_fp, "Error: FFmpeg failed to convert %s to %s (exit code %d)\n", input_raw, output_mp3, ret);
        FILE *err_fp = fopen("ffmpeg_convert_err.txt", "r");
        if (err_fp) {
            char err_buf[1024];
            while (fgets(err_buf, sizeof(err_buf), err_fp)) {
                fprintf(log_fp, "FFmpeg error: %s", err_buf);
            }
            fclose(err_fp);
        }
        remove("ffmpeg_convert_err.txt");
        return 0;
    }

    remove("ffmpeg_convert_err.txt");
    fprintf(log_fp, "Successfully converted %s to %s\n", input_raw, output_mp3);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_raw> <output_mp3>\n", argv[0]);
        return 1;
    }

    FILE *log_fp = fopen("convert_log.txt", "a");
    if (!log_fp) {
        fprintf(stderr, "Error: Cannot open convert_log.txt\n");
        return 1;
    }

    const char *input_raw = argv[1];
    const char *output_mp3 = argv[2];
    int sample_rate = 44100; // Matches audio_main.c
    int channels = 1;        // Mono
    int bitrate = 192;       // Standard MP3 bitrate (kbps)

    if (!convert_to_mp3(input_raw, output_mp3, sample_rate, channels, bitrate, log_fp)) {
        fprintf(stderr, "Error: Conversion failed\n");
        fclose(log_fp);
        return 1;
    }

    printf("Converted %s to %s\n", input_raw, output_mp3);
    fclose(log_fp);
    return 0;
}
