/* pin_pick_test - driver for the harness pin_pick.pal: prints which pin file khtpm_pin_pick.c chooses.
 * Usage: pin_pick_test <house> <board_root|-> [<out_size>]      prints `pick=<path>` (readable = the file can be opened for reading).
 * Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/pin_pick_test.+x pin_pick_test.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../khtpm_pin_pick.c"
static int can_read(const char *p) {
    FILE *f = fopen(p, "r");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pin_pick_test <house> <board_root|-> [<out_size>]\n");
        return 2;
    }
    size_t n = argc >= 4 ? (size_t)atoi(argv[3]) : 4096;
    if (n == 0 || n > 65536) {
        fprintf(stderr, "pin_pick_test: bad out_size\n");
        return 2;
    }
    char *out = malloc(n + 1);
    if (!out) {
        return 1;
    }
    memset(out, 'Z', n);
    out[n] = 0;
    pin_path_pick(argv[1], strcmp(argv[2], "-") ? argv[2] : "", can_read, out, n);
    printf("pick=%s\n", out);
    printf("len=%zu\n", strlen(out));
    free(out);
    return 0;
}
