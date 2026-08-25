#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SANDBOX_SCOPE_DEFAULT_ROOT
#define SANDBOX_SCOPE_DEFAULT_ROOT "projects/gem-xo/sandbox"
#endif

static void sandbox_scope_trim(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static int sandbox_scope_read_kv(const char *path, const char *key, char *out, size_t out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = line;
        sandbox_scope_trim(trimmed);
        if (*trimmed == '\0' || *trimmed == '#') continue;

        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';

        char *k = trimmed;
        char *v = eq + 1;
        sandbox_scope_trim(k);
        sandbox_scope_trim(v);

        if (strcmp(k, key) == 0) {
            snprintf(out, out_size, "%s", v);
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}

static void sandbox_scope_join(const char *base, const char *path, char *out, size_t out_size) {
    if (!path || !*path) {
        snprintf(out, out_size, "%s", base && *base ? base : ".");
        return;
    }
    if (path[0] == '/') {
        snprintf(out, out_size, "%s", path);
        return;
    }
    if (!base || !*base) {
        snprintf(out, out_size, "%s", path);
        return;
    }
    if (strcmp(base, "/") == 0) snprintf(out, out_size, "/%s", path);
    else snprintf(out, out_size, "%s/%s", base, path);
}

static int sandbox_scope_normalize(const char *path, char *out, size_t out_size) {
    if (!path || !*path) {
        if (!getcwd(out, out_size)) return 0;
        return 1;
    }

    char combined[PATH_MAX];
    if (path[0] == '/') snprintf(combined, sizeof(combined), "%s", path);
    else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) return 0;
        sandbox_scope_join(cwd, path, combined, sizeof(combined));
    }

    char scratch[PATH_MAX];
    snprintf(scratch, sizeof(scratch), "%s", combined);

    char *segments[PATH_MAX / 2];
    int depth = 0;
    char *saveptr = NULL;
    for (char *token = strtok_r(scratch, "/", &saveptr); token; token = strtok_r(NULL, "/", &saveptr)) {
        if (strcmp(token, "") == 0 || strcmp(token, ".") == 0) continue;
        if (strcmp(token, "..") == 0) {
            if (depth > 0) depth--;
            continue;
        }
        if (depth < (int)(sizeof(segments) / sizeof(segments[0]))) {
            segments[depth++] = token;
        }
    }

    size_t pos = 0;
    if (pos < out_size) out[pos++] = '/';
    for (int i = 0; i < depth; i++) {
        size_t len = strlen(segments[i]);
        if (pos + len >= out_size) return 0;
        memcpy(out + pos, segments[i], len);
        pos += len;
        if (i + 1 < depth) {
            if (pos + 1 >= out_size) return 0;
            out[pos++] = '/';
        }
    }

    if (depth == 0) {
        if (out_size < 2) return 0;
        out[0] = '/';
        out[1] = '\0';
        return 1;
    }

    out[pos] = '\0';
    return 1;
}

static int sandbox_scope_is_within_root(const char *path, const char *root) {
    char normalized_root[PATH_MAX];
    char normalized_path[PATH_MAX];
    if (!sandbox_scope_normalize(root, normalized_root, sizeof(normalized_root))) return 0;
    if (!sandbox_scope_normalize(path, normalized_path, sizeof(normalized_path))) return 0;

    size_t root_len = strlen(normalized_root);
    if (strncmp(normalized_path, normalized_root, root_len) != 0) return 0;
    return normalized_path[root_len] == '\0' || normalized_path[root_len] == '/';
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: sandbox_scope_op <load-root|normalize|is-within-root> [...]\n");
        return 1;
    }

    if (strcmp(argv[1], "load-root") == 0) {
        const char *config_path = (argc > 2) ? argv[2] : NULL;
        const char *fallback = (argc > 3) ? argv[3] : SANDBOX_SCOPE_DEFAULT_ROOT;
        char root[PATH_MAX];
        snprintf(root, sizeof(root), "%s", fallback ? fallback : SANDBOX_SCOPE_DEFAULT_ROOT);
        if (config_path) {
            sandbox_scope_read_kv(config_path, "sandbox_root", root, sizeof(root));
        }
        printf("%s\n", root);
        return 0;
    }

    if (strcmp(argv[1], "normalize") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: sandbox_scope_op normalize <path>\n");
            return 1;
        }
        char out[PATH_MAX];
        if (!sandbox_scope_normalize(argv[2], out, sizeof(out))) return 1;
        printf("%s\n", out);
        return 0;
    }

    if (strcmp(argv[1], "is-within-root") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: sandbox_scope_op is-within-root <path> <root>\n");
            return 1;
        }
        printf("%d\n", sandbox_scope_is_within_root(argv[2], argv[3]) ? 1 : 0);
        return 0;
    }

    fprintf(stderr, "Unknown action: %s\n", argv[1]);
    return 1;
}
