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
    #include <stdarg.h>
    #include <string.h>
    #include <ctype.h>
    #include <dirent.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <regex.h>
    #include <time.h>
    #include <sys/stat.h>
    #include <sys/types.h>

    /* Windows sandbox and process support. See the long note above
     * tool_run_script's Windows branch for what does and does not carry over
     * from the bwrap version - the short version is that this is WEAKER than
     * Linux containment and the file says so rather than implying parity. */
    #ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #include <errno.h>
    #define HT_WIN32 1

/* MinGW ships POSIX regex in libregex.a, which this project's build does not
 * link (Linux gets it from libc). scripts/build.ps1 adds it there rather than
 * rewriting grep_files' matcher against PCRE - the POSIX regexp semantics are
 * what that code is written against, and a silent behaviour change in a
 * model-facing grep would be much worse than one more flag. */
#define getcwd(b, n)  _getcwd((b), (n))
    #define access(p, m)  _access((p), (m))
    #define unlink(p)     _unlink(p)
    #define F_OK 0
    #define X_OK 1

    /* MinGW has no fsync. _commit(fd) is the flush-it-now equivalent and is
     * what write_file/edit_file actually need - durability of a small text
     * file before returning, not a POSIX fsync guarantee. */
    static int ht_fsync(int fd) { return _commit(fd); }
    #define fsync(fd) ht_fsync(fd)

    /* No mkdir shim: nothing here calls mkdir. tool_write_file explicitly
     * refuses to create directories ("No mkdir -p: a model that can invent
     * directory trees can sprawl one"), so a shim would be dead code. */

    /* Run `exe` with args, capture stdout, discard stderr. Same contract as the
     * POSIX branch it replaces: stdout returned NUL-terminated, stderr dropped
     * so a tool cannot corrupt the transcript, exit code reported separately.
     * Used for the external-op dispatcher, which is the same pipe+dup2 dance
     * horn_turn's execute_tool does. */
    static int ht_run_capture(const char *exe, char *const argv[],
                              char *out, size_t out_cap, int *exit_code) {
        char exebuf[MAX_PATH];
        snprintf(exebuf, sizeof(exebuf), "%s", exe);
        for (char *q = exebuf; *q; q++) if (*q == '/') *q = '\\';

        wchar_t wexe[MAX_PATH * 2];
        if (MultiByteToWideChar(CP_UTF8, 0, exebuf, -1, wexe, MAX_PATH * 2) == 0) return -1;

        size_t need = 1;
        for (int i = 0; argv[i]; i++) need += strlen(argv[i]) * 4 + 4;
        wchar_t *cmd = calloc(need, sizeof(wchar_t));
        if (!cmd) return -1;
        {
            wchar_t *p = cmd;
            *p++ = L'"';
            p += MultiByteToWideChar(CP_UTF8, 0, exebuf, -1, p, (int)(need - 1));
            p += wcslen(p);
            *p++ = L'"';
            for (int i = 0; argv[i]; i++) {
                *p++ = L' ';
                p += MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, p, (int)(need - 1));
                p += wcslen(p);
            }
        }

        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        HANDLE rd, wr, nul;
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);

        if (!CreatePipe(&rd, &wr, &sa, 0)) { free(cmd); return -1; }
        /* The write end MUST stay inheritable - that is how the child gets it.
         * Clearing HANDLE_FLAG_INHERIT here (the intuitive "don't leak my
         * handles" move) makes the child's stdout invalid: the command runs,
         * exits 0, and every byte it printed is silently discarded. Verified
         * by probe - cleared gives out=[], inherited gives out=[hello]. Only
         * the READ end is made non-inheritable; the parent's copy of the write
         * end is closed right after CreateProcess, which is what actually stops
         * the leak and lets the read side see EOF. */
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = wr;
        si.dwFlags |= STARTF_USESTDHANDLES;
        nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &sa, OPEN_EXISTING, 0, NULL);
        if (nul != INVALID_HANDLE_VALUE) si.hStdError = nul;

        BOOL ok = CreateProcessW(wexe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                                 NULL, NULL, &si, &pi);
        free(cmd);
        CloseHandle(wr);
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        if (!ok) { CloseHandle(rd); return -1; }

        out[0] = '\0';
        size_t got = 0;
        for (;;) {
            DWORD chunk = 0;
            DWORD room = (DWORD)((out_cap - 1 - got) > 0x10000 ? 0x10000 : (out_cap - 1 - got));
            if (room == 0) break;
            if (!ReadFile(rd, out + got, room, &chunk, NULL)) break;
            if (chunk == 0) break;
            got += chunk;
        }
        out[got] = '\0';
        CloseHandle(rd);

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (exit_code) *exit_code = (int)code;
        return 0;
    }
    #else
    #include <unistd.h>
    #include <signal.h>
    #include <sys/wait.h>
    #endif

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


/* Append to a fixed buffer, returning the NEW offset, clamped.
 *
 * snprintf returns the length it WOULD have written, so
 *     o += snprintf(out + o, out_sz - o, ...)
 * overshoots once the output is truncated. The next call then computes
 * out_sz - o with o > out_sz, which in size_t arithmetic is a number near
 * 2^64, and glibc's printf machinery tries to allocate it:
 *     xrealloc: cannot allocate 18446744071964721152 bytes
 * That aborted the whole suite mid-run, from an ordinary long tool result.
 *
 * Returns the offset actually written, never more than out_sz - 1, so the
 * next call always has a non-negative remaining size. */
