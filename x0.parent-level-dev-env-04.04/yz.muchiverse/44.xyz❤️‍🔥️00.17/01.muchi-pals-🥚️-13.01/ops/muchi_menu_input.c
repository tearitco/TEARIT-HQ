/* muchi_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever muchi-pals screen is currently showing, modeled directly on
 * pal-chain's own ops/chain_menu_input.c (real, live-verified precedent
 * for this whole family's href+${piece_methods} chtpm standard - see
 * xyzos-standards.txt sec.6/12/16/18). Screen SWITCHING (main<->faucet<->
 * store<->pets<->processes<->user) is a real chtpm <button href="...">,
 * handled entirely by chtpm_parser_pal.c - never this op's job (sec.18).
 * "Which screen is current" is derived fresh every call from
 * pieces/display/current_layout.txt, never separately tracked mutable
 * state, exactly like chain_menu_input.c's own get_current_piece_id().
 *
 * PETS SCREEN IS ONE FILE, NOT TWO (a real, considered choice, not an
 * oversight): xyzos-standards.txt sec.13 (variable-length submenus) says
 * a dynamic-length list/detail flow is an op-driven CONTENT SWAP within
 * one screen, and sec.18 forbids simulating a screen change via op-side
 * state instead of a real href. Both together rule out a separate
 * pet_detail.chtpm reached by some synthetic "GOTO" - so pets.chtpm
 * alone serves BOTH states, and THIS op regenerates
 * projects/muchi-pals/pieces/pets/piece.pdl's own METHOD rows every
 * call whenever the current screen is "pets": no pet selected -> one
 * SELECT_PET:<id> row per owned egg/pet (a real, if unusual, use of
 * piece.pdl as a GENERATED file rather than hand-authored data - every
 * other piece.pdl in this project is static); a pet selected -> the
 * real per-pet action rows (Feed/Clean/Sleep/Train/Export/Destroy) plus
 * a "Back to List" row that clears the selection without leaving the
 * screen. A separate, ALWAYS-present hardcoded href in pets.chtpm
 * itself ("Back to Main") is the one static escape hatch back to main
 * regardless of which of the two states is showing.
 *
 * ACTIVE-TARGET INDIRECTION (xyzos-standards.txt sec.21): per-pet real
 * pal ops (toggle_sleep.pal/clean_pet.pal/feed_pet.pal) can't receive
 * "which pet" as an argument - prisc+x's own main() has no argv-into-a-
 * launched-script mechanism. This op copies the selected pet's own
 * relevant state.txt key(s) into the fixed pieces/system/
 * active_target.txt before launching such a script, and copies the
 * mutated key(s) back into the pet's own REAL state.txt afterward -
 * this op is the ONLY thing that ever needs to know which real piece
 * "active_target.txt" currently aliases.
 *
 * Self-contained, no shared headers (matches every other op here).
 * Usage: muchi_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#define getpid _getpid
#else
#include <signal.h>
#endif

#define MAX_LINE 512
#define PROJ_MAX_PATH 4096
#define PATH_BUF (PROJ_MAX_PATH + 256)
#define MAX_MENU_ITEMS 32
#define MAX_PETS 64
#define FEED_COST 5
#define COIN_STAKE 10
#define EGG_COST 20

typedef struct {
    char label[160];
    char command[256];
} MenuItem;

static char project_root[PROJ_MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static void read_kv_str_local(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0'; /* CRLF-safe */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str_local(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (!found && strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static void append_ledger(const char *piece_id, const char *key, const char *value, const char *trigger) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] StateChange: %s %s %s | Trigger: %s\n", ts, piece_id, key, value, trigger);
    fclose(f);
}

/* ---- active-target indirection (xyzos-standards.txt sec.21.2) ---- */

static void active_target_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/system/active_target.txt", project_root);
}

static void pet_state_path(const char *pet_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);
}

static int copy_in_int(const char *pet_id, const char *key) {
    char src[PATH_BUF], dst[PATH_BUF];
    pet_state_path(pet_id, src, sizeof(src));
    active_target_path(dst, sizeof(dst));
    int val = read_kv_int(src, key, 0);
    write_kv_int(dst, key, val);
    return val;
}

