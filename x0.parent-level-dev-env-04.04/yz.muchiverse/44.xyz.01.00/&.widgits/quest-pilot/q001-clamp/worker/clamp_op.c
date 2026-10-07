#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

static int parse_long_long(const char *str, long long *out) {
    if (!str || *str == '\0') {
        return 0;
    }

    const char *p = str;
    if (*p == '+' || *p == '-') {
        p++;
    }

    if (*p == '\0') {
        return 0;
    }

    for (; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) {
            return 0;
        }
    }

    errno = 0;
    char *endptr;
    long long val = strtoll(str, &endptr, 10);

    if (errno == ERANGE) {
        return 0;
    }

    if (endptr != str + strlen(str)) {
        return 0;
    }

    *out = val;
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <value> <min> <max>\n", argv[0]);
        return 2;
    }

    long long value, min, max;

    if (!parse_long_long(argv[1], &value) ||
        !parse_long_long(argv[2], &min) ||
        !parse_long_long(argv[3], &max)) {
        fprintf(stderr, "Invalid integer argument\n");
        return 2;
    }

    if (min > max) {
        fprintf(stderr, "Min greater than max\n");
        return 2;
    }

    if (value < min) value = min;
    else if (value > max) value = max;

    printf("%lld\n", value);
    return 0;
}
