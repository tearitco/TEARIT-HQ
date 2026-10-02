/* horn_tool_exec - execute ONE tool call chosen by the model.
 *
 * Called by horn_turn when a provider replies with tool_calls. argv[1] is
 * the tool name, argv[2] is the raw JSON arguments object exactly as the
 * model produced it.
 *
 * SECURITY: tools/horn_tools.json is the entire allowlist. A tool the
 * model asks for that is not listed there is REFUSED, never dispatched.
 * The model is not trusted to only ask for things that were offered, and
 * a hallucinated name must not become a process launch. This is the one
 * place that decides, so it is the one place to audit.
 *
 * A manifest entry is dispatched one of two ways:
 *   - value "builtin:<handler>" runs a handler compiled in below
 *   - anything else is treated as an op name and executed as
 *     ops/+x/<value>.+x, with the raw JSON as argv[1]
 * The second path is how HALO will add Concept Bank tools: drop a binary
 * in ops/+x/, name it in the manifest, no C change here.
 *
 * Everything built in is READ-ONLY. Nothing in this file writes, deletes,
 * moves or executes. Adding a mutating tool has to be a deliberate edit.
 *
 * Usage: horn_tool_exec.+x <tool_name> <json_args>
 * Result is printed to stdout and captured by horn_turn as the tool
 * message content.
 * Self-contained: own root resolution, own helpers, no shared headers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF   (MAX_PATH + 256)
#define ARGS_CAP   65536
#define RESULT_CAP 65536

/* Caps. A tool result goes back into the model's context, so an unbounded
 * read would blow the context (and the 262K window) on one directory with
 * ten thousand files. */
#define READ_DEFAULT_CAP  8000
#define READ_HARD_CAP     65536
#define GREP_DEFAULT_MAX  60
#define GREP_HARD_MAX     500
#define GREP_MAX_FILE_BYTES (1024 * 1024)
#define LIST_MAX_ENTRIES  300

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

/* ── tiny JSON readers ────────────────────────────────────────────────
 *
 * Deliberately not a general JSON parser: these read one flat argument
 * object the model itself produced, and a real parser would be more code
 * than the thing it replaces. Every value is bounds-checked and the
 * result is always a NUL-terminated copy.
 */

/* Read a string value for "key" out of a flat JSON object. Handles the
 * escape forms a model can emit inside a path or pattern. */
static int json_get_str(const char *json, const char *key,
                        char *out, size_t out_sz) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return 0;
    p++;

    size_t o = 0;
    while (*p && o < out_sz - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                case '"': out[o++] = '"';  break;
                case '\\':out[o++] = '\\'; break;
                case '/': out[o++] = '/';  break;
                default:  out[o++] = '\\'; out[o++] = *p; break;
            }
            p++;
            continue;
        }
        if (*p == '"') break;
        out[o++] = *p++;
    }
    out[o] = '\0';
    return 1;
}

static int json_get_int(const char *json, const char *key, int dflt, int max) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return dflt;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return dflt;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    long v = strtol(p, NULL, 10);
    if (v <= 0) return dflt;
    return (v > max) ? max : (int)v;
}

/* ── path resolution ────────────────────────────────────────────────── */

/* Resolve a model-supplied path against the project root.
 *
 * Absolute paths are honoured: the model is deliberately allowed to read
 * outside the project (it is a coding assistant and the house tree is the
 * context), but the result is still capped and never followed through a
 * symlink loop by the callers below. Relative paths are anchored at the
 * project root so the model does not have to know where it was launched.
 */
static void resolve_path(const char *in, char *out, size_t out_sz) {
    if (!in || !in[0]) { snprintf(out, out_sz, "%s", project_root); return; }
    if (in[0] == '/') { snprintf(out, out_sz, "%s", in); return; }
    snprintf(out, out_sz, "%s/%s", project_root, in);
}

/* ── builtin: list_dir ─────────────────────────────────────────────── */

