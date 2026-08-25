/* hm_assert_file - assert file equals exact content or contains substring.
 * Usage: hm_assert_file.+x <path> --equals|--contains <expected> [label]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: hm_assert_file.+x <path> --equals|--contains <expected> [label]\n");
        return 1;
    }
    const char *path = argv[1];
    const char *mode = argv[2];
    const char *expected = argv[3];
    const char *label = argc >= 5 ? argv[4] : expected;

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("FAIL: %s (cannot open %s)\n", label, path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 1; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);

    int ok = 0;
    if (strcmp(mode, "--equals") == 0)
        ok = (strcmp(buf, expected) == 0);
    else if (strcmp(mode, "--contains") == 0)
        ok = (strstr(buf, expected) != NULL);
    else {
        printf("FAIL: bad mode %s\n", mode);
        free(buf);
        return 1;
    }
    free(buf);
    if (ok) { printf("PASS: %s\n", label); return 0; }
    printf("FAIL: %s\n", label);
    return 1;
}
