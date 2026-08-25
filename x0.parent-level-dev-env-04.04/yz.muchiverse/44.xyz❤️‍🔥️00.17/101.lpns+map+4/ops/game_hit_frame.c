/* game_hit_frame.c - Display current frame (stub for now)
 * In full chtpm integration, this would update the HTML/XML display
 * For now, just cat the frame to stdout for verification */

#include <stdio.h>
#include <stdlib.h>

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