static size_t appendf(char *buf, size_t sz, size_t off, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
static size_t appendf(char *buf, size_t sz, size_t off, const char *fmt, ...) {
    if (off >= sz) return sz - 1;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + off, sz - off, fmt, ap);
    va_end(ap);
    if (n < 0) return off;
    size_t wrote = (size_t)n;
    return (wrote >= sz - off) ? sz - 1 : off + wrote;
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
    o = appendf(out, out_sz, o, "%s (%d entries)\n", rel, n);
    for (int i = 0; i < n && o < out_sz - PATH_BUF - 8; i++) {
        char p[PATH_BUF];
        snprintf(p, sizeof(p), "%s/%s", full, names[i]);
        struct stat st;
        int is_dir = (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
        o = appendf(out, out_sz, o, "  %s%s\n",
                              names[i], is_dir ? "/" : "");
        free(names[i]);
    }
    if (truncated)
        o = appendf(out, out_sz, o,
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
        o = appendf(out, out_sz, o, "grep /%s/ for /%s/\n", rel, pat);
        FILE *f1 = fopen(root, "rb");
        if (!f1) { appendf(out, out_sz, o, "error: cannot read '%s'", rel); return 1; }
        char line[4096];
        int ln = 0;
        while (fgets(line, sizeof(line), f1) && found < max && o < out_sz - 1024) {
            ln++;
            if (regexec(&re, line, 0, NULL, 0) == 0) {
                o = appendf(out, out_sz, o, "%s:%d: %.300s\n", rel, ln, line);
                found++;
            }
        }
        fclose(f1);
        if (!found) appendf(out, out_sz, o, "no matches in %s\n", rel);
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
    o = appendf(out, out_sz, o, "grep /%s/ for /%s/\n", pat, rel);

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
                    o = appendf(out, out_sz, o, "%s:%d: %.300s\n",
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

    if (found == 0) appendf(out, out_sz, o, "no matches in %d files\n", scanned);
    else if (found >= max)
        appendf(out, out_sz, o, "... (stopped at max_matches=%d)\n", max);

    regfree(&re);
    return 0;
}


/* ── builtin: write_file ──────────────────────────────────────────────
 *
 * The first tool that MUTATES. Everything above is read-only, so this is
 * where the risk argument actually starts, and the containment is
 * deliberate rather than incidental:
 *
 *   - confined to the project root. An absolute path outside it, or a ".."
 *     that escapes it, is refused. There is no flag to widen this.
 *   - denied paths, listed explicitly and reviewable below. The important
 *     one is tools/horn_tools.json: that file IS the tool allowlist, so a
 *     model able to write it can add a tool that runs anything. Self-
 *     escalation has to be closed off or the allowlist is decorative.
 *   - atomic: written to a temp file and renamed, so an interrupted or
 *     failed write cannot leave a half-written source file behind.
 *   - size-capped, and it will not create directories on demand - a model
 *     cannot sprawl a tree by writing into paths it invented.
 *
 * Returns the number of bytes written. */

#define WRITE_HARD_CAP (1024 * 1024)

/* rename() that replaces an existing destination.
 *
 * POSIX rename() replaces atomically. MSVCRT's rename() - which is what
 * MinGW's <stdio.h> binds to on Windows - does NOT: it fails with EEXIST when
 * the destination is already there. So the plain rename(tmp, full) made
 * write_file and edit_file unable to overwrite ANY existing file: every
 * replacement, including editing a file the model itself had just created,
 * died with "failed to write". The whole point of the sibling-temp-then-swap
 * dance is that it replaces; on this platform it silently did not.
 *
 * MoveFileExW + MOVEFILE_REPLACE_EXISTING is the equivalent atomic replace.
 * Widened with CP_ACP rather than CP_UTF8 because the project root arrives
 * from the environment and the model supplies ASCII relative paths, and
 * CP_UTF8 would need winows.h, which does not exist. */
static int replace_file(const char *from, const char *to) {
#ifdef _WIN32
    wchar_t wf[PATH_BUF], wt[PATH_BUF];
    if (MultiByteToWideChar(CP_ACP, 0, from, -1, wf, PATH_BUF) == 0) return -1;
    if (MultiByteToWideChar(CP_ACP, 0, to, -1, wt, PATH_BUF) == 0) return -1;
    return MoveFileExW(wf, wt, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
#else
    return rename(from, to);
#endif
}

/* Paths the model may never write. Keep this list tight and keep the
 * reasoning in the comment; it is a security boundary, not a preference. */
static const char *WRITE_DENY[] = {
    ".git",              /* history and index: corrupting it breaks every other tool */
    "chats",             /* the human's own conversation history */
    "pieces",            /* live runtime state the running harness is using */
    "system",            /* compiled binaries - a text write would just corrupt them */
    "ops/+x",            /* compiled op binaries, same reason */
    "tools/horn_tools.json", /* THE ALLOWLIST. Writing it = self-escalation. */
    NULL
};

static int path_denied(const char *root, const char *full) {
    for (int i = 0; WRITE_DENY[i]; i++) {
        char probe[PATH_BUF];
        snprintf(probe, sizeof(probe), "%s/%s", root, WRITE_DENY[i]);
        size_t pl = strlen(probe);
        if (strncmp(full, probe, pl) == 0 &&
            (full[pl] == '\0' || full[pl] == '/'))
            return 1;
    }
    /* Key-shaped files anywhere: *.pdl, *_api_key.txt, state/raw_*.txt.
     * Already committed to the repo once (2026-10-01); do not hand-write
     * another one. */
    const char *base = strrchr(full, '/');
    base = base ? base + 1 : full;
    if (strstr(base, "api_key") || strstr(base, "raw_") ||
        (strlen(base) > 4 && strcmp(base + strlen(base) - 4, ".pdl") == 0))
        return 1;
    return 0;
}

/* Case-insensitive prefix test, ASCII only. Windows path comparison is
 * case-insensitive, so a case-sensitive strncmp would refuse a legitimate
 * write whose spelling merely differed from PRISC_PROJECT_ROOT. This makes
 * the check match the filesystem's own rule instead of a stricter one the
 * platform does not have. ASCII fold rather than tolower() because tolower is
 * locale-dependent and would drag the C locale's opinion about which bytes
 * are letters into a security decision. */
static int path_prefix_ci(const char *s, const char *pfx) {
    while (*pfx) {
        char a = *s++, b = *pfx++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;                 /* also catches s ending early: '\0' != pfx byte */
    }
    return 1;
}

/* Resolve a model-supplied path for WRITING: must land inside the project
 * root. Unlike resolve_path (which is deliberately permissive so the model
 * can read anywhere), this one refuses to escape. Returns 0 on refusal. */
static int resolve_write_path(const char *in, char *out, size_t out_sz) {
    if (!in || !in[0]) return 0;
    if (in[0] == '/') return 0;               /* absolute paths are out */
#ifdef _WIN32
    /* ...and so are Windows ones, which the check above does not catch: a
     * drive-absolute "C:/Windows/Temp/x" does not begin with '/'. Without this
     * it was not an ESCAPE - it got joined onto the root and normalised into
     * "C:/<project>/C:/Windows/Temp/x", which then failed the existence check
     * with a misleading "does not exist". The model was contained either way,
     * but told the wrong reason. Reject the drive form explicitly, including
     * the backslash spelling and the UNC-ish leading separator pair. */
    if ((in[0] && in[1] == ':' && isalpha((unsigned char)in[0])) ||
        (in[0] == '\\' && in[1] == '\\'))
        return 0;
#endif
    /* snprintf INTO the caller's buffer. This was asprintf(&out, ...),
     * which took the address of this function's own pointer parameter and
     * allocated somewhere else entirely - so `full` in the caller stayed
     * uninitialised, stat() failed, and write_file reported "does not
     * exist" for a file that plainly existed. Same mistake as the argv
     * use-after-free: writing to &a-parameter instead of *a-buffer. */
    snprintf(out, out_sz, "%s/%s", project_root, in);

    /* Normalise "." and ".." by hand - realpath() would resolve symlinks,
     * which we do NOT want: a symlink inside the project could point out of
     * it, and lexically-normalising the input is what actually bounds the
     * write to the path the model named. */
    char norm[PATH_BUF];
    size_t o = 0;
    norm[0] = '\0';
    const char *p = out;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t seglen = (size_t)(p - seg);
        if (seglen == 1 && seg[0] == '.') continue;
        if (seglen == 2 && seg[0] == '.' && seg[1] == '.') {
            while (o > 0 && norm[o - 1] != '/') o--;
            if (o > 0) o--;
            continue;
        }
        if (o + seglen + 2 >= sizeof(norm)) return 0;
        norm[o++] = '/';
        memcpy(norm + o, seg, seglen);
        o += seglen;
    }
    norm[o] = '\0';

    /* The normaliser above prefixes EVERY segment with '/', the first one
     * included. On Linux that accidentally agrees with project_root, which
     * already begins with '/'. On Windows project_root begins with a drive
     * letter ("C:/..."), so norm came out as "/C:/..." and the prefix test
     * below never matched: every single write_file and edit_file call was
     * refused as an escape, including paths plainly inside the project. The
     * model could not write anything at all. Drop the leading separator when
     * the root has none - verified by e2e, which had 8 write assertions
     * failing on this alone. */
    const char *cand = norm;
    if (cand[0] == '/' && project_root[0] != '/') cand++;

    size_t rl = strlen(project_root);
    if (!path_prefix_ci(cand, project_root)) return 0;
    if (cand[rl] != '/' && cand[rl] != '\0') return 0;
    if (cand[rl] == '\0') return 0;            /* the root itself */

    snprintf(out, out_sz, "%s", cand);
    return 1;
}

static int tool_write_file(const char *args, char *out, size_t out_sz) {
    char rel[PATH_BUF];
    if (!json_get_str(args, "path", rel, sizeof(rel)) || !rel[0]) {
        snprintf(out, out_sz, "error: write_file needs a \"path\" relative to the project root");
        return 1;
    }
    /* Content can be up to a megabyte, so it cannot go on the stack. */
    static char big[WRITE_HARD_CAP + 1];
    if (!json_get_str(args, "content", big, sizeof(big))) {
        snprintf(out, out_sz, "error: write_file needs a \"content\" string");
        return 1;
    }
    const char *body = big;

    char full[PATH_BUF];
    if (!resolve_write_path(rel, full, sizeof(full))) {
        snprintf(out, out_sz,
                 "error: '%s' is outside the project root or escapes it via '..'. "
                 "write_file takes a path relative to the project root.", rel);
        return 1;
    }
    if (path_denied(project_root, full)) {
        snprintf(out, out_sz,
                 "error: '%s' is a protected path and cannot be written by the model. "
                 "Protected: .git, chats, pieces, system, ops/+x, "
                 "tools/horn_tools.json (the tool allowlist), and key-shaped files.",
                 rel);
        return 1;
    }

    struct stat st;
    if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(out, out_sz, "error: '%s' is a directory", rel);
        return 1;
    }
    /* No mkdir -p: a model that can invent directory trees can sprawl one. */
    if (stat(full, &st) != 0) {
        snprintf(out, out_sz,
                 "error: '%s' does not exist. write_file will not create new "
                 "directories - create it yourself if it is really needed.", rel);
        return 1;
    }

    size_t len = strlen(body);
    if (len > WRITE_HARD_CAP) {
        snprintf(out, out_sz,
                 "error: content is %zu bytes, over the %d byte limit", len, WRITE_HARD_CAP);
        return 1;
    }

    char *tmp = NULL;
    if (asprintf(&tmp, "%s.horn-tmp", full) < 0 || !tmp) {
        snprintf(out, out_sz, "error: out of memory");
        return 1;
    }
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        snprintf(out, out_sz, "error: cannot write '%s'", rel);
        free(tmp);
        return 1;
    }
    size_t w = fwrite(body, 1, len, f);
    /* fsync before rename: a rename that lands before the data does gives
     * you a valid-looking empty file, which is worse than a failed write. */
    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (w != len || replace_file(tmp, full) != 0) {
        unlink(tmp);
        snprintf(out, out_sz, "error: failed to write '%s'", rel);
        free(tmp);
        return 1;
    }
    free(tmp);

    snprintf(out, out_sz, "wrote %zu bytes to %s", len, rel);
    return 0;
}


/* ── builtin: edit_file ───────────────────────────────────────────────
 *
 * gem-dev's semantics (ops/src/edit_file.c there): replace an exact string,
 * not rewrite the file. That is a materially safer primitive than
 * write_file and it is the house's own choice - a model holding a full
 * overwrite can discard content it never meant to touch, and nothing in
 * the diff would make that obvious at the call site.
 *
 * Refuses on: no match, or more than one match without replace_all. Both
 * mean the edit was not the one the model intended, which is the case worth
 * stopping for. */

#define EDIT_MAX_FILE (1024 * 1024)

static int tool_edit_file(const char *args, char *out, size_t out_sz) {
    char rel[PATH_BUF], needle[8192], repl[8192];
    if (!json_get_str(args, "path", rel, sizeof(rel)) || !rel[0]) {
        snprintf(out, out_sz, "error: edit_file needs a \"path\"");
        return 1;
    }
    if (!json_get_str(args, "search", needle, sizeof(needle))) {
        snprintf(out, out_sz, "error: edit_file needs a \"search\" string");
        return 1;
    }
    if (!json_get_str(args, "replace", repl, sizeof(repl))) {
        snprintf(out, out_sz, "error: edit_file needs a \"replace\" string");
        return 1;
    }
    int all = json_get_int(args, "replace_all", 0, 1);

    char full[PATH_BUF];
    if (!resolve_write_path(rel, full, sizeof(full))) {
        snprintf(out, out_sz,
                 "error: '%s' is outside the project root or escapes it via '..'.", rel);
        return 1;
    }
    if (path_denied(project_root, full)) {
        snprintf(out, out_sz, "error: '%s' is a protected path and cannot be edited.", rel);
        return 1;
    }

    struct stat st;
    if (stat(full, &st) != 0) {
        snprintf(out, out_sz,
                 "error: '%s' does not exist - use write_file to create it.", rel);
        return 1;
    }
    if ((size_t)st.st_size > EDIT_MAX_FILE) {
        snprintf(out, out_sz, "error: '%s' is %lld bytes, over the edit limit", rel, (long long)st.st_size);
        return 1;
    }

    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) { snprintf(out, out_sz, "error: out of memory"); return 1; }
    FILE *f = fopen(full, "rb");
    if (!f) { free(buf); snprintf(out, out_sz, "error: cannot read '%s'", rel); return 1; }
    size_t got = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    buf[got] = '\0';

    if (!needle[0]) { free(buf); snprintf(out, out_sz, "error: \"search\" is empty"); return 1; }

    int hits = 0;
    for (char *q = buf; (q = strstr(q, needle)) != NULL; q += strlen(needle)) hits++;

    if (hits == 0) {
        free(buf);
        snprintf(out, out_sz, "error: the search text was not found in %s - nothing changed", rel);
        return 1;
    }
    if (hits > 1 && !all) {
        free(buf);
        snprintf(out, out_sz,
                 "error: the search text appears %d times in %s. Refusing to guess "
                 "which one you meant - add more surrounding context to make it "
                 "unique, or set replace_all.", hits, rel);
        return 1;
    }

    /* Rebuild rather than edit in place: the replacement may be a different
     * length, and the file is written atomically anyway. */
    size_t nlen = strlen(needle), rlen = strlen(repl);
    size_t worst = got + hits * (rlen + 64) + 64;
    char *outbuf = malloc(worst);
    if (!outbuf) { free(buf); snprintf(out, out_sz, "error: out of memory"); return 1; }

    /* One pass: copy up to each match, then the replacement. */
    size_t o = 0, replaced = 0;
    char *q = buf;
    for (;;) {
        char *m = strstr(q, needle);
        if (!m) {
            size_t rest = strlen(q);
            memcpy(outbuf + o, q, rest);
            o += rest;
            break;
        }
        size_t chunk = (size_t)(m - q);
        memcpy(outbuf + o, q, chunk);
        o += chunk;
        memcpy(outbuf + o, repl, rlen);
        o += rlen;
        replaced++;
        q = m + nlen;
    }
    outbuf[o] = '\0';
    free(buf);

    char *tmp = NULL;
    if (asprintf(&tmp, "%s.horn-tmp", full) < 0 || !tmp) {
        free(outbuf); snprintf(out, out_sz, "error: out of memory"); return 1;
    }
    FILE *tf = fopen(tmp, "wb");
    if (!tf) { free(outbuf); free(tmp); snprintf(out, out_sz, "error: cannot write '%s'", rel); return 1; }
    fwrite(outbuf, 1, o, tf);
    fflush(tf);
    fsync(fileno(tf));
    fclose(tf);
    if (replace_file(tmp, full) != 0) {
        unlink(tmp); free(outbuf); free(tmp);
        snprintf(out, out_sz, "error: failed to write '%s'", rel);
        return 1;
    }
    free(outbuf);
    free(tmp);

    snprintf(out, out_sz, "edited %s: %d replacement%s of %zu bytes -> %zu bytes",
             rel, (int)replaced, replaced == 1 ? "" : "s", got, o);
    return 0;
}

