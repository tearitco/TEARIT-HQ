#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("pieces/manager/sim_control.txt", "w");
    if (f) {
        fprintf(f, "0\n");
        fclose(f);
    }
    printf("Simulation Paused.");
    return 0;
}
