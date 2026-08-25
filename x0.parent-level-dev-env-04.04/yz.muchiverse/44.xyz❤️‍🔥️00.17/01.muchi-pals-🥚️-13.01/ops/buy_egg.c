/* buy_egg - one verb, one binary, no shared headers.
 * The store's "buy" verb: checks/deducts EGG_COST tokens from an owner
 * piece, then shells out to the existing generate_egg.+x to actually
 * mint the egg - reuses that op's species-pick/serial/piece-creation
 * logic rather than duplicating it, matching the project's own
 * piece_manager-style "call the other op" convention.
 *
 * Usage: buy_egg.+x <owner_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#define MAX_LINE 512
#define PROJ_MAX_PATH 4096
#define PATH_BUF (PROJ_MAX_PATH + 256)
#define EGG_COST 20

static char project_root[PROJ_MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_write_tokens(const char *owner_id, int delta, int *out_balance) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, owner_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int tokens = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "tokens") == 0) tokens = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (tokens + delta < 0) { *out_balance = tokens; return 0; }
    tokens += delta;

    f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "tokens") == 0) { fprintf(f, "tokens=%d\n", tokens); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);

    *out_balance = tokens;
    return 1;
}

#ifdef _WIN32
/* Windows has no real argv array for CreateProcess - just one command
 * line string - so each arg has to be quoted with the documented MS CRT
 * algorithm or the child's own argv parsing splits it wrong (this matters
 * here specifically because project_root can contain spaces, e.g. a path
 * under "OneDrive\Desktop\...(1)"). */
static void win_quote_arg(const char *arg, char *out, size_t out_sz) {
    size_t len = 0;
    if (out_sz < 3) { out[0] = '\0'; return; }
    out[len++] = '"';
    for (const char *p = arg; *p && len < out_sz - 2; ) {
        size_t backslashes = 0;
        while (*p == '\\') { backslashes++; p++; }
        if (*p == '"' || *p == '\0') {
            for (size_t i = 0; i < backslashes * 2 && len < out_sz - 2; i++) out[len++] = '\\';
            if (*p == '"') { if (len < out_sz - 2) { out[len++] = '\\'; out[len++] = '"'; } p++; }
        } else {
            for (size_t i = 0; i < backslashes && len < out_sz - 2; i++) out[len++] = '\\';
            if (len < out_sz - 2) out[len++] = *p;
            p++;
        }
    }
    out[len++] = '"';
    out[len] = '\0';
}

/* popen()-equivalent that goes straight through CreateProcess instead of
 * cmd.exe - cmd.exe only understands double-quote quoting (this codebase's
 * popen() calls use single quotes, a POSIX shell convention cmd.exe
 * doesn't share), so this bypasses cmd.exe entirely. */
static FILE *win_run_capture(const char *exe_path, const char *arg1, HANDLE *out_proc) {
    char qexe[PATH_BUF], qarg1[PATH_BUF];
    char cmdline[PATH_BUF * 2];
    win_quote_arg(exe_path, qexe, sizeof(qexe));
    win_quote_arg(arg1, qarg1, sizeof(qarg1));
    snprintf(cmdline, sizeof(cmdline), "%s %s", qexe, qarg1);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    HANDLE read_pipe, write_pipe;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) return NULL;
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(write_pipe);
    if (!ok) { CloseHandle(read_pipe); return NULL; }
    CloseHandle(pi.hThread);
    *out_proc = pi.hProcess;

    int fd = _open_osfhandle((intptr_t)read_pipe, _O_RDONLY);
    if (fd == -1) { CloseHandle(read_pipe); CloseHandle(pi.hProcess); return NULL; }
    return _fdopen(fd, "r");
}

static void win_run_close(FILE *pf, HANDLE proc) {
    if (pf) fclose(pf); /* also closes the pipe's read handle/fd */
    WaitForSingleObject(proc, INFINITE);
    CloseHandle(proc);
}
#endif

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <owner_piece_id>\n", argv[0]);
        return 1;
    }
    const char *owner_id = argv[1];
    resolve_root();

    int balance = 0;
    if (!read_write_tokens(owner_id, -EGG_COST, &balance)) {
        printf("Buy failed: need %d tokens, have %d.\n", EGG_COST, balance);
        return 1;
    }

    char exe_path[PATH_BUF];
    snprintf(exe_path, sizeof(exe_path), "%s/ops/+x/generate_egg.+x", project_root);
    char egg_id[64] = "";
#ifdef _WIN32
    HANDLE proc;
    FILE *pf = win_run_capture(exe_path, owner_id, &proc);
    if (!pf) {
        printf("Buy failed: could not mint egg.\n");
        read_write_tokens(owner_id, EGG_COST, &balance);
        return 1;
    }
    if (!fgets(egg_id, sizeof(egg_id), pf)) egg_id[0] = '\0';
    win_run_close(pf, proc);
#else
    /* Wider than exe_path/owner_id's own PATH_BUF each, so gcc can prove
     * the quoted concatenation of both (plus quotes/space) can't truncate. */
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' '%s'", exe_path, owner_id);
    FILE *pf = popen(cmd, "r");
    if (!pf) {
        printf("Buy failed: could not mint egg.\n");
        /* Refund - the deduction above already happened. */
        read_write_tokens(owner_id, EGG_COST, &balance);
        return 1;
    }
    if (!fgets(egg_id, sizeof(egg_id), pf)) egg_id[0] = '\0';
    pclose(pf);
#endif
    egg_id[strcspn(egg_id, "\r\n")] = '\0'; /* CRLF-safe - Windows' CRT translates \n to \r\n on non-binary stdout */

    if (egg_id[0] == '\0') {
        printf("Buy failed: mint returned nothing.\n");
        read_write_tokens(owner_id, EGG_COST, &balance);
        return 1;
    }

    printf("Bought %s! Balance: %d\n", egg_id, balance);
    return 0;
}