/* ── builtin: run_script ─────────────────────────────────────────────
 *
 * gem-dev's cmd_exec, plus a real sandbox. gem-dev gates on a y/n prompt
 * and a "../"-count sandbox_depth check in config/context.txt; the
 * approval half is done by horn_turn (see the "gate" list in the
 * manifest) because this file must stay free of UI policy.
 *
 * The sandbox is the part gem-dev does not have, and it is the part that
 * matters now that a model can write. bwrap gives us, verified live on
 * this box:
 *   --unshare-net      no network at all, so a read-only tool cannot
 *                      become an exfiltration channel and no command can
 *                      fetch a payload to execute
 *   --ro-bind / /      the whole filesystem read-only
 *   --bind <root>      ...except the project, which stays writable so
 *                      run_script and write_file can compose
 *   --tmpfs /home /root
 *                      the user's home is EMPTY: no .ssh, no .aws, no
 *                      shell rc files, and crucially no reachable copy of
 *                      the API keys that live under &.widgits/open-hai
 *   --dev /dev --proc /proc
 *                      minimal devices
 * If bwrap is missing, refuse rather than fall back to an unsandboxed
 * shell - silently dropping the sandbox would be the worst outcome here. */

#define SCRIPT_TIMEOUT_DEFAULT 60
#define SCRIPT_TIMEOUT_MAX 300