/* Sorting root for cmp_entry. A file-scope global rather than a nested
 * function: nested functions are a GCC extension, and this file is
 * built with plain -Wall -Wextra on whatever cc is present. */
static char g_sort_root[MAX_PATH];

static int cmp_entry(const void *a, const void *b) {
    const char *na = *(const char *const *)a;
    const char *nb = *(const char *const *)b;
    char pa[PATH_BUF], pb[PATH_BUF];
    snprintf(pa, sizeof(pa), "%s/%s", g_sort_root, na);
    snprintf(pb, sizeof(pb), "%s/%s", g_sort_root, nb);
    struct stat sa, sb;
    int da = (stat(pa, &sa) == 0 && S_ISDIR(sa.st_mode)) ? 0 : 1;
    int db = (stat(pb, &sb) == 0 && S_ISDIR(sb.st_mode)) ? 0 : 1;
    if (da != db) return da - db;
    return strcmp(na, nb);
}

static int tool_list_dir(const char *args, char *out, size_t out_sz) {
    char rel[PATH_BUF];
    if (!json_get_str(args, "path", rel, sizeof(rel)) || !rel[0])
        snprintf(rel, sizeof(rel), ".");

    char full[MAX_PATH];
    resolve_path(rel, full, sizeof(full));

    DIR *d = opendir(full);
    if (!d) {
        snprintf(out, out_sz, "error: cannot open directory '%s'", rel);
        return 1;
    }

    struct dirent *e;
    char  *names[LIST_MAX_ENTRIES];
    int    n = 0, truncated = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (n < LIST_MAX_ENTRIES) names[n++] = strdup(e->d_name);
        else truncated = 1;
    }
    closedir(d);

    /* Directories first, then files, each alphabetical - an unsorted
     * readdir order makes the model re-ask the same question. */
    snprintf(g_sort_root, sizeof(g_sort_root), "%s", full);
    qsort(names, (size_t)n, sizeof(char *), cmp_entry);

    size_t o = 0;
    o += (size_t)snprintf(out + o, out_sz - o, "%s (%d entries)\n", rel, n);
    for (int i = 0; i < n && o < out_sz - PATH_BUF - 8; i++) {
        char p[PATH_BUF];
        snprintf(p, sizeof(p), "%s/%s", full, names[i]);
        struct stat st;
        int is_dir = (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
        o += (size_t)snprintf(out + o, out_sz - o, "  %s%s\n",
                              names[i], is_dir ? "/" : "");
        free(names[i]);
    }
    if (truncated)
        o += (size_t)snprintf(out + o, out_sz - o,
                              "  ... (more than %d entries; listing truncated)\n",
                              LIST_MAX_ENTRIES);
    return 0;
}

/* ── builtin: read_file ────────────────────────────────────────────── */

static int tool_read_file(const char *args, char *out, size_t out_sz) {
    char rel[PATH_BUF];
    if (!json_get_str(args, "path", rel, sizeof(rel)) || !rel[0]) {
        snprintf(out, out_sz, "error: read_file needs a \"path\"");
        return 1;
    }
    int cap = json_get_int(args, "max_bytes", READ_DEFAULT_CAP, READ_HARD_CAP);

    char full[PATH_BUF];
    resolve_path(rel, full, sizeof(full));

    struct stat st;
    if (stat(full, &st) != 0) {
        snprintf(out, out_sz, "error: no such file '%s'", rel);
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        snprintf(out, out_sz, "error: '%s' is a directory - use list_dir", rel);
        return 1;
    }

    FILE *f = fopen(full, "rb");
    if (!f) {
        snprintf(out, out_sz, "error: cannot read '%s'", rel);
        return 1;
    }
    size_t n = fread(out, 1, out_sz - 1, f);
    fclose(f);
    out[n] = '\0';

    /* Refuse binary content: sending raw bytes into a chat context wastes
     * it and can confuse the model badly. Say so plainly instead. */
    size_t scan = n < 512 ? n : 512;
    int binary = 0;
    for (size_t i = 0; i < scan; i++) {
        unsigned char c = (unsigned char)out[i];
        if (c == 0) { binary = 1; break; }
    }
    if (binary) {
        snprintf(out, out_sz,
                 "error: '%s' looks binary (%zu bytes); not returning its contents", rel, n);
        return 1;
    }

    if ((int)n > cap) {
        out[cap] = '\0';
        char tail[256];
        snprintf(tail, sizeof(tail),
                 "\n... [truncated at %d of %zu bytes - re-read with max_bytes to get more]\n",
                 cap, n);
        size_t tl = strlen(tail);
        if (cap + tl < out_sz) memcpy(out + cap, tail, tl + 1);
    }
    return 0;
}

