/* install_ledger_op - labeled-install ledger + hosts lookup (spec: install-harness/SPEC.md).
 * Commands: next | append | list | resolve.  Exit 0 ok, 1 refused/not found, 2 usage/bad input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXF 8

/* Split a line in place on '|'; returns field count. */
static int split(char *s, char **f) {
    int n = 0;
    for (;;) {
        if (n < MAXF) f[n] = s;
        n++;
        s = strchr(s, '|');
        if (!s) break;
        *s++ = 0;
    }
    return n > MAXF ? MAXF : n;
}

/* Read next ledger/hosts line into buf (newline stripped). */
static int getrow(FILE *fp, char *buf, int sz) {
    if (!fgets(buf, sz, fp)) return 0;
    buf[strcspn(buf, "\r\n")] = 0;
    return 1;
}

/* Index (1-based, counting parsed rows) of first valid row of kind with label; 0 if none. */
static int find(const char *path, const char *kind, const char *label) {
    FILE *fp = fopen(path, "r");
    char buf[2048], *f[MAXF];
    int idx = 0, hit = 0;
    if (!fp) return 0;
    while (!hit && getrow(fp, buf, sizeof buf)) {
        int n = split(buf, f);
        if (n == 6 && !strcmp(f[0], "INSTALL")) idx++;
        else if (n == 3 && !strcmp(f[0], "REMOVE")) idx++;
        else continue;
        if (!strcmp(f[0], kind) && !strcmp(f[1], label)) hit = idx;
    }
    fclose(fp);
    return hit;
}

static int valid_label(const char *s) {
    if (*s < 'a' || *s > 'z') return 0;
    while ((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')) s++;
    if (s[0] != '-' || s[1] != 'v' || s[2] < '1' || s[2] > '9') return 0;
    for (s += 2; *s; s++) if (*s < '0' || *s > '9') return 0;
    return 1;
}

static int clean_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (!*argv[i] || strpbrk(argv[i], "|\n\r")) return 0;
    return 1;
}

static int cmd_next(const char *path, const char *name) {
    FILE *fp = fopen(path, "r");
    char buf[2048], *f[MAXF];
    size_t nl = strlen(name);
    long best = 0;
    while (fp && getrow(fp, buf, sizeof buf)) {
        if (split(buf, f) != 6 || strcmp(f[0], "INSTALL")) continue;
        if (strncmp(f[1], name, nl) || strncmp(f[1] + nl, "-v", 2)) continue;
        char *d = f[1] + nl + 2, *e;
        if (!*d) continue;
        long v = strtol(d, &e, 10);
        if (*e || v < 1) continue;
        if (v > best) best = v;
    }
    if (fp) fclose(fp);
    printf("%s-v%ld\n", name, best + 1);
    return 0;
}

static int cmd_append(int argc, char **argv) {
    const char *path = argv[2], *kind = argv[3];
    FILE *fp;
    if (!strcmp(kind, "INSTALL") && argc == 8) {
        if (!valid_label(argv[4])) return 2;
        if (find(path, "INSTALL", argv[4])) return 1;
        if (!(fp = fopen(path, "a"))) return 1;
        fprintf(fp, "INSTALL|%s|%s|%ld|%s|%s\n", argv[4], argv[5], (long)time(NULL), argv[6], argv[7]);
    } else if (!strcmp(kind, "REMOVE") && argc == 5) {
        if (!valid_label(argv[4])) return 2;
        if (!find(path, "INSTALL", argv[4]) || find(path, "REMOVE", argv[4])) return 1;
        if (!(fp = fopen(path, "a"))) return 1;
        fprintf(fp, "REMOVE|%s|%ld\n", argv[4], (long)time(NULL));
    } else return 2;
    return fclose(fp) ? 1 : 0;
}

static int cmd_list(const char *path) {
    FILE *fp = fopen(path, "r");
    char buf[2048], *f[MAXF];
    int idx = 0;
    while (fp && getrow(fp, buf, sizeof buf)) {
        int n = split(buf, f);
        if (n == 3 && !strcmp(f[0], "REMOVE")) { idx++; continue; }
        if (n != 6 || strcmp(f[0], "INSTALL")) continue;
        idx++;
        if (find(path, "INSTALL", f[1]) != idx) continue; /* only first install of a label */
        printf("%s %s %s %s\n", f[1], f[2], f[5], find(path, "REMOVE", f[1]) ? "removed" : "kept");
    }
    if (fp) fclose(fp);
    return 0;
}

static int cmd_resolve(const char *path, const char *name) {
    FILE *fp = fopen(path, "r");
    char buf[2048], *f[MAXF];
    int rc = 1;
    while (fp && rc && getrow(fp, buf, sizeof buf)) {
        if (buf[0] == '#' || split(buf, f) != 3) continue;
        if (!strcmp(f[0], "HOST") && !strcmp(f[1], name)) { puts(f[2]); rc = 0; }
    }
    if (fp) fclose(fp);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2 || !clean_args(argc, argv)) return 2;
    if (!strcmp(argv[1], "next") && argc == 4) return cmd_next(argv[2], argv[3]);
    if (!strcmp(argv[1], "append") && argc >= 4) return cmd_append(argc, argv);
    if (!strcmp(argv[1], "list") && argc == 3) return cmd_list(argv[2]);
    if (!strcmp(argv[1], "resolve") && argc == 4) return cmd_resolve(argv[2], argv[3]);
    return 2;
}
