/* tk_assert_contains - test-harness op. Checks whether a file (normally
 * a session's current_frame.txt) contains a given substring, prints a
 * PASS/FAIL line, and exits 0/1 accordingly - the standard evidence
 * format used across this test-harness (see README.txt).
 *
 * Self-contained, no shared headers.
 * Usage: tk_assert_contains.+x <file> <expected_substring> [check_label] */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tk_assert_contains.+x <file> <expected_substring> [check_label]\n");
        return 1;
    }
    const char *path = argv[1];
    const char *expected = argv[2];
    const char *label = argc >= 4 ? argv[3] : expected;

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("FAIL: %s (could not open %s)\n", label, path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); printf("FAIL: %s (out of memory)\n", label); return 1; }
    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0';
    fclose(f);

    int ok = strstr(buf, expected) != NULL;
    free(buf);

    if (ok) {
        printf("PASS: %s\n", label);
        return 0;
    } else {
        printf("FAIL: %s (expected substring not found in %s)\n", label, path);
        return 1;
    }
}