/* ── builtin: grep_files ───────────────────────────────────────────── */

static int tool_grep_files(const char *args, char *out, size_t out_sz) {
    char pat[1024], rel[PATH_BUF];
    if (!json_get_str(args, "pattern", pat, sizeof(pat)) || !pat[0]) {
        snprintf(out, out_sz, "error: grep_files needs a \"pattern\"");
        return 1;
    }
    if (!json_get_str(args, "path", rel, sizeof(rel)) || !rel[0])
        snprintf(rel, sizeof(rel), ".");
    int max = json_get_int(args, "max_matches", GREP_DEFAULT_MAX, GREP_HARD_MAX);

    regex_t re;
    int rc = regcomp(&re, pat, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) {
        char ebuf[256];
        regerror(rc, &re, ebuf, sizeof(ebuf));
        snprintf(out, out_sz, "error: bad regex '%s': %s", pat, ebuf);
        return 1;
    }

    char root[PATH_BUF];
    resolve_path(rel, root, sizeof(root));

    /* Runtime trees the model must not grep. It searches them and finds its
     * OWN conversation: the session transcript contains the question it was
     * just asked, so "find where X is defined" matches the question text
     * before the real source, and the model confidently reports a match
     * that is really just its own prompt echoed back. Observed live. */
    static const char *SKIP_DIRS[] = { "chats", "pieces", NULL };
    int skip_this = 0;
    for (int k = 0; SKIP_DIRS[k]; k++) {
        char probe[PATH_BUF];
        snprintf(probe, sizeof(probe), "%s/%s", project_root, SKIP_DIRS[k]);
        if (strcmp(root, probe) == 0) skip_this = 1;
    }
    if (skip_this) {
        snprintf(out, out_sz,
                 "error: refusing to search '%s' - it holds this session's own "
                 "transcript and generated runtime state, so results would "
                 "just be your own question echoed back. Search the source "
                 "directories instead.", rel);
        return 1;
    }

    /* A FILE path greps that one file. Only a directory walks. The model
     * naturally passes the file it wants ("grep for X in horn_turn.c"), and
     * treating that as a directory produced "no matches in 0 files" for a
     * symbol that is definitely in the file. */
    struct stat rst;
    if (stat(root, &rst) == 0 && S_ISREG(rst.st_mode)) {
        size_t o = 0;
        int found = 0;
        o += (size_t)snprintf(out + o, out_sz - o, "grep /%s/ for /%s/\n", rel, pat);
        FILE *f1 = fopen(root, "rb");
        if (!f1) { snprintf(out + o, out_sz - o, "error: cannot read '%s'", rel); return 1; }
        char line[4096];
        int ln = 0;
        while (fgets(line, sizeof(line), f1) && found < max && o < out_sz - 1024) {
            ln++;
            if (regexec(&re, line, 0, NULL, 0) == 0) {
                o += (size_t)snprintf(out + o, out_sz - o, "%s:%d: %.300s\n", rel, ln, line);
                found++;
            }
        }
        fclose(f1);
        if (!found) snprintf(out + o, out_sz - o, "no matches in %s\n", rel);
        regfree(&re);
        return 0;
    }

    /* Walk with an explicit stack. The tree here contains directories with
     * names containing emoji and spaces, so nothing may be word-split.
     *
     * ONE counter for push and pop. The first version kept `sp` and
     * `nstack` separately and seeded only `nstack`, so the very first pop
     * was `stack[--sp]` with sp == 0 - an out-of-bounds read of stack[-1],
     * i.e. whatever pointer happened to sit before the array. opendir
     * failed on it, the walk silently visited nothing, and grep_files
     * reported "no matches in 0 files" for a pattern that is plainly
     * present in the directory it was pointed at. */
    char *stack[512];
    int   sp = 0;
    stack[sp++] = strdup(root);

    size_t o = 0;
    int    found = 0, scanned = 0;
    o += (size_t)snprintf(out + o, out_sz - o, "grep /%s/ for /%s/\n", pat, rel);

    while (sp > 0 && found < max && o < out_sz - 1024) {
        char *dir = stack[--sp];
        DIR *d = opendir(dir);
        if (!d) { free(dir); continue; }
        struct dirent *e;
        while ((e = readdir(d)) != NULL && found < max) {
            if (e->d_name[0] == '.') continue;   /* skip .git, .*, hidden */
            char p[PATH_BUF];
            snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
            struct stat st;
            if (stat(p, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                int skip = 0;
                for (int k = 0; SKIP_DIRS[k]; k++)
                    if (strcmp(e->d_name, SKIP_DIRS[k]) == 0) skip = 1;
                if (!skip && sp < 512) stack[sp++] = strdup(p);
                continue;
            }
            if (!S_ISREG(st.st_mode)) continue;
            if (st.st_size > GREP_MAX_FILE_BYTES) continue;

            FILE *f = fopen(p, "rb");
            if (!f) continue;
            char line[4096];
            int  ln = 0;
            scanned++;
            while (fgets(line, sizeof(line), f) && found < max) {
                ln++;
                if (line[strcspn(line, "\r\n")] == '\0' && feof(f)) break;
                if (regexec(&re, line, 0, NULL, 0) == 0) {
                    /* Show project-relative paths: an absolute path is
                     * ~200 chars of this house's tree on every single line,
                     * which is most of the tool result's budget. Fall back
                     * to absolute only when the relative form would be
                     * empty, i.e. the file IS the root. (The first version
                     * guarded this backwards - `if (shown[0] != '/') shown = p;`
                     * threw away the good relative form and kept the long
                     * one, which is the opposite of what it looks like.) */
                    const char *shown = p;
                    size_t rootlen = strlen(project_root);
                    if (strncmp(p, project_root, rootlen) == 0 && p[rootlen] == '/'
                        && p[rootlen + 1] != '\0')
                        shown = p + rootlen + 1;
                    o += (size_t)snprintf(out + o, out_sz - o, "%s:%d: %.300s\n",
                                          shown, ln, line);
                    found++;
                }
            }
            fclose(f);
        }
        closedir(d);
        free(dir);
    }
    for (int i = 0; i < sp; i++) free(stack[i]);

    if (found == 0) snprintf(out + o, out_sz - o, "no matches in %d files\n", scanned);
    else if (found >= max)
        snprintf(out + o, out_sz - o, "... (stopped at max_matches=%d)\n", max);

    regfree(&re);
    return 0;
}

/* ── manifest: the allowlist ───────────────────────────────────────── */

/* Read the op that tools/horn_tools.json maps "name" to. Returns 0 when
 * the tool is NOT in the manifest, which is a refusal, not an error. */
static int manifest_lookup(const char *name, char *out, size_t out_sz) {
    char *path = NULL;
    if (asprintf(&path, "%s/tools/horn_tools.json", project_root) < 0 || !path) return 0;

    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) {
        fprintf(stderr, "horn_tool_exec: cannot read tools/horn_tools.json\n");
        return 0;
    }
    char *buf = malloc(ARGS_CAP);
    if (!buf) { fclose(f); return 0; }
    size_t n = fread(buf, 1, ARGS_CAP - 1, f);
    buf[n] = '\0';
    fclose(f);

    /* Look for the quoted name inside the "ops" object. Anchoring on the
     * object matters: the same names also appear in the "tools" array, and
     * matching the first occurrence would pick up the description text. */
    const char *ops = strstr(buf, "\"ops\"");
    int found = 0;
    if (ops) {
        char pat[256];
        snprintf(pat, sizeof(pat), "\"%s\"", name);
        const char *p = strstr(ops, pat);
        if (p) {
            p += strlen(pat);
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p == ':') {
                p++;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '"') {
                    p++;
                    size_t o = 0;
                    while (*p && *p != '"' && o < out_sz - 1) out[o++] = *p++;
                    out[o] = '\0';
                    found = 1;
                }
            }
        }
    }
    free(buf);
    return found;
}

