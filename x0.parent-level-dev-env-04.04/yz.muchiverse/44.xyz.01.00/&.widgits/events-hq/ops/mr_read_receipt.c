/* mr_read_receipt — copy one key from a frame receipt into event state.
 * Usage:
 *   mr_read_receipt.+x <entity_dir> <house_root> <receipt> <key> <var_name> [switch=expect]
 * The compiler keeps four fields (MAX_FIELDS). The fourth is
 * switch=expect, for example on_harold=17. house_root is unused.
 * A missing receipt stores NONE and writes the switch as 0. A match
 * writes 1. `if` compares switches.txt to 1 or 0.
 */
#include <stdio.h>
#include <string.h>
#include "mr_clock_common.h"

static void read_key(const char *path, const char *key, char *out, size_t n) {
    snprintf(out, n, "NONE");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            size_t L = strlen(v);
            while (L && (v[L - 1] == '\n' || v[L - 1] == '\r')) v[--L] = 0;
            snprintf(out, n, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "Usage: mr_read_receipt.+x <entity_dir> <house_root> <receipt> <key> <var_name> [switch=expect]\n");
        return 1;
    }
    const char *entity = argv[1];
    (void)argv[2];
    const char *receipt = argv[3];
    const char *key = argv[4];
    const char *var_name = argv[5];
    const char *match = argc >= 7 ? argv[6] : "";
    char sw_buf[128] = "";
    char expect_buf[128] = "";
    const char *eq = strchr(match, '=');
    if (eq) {
        size_t n = (size_t)(eq - match);
        if (n >= sizeof(sw_buf)) n = sizeof(sw_buf) - 1;
        memcpy(sw_buf, match, n);
        sw_buf[n] = '\0';
        snprintf(expect_buf, sizeof(expect_buf), "%s", eq + 1);
    }
    const char *sw = sw_buf;
    const char *expect = expect_buf;

    char value[256];
    read_key(receipt, key, value, sizeof(value));

    char state_root[MR_PATH_BUF];
    mr_state_root(entity, state_root, sizeof(state_root));
    char vars_path[MR_PATH_BUF];
    snprintf(vars_path, sizeof(vars_path), "%s/variables.txt", state_root);
    mr_kv_set(vars_path, var_name, value);

    if (sw[0]) {
        char sw_path[MR_PATH_BUF];
        snprintf(sw_path, sizeof(sw_path), "%s/switches.txt", state_root);
        int match = expect[0] && strcmp(value, expect) == 0;
        mr_kv_set(sw_path, sw, match ? "1" : "0");
    }
    printf("READ_RECEIPT key=%s value=%s\n", key, value);
    return 0;
}