static int copy_out_int(const char *pet_id, const char *key) {
    char src[PATH_BUF], dst[PATH_BUF];
    active_target_path(src, sizeof(src));
    pet_state_path(pet_id, dst, sizeof(dst));
    int val = read_kv_int(src, key, 0);
    write_kv_int(dst, key, val);
    return val;
}

static void active_target_clear(void) {
    char path[PATH_BUF];
    active_target_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

/* Runs a real pal-native op (pal/ops_native/<name>.pal) via
 * system/prisc+x, synchronously - same "dispatch, run to completion,
 * then compose" shape every other op call in this family already uses
 * (xyzos-standards.txt sec.0-CORRECTED). PRISC_PROJECT_ROOT is already in
 * this process's own environment (set by button.sh), inherited by the
 * child automatically - no need to re-pass it. */
static void run_pal_op(const char *pal_relpath) {
    char exe_path[PATH_BUF], script_path[PATH_BUF], cmd[PATH_BUF * 2 + 64];
    snprintf(exe_path, sizeof(exe_path), "%s/system/prisc+x", project_root);
    snprintf(script_path, sizeof(script_path), "%s/%s", project_root, pal_relpath);
    snprintf(cmd, sizeof(cmd), "'%s' '%s' >/dev/null 2>&1", exe_path, script_path);
    int rc = system(cmd);
    (void)rc;
}

/* ---- plain C-op invocation (unchanged real ops - generate_egg is
 * called FROM buy_egg.pal itself, not from here; train_pet/export_card/
 * destroy_card stay genuinely C, per PAL-VS-C-ARCHITECTURE.txt sec.4's
 * own "bad candidate" criteria - string/registry-heavy or image work) */

#ifdef _WIN32
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
    if (pf) fclose(pf);
    WaitForSingleObject(proc, INFINITE);
    CloseHandle(proc);
}
#endif

static void run_action_op(const char *op_name, const char *arg, char *msg_out, size_t msg_sz) {
    char exe_path[PATH_BUF];
    snprintf(exe_path, sizeof(exe_path), "%s/ops/+x/%s.+x", project_root, op_name);
    msg_out[0] = '\0';
#ifdef _WIN32
    HANDLE proc;
    FILE *pf = win_run_capture(exe_path, arg, &proc);
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    win_run_close(pf, proc);
#else
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' '%s'", exe_path, arg);
    FILE *pf = popen(cmd, "r");
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    pclose(pf);
#endif
    msg_out[strcspn(msg_out, "\r\n")] = '\0';
}

static void window_pid_path(const char *pet_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/world_01/map_lobby/%s/window.pid", project_root, pet_id);
}

static int is_pid_alive(long pid) {
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    int alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
#else
    return kill((pid_t)pid, 0) == 0;
#endif
}