int main(int argc, char *argv[]) {
    resolve_root();

    if (argc < 2 || !argv[1][0]) {
        fprintf(stderr, "usage: horn_tool_exec.+x <tool_name> [json_args]\n");
        return 2;
    }
    const char *name = argv[1];
    const char *args = (argc > 2) ? argv[2] : "{}";

    char mapped[256];
    if (!manifest_lookup(name, mapped, sizeof(mapped))) {
        /* Refused. Say it in the tool result so the model learns what it
         * may use, rather than silently getting an empty string back. */
        printf("error: tool '%s' is not registered in tools/horn_tools.json. "
               "Only registered tools may be called.\n", name);
        return 1;
    }

    char *result = malloc(RESULT_CAP);
    if (!result) { fprintf(stderr, "horn_tool_exec: out of memory\n"); return 3; }
    result[0] = '\0';

    if (strcmp(mapped, "builtin:list_dir") == 0) {
        tool_list_dir(args, result, RESULT_CAP);
    } else if (strcmp(mapped, "builtin:read_file") == 0) {
        tool_read_file(args, result, RESULT_CAP);
    } else if (strcmp(mapped, "builtin:grep_files") == 0) {
        tool_grep_files(args, result, RESULT_CAP);
    } else {
        /* External op: dispatch it. fork/exec rather than system(), so the
         * JSON args reach argv untouched - system() would need quoting
         * that a model-supplied regex cannot be trusted with. */
        char *op = NULL;
        if (asprintf(&op, "%s/ops/+x/%s.+x", project_root, mapped) < 0 || !op) {
            free(result);
            return 3;
        }
        int pipefd[2];
        if (pipe(pipefd) != 0) {
            snprintf(result, RESULT_CAP, "error: cannot create pipe for tool '%s'", name);
            free(op); free(result);
            return 1;
        }
        pid_t pid = fork();
        if (pid < 0) {
            snprintf(result, RESULT_CAP, "error: cannot fork for tool '%s'", name);
            close(pipefd[0]); close(pipefd[1]); free(op); free(result);
            return 1;
        }
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
            execl(op, mapped, args, (char *)NULL);
            _exit(127);
        }
        close(pipefd[1]);
        size_t got = 0;
        ssize_t r;
        while (got < RESULT_CAP - 1 &&
               (r = read(pipefd[0], result + got, RESULT_CAP - 1 - got)) > 0)
            got += (size_t)r;
        result[got] = '\0';
        close(pipefd[0]);
        int st = 0;
        waitpid(pid, &st, 0);
        if (got == 0) {
            snprintf(result, RESULT_CAP,
                     "error: tool '%s' (op %s) produced no output (exit %d)",
                     name, mapped, WIFEXITED(st) ? WEXITSTATUS(st) : -1);
        }
        free(op);
    }

    fputs(result, stdout);
    free(result);
    return 0;
}