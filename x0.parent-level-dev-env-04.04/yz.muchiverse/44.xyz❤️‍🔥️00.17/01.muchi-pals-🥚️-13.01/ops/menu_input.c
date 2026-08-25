/* menu_input - one verb, one binary, no shared headers.
 * Interprets one raw keycode against pieces/system/menu_state.txt
 * (screen, cursor) - moves the cursor on up/down, and on enter, either
 * changes screen or performs an action by shelling out to the relevant
 * single-purpose op (claim_tokens/coin_flip/buy_egg/hatch_egg), capturing
 * its printed message into menu_state.txt's last_message field for
 * compose_menu to display. 'b' is a back-to-main shortcut from any
 * submenu. This op is the router; it does not itself mutate tokens or
 * mint/hatch eggs - those stay in their own ops per doctrine.
 *
 * Usage: menu_input.+x <keycode> [owner_piece_id]
 * owner_piece_id defaults to "user_01" (single-user v1) - prisc+x's
 * generic custom-op dispatch only ever passes ONE argument to a handler
 * (either a register's value or a literal, never both - see
 * exec_custom_op() in prisc+x.c), so the pal script can only hand this
 * op the keycode; a real multi-user version would need a "current user"
 * pointer file read here instead of a hardcoded default.
 *
 * Screens and option counts:
 *   main       (5):            0=User 1=Faucet 2=Store 3=Pets 4=Processes
 *   user       (1):             0=Back
 *   faucet     (3):             0=Claim Tokens 1=Coin Flip 2=Back
 *   store      (2):             0=Buy Egg 1=Back
 *   pets   (pet_count + 1): 0..pet_count-1 = one row per owned egg/pet
 *                           (unhatched -> hatch it, hatched -> open its
 *                           per-pet pet_detail screen), pet_count = Back
 *   pet_detail (8): 0=Open Window 1=Feed 2=Clean 3=Sleep/Wake 4=Train
 *                   5=Export Card 6=Destroy Card 7=Back (-> pets, not
 *                   main - the global 'b' shortcut still jumps straight
 *                   to main from anywhere). Operates on menu_state.txt's
 *                   own selected_pet field, set when entering this
 *                   screen from a pets row.
 *   processes  (1): 0=Back. An on-demand snapshot of which pets have a
 *                   live egg_window process (list_processes.+x, backed
 *                   by the same window.pid markers the duplicate-window
 *                   guard uses) - refreshed once on entry, not live-
 *                   updating; re-enter the screen to refresh again.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <signal.h>
#endif

#define MAX_LINE 512
#define PROJ_MAX_PATH 4096
#define PATH_BUF (PROJ_MAX_PATH + 256)

#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[PROJ_MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void menu_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/system/menu_state.txt", project_root);
}

static void load_menu_state(char *screen, size_t screen_sz, int *cursor, char *msg, size_t msg_sz,
                             char *selected_pet, size_t selected_pet_sz, int *digit_accum) {
    strncpy(screen, "main", screen_sz - 1);
    screen[screen_sz - 1] = '\0';
    *cursor = 0;
    msg[0] = '\0';
    selected_pet[0] = '\0';
    *digit_accum = 0;

    char path[PATH_BUF];
    menu_state_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0'; /* CRLF-safe - a Windows-touched file can have \r\n endings */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;
        if (strcmp(key, "screen") == 0) { strncpy(screen, val, screen_sz - 1); screen[screen_sz - 1] = '\0'; }
        else if (strcmp(key, "cursor") == 0) *cursor = atoi(val);
        else if (strcmp(key, "last_message") == 0) { strncpy(msg, val, msg_sz - 1); msg[msg_sz - 1] = '\0'; }
        else if (strcmp(key, "selected_pet") == 0) { strncpy(selected_pet, val, selected_pet_sz - 1); selected_pet[selected_pet_sz - 1] = '\0'; }
        else if (strcmp(key, "digit_accum") == 0) *digit_accum = atoi(val);
    }
    fclose(f);
}

