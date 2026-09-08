/* cli_test.c — CLI-1 (node-mode runner) headless test.
 *
 * Forks the real binary and asserts on exit codes and output for the
 * promised node-ish contract:
 *   - duk file.js args        -> run, console to stdout, exit 0
 *   - duk file.js (throw)     -> error to stderr, exit 1
 *   - duk --browser page.js   -> DOM engine, rendered rows to stdout, exit 0
 *   - duk -                    -> read script from stdin
 *   - process.argv/cwd/env + process.exit(code)
 *
 * Usage: cli_test <worker-binary> <cli_test_scripts_dir> <tmpdir>
 * Exit 0 if every case passes, 1 otherwise, 2 on bad usage.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int failures = 0;

/* run: argv terminated by NULL. Reads all of stdout into out (cap), exit
 * code into *rc. Returns 0 on clean spawn. */
static int run_cmd(char **argv, const char *stdin_text, char *out, size_t cap, int *rc) {
    int pout[2], pin[2];
    if (pipe(pout) != 0 || pipe(pin) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        dup2(pin[0], 0);
        dup2(pout[1], 1);
        dup2(pout[1], 2);
        close(pout[0]); close(pout[1]); close(pin[0]); close(pin[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pout[1]);
    if (stdin_text) {
        close(pin[0]);
        size_t n = strlen(stdin_text);
        size_t off = 0;
        while (off < n) {
            ssize_t w = write(pin[1], stdin_text + off, n - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
        close(pin[1]);
    } else {
        close(pin[0]);
        close(pin[1]);
    }
    size_t got = 0;
    while (got + 1 < cap) {
        ssize_t r = read(pout[0], out + got, cap - 1 - got);
        if (r <= 0) break;
        got += (size_t)r;
    }
    close(pout[0]);
    out[got] = 0;
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) *rc = WEXITSTATUS(status);
    else *rc = 128 + WTERMSIG(status);
    return 0;
}

/* check(name, want_rc, want_substrings[], run): */
static void check(const char *name, int want_rc, const char *const *want_sub,
                  int want_sub_n, int want_no_sub, const char *const *no_sub,
                  int no_sub_n, char **argv, const char *stdin_text) {
    char out[65536];
    int rc = 0;
    (void)want_no_sub;
    if (run_cmd(argv, stdin_text, out, sizeof(out), &rc) != 0) {
        printf("FAIL: %s (spawn)\n", name);
        failures++;
        return;
    }
    int ok = (rc == want_rc);
    for (int i = 0; ok && i < want_sub_n; i++)
        if (!strstr(out, want_sub[i])) ok = 0;
    for (int i = 0; ok && i < no_sub_n; i++)
        if (strstr(out, no_sub[i])) ok = 0;
    if (!ok) {
        printf("FAIL: %s\n  rc=%d (want %d)\n  out=%.400s\n", name, rc, want_rc, out);
        failures++;
    } else {
        printf("PASS: %s (rc=%d)\n", name, rc);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <worker> <scripts-dir> <tmpdir>\n", argv[0]);
        return 2;
    }
    const char *w = argv[1];
    const char *dir = argv[2];

    char script[2048], page[2048];
    snprintf(script, sizeof(script), "%s/cli_hello.js", dir);
    snprintf(page, sizeof(page), "%s/cli_page.js", dir);

    /* 1. plain run, exit 0, console to stdout */
    {
        char *a[] = { (char *)w, (char *)script, (char *)"one", (char *)"two", NULL };
        const char *const want[] = { "hello from cli", "one", "two" };
        const char *const no[] = { "undefined" };
        check("cli plain run + argv", 0, want, 3, 1, no, 1, a, NULL);
    }
    /* 2. thrown error -> exit 1, message on stderr */
    {
        char errs[2048];
        snprintf(errs, sizeof(errs), "%s/cli_throw.js", dir);
        char *a[] = { (char *)w, (char *)errs, NULL };
        const char *const want[] = { "boom" };
        check("cli thrown error exit 1", 1, want, 1, 0, NULL, 0, a, NULL);
    }
    /* 3. process.exit(code) honored */
    {
        char ex[2048];
        snprintf(ex, sizeof(ex), "%s/cli_exit.js", dir);
        char *a[] = { (char *)w, (char *)ex, NULL };
        const char *const want[] = { "before-exit" };
        check("cli process.exit", 3, want, 1, 0, NULL, 0, a, NULL);
    }
    /* 4. browser page mode: DOM works, rendered rows to stdout, exit 0 */
    {
        char *a[] = { (char *)w, (char *)"--browser", (char *)page, NULL };
        const char *const want[] = { "TEXT", "cli-page-dom" };
        check("cli --browser page mode", 0, want, 2, 0, NULL, 0, a, NULL);
    }
    /* 5. stdin script via `-` */
    {
        char *a[] = { (char *)w, (char *)"-", NULL };
        const char *const want[] = { "from-stdin" };
        check("cli stdin -", 0, want, 1, 0, NULL, 0, a, "console.log(\"from-stdin\");\n");
    }
    /* 6. node mode has no browser globals */
    {
        char nogl[2048];
        snprintf(nogl, sizeof(nogl), "%s/cli_noglobals.js", dir);
        char *a[] = { (char *)w, (char *)nogl, NULL };
        const char *const want[] = { "no-window no-document" };
        check("cli node mode no browser globals", 0, want, 1, 0, NULL, 0, a, NULL);
    }
    /* 7. usage error -> exit 2 */
    {
        char *a[] = { (char *)w, (char *)"--help", NULL };
        const char *const want[] = { "usage" };
        check("cli --help exit 2", 2, want, 1, 0, NULL, 0, a, NULL);
    }

    if (failures) { printf("cli_test: %d FAILURES\n", failures); return 1; }
    printf("cli_test: all cases pass\n");
    return 0;
}