static long read_window_pid(const char *pet_id) {
    char path[PATH_BUF];
    window_pid_path(pet_id, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long pid = 0;
    if (fscanf(f, "%ld", &pid) != 1) pid = 0;
    fclose(f);
    return pid;
}

static void write_window_pid(const char *pet_id, long pid) {
    char path[PATH_BUF];
    window_pid_path(pet_id, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", pid);
    fclose(f);
}

/* Ported verbatim in behavior from ops/menu_input.c's own
 * spawn_egg_window - see that file's own header comment for the full
 * "why not captured/waited on" rationale. */
/* egg_window is a DETACHED process, deliberately outliving this whole
 * session (self-ticking pets keep running after the menu quits - dox/
 * 01-architecture.md's own "Step 7"). button.sh's own session-isolation
 * (dox/03-session-isolation.md) runs this process under a THROWAWAY
 * PRISC_PROJECT_ROOT (a per-session directory deleted the instant this
 * session exits) - egg_window resolves that env var ONCE at its own
 * startup and uses it for its ENTIRE lifetime, so inheriting the
 * session-scoped value verbatim would leave every open pet window
 * unable to read/write its own state.txt the moment the menu closed.
 * PRISC_REAL_PROJECT_ROOT (set by button.sh alongside the session-scoped
 * PRISC_PROJECT_ROOT) is the stable, real project root - override the
 * env var to THAT, specifically for this one long-lived child, before
 * it execs. Falls back to project_root itself (this process's own,
 * possibly session-scoped root) if that var isn't set, so direct
 * invocation outside button.sh's session wrapper still works. */
static void spawn_egg_window(const char *pet_id, char *msg_out, size_t msg_sz) {
    if (is_pid_alive(read_window_pid(pet_id))) {
        snprintf(msg_out, msg_sz, "%s's window is already open.", pet_id);
        return;
    }

    const char *real_root = getenv("PRISC_REAL_PROJECT_ROOT");
    if (!real_root || !real_root[0]) real_root = project_root;

    char window_path[PATH_BUF];
#ifdef _WIN32
    snprintf(window_path, sizeof(window_path), "%s/system/egg_window.exe", project_root);
    char qexe[PATH_BUF], qarg[PATH_BUF], cmdline[PATH_BUF * 2];
    win_quote_arg(window_path, qexe, sizeof(qexe));
    win_quote_arg(pet_id, qarg, sizeof(qarg));
    snprintf(cmdline, sizeof(cmdline), "%s %s", qexe, qarg);

    /* CreateProcessA's own lpEnvironment=NULL inherits THIS process's
     * current environment block at call time (no fork(), so this is the
     * only lever available) - temporarily override PRISC_PROJECT_ROOT
     * around the call, then restore it, rather than hand-building a
     * custom Win32 environment block just for one variable. */
    char saved_root[PATH_BUF];
    const char *cur = getenv("PRISC_PROJECT_ROOT");
    snprintf(saved_root, sizeof(saved_root), "%s", cur ? cur : "");
    _putenv_s("PRISC_PROJECT_ROOT", real_root);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);

    _putenv_s("PRISC_PROJECT_ROOT", saved_root);

    if (ok) {
        write_window_pid(pet_id, (long)pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        snprintf(msg_out, msg_sz, "Opened %s in a new window.", pet_id);
    } else {
        snprintf(msg_out, msg_sz, "Could not open window for %s.", pet_id);
    }
#else
    snprintf(window_path, sizeof(window_path), "%s/system/egg_window", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        setenv("PRISC_PROJECT_ROOT", real_root, 1);
        execl(window_path, window_path, pet_id, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        write_window_pid(pet_id, (long)pid);
        snprintf(msg_out, msg_sz, "Opened %s in a new window.", pet_id);
    } else {
        snprintf(msg_out, msg_sz, "Could not open window for %s.", pet_id);
    }
#endif
}

/* ---- chtpm bridge (mirrors chain_menu_input.c's own shape exactly) ---- */

static void get_current_piece_id(char *out, size_t out_sz) {
    snprintf(out, out_sz, "main");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(layout_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        const char *slash = strrchr(line, '/');
        const char *base = slash ? slash + 1 : line;
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", base);
        char *dot = strstr(tmp, ".chtpm");
        if (dot) *dot = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
#pragma GCC diagnostic pop
    }
    fclose(f);
}

static void write_chtpm_bridge(const char *piece_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=muchi-pals\n");
        fprintf(f, "active_target_id=%s\n", piece_id);
        fclose(f);
    }
}

static int load_menu_items(const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/muchi-pals/pieces/%s/piece.pdl", project_root, piece_id);
#pragma GCC diagnostic pop
    FILE *f = fopen(pdl_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max_items && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *label = trim(p1 + 1);
        char *command = trim(p2 + 1);
        snprintf(items[n].label, sizeof(items[n].label), "%s", label);
        snprintf(items[n].command, sizeof(items[n].command), "%s", command);
        n++;
    }
    fclose(f);
    return n;
}

/* ---- pets screen: piece.pdl is GENERATED, not hand-authored (see this
 * file's own top-of-file comment for the full why - xyzos-standards.txt
 * sec.13/18) ---- */

static void pets_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/system/pets_screen_state.txt", project_root);
}

static void get_selected_pet(char *out, size_t out_sz) {
    char path[PATH_BUF];
    pets_state_path(path, sizeof(path));
    read_kv_str_local(path, "selected_pet", out, out_sz);
}

static void set_selected_pet(const char *pet_id) {
    char path[PATH_BUF];
    pets_state_path(path, sizeof(path));
    write_kv(path, "selected_pet", pet_id);
}

static void write_pets_pdl_header(FILE *pf) {
    fprintf(pf, "SECTION      | KEY                | VALUE\n");
    fprintf(pf, "----------------------------------------\n");
    fprintf(pf, "META         | piece_id           | pets\n");
    fprintf(pf, "META         | version            | 1.0\n\n");
}

static void regenerate_pets_piece_pdl(void) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/muchi-pals/pieces/pets/piece.pdl", project_root);
    FILE *pf = fopen(pdl_path, "w");
    if (!pf) return;
    write_pets_pdl_header(pf);

    char selected[64];
    get_selected_pet(selected, sizeof(selected));

    if (selected[0]) {
        char pet_state[PATH_BUF];
        pet_state_path(selected, pet_state, sizeof(pet_state));
        char species[64], emoji[32];
        read_kv_str_local(pet_state, "species_name", species, sizeof(species));
        read_kv_str_local(pet_state, "species_emoji", emoji, sizeof(emoji));
        int asleep = read_kv_int(pet_state, "asleep", 0);

        fprintf(pf, "METHOD       | Open Window                            | OPEN_WINDOW\n");
        fprintf(pf, "METHOD       | Feed (-5 tokens, -30 hunger)           | FEED_PET\n");
        fprintf(pf, "METHOD       | Clean                                  | CLEAN_PET\n");
        fprintf(pf, "METHOD       | %-38s | TOGGLE_SLEEP\n", asleep ? "Wake" : "Sleep");
        fprintf(pf, "METHOD       | Train (-20 energy)                     | TRAIN_PET\n");
        fprintf(pf, "METHOD       | Export Card                            | EXPORT_CARD\n");
        fprintf(pf, "METHOD       | Destroy Card                           | DESTROY_CARD\n");
        fprintf(pf, "METHOD       | Back to List                           | DESELECT_PET\n");
        (void)species; (void)emoji; /* available for a fuller title, kept minimal for now */
    } else {
        char inv_path[PATH_BUF];
        snprintf(inv_path, sizeof(inv_path), "%s/pieces/world_01/map_lobby/user_01/inventory.txt", project_root);
        FILE *inv = fopen(inv_path, "r");
        if (inv) {
            char pet_id[64];
            int row = 0;
            while (row < MAX_PETS && fgets(pet_id, sizeof(pet_id), inv)) {
                pet_id[strcspn(pet_id, "\r\n")] = '\0';
                if (!pet_id[0]) continue;
                char pet_state[PATH_BUF];
                pet_state_path(pet_id, pet_state, sizeof(pet_state));
                char species[64], emoji[32];
                read_kv_str_local(pet_state, "species_name", species, sizeof(species));
                read_kv_str_local(pet_state, "species_emoji", emoji, sizeof(emoji));
                int hatched = read_kv_int(pet_state, "hatched", 0);
                char label[160];
                /* emoji/pet_id/species are each bounded well under
                 * label's own 160 bytes by their own read_kv_str_local
                 * out_sz (32/64/64) - gcc can't see that invariant
                 * across the call boundary, same class of suppression
                 * already used elsewhere in this codebase (see
                 * ops/train_pet.c's own next_unlocked_skill()). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                if (hatched) {
                    int hp = read_kv_int(pet_state, "hp", 0);
                    int hp_max = read_kv_int(pet_state, "hp_max", 0);
                    snprintf(label, sizeof(label), "%s %-8s %-10s HP:%d/%d",
                             emoji[0] ? emoji : "?", pet_id, species[0] ? species : "?", hp, hp_max);
                } else {
                    snprintf(label, sizeof(label), "%s %-8s %-10s (unhatched)",
                             emoji[0] ? emoji : "?", pet_id, species[0] ? species : "?");
                }
#pragma GCC diagnostic pop
                fprintf(pf, "METHOD       | %-38s | SELECT_PET:%s\n", label, pet_id);
                row++;
            }
            fclose(inv);
        }
    }
    fclose(pf);
}

/* ---- main dispatch ---- */

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    int key = atoi(argv[1]);

    char active_piece[128];
    get_current_piece_id(active_piece, sizeof(active_piece));

    if (strcmp(active_piece, "pets") == 0) regenerate_pets_piece_pdl();

    if (key == 0) {
        /* idle-sync: re-derive the current screen and refresh the chtpm
         * bridge if it changed - a genuine no-op otherwise, matching
         * chain_menu_input.c's own key==0 branch exactly. */
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(active_piece, current_target) == 0) return 0;

        write_chtpm_bridge(active_piece);
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/muchi_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(active_piece, items, MAX_MENU_ITEMS);

    /* xyzos-standards.txt sec.16.2/16.2a: real chtpm_parser_pal.c's own
     * load_dynamic_methods() starts method_idx at 2 (confirmed by
     * direct read, system/chtpm_parser_pal.c ~line 1003 - unconditional
     * for any non-"loader" active_id, regardless of layout content), so
     * a piece.pdl's Nth METHOD row generates onClick="KEY:(N+1)".
     * resolved_item here is a 1-BASED row number (key value minus 1),
     * matching chain_menu_input.c's own real, live-verified convention
     * EXACTLY - including its own items[resolved_item - 1] access
     * below, not items[resolved_item] (a real bug this project hit
     * live: clicking row 1 dispatched row 2's command instead, traced
     * to exactly this missing second "-1"). */
    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    if (resolved_item < 1 || resolved_item > item_count) {
        write_chtpm_bridge(active_piece);
        return 0;
    }

    const char *cmd = items[resolved_item - 1].command;
    char msg[MAX_LINE] = "";
    char user_state[PATH_BUF];
    snprintf(user_state, sizeof(user_state), "%s/pieces/world_01/map_lobby/user_01/state.txt", project_root);
    char msg_path[PATH_BUF];
    snprintf(msg_path, sizeof(msg_path), "%s/pieces/system/last_message.txt", project_root);

    if (strcmp(cmd, "CLAIM_TOKENS") == 0) {
        run_pal_op("pal/ops_native/claim_tokens.pal");
        int tokens = read_kv_int(user_state, "tokens", 0);
        snprintf(msg, sizeof(msg), "Claimed 10 tokens! Balance: %d", tokens);

    } else if (strcmp(cmd, "COIN_FLIP") == 0) {
        int tokens = read_kv_int(user_state, "tokens", 0);
        if (tokens < COIN_STAKE) {
            snprintf(msg, sizeof(msg), "Coin flip failed: need %d tokens to stake, have %d.", COIN_STAKE, tokens);
        } else {
            /* Randomness generated HERE, in C, and handed to the pal
             * script through the same active_target.txt scratch file
             * every other value crosses this boundary through - no new
             * ecall syscall needed (xyzos-standards.txt sec.21.5, revised
             * after direct feedback that a plain C-generated value
             * handed through the existing file mechanism is simpler
             * than growing the VM). */
            srand((unsigned int)(time(NULL) ^ getpid()));
            int heads = rand() % 2;
            char at_path[PATH_BUF];
            active_target_path(at_path, sizeof(at_path));
            write_kv_int(at_path, "heads", heads);
            run_pal_op("pal/ops_native/coin_flip.pal");
            active_target_clear();
            tokens = read_kv_int(user_state, "tokens", 0);
            if (heads) snprintf(msg, sizeof(msg), "Heads! Won %d tokens. Balance: %d", COIN_STAKE, tokens);
            else snprintf(msg, sizeof(msg), "Tails. Lost %d tokens. Balance: %d", COIN_STAKE, tokens);
        }

    } else if (strcmp(cmd, "BUY_EGG") == 0) {
        int tokens = read_kv_int(user_state, "tokens", 0);
        if (tokens < EGG_COST) {
            snprintf(msg, sizeof(msg), "Buy failed: need %d tokens, have %d.", EGG_COST, tokens);
        } else {
            char serial_path[PATH_BUF];
            snprintf(serial_path, sizeof(serial_path), "%s/pieces/system/serial_counter.txt", project_root);
            FILE *sf = fopen(serial_path, "r");
            int serial_before = 0;
            if (sf) { if (fscanf(sf, "%d", &serial_before) != 1) serial_before = 0; fclose(sf); }
            run_pal_op("pal/ops_native/buy_egg.pal");
            tokens = read_kv_int(user_state, "tokens", 0);
            sf = fopen(serial_path, "r");
            int serial_after = serial_before;
            if (sf) { if (fscanf(sf, "%d", &serial_after) != 1) serial_after = serial_before; fclose(sf); }
            if (serial_after > serial_before) {
                snprintf(msg, sizeof(msg), "Bought egg_%d! Balance: %d", serial_after, tokens);
            } else {
                snprintf(msg, sizeof(msg), "Buy failed: mint did not complete.");
            }
        }

    } else if (strncmp(cmd, "SELECT_PET:", 11) == 0) {
        set_selected_pet(cmd + 11);
        regenerate_pets_piece_pdl();

    } else if (strcmp(cmd, "DESELECT_PET") == 0) {
        set_selected_pet("");
        regenerate_pets_piece_pdl();

    } else if (strcmp(cmd, "OPEN_WINDOW") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        if (selected[0]) spawn_egg_window(selected, msg, sizeof(msg));

    } else if (strcmp(cmd, "FEED_PET") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        int tokens = read_kv_int(user_state, "tokens", 0);
        if (!selected[0]) {
            /* no-op */
        } else if (tokens < FEED_COST) {
            snprintf(msg, sizeof(msg), "Feed failed: need %d tokens, have %d.", FEED_COST, tokens);
        } else {
            write_kv_int(user_state, "tokens", tokens - FEED_COST);
            copy_in_int(selected, "hunger");
            run_pal_op("pal/ops_native/feed_pet.pal");
            int new_hunger = copy_out_int(selected, "hunger");
            active_target_clear();
            char valbuf[16];
            snprintf(valbuf, sizeof(valbuf), "%d", new_hunger);
            append_ledger(selected, "hunger", valbuf, "feed_pet");
            snprintf(msg, sizeof(msg), "Fed %s. Hunger: %d. Balance: %d", selected, new_hunger, tokens - FEED_COST);
        }

    } else if (strcmp(cmd, "CLEAN_PET") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        if (selected[0]) {
            int poop_before = copy_in_int(selected, "poop_count");
            run_pal_op("pal/ops_native/clean_pet.pal");
            copy_out_int(selected, "poop_count");
            active_target_clear();
            append_ledger(selected, "poop_count", "0", "clean_pet");
            if (poop_before == 0) snprintf(msg, sizeof(msg), "%s was already clean.", selected);
            else snprintf(msg, sizeof(msg), "Cleaned up after %s (%d mess%s).", selected, poop_before, poop_before == 1 ? "" : "es");
        }

    } else if (strcmp(cmd, "TOGGLE_SLEEP") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        if (selected[0]) {
            copy_in_int(selected, "asleep");
            run_pal_op("pal/ops_native/toggle_sleep.pal");
            int asleep = copy_out_int(selected, "asleep");
            active_target_clear();
            append_ledger(selected, "asleep", asleep ? "1" : "0", "toggle_sleep");
            if (asleep) snprintf(msg, sizeof(msg), "%s is now asleep.", selected);
            else snprintf(msg, sizeof(msg), "%s woke up.", selected);
            regenerate_pets_piece_pdl(); /* Sleep/Wake row label must flip */
        }

    } else if (strcmp(cmd, "TRAIN_PET") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        if (selected[0]) run_action_op("train_pet", selected, msg, sizeof(msg));

    } else if (strcmp(cmd, "EXPORT_CARD") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        if (selected[0]) run_action_op("export_card", selected, msg, sizeof(msg));

    } else if (strcmp(cmd, "DESTROY_CARD") == 0) {
        char selected[64];
        get_selected_pet(selected, sizeof(selected));
        if (selected[0]) run_action_op("destroy_card", selected, msg, sizeof(msg));
    }

    if (msg[0]) write_kv(msg_path, "last_message", msg);
    write_chtpm_bridge(active_piece);
    return 0;
}
