#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void strip_ansi(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '\x1b') {
            src++;
            if (*src == '[') {
                src++;
                while (*src && !((*src >= '@' && *src <= '~'))) src++;
                if (*src) src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

char* get_config_path() {
    FILE *f = fopen("projects/groq-ollama/config/paths.txt", "r");
    if (!f) return strdup("groq-ollama"); // Fallback to PATH
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp(line, "groq_ollama_bin=", 16) == 0) {
            char *val = strdup(line + 16);
            val[strcspn(val, "\r\n")] = 0;
            fclose(f);
            return val;
        }
    }
    fclose(f);
    return strdup("groq-ollama");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: groq-ollama_bridge <prompt>\n");
        return 1;
    }

    char *bin_path = get_config_path();
    char *cmd = NULL;
    // Use -y for YOLO mode and text output format
    if (asprintf(&cmd, "%s \"%s\" -y --output-format text --chat-recording false", bin_path, argv[1]) < 0) {
        free(bin_path);
        return 1;
    }

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen failed");
        free(cmd);
        free(bin_path);
        return 1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strip_ansi(buffer);
        printf("%s", buffer);
    }

    pclose(fp);
    free(cmd);
    free(bin_path);
    return 0;
}
