#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc < 3) return 1;
    FILE *f = fopen(argv[1], "a");
    if (!f) return 1;
    fprintf(f, "[2026-07-31 00:00:00] KEY_PRESSED: %d\n", atoi(argv[2]));
    fclose(f);
    return 0;
}
