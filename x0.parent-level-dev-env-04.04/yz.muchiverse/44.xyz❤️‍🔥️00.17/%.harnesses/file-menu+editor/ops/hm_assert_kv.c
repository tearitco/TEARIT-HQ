/* hm_assert_kv - assert KEY=VALUE in a kv file.
 * Usage: hm_assert_kv.+x <path> <key> <expected_value> [label]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: hm_assert_kv.+x <path> <key> <expected> [label]\n");
        return 1;
    }
    const char *path = argv[1];
    const char *key = argv[2];
    const char *exp = argv[3];
    const char *label = argc >= 5 ? argv[4] : key;

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("FAIL: %s (no file)\n", label);
        return 1;
    }
    char line[2048];
    size_t klen = strlen(key);
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            found = (strcmp(v, exp) == 0);
            if (!found)
                printf("FAIL: %s (got '%s' expected '%s')\n", label, v, exp);
            break;
        }
    }
    fclose(f);
    if (!found && ftell(stdin) == 0) { /* silence */ }
    if (found) { printf("PASS: %s\n", label); return 0; }
    /* re-check if key missing */
    f = fopen(path, "r");
    int has_key = 0;
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') has_key = 1;
        }
        fclose(f);
    }
    if (!has_key) printf("FAIL: %s (key missing)\n", label);
    return 1;
}
