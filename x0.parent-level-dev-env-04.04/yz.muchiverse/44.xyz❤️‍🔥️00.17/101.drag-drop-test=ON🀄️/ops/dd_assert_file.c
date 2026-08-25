/* dd_assert_file.c - Check file exists and contains substring
 *
 * Usage: dd_assert_file <file> "<expected_substring>" ["<check_label>"]
 *
 * Prints PASS/FAIL with label, exits 0/1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file> <expected_substring> [check_label]\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    const char *expected = argv[2];
    const char *label = (argc > 3) ? argv[3] : "assert";

    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf("FAIL [%s]: file not found: %s\n", label, filepath);
        return 1;
    }

    char buf[4096];
    int found = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, expected)) {
            found = 1;
            break;
        }
    }
    fclose(f);

    if (found) {
        printf("PASS [%s]: found '%s' in %s\n", label, expected, filepath);
        return 0;
    } else {
        printf("FAIL [%s]: '%s' not found in %s\n", label, expected, filepath);
        return 1;
    }
}
