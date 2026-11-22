/* rta_pull_op — save one bounded map source from a catalog.
 *   rta_pull_op list <catalog.pdl>
 *   rta_pull_op pull <catalog.pdl> <id> <outdir> [--max-bytes N]
 * Row: SRC | id | http|file|later | location | ext
 * http is fetched with curl. file is a path beside the catalog (no ..).
 * later is listed and refused: the body is not a desk-sized file.
 * Default cap 32 MiB. A miss, an oversize body, or a bad id writes nothing.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rta_pull_op.+x rta_pull_op.c
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXSRC 64
#define LINE 2048
#define DEFAULT_MAX (32 * 1024 * 1024)

typedef struct {
    char id[64];
    char kind[16];
    char loc[1600];
    char ext[16];
} Src;

static int split_pipe(char *line, char **f, int n) {
    int c = 0;
    f[c++] = line;
    for (char *p = line; *p && c < n; p++) {
        if (*p == '|') { *p = 0; f[c++] = p + 1; }
    }
    return c;
}

static void trim(char *s) {
    char *a = s;
    while (*a == ' ' || *a == '\t') a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
}

static int id_ok(const char *s) {
    if (!s[0] || strlen(s) > 40) return 0;
    for (const char *p = s; *p; p++)
        if (!islower((unsigned char)*p) && !isdigit((unsigned char)*p) && *p != '-') return 0;
    return 1;
}

static int load_catalog(const char *path, Src *out, int cap) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[LINE];
    int n = 0;
    while (fgets(line, sizeof line, fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *f[6];
        int c = split_pipe(line, f, 6);
        if (c < 5) continue;
        trim(f[0]);
        if (strcmp(f[0], "SRC") != 0) continue;
        if (n >= cap) { fclose(fp); return -2; }
        trim(f[1]); trim(f[2]); trim(f[3]); trim(f[4]);
        if (!id_ok(f[1])) { fclose(fp); return -3; }
        snprintf(out[n].id, sizeof out[n].id, "%s", f[1]);
        snprintf(out[n].kind, sizeof out[n].kind, "%s", f[2]);
        snprintf(out[n].loc, sizeof out[n].loc, "%s", f[3]);
        snprintf(out[n].ext, sizeof out[n].ext, "%s", f[4]);
        n++;
    }
    fclose(fp);
    return n;
}

static int copy_capped(const char *src, const char *dst, long maxb) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    if (fseek(in, 0, SEEK_END) != 0) { fclose(in); return -1; }
    long sz = ftell(in);
    if (sz < 0 || sz > maxb) { fclose(in); return -2; }
    rewind(in);
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[8192];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, in)) > 0) {
        if (fwrite(buf, 1, k, out) != k) { fclose(in); fclose(out); unlink(dst); return -1; }
    }
    fclose(in);
    if (fclose(out) != 0) { unlink(dst); return -1; }
    return 0;
}

static int curl_get(const char *url, const char *dst, long maxb) {
    char maxs[32];
    snprintf(maxs, sizeof maxs, "%ld", maxb);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execlp("curl", "curl", "-fsSL", "--max-time", "90", "--max-filesize", maxs,
               "-A", "LyfeDesk-rta-pull/1.0", "-o", dst, url, (char *)NULL);
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        unlink(dst);
        return -1;
    }
    struct stat sb;
    if (stat(dst, &sb) != 0 || sb.st_size <= 0 || sb.st_size > maxb) {
        unlink(dst);
        return -2;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: rta_pull_op list <catalog> | pull <catalog> <id> <outdir> [--max-bytes N]\n");
        return 2;
    }
    Src src[MAXSRC];
    int n = load_catalog(argv[2], src, MAXSRC);
    if (n < 0) { fprintf(stderr, "bad catalog\n"); return 2; }
    if (strcmp(argv[1], "list") == 0) {
        for (int i = 0; i < n; i++)
            printf("%s %s\n", src[i].id, src[i].kind);
        printf("count=%d\n", n);
        return 0;
    }
    if (strcmp(argv[1], "pull") != 0 || argc < 5) {
        fprintf(stderr, "usage: rta_pull_op pull <catalog> <id> <outdir> [--max-bytes N]\n");
        return 2;
    }
    long maxb = DEFAULT_MAX;
    for (int i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--max-bytes") == 0 && i + 1 < argc) maxb = atol(argv[++i]);
    }
    if (maxb < 1) { fprintf(stderr, "bad cap\n"); return 2; }
    int hit = -1;
    for (int i = 0; i < n; i++) if (strcmp(src[i].id, argv[3]) == 0) hit = i;
    if (hit < 0) { fprintf(stderr, "unknown id\n"); return 3; }
    if (strcmp(src[hit].kind, "later") == 0) {
        fprintf(stderr, "not a bounded file: %s\n", src[hit].loc);
        return 5;
    }
    if (mkdir(argv[4], 0755) != 0 && errno != EEXIST) { perror("outdir"); return 4; }
    char dest[1024];
    snprintf(dest, sizeof dest, "%s/%s.%s", argv[4], src[hit].id, src[hit].ext);
    int rc;
    if (strcmp(src[hit].kind, "file") == 0) {
        if (strstr(src[hit].loc, "..") || src[hit].loc[0] == '/') {
            fprintf(stderr, "file path must sit beside the catalog\n");
            return 2;
        }
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", argv[2]);
        char *sl = strrchr(dir, '/');
        if (sl) *sl = 0; else snprintf(dir, sizeof dir, ".");
        char from[1600];
        snprintf(from, sizeof from, "%s/%s", dir, src[hit].loc);
        rc = copy_capped(from, dest, maxb);
    } else if (strcmp(src[hit].kind, "http") == 0) {
        if (strncmp(src[hit].loc, "https://", 8) != 0) { fprintf(stderr, "http row needs https\n"); return 2; }
        rc = curl_get(src[hit].loc, dest, maxb);
    } else {
        fprintf(stderr, "unknown kind\n");
        return 2;
    }
    if (rc == -2) { fprintf(stderr, "oversize or empty\n"); unlink(dest); return 4; }
    if (rc != 0) { fprintf(stderr, "pull failed\n"); unlink(dest); return 4; }
    struct stat sb;
    if (stat(dest, &sb) != 0) return 4;
    printf("id=%s bytes=%ld file=%s.%s\n", src[hit].id, (long)sb.st_size, src[hit].id, src[hit].ext);
    char rec[1024];
    snprintf(rec, sizeof rec, "%s/%s.receipt", argv[4], src[hit].id);
    FILE *rf = fopen(rec, "w");
    if (!rf) return 4;
    fprintf(rf, "id=%s bytes=%ld file=%s.%s\n", src[hit].id, (long)sb.st_size, src[hit].id, src[hit].ext);
    fclose(rf);
    return 0;
}
