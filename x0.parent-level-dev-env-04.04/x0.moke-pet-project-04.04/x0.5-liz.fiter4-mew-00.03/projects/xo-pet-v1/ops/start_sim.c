#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("pieces/manager/sim_control.txt", "w");
    if (f) {
        fprintf(f, "1\n");
        fclose(f);
    }
    printf("Simulation Started.");
    return 0;
}
