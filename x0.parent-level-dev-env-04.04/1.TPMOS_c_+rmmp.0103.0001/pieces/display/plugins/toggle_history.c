#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE *f = fopen("pieces/display/state.txt", "r");
    char state[16] = "on";
    if (f) {
        char line[64];
        if (fgets(line, sizeof(line), f)) {
            if (strstr(line, "off")) strcpy(state, "on");
            else strcpy(state, "off");
        }
        fclose(f);
    }
    
    f = fopen("pieces/display/state.txt", "w");
    if (f) {
        fprintf(f, "frame_history=%s\n", state);
        fclose(f);
    }
    return 0;
}