static void save_menu_state(const char *screen, int cursor, const char *msg, const char *selected_pet, int digit_accum) {
    char path[PATH_BUF];
    menu_state_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "screen=%s\n", screen);
    fprintf(f, "cursor=%d\n", cursor);
    fprintf(f, "last_message=%s\n", msg);
    fprintf(f, "selected_pet=%s\n", selected_pet);
    fprintf(f, "digit_accum=%d\n", digit_accum);
    fclose(f);
}

static void inventory_path(const char *owner_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/world_01/map_lobby/%s/inventory.txt", project_root, owner_id);
}

static int count_pets(const char *owner_id) {
    char path[PATH_BUF];
    inventory_path(owner_id, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0'; /* CRLF-safe - a Windows-touched file can have \r\n endings */
        if (line[0]) n++;
    }
    fclose(f);
    return n;
}

static int get_pet_id_at(const char *owner_id, int index, char *out, size_t out_sz) {
    char path[PATH_BUF];
    inventory_path(owner_id, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int i = 0;
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0'; /* CRLF-safe - a Windows-touched file can have \r\n endings */
        if (!line[0]) continue;
        if (i == index) {
            /* Piece ids are genuinely short ("egg_1") despite line[] being
             * declared MAX_LINE; same class of warning fixed in
             * mutaclsym/system/prisc+x.c and generate_egg.c - suppressed
             * narrowly rather than widening out[] to match line[]'s size. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", line);
#pragma GCC diagnostic pop
            found = 1;
            break;
        }
        i++;
    }
    fclose(f);
    return found;
}

static int read_piece_int(const char *piece_id, const char *key, int def) {
    char path[PATH_BUF + 32];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, piece_id);
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static int option_count(const char *screen, const char *owner_id) {
    if (strcmp(screen, "main") == 0) return 5;
    if (strcmp(screen, "faucet") == 0) return 3;
    if (strcmp(screen, "store") == 0) return 2;
    if (strcmp(screen, "pets") == 0) return count_pets(owner_id) + 1; /* + Back */
    if (strcmp(screen, "pet_detail") == 0) return 8;
    return 1; /* user/processes: just Back */
}

#ifdef _WIN32
/* Windows has no real argv array for CreateProcess - just one command
 * line string - so each arg has to be quoted with the documented MS CRT
 * algorithm or the child's own argv parsing splits it wrong (this matters
 * here specifically because project_root/pet ids can contain spaces, e.g.
 * a path under "OneDrive\Desktop\...(1)"). */
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
 * cmd.exe - cmd.exe only understands double-quote quoting (the rest of
 * this codebase's popen() calls use single quotes, which is a POSIX shell
 * convention cmd.exe doesn't share), so this bypasses cmd.exe entirely. */
static FILE *win_run_capture(const char *exe_path, const char *arg1, const char *arg2, HANDLE *out_proc) {
    char qexe[PATH_BUF], qarg1[PATH_BUF], qarg2[PATH_BUF];
    char cmdline[PATH_BUF * 3];
    win_quote_arg(exe_path, qexe, sizeof(qexe));
    win_quote_arg(arg1, qarg1, sizeof(qarg1));
    if (arg2) {
        win_quote_arg(arg2, qarg2, sizeof(qarg2));
        snprintf(cmdline, sizeof(cmdline), "%s %s %s", qexe, qarg1, qarg2);
    } else {
        snprintf(cmdline, sizeof(cmdline), "%s %s", qexe, qarg1);
    }

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

static void run_action_op(const char *op_name, const char *arg, char *msg_out, size_t msg_sz) {
    char exe_path[PATH_BUF];
    snprintf(exe_path, sizeof(exe_path), "%s/ops/+x/%s.+x", project_root, op_name);
    msg_out[0] = '\0';
#ifdef _WIN32
    HANDLE proc;
    FILE *pf = win_run_capture(exe_path, arg, NULL, &proc);
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    win_run_close(pf, proc);
#else
    /* Wider than exe_path/arg's own PATH_BUF each, so gcc can prove the
     * quoted concatenation of both (plus quotes/space) can't truncate. */
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' '%s'", exe_path, arg);
    FILE *pf = popen(cmd, "r");
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    pclose(pf);
#endif
    msg_out[strcspn(msg_out, "\r\n")] = '\0'; /* CRLF-safe */
}

/* Same as run_action_op but for ops that need two args (feed_pet needs
 * both the pet id and the owner id to charge tokens) - prisc+x's own
 * custom-op dispatch can only ever hand this whole process one keycode
 * (see this file's own header comment), but nothing stops menu_input
 * itself from shelling out to a two-arg op directly. */
static void run_action_op2(const char *op_name, const char *arg1, const char *arg2, char *msg_out, size_t msg_sz) {
    char exe_path[PATH_BUF];
    snprintf(exe_path, sizeof(exe_path), "%s/ops/+x/%s.+x", project_root, op_name);
    msg_out[0] = '\0';
#ifdef _WIN32
    HANDLE proc;
    FILE *pf = win_run_capture(exe_path, arg1, arg2, &proc);
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    win_run_close(pf, proc);
#else
    /* Wider than exe_path/arg1/arg2's own PATH_BUF each, so gcc can prove
     * the quoted concatenation of all three (plus quotes/spaces) can't
     * truncate. */
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s'", exe_path, arg1, arg2);
    FILE *pf = popen(cmd, "r");
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    pclose(pf);
#endif
    msg_out[strcspn(msg_out, "\r\n")] = '\0'; /* CRLF-safe */
}

static void window_pid_path(const char *pet_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/world_01/map_lobby/%s/window.pid", project_root, pet_id);
}

/* True if a process with this pid currently exists - used both to skip
 * spawning a duplicate window for a pet that already has one open, and
 * (implicitly, by being wrong when it isn't) to let a stale marker from
 * an already-closed window get overwritten by a fresh spawn. */
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

/* Opens a pet's GL window as an independent, long-running process - not
 * captured/waited on, unlike run_action_op's short-lived ops. Once this
 * (short-lived) menu_input process exits, the window reparents to init
 * and keeps running on its own; no double-fork needed for that. The
 * window is meant to genuinely outlive this session (it self-ticks its
 * own pet independently - see egg_window.c's own header comment), so
 * nothing here tracks or passes a session PID.
 *
 * Refuses to spawn a second window for a pet that already has one open
 * (tracked via a per-pet pieces/world_01/map_lobby/<pet_id>/window.pid
 * marker, checked for liveness via is_pid_alive) - repeatedly hitting
 * Open Window on the same pet used to pile up duplicate windows, each
 * with its own GL context and self-tick timer, which is exactly the kind
 * of accumulation that made things sluggish/unresponsive over a long
 * session. */
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

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <keycode> [owner_piece_id]\n", argv[0]);
        return 1;
    }
    int key = atoi(argv[1]);
    const char *owner_id = (argc >= 3) ? argv[2] : "user_01";
    resolve_root();

    char screen[32];
    int cursor;
    char msg[256];
    char selected_pet[64];
    int digit_accum;
    load_menu_state(screen, sizeof(screen), &cursor, msg, sizeof(msg), selected_pet, sizeof(selected_pet), &digit_accum);

    int count = option_count(screen, owner_id);
    if (count < 1) count = 1;

    /* Multi-digit direct-select, ported from real 1.TPMOS's
     * chtmp_parser.c (the live renderer behind its numbered-panel
     * menus - confirmed via its compiled +x binary; the sibling
     * chtmp_player.c has no digit handling and no +x artifact, i.e.
     * dead code) rather than guessed: each digit keystroke accumulates
     * into digit_accum (persisted in menu_state.txt across the
     * short-lived per-keypress process runs this project uses instead
     * of chtmp_parser.c's one long-lived process), bounds-checked
     * against THIS screen's live option_count() on every keystroke -
     * if the accumulated value would exceed count, it restarts from
     * just this digit instead (so typing e.g. "12" when only 8 options
     * exist re-settles on row 2, not silently discarding the 2). A
     * digit only PREVIEWS a cursor jump - it does not activate the row
     * itself; activation is Enter-only, same as chtmp_parser.c's
     * do_jump()/process_key() split. Any non-digit key abandons a
     * pending multi-digit sequence (matches real 1.TPMOS exactly - see
     * nav-refactor-2.txt for the fuller writeup of where this was
     * ported from and why the earlier single-key-immediate-activate
     * version this replaced was wrong). */
    int is_digit = (key >= '0' && key <= '9');
    int is_enter = (key == 10 || key == 13);

    if (is_digit) {
        int d = key - '0';
        int new_val = digit_accum * 10 + d;
        if (new_val > 0 && new_val <= count) {
            digit_accum = new_val;
            cursor = new_val - 1;
        } else if (d > 0 && d <= count) {
            digit_accum = d;
            cursor = d - 1;
        } else {
            digit_accum = 0;
        }
        msg[0] = '\0';
        save_menu_state(screen, cursor, msg, selected_pet, digit_accum);
        return 0;
    }
    digit_accum = 0; /* any non-digit key abandons a pending multi-digit sequence */

    if (key == 'w' || key == ARROW_UP) {
        cursor = (cursor - 1 + count) % count;
        msg[0] = '\0';
    } else if (key == 's' || key == ARROW_DOWN) {
        cursor = (cursor + 1) % count;
        msg[0] = '\0';
    } else if (key == 'b') {
        strcpy(screen, "main");
        cursor = 0;
        msg[0] = '\0';
    } else if (is_enter) {
        if (strcmp(screen, "main") == 0) {
            const char *targets[] = {"user", "faucet", "store", "pets", "processes"};
            strcpy(screen, targets[cursor]);
            cursor = 0;
            msg[0] = '\0';
            /* Refresh the snapshot on entry - see this file's header
             * comment on the processes screen for why this isn't a
             * live-updating view. */
            if (strcmp(screen, "processes") == 0) run_action_op("list_processes", owner_id, msg, sizeof(msg));
        } else if (strcmp(screen, "faucet") == 0) {
            if (cursor == 0) run_action_op("claim_tokens", owner_id, msg, sizeof(msg));
            else if (cursor == 1) run_action_op("coin_flip", owner_id, msg, sizeof(msg));
            else { strcpy(screen, "main"); cursor = 0; msg[0] = '\0'; }
        } else if (strcmp(screen, "store") == 0) {
            if (cursor == 0) run_action_op("buy_egg", owner_id, msg, sizeof(msg));
            else { strcpy(screen, "main"); cursor = 0; msg[0] = '\0'; }
        } else if (strcmp(screen, "pets") == 0) {
            int pet_count = count_pets(owner_id);
            if (cursor < pet_count) {
                char pet_id[64];
                if (get_pet_id_at(owner_id, cursor, pet_id, sizeof(pet_id))) {
                    if (read_piece_int(pet_id, "hatched", 0)) {
                        strcpy(screen, "pet_detail");
                        snprintf(selected_pet, sizeof(selected_pet), "%s", pet_id);
                        cursor = 0;
                        msg[0] = '\0';
                    } else {
                        run_action_op("hatch_egg", pet_id, msg, sizeof(msg));
                    }
                }
            } else {
                strcpy(screen, "main");
                cursor = 0;
                msg[0] = '\0';
            }
        } else if (strcmp(screen, "pet_detail") == 0) {
            if (cursor == 0) spawn_egg_window(selected_pet, msg, sizeof(msg));
            else if (cursor == 1) run_action_op2("feed_pet", selected_pet, owner_id, msg, sizeof(msg));
            else if (cursor == 2) run_action_op("clean_pet", selected_pet, msg, sizeof(msg));
            else if (cursor == 3) run_action_op("toggle_sleep", selected_pet, msg, sizeof(msg));
            else if (cursor == 4) run_action_op("train_pet", selected_pet, msg, sizeof(msg));
            else if (cursor == 5) run_action_op("export_card", selected_pet, msg, sizeof(msg));
            else if (cursor == 6) run_action_op("destroy_card", selected_pet, msg, sizeof(msg));
            else { strcpy(screen, "pets"); cursor = 0; msg[0] = '\0'; }
        } else {
            /* user/processes: only option is Back */
            strcpy(screen, "main");
            cursor = 0;
            msg[0] = '\0';
        }
    }

    save_menu_state(screen, cursor, msg, selected_pet, digit_accum);
    return 0;
}
