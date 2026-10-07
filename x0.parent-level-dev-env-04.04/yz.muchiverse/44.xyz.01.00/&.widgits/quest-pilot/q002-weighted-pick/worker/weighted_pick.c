#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

#define CONST 0x9E3779B97F4A7C15ULL

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) *(--end) = '\0';
    return s;
}

static int parse_uint64(const char *s, uint64_t *out) {
    if (!s || !*s) return 0;
    uint64_t val = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        unsigned digit = *s - '0';
        if (val > (UINT64_MAX - digit) / 10) return 0;  // overflow
        val = val * 10 + digit;
        s++;
    }
    *out = val;
    return 1;
}

static int parse_uint32(const char *s, uint32_t *out) {
    if (!s || !*s) return 0;
    uint64_t val = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        unsigned digit = *s - '0';
        if (val > (UINT32_MAX - digit) / 10) return 0;  // overflow
        val = val * 10 + digit;
        s++;
    }
    *out = (uint32_t)val;
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 4) return 2;

    uint64_t seed, counter;
    if (!parse_uint64(argv[2], &seed)) return 2;
    if (!parse_uint64(argv[3], &counter)) return 2;

    FILE *fp = fopen(argv[1], "r");
    if (!fp) return 2;

    typedef struct {
        char *name;
        uint64_t weight;
    } Row;

    Row *rows = NULL;
    size_t rows_cap = 0, rows_len = 0;
    uint64_t total = 0;

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1) {
        char *p = trim(line);
        if (*p == '\0' || *p == '#') continue;

        char *sep1 = strchr(p, '|');
        if (!sep1) continue;
        *sep1 = '\0';
        char *sep2 = strchr(sep1 + 1, '|');
        if (!sep2) continue;
        *sep2 = '\0';

        char *field1 = trim(p);
        char *field2 = trim(sep1 + 1);
        char *field3 = trim(sep2 + 1);

        if (strcmp(field1, "W") != 0) continue;
        if (*field2 == '\0' || strchr(field2, '|')) continue;
        uint32_t w32;
        if (!parse_uint32(field3, &w32)) continue;
        uint64_t weight = w32;

        if (rows_len == rows_cap) {
            size_t newcap = rows_cap ? rows_cap * 2 : 8;
            Row *tmp = realloc(rows, newcap * sizeof(Row));
            if (!tmp) {
                /* out of memory */
                free(line);
                fclose(fp);
                for (size_t i = 0; i < rows_len; ++i) free(rows[i].name);
                free(rows);
                return 2;
            }
            rows = tmp;
            rows_cap = newcap;
        }
        rows[rows_len].name = strdup(field2);
        rows[rows_len].weight = weight;
        total += weight;
        rows_len++;
    }
    free(line);
    fclose(fp);

    if (total == 0) {
        for (size_t i = 0; i < rows_len; ++i) free(rows[i].name);
        free(rows);
        return 3;
    }

    uint64_t x = seed + counter * CONST;
    uint64_t z = x + CONST;
    z ^= z >> 30;
    z *= 0xBF58476D1CE4E5B9ULL;
    z ^= z >> 27;
    z *= 0x94D049BB133111EBULL;
    z ^= z >> 31;

    uint64_t r = z % total;

    uint64_t cum = 0;
    for (size_t i = 0; i < rows_len; ++i) {
        cum += rows[i].weight;
        if (cum > r) {
            printf("%s\n", rows[i].name);
            break;
        }
    }

    for (size_t i = 0; i < rows_len; ++i) free(rows[i].name);
    free(rows);
    return 0;
}
