#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GUI_STATE "projects/tsots-online/manager/gui_state.txt"

int main() {
    char auth_status[64] = "GUEST";

    while (1) {
        FILE *fp = fopen(GUI_STATE, "w");
        if (fp) {
            fprintf(fp, "auth_status=%s\n", auth_status);
            fclose(fp);
        }
        usleep(100000);
    }
    return 0;
}
