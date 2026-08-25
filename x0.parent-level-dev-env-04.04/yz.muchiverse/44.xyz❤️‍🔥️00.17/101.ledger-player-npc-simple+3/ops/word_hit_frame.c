/* word_hit_frame.c - Display word game frame */

#include <stdio.h>

int main() {
    FILE *frame = fopen("pieces/display/frame.txt", "r");
    if (frame) {
        char line[1024];
        while (fgets(line, sizeof(line), frame)) {
            printf("%s", line);
        }
        fclose(frame);
    }
    return 0;
}