static int bwrap_available(void) {
#ifdef HT_WIN32
    return 1;   /* the Job Object path needs no external helper */
#else
    return access("/usr/bin/bwrap", X_OK) == 0;
#endif
}

/* Whole-file reader for the small config files only - this file's other
 * reads are bounded and size-checked. */
static char *read_small_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = malloc(8192);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, 8191, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static int tool_run_script(const char *args, char *out, size_t out_sz) {
    char cmd[16384];
    if (!json_get_str(args, "command", cmd, sizeof(cmd)) || !cmd[0]) {
        snprintf(out, out_sz, "error: run_script needs a \"command\"");
        return 1;
    }
    int tmo = json_get_int(args, "timeout_s", SCRIPT_TIMEOUT_DEFAULT, SCRIPT_TIMEOUT_MAX);

    /* gem-dev's sandbox_depth check: at most N "../" sequences in a command.
     * Kept because it is cheap and it is the house's own rule, even though
     * bwrap is doing the real containment. */
    int max_depth = 1;
    {
        char *cfg = NULL;
        if (asprintf(&cfg, "%s/config/context.txt", project_root) >= 0 && cfg) {
            char *cb = read_small_file(cfg);
            if (cb) {
                char *p = strstr(cb, "sandbox_depth=");
                if (p) max_depth = atoi(p + strlen("sandbox_depth="));
                free(cb);
            }
            free(cfg);
        }
    }
    int ups = 0;
    for (const char *p = cmd; (p = strstr(p, "../")) != NULL; p += 3) ups++;
    if (ups > max_depth) {
        snprintf(out, out_sz,
                 "error: command has %d '../' sequences, over sandbox_depth=%d "
                 "(gem-dev's rule). Use absolute or project-relative paths.", ups, max_depth);
        return 1;
    }

    if (!bwrap_available()) {
        snprintf(out, out_sz,
                 "error: bwrap (bubblewrap) is not available, so run_script refuses "
                 "to execute. Running unsandboxed would give the model an "
                 "unrestricted shell with your API keys readable.");
        return 1;
    }

    /* Command goes in via argv to /bin/sh -c, never interpolated into a
     * system() string: the command is model-authored, and building a shell
     * string around it is how injection becomes trivial. */
    /* Mount layout, verified live:
     *   --ro-bind / /           whole filesystem read-only
     *   --bind root root        ...except the project, writable, so
     *                           run_script can build and write_file can
     *                           land changes
     *   --tmpfs <house>/&.widgits   BLANK. This is where every provider
     *                           key lives, and it is a SIBLING of the
     *                           project, not inside it. The first
     *                           attempt blanked all of /home instead,
     *                           which also erased the project (the tree
     *                           lives under /home/no/Desktop/...) and
     *                           bwrap failed with "Can't chdir to ...".
     *   --tmpfs /root /tmp      no shell rc files, and scratch space
     *                           that cannot outlive the command
     *   --unshare-net           no network: stops a read tool becoming an
     *                           exfiltration channel and stops any
     *                           command fetching a payload to run
     */
    /* Fixed buffer, not a heap pointer: this value lives in argv until
     * execv, and the first version asprintf'd it and then free()d it while
     * argv still referenced it. bwrap then received freed memory as the
     * tmpfs target, mis-parsed every following argument, and tried to exec
     * the project path as the command. A use-after-free that surfaced as a
     * baffling "execvp <project path>" - worth a fixed buffer forever. */
    char secrets[PATH_BUF];
    secrets[0] = '\0';
    {
        char house[MAX_PATH];
        snprintf(house, sizeof(house), "%s", project_root);
        char *slash = strrchr(house, '/');
        if (slash && slash != house) *slash = '\0';
        snprintf(secrets, sizeof(secrets), "%s/&.widgits", house);
    }

#ifdef HT_WIN32
    /* ---------------------------------------------------------------------
     * THE WINDOWS SANDBOX - AND AN HONEST STATEMENT OF WHAT IT IS NOT
     *
     * bwrap gives namespace isolation. Windows has no equivalent primitive, so
     * this is a genuinely weaker sandbox and pretending otherwise would be the
     * worst outcome here, because the whole point of this gate is that a model
     * may now write. What is actually enforced:
     *
     *   ENFORCED  - a Job Object, so the command and everything it spawns die
     *               together, cannot break away, and cannot outlive this op.
     *               This replaces bwrap --new-session + killpg, and is the one
     *               containment guarantee that maps cleanly.
     *   ENFORCED  - the command runs with the working directory pinned to the
     *               project, so "../" cannot walk out of it.
     *   ENFORCED  - PATH is replaced with a minimal system list before exec, so
     *               a command cannot pick up something out of the project tree.
     *
     *   NOT ENFORCED - read-only filesystem. bwrap's `--ro-bind / /` has no
     *               Windows counterpart without a container or AppContainer.
     *               The command can write anywhere the user can.
     *   NOT ENFORCED - no network. `--unshare-net` has no per-process Windows
     *               equivalent. A command CAN reach the network, so a read tool
     *               remains a theoretical exfiltration channel.
     *   NOT ENFORCED - the tmpfs blanking of the secrets directory.
     *
     * The secrets directory is therefore NOT hidden. That is the single most
     * important difference and the reason the refusal below is a refusal rather
     * than a fallback: a model-authored command on Windows can read
     * &.widgits/API_KEYS, whereas on Linux it cannot.
     *
     * Rather than silently accept that, run_script refuses on Windows unless
     * the operator has made a deliberate choice, via HORN_WIN_SANDBOX:
     *
     *   (unset)  refuse. The safe default, and what ships.
     *   =ack     run anyway, with the limits above stated in the transcript.
     *
     * Anyone setting it has accepted the gap knowingly. That is a decision to
     * make with eyes open, which is what the Linux branch's "refuse if bwrap is
     * missing" was already doing - it just refused for a different reason.
     */
    {
        const char *ack = getenv("HORN_WIN_SANDBOX");
        if (!ack || strcmp(ack, "ack") != 0) {
            snprintf(out, out_sz,
                     "error: run_script refuses to execute on Windows without an "
                     "explicit sandbox acknowledgement.\n"
                     "\n"
                     "Windows has no equivalent of bubblewrap's namespace "
                     "isolation. This build enforces process containment (a Job "
                     "Object, so the command and its children all die together) "
                     "and a pinned working directory, but it CANNOT enforce:\n"
                     "  - a read-only filesystem\n"
                     "  - no network access\n"
                     "  - hiding the secrets directory (&.widgits), which holds "
                     "provider keys\n"
                     "\n"
                     "On Linux those three are what stop a model-authored command "
                     "from reading your API keys. Here it can. Running anyway is "
                     "therefore a deliberate choice, not a default.\n"
                     "\n"
                     "To accept that risk, set HORN_WIN_SANDBOX=ack. The command "
                     "and its output are recorded either way.");
            return 1;
        }
    }

    /* Minimal, system-only PATH. The project tree is on the caller's PATH for
     * good local reasons, but a model-authored command should not be able to
     * resolve a tool out of a directory it can also write to. */
    SetEnvironmentVariableW(L"PATH", L"C:\\Windows\\System32;C:\\Windows;C:\\Windows\\System32\\Wbem");

    char comspec[MAX_PATH * 2];
    if (GetEnvironmentVariableA("ComSpec", comspec, sizeof(comspec)) == 0)
        snprintf(comspec, sizeof(comspec), "C:\\Windows\\System32\\cmd.exe");
    for (char *q = comspec; *q; q++) if (*q == '/') *q = '\\';

    /* Build the command line by hand. The command is model-authored, so it goes
     * to the shell as a SINGLE argument - exactly the reason the POSIX branch
     * passes it to /bin/sh -c via argv rather than through system(). A
     * CreateProcess command line is one flat string, so the quoting below is the
     * security boundary and is written to be boring rather than clever. */
    char cmdline[PATH_BUF * 2];
    {
        size_t k = 0;
        /* lpCommandLine must be writable, hence the buffer. */
        cmdline[k++] = '"';
        for (const char *q = comspec; *q && k < sizeof(cmdline) - 8; q++) cmdline[k++] = *q;
        cmdline[k++] = '"';
        cmdline[k++] = ' ';
        cmdline[k++] = '/';
        cmdline[k++] = 'c';
        cmdline[k++] = ' ';
        /* Escape the command for cmd's parser: a command containing a quote
         * would otherwise close the argument early and let the rest run as a
         * separate command. Doubling the quote character is cmd's own escape. */
        for (const char *q = cmd; *q && k < sizeof(cmdline) - 2; q++) {
            if (*q == '"') cmdline[k++] = '"';
            cmdline[k++] = *q;
        }
        cmdline[k] = '\0';
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    HANDLE tmp;
    if (!CreatePipe(&rd, &tmp, &sa, 0)) {
        snprintf(out, out_sz, "error: cannot create pipe");
        return 1;
    }
    /* The write end stays inheritable so the child actually receives it.
     * Clearing HANDLE_FLAG_INHERIT on it would make the command run and exit 0
     * while discarding every byte it printed - verified by probe. Only the read
     * end is made non-inheritable; the parent's copy of the write end is closed
     * after CreateProcess, which is what lets the read side see EOF. */
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    wr = tmp;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    /* --- THE JOB OBJECT. This is the actual containment. --- */
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (!job) {
        CloseHandle(rd); CloseHandle(wr);
        snprintf(out, out_sz, "error: cannot create Job Object (win32 error %lu) - refusing to run uncontained",
                 (unsigned long)GetLastError());
        return 1;
    }
    {
        /* KILL_ON_JOB_CLOSE is the equivalent of bwrap --die-with-parent: if this
         * op dies, so does the command and every descendant. WITHOUT it a
         * command that outlives the timeout would keep running with the user's
         * privileges, which is the failure mode the sandbox exists to prevent. */
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jl;
        ZeroMemory(&jl, sizeof(jl));
        jl.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jl, sizeof(jl))) {
            CloseHandle(job); CloseHandle(rd); CloseHandle(wr);
            snprintf(out, out_sz, "error: cannot arm Job Object (win32 error %lu) - refusing to run uncontained",
                     (unsigned long)GetLastError());
            return 1;
        }
    }

    wchar_t wexe[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, comspec, -1, wexe, MAX_PATH * 2) == 0) {
        CloseHandle(job); CloseHandle(rd); CloseHandle(wr);
        snprintf(out, out_sz, "error: cannot widen shell path");
        return 1;
    }
    wchar_t wcmd[PATH_BUF * 4];
    if (MultiByteToWideChar(CP_UTF8, 0, cmdline, -1, wcmd, PATH_BUF * 4) == 0) {
        CloseHandle(job); CloseHandle(rd); CloseHandle(wr);
        snprintf(out, out_sz, "error: cannot widen command line");
        return 1;
    }
    /* CreateProcessW takes lpCurrentDirectory as a wide string; project_root is
     * narrow. */
    wchar_t wroot[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, project_root, -1, wroot, MAX_PATH * 2) == 0) {
        CloseHandle(job); CloseHandle(rd); CloseHandle(wr);
        snprintf(out, out_sz, "error: cannot widen project root");
        return 1;
    }

    /* CREATE_NO_WINDOW alone here - no DETACHED_PROCESS, which is invalid in
     * combination and is what made the horn_turn detach fail with error 87. */
    BOOL ok = CreateProcessW(wexe, wcmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                             NULL, wroot, &si, &pi);
    CloseHandle(wr);
    if (!ok) {
        DWORD e = GetLastError();
        CloseHandle(job); CloseHandle(rd);
        snprintf(out, out_sz, "error: cannot start command (win32 error %lu)", (unsigned long)e);
        return 1;
    }
    /* Assign AFTER creation: a process started suspended cannot spawn children
     * before it is in the job, which would otherwise be an escape. */
    if (!AssignProcessToJobObject(job, pi.hProcess)) {
        DWORD e = GetLastError();
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        CloseHandle(job); CloseHandle(rd);
        snprintf(out, out_sz, "error: cannot contain command in a Job Object (win32 error %lu)",
                 (unsigned long)e);
        return 1;
    }

    /* Wait AND drain together, because neither alone works.
     *
     * Draining to EOF first deadlocks on any command that fills the pipe buffer
     * while the parent waits - the reason the POSIX branch reads first. But
     * draining first ALSO means a long-running command blocks here and the
     * timeout below never gets a chance to fire: a 30s ping with timeout_s=3
     * ran the full 30s, because the read did not return until the child exited.
     * Both orderings are wrong in different cases.
     *
     * PeekNamedPipe reports how much is buffered WITHOUT blocking, so this loop
     * drains what is available, polls the process, and only stops when the child
     * has exited AND the pipe has nothing left. That gets the deadlock
     * protection and a working timeout at the same time.
     */
    char *buf = malloc(out_sz);
    if (!buf) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        CloseHandle(job); CloseHandle(rd);
        return 1;
    }
    buf[0] = '\0';
    size_t got = 0;
    int timed_out = 0;
    {
        DWORD start = GetTickCount();
        for (;;) {
            /* Drain whatever is buffered right now. */
            for (;;) {
                DWORD avail = 0;
                if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL)) break;
                if (avail == 0) break;
                DWORD room = (DWORD)(out_sz - 1 - got);
                if (room > avail) room = avail;
                DWORD chunk = 0;
                if (!ReadFile(rd, buf + got, room, &chunk, NULL) || chunk == 0) break;
                got += chunk;
                buf[got] = '\0';
            }
            if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
                /* One final drain: the child may have written just before exit. */
                for (;;) {
                    DWORD avail = 0;
                    if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) || avail == 0) break;
                    DWORD room = (DWORD)(out_sz - 1 - got);
                    if (room > avail) room = avail;
                    DWORD chunk = 0;
                    if (!ReadFile(rd, buf + got, room, &chunk, NULL) || chunk == 0) break;
                    got += chunk;
                    buf[got] = '\0';
                }
                break;
            }
            if ((GetTickCount() - start) / 1000 >= (DWORD)tmo) {
                timed_out = 1;
                /* TerminateJobObject kills every descendant, which is what one
                 * killpg did on POSIX. KILL_ON_JOB_CLOSE would also do it. */
                TerminateJobObject(job, 1);
                WaitForSingleObject(pi.hProcess, 5000);
                break;
            }
            Sleep(50);
        }
    }
    buf[got] = '\0';
    CloseHandle(rd);
    {
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(job);
        if (timed_out) {
            size_t o = strlen(buf);
            appendf(buf, out_sz, o, "\n[terminated after %d s - job object killed the tree]", tmo);
            got = strlen(buf);
        }
        snprintf(out, out_sz, "%s\n[exit %d]", buf, (int)code);
    }
    free(buf);
    return 0;
#else
    char *argv[32];
    int argc = 0;
    argv[argc++] = "/usr/bin/bwrap";
    /* --ro-bind takes SOURCE and DEST: two arguments, not one. Pushing a
     * single "/" made bwrap swallow the next flag as its destination, shift
     * every following argument by one, and finally try to exec the PROJECT
     * ROOT as the command - reported as the baffling
     *   bwrap: execvp /home/.../^.hai-horn: No such file or directory
     * A shell bisect of the same flags passed the whole time, because the
     * shell version had both slashes. Found by strace, not by reading. */
    argv[argc++] = "--ro-bind";  argv[argc++] = "/";
    argv[argc++] = "/";
    argv[argc++] = "--bind";     argv[argc++] = project_root;
    argv[argc++] = project_root;
    if (secrets[0]) { argv[argc++] = "--tmpfs"; argv[argc++] = secrets; }
    argv[argc++] = "--tmpfs";    argv[argc++] = "/root";
    argv[argc++] = "--tmpfs";    argv[argc++] = "/tmp";
    argv[argc++] = "--dev";      argv[argc++] = "/dev";
    argv[argc++] = "--proc";     argv[argc++] = "/proc";
    argv[argc++] = "--unshare-net";
    argv[argc++] = "--die-with-parent";
    argv[argc++] = "--new-session";
    argv[argc++] = "--chdir";    argv[argc++] = project_root;
    argv[argc++] = "/bin/sh";
    argv[argc++] = "-c";
    argv[argc++] = cmd;
    argv[argc] = NULL;

    if (getenv("HORN_DEBUG_ARGV")) {
        for (int i = 0; i < argc; i++)
            fprintf(stderr, "argv[%d]=%s\n", i, argv[i]);
    }

    int fds[2];
    if (pipe(fds) != 0) { snprintf(out, out_sz, "error: cannot create pipe"); return 1; }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]); close(fds[1]);
        snprintf(out, out_sz, "error: cannot fork");
        return 1;
    }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        execv(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);

    /* Drain the pipe BEFORE waiting: a chatty command fills the 64K buffer
     * and deadlocks if the parent waits first. gem-dev's POSIX branch does
     * wait-then-read and has exactly this bug, and its single read()
     * silently truncates anything longer than one buffer. Read to EOF. */
    char *buf = malloc(out_sz);
    if (!buf) { close(fds[0]); waitpid(pid, NULL, 0); return 1; }
    size_t got = 0;
    ssize_t r;
    while (got < out_sz - 1 && (r = read(fds[0], buf + got, out_sz - 1 - got)) > 0)
        got += (size_t)r;
    buf[got] = '\0';
    close(fds[0]);

    /* Kill the whole process group on timeout - bwrap --new-session puts the
     * child in its own group, so one killpg reaches everything it spawned. */
    int st = 0;
    for (int waited = 0; waited < tmo; waited++) {
        pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) goto done;
        if (waited == tmo - 1) {
            killpg(pid, SIGKILL);
            waitpid(pid, &st, 0);
            size_t o = strlen(buf);
            o = appendf(buf, out_sz, o, "\n[terminated after %d s]", tmo);
            got = strlen(buf);
            goto done;
        }
        sleep(1);
    }
done:;
    snprintf(out, out_sz, "%s\n[exit %d]", buf,
             WIFEXITED(st) ? WEXITSTATUS(st) : (WIFSIGNALED(st) ? -WTERMSIG(st) : -1));
    free(buf);
    return 0;
#endif /* HT_WIN32 */
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
    } else if (strcmp(mapped, "builtin:write_file") == 0) {
        tool_write_file(args, result, RESULT_CAP);
    } else if (strcmp(mapped, "builtin:edit_file") == 0) {
        tool_edit_file(args, result, RESULT_CAP);
    } else if (strcmp(mapped, "builtin:run_script") == 0) {
        tool_run_script(args, result, RESULT_CAP);
    } else {
        /* External op: dispatch it. fork/exec rather than system(), so the
         * JSON args reach argv untouched - system() would need quoting
         * that a model-supplied regex cannot be trusted with. */
        char *op = NULL;
        if (asprintf(&op, "%s/ops/+x/%s.+x", project_root, mapped) < 0 || !op) {
            free(result);
            return 3;
        }
#ifdef HT_WIN32
        {
            /* Same contract as the POSIX branch below: JSON args go straight to
             * argv, never through a shell, so a model-supplied regex cannot
             * break out of an argument. ht_run_capture owns the pipe, the
             * stdout redirect and the NUL stderr. */
            char *av[3];
            av[0] = (char *)mapped;
            av[1] = (char *)args;
            av[2] = NULL;
            int rc = -1, code = -1;
            if (ht_run_capture(op, av, result, RESULT_CAP, &rc) == 0 && rc == 0) {
                if (result[0] == '\0')
                    snprintf(result, RESULT_CAP,
                             "error: tool '%s' (op %s) produced no output (exit %d)",
                             name, mapped, code);
                free(op);
                return 0;
            }
            snprintf(result, RESULT_CAP,
                     "error: tool '%s' (op %s) failed to run (win32 error %d)",
                     name, mapped, rc);
            free(op);
            return 1;
        }
#else
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
#endif /* HT_WIN32 */
    }

    fputs(result, stdout);
    free(result);
    return 0;
}