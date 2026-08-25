/* pc_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever piececraft-xyz screen is currently showing. Modeled directly on
 * @.apps/civ-txt's own ops/civ_menu_input.c (real, proven,
 * live-verified precedent): screen SWITCHING is a real chtpm <button
 * href="..."> handled entirely by chtpm_parser_pal.c, never this op's
 * job; "which screen is current" is derived fresh every call from
 * pieces/display/current_layout.txt, never separately tracked mutable
 * state.
 *
 * P1 scope (CLONE ONLY - replicates civ-txt exactly): new_game.chtpm
 * (setup options writing config.txt) -> main.chtpm (turn counter, one
 * real END_TURN action). No Minecraft features yet - this is the
 * skeleton proving the real CHTPM nav + piece.pdl dispatch + ledger
 * loop works for THIS project, exactly like civ-txt did before board-viewer
 * was added.
 *
 * piececraft-xyz-specific commands (clone phase):
 *   SET_VICTORY:<conquest|score_turnlimit|tech_score>
 *   SET_MAP_SIZE:<small|medium>
 *   SET_COMBAT:<abstract|per_unit>
 *   CONFIRM_START - locks in setup, navigates player to main.chtpm
 *   END_TURN - advances turn counter, appends ledger entry
 *   OPEN_BOARD_WIDGET - spawns the shared board-viewer widget
 *
 * Self-contained, no shared headers.
 * Usage: pc_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include "win_posix_shim.h"

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[256];
} MenuItem;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

#ifdef _WIN32
/* Launch house PE under ops/+x/*.+x.
 * Critical: emoji house paths + '+x' dir break CreateProcessA/CopyFileA
 * (ERROR_INVALID_NAME 123). Always use wide (UTF-16) path APIs and stage
 * a temp .exe under %TEMP% before CreateProcessW. */
static int win_run_pe(const char *exe_path, const char *args_tail,
                      const char *cwd_hint, int wait_ms, int detached) {
    wchar_t wcwd[PATH_BUF];
    wcwd[0] = 0;
    if (cwd_hint && cwd_hint[0] && !(cwd_hint[0] == '.' && cwd_hint[1] == '\0')) {
        if (MultiByteToWideChar(CP_UTF8, 0, cwd_hint, -1, wcwd, PATH_BUF) <= 0)
            MultiByteToWideChar(CP_ACP, 0, cwd_hint, -1, wcwd, PATH_BUF);
    } else {
        if (!GetCurrentDirectoryW(PATH_BUF, wcwd))
            return 0;
    }

    wchar_t wrel[PATH_BUF];
    if (MultiByteToWideChar(CP_UTF8, 0, exe_path, -1, wrel, PATH_BUF) <= 0)
        MultiByteToWideChar(CP_ACP, 0, exe_path, -1, wrel, PATH_BUF);
    for (wchar_t *p = wrel; *p; p++) if (*p == L'/') *p = L'\\';

    wchar_t wapp[PATH_BUF];
    if (wrel[0] && wrel[1] == L':') {
        wcsncpy(wapp, wrel, PATH_BUF - 1);
        wapp[PATH_BUF - 1] = 0;
    } else {
        _snwprintf(wapp, PATH_BUF, L"%s\\%s", wcwd, wrel);
        wapp[PATH_BUF - 1] = 0;
    }

    if (GetFileAttributesW(wapp) == INVALID_FILE_ATTRIBUTES) {
        wchar_t try_exe[PATH_BUF];
        _snwprintf(try_exe, PATH_BUF, L"%s.exe", wapp);
        if (GetFileAttributesW(try_exe) != INVALID_FILE_ATTRIBUTES)
            wcsncpy(wapp, try_exe, PATH_BUF - 1);
        else
            return 0;
    }

    wchar_t wtmp[MAX_PATH];
    if (GetTempPathW(MAX_PATH, wtmp) == 0) return 0;
    wchar_t wtmp_exe[MAX_PATH];
    _snwprintf(wtmp_exe, MAX_PATH, L"%shouse_pe_%lu_%lu.exe", wtmp,
               (unsigned long)GetCurrentProcessId(),
               (unsigned long)GetTickCount());
    if (!CopyFileW(wapp, wtmp_exe, FALSE)) return 0;

    wchar_t wcmd[PATH_BUF * 2];
    if (args_tail && args_tail[0]) {
        wchar_t wargs[PATH_BUF];
        if (MultiByteToWideChar(CP_UTF8, 0, args_tail, -1, wargs, PATH_BUF) <= 0)
            MultiByteToWideChar(CP_ACP, 0, args_tail, -1, wargs, PATH_BUF);
        _snwprintf(wcmd, PATH_BUF * 2, L"\"%s\" %s", wtmp_exe, wargs);
    } else {
        _snwprintf(wcmd, PATH_BUF * 2, L"\"%s\"", wtmp_exe);
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    DWORD flags = CREATE_NO_WINDOW;
    if (detached) flags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;

    if (!CreateProcessW(wtmp_exe, wcmd, NULL, NULL, FALSE, flags, NULL, wcwd, &si, &pi))
        return 0;

    if (wait_ms != 0)
        WaitForSingleObject(pi.hProcess, wait_ms < 0 ? INFINITE : (DWORD)wait_ms);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DeleteFileW(wtmp_exe);
    return (wait_ms == 0) ? 1 : (code == 0);
}
#endif

static void read_kv_str_local(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str_local(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

/* REAL FIX 2026-08-04, direct user report ("cycle speed stops
 * autotick... toggle doesn't restart it") - real root cause: this op
 * and pc_clock_daemon.c (a real, ALWAYS-running background process,
 * same session) both do this exact real read-whole-file/rewrite-
 * whole-file sequence on the SAME real world_01/state.txt, completely
 * independently, with no coordination at all. If they interleave (the
 * daemon reads its own snapshot, THIS op writes a real change, then
 * the daemon finishes writing its OWN now-stale snapshot), the
 * daemon's own write silently OVERWRITES the real change with old
 * data - a genuine lost-update race, not a logic bug in either write.
 * Real fix: a real OS file lock (flock), held for the ENTIRE real
 * read-then-write sequence via ONE shared file descriptor (not two
 * separate fopen calls like before - that gap between them was
 * exactly where the race lived) - any other process's own real
 * write_kv() now genuinely blocks until this one fully finishes. */
static void write_kv(const char *path, const char *key, const char *value) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    FILE *f = fdopen(fd, "r+");
    if (!f) { flock(fd, LOCK_UN); close(fd); return; }

    char lines[64][MAX_LINE];
    int nlines = 0;
    while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;

    size_t key_len = strlen(key);
    int found = 0;
    fseek(f, 0, SEEK_SET);
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fflush(f);
    long endpos = ftell(f);
    if (endpos >= 0) { int _rc = ftruncate(fd, endpos); (void)_rc; }
    flock(fd, LOCK_UN);
    fclose(f);
}

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/piececraft-xyz/pieces/%s/piece.pdl", root, piece_id);
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

static void write_chtpm_bridge(const char *piece_id) {
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=piececraft-xyz\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "new_game");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
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

/* Forward declaration - real definition is later in this file, needed
 * here so ledger_append() below can resolve real_root internally
 * (same real symlink-timing fix as advance_tick(), see that function's
 * own header comment). */
static void resolve_real_root(const char *proj_root, char *out, size_t out_sz);

static void ledger_append(const char *proj_root, int turn, const char *actor, const char *action_type, const char *details) {
    char root[PATH_BUF];
    resolve_real_root(proj_root, root, sizeof(root));
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%s|%s|%s\n", ts, turn, actor, action_type, details);
    fclose(f);
}

/* REAL, NEW 2026-08-04, direct instruction (real house precedent
 * found: #.ref/Mar$.$treetRace]Q]k32]4K/wsr_clock.c) - launches the
 * real PERSISTENT clock daemon (ops/pc_clock_daemon.c) once per world,
 * right after generation. Real, simple "already running" check via
 * its own PID file (matches this house's own real precedent, wsr_
 * clock.c's own pid-file convention) - `kill -0` tests liveness
 * without actually signaling, so a stale PID from a crashed/killed
 * previous session doesn't block a fresh launch. */
static void launch_clock_daemon_if_needed(const char *proj_root) {
    char real_root[PATH_BUF];
    resolve_real_root(proj_root, real_root, sizeof(real_root));
    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/pieces/system/pc_clock_daemon.pid", real_root);
    FILE *pf = fopen(pid_path, "r");
    if (pf) {
        int pid = 0;
        if (fscanf(pf, "%d", &pid) == 1 && pid > 0 && kill(pid, 0) == 0) {
            fclose(pf);
            return; /* already real, alive */
        }
        fclose(pf);
    }
#ifdef _WIN32
    {
        char env_kv[PATH_BUF + 32];
        snprintf(env_kv, sizeof(env_kv), "PRISC_PROJECT_ROOT=%s", proj_root);
        _putenv(env_kv);
        /* Prefer real_root ops (persistent), cwd = session for relative env */
        char daemon_path[PATH_BUF];
        snprintf(daemon_path, sizeof(daemon_path), "%s\\ops\\+x\\pc_clock_daemon.+x", real_root);
        for (char *p = daemon_path; *p; p++) if (*p == '/') *p = '\\';
        if (!win_run_pe(daemon_path, NULL, proj_root, 0, 1))
            win_run_pe("ops\\+x\\pc_clock_daemon.+x", NULL, proj_root, 0, 1);
        /* best-effort pid file (process id unknown after detached spawn) */
        FILE *wf = fopen(pid_path, "w");
        if (wf) { fprintf(wf, "1\n"); fclose(wf); }
    }
#else
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "PRISC_PROJECT_ROOT='%s' setsid '%s/ops/+x/pc_clock_daemon.+x' >/dev/null 2>&1 < /dev/null &",
             proj_root, proj_root);
    { int _rc = system(cmd); (void)_rc; }
#endif
}

/* Host project id for board-viewer ledger scoping (board-viewer:<host>). */
static const char *host_project_id(void) {
    const char *pid = getenv("PRISC_PROJECT_ID");
    if (pid && pid[0]) return pid;
    return "piececraft-xyz";
}

/* Spawn or refocus &.widgits/board-viewer (GL map widget).
 * Linux: button.sh run-widget. Windows: button.ps1 run-widget.
 * Only triggered by the OPEN_BOARD_WIDGET menu command (restored to 00.10
 * behavior - the Win pass had added auto-open on CONFIRM_START/CONFIRM_
 * START_DEBUG, which wrongly opened the widget without the user pressing
 * the "View Board" button; that drift is removed here). */
static void open_board_widget(const char *project_root, char *message, size_t message_sz) {
    char house_root_path[PATH_BUF], house_root[PATH_BUF] = "";
    snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
    FILE *hf = fopen(house_root_path, "r");
    if (hf) {
        if (fgets(house_root, sizeof(house_root), hf))
            house_root[strcspn(house_root, "\r\n")] = '\0';
        fclose(hf);
    }

    char real_root_path[PATH_BUF], real_root[PATH_BUF] = "";
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", project_root);
    FILE *rf = fopen(real_root_path, "r");
    if (rf) {
        if (fgets(real_root, sizeof(real_root), rf))
            real_root[strcspn(real_root, "\r\n")] = '\0';
        fclose(rf);
    }
    if (!real_root[0]) {
        /* session may only have "." root — fall back to real_root file or CWD */
        char rr[PATH_BUF];
        resolve_real_root(project_root, rr, sizeof(rr));
        snprintf(real_root, sizeof(real_root), "%s", rr);
    }

    const char *host = host_project_id();
    char ledger_id[256];
    snprintf(ledger_id, sizeof(ledger_id), "board-viewer:%s", host);

    char widget_button[PATH_BUF];
#ifdef _WIN32
    snprintf(widget_button, sizeof(widget_button), "%s/&.widgits/board-viewer/button.ps1", house_root);
#else
    snprintf(widget_button, sizeof(widget_button), "%s/&.widgits/board-viewer/button.sh", house_root);
#endif

    char peer_session_root[PATH_BUF] = "";
#ifndef _WIN32
    /* Linux only: popen with env PREFIX is not valid on cmd.exe (Win). */
    if (house_root[0]) {
        char peer_cmd[PATH_BUF * 2];
        snprintf(peer_cmd, sizeof(peer_cmd),
                 "PRISC_PROJECT_ROOT='%s' '%s/&.widgits/board-viewer/ops/+x/ledger_peers.+x' widget 2>/dev/null",
                 project_root, house_root);
        FILE *pf = popen(peer_cmd, "r");
        if (pf) {
            char peer_line[MAX_LINE];
            while (fgets(peer_line, sizeof(peer_line), pf)) {
                peer_line[strcspn(peer_line, "\r\n")] = '\0';
                char *save = NULL;
                char *sess_tok = strtok_r(peer_line, "|", &save);
                strtok_r(NULL, "|", &save);
                strtok_r(NULL, "|", &save);
                char *proj_tok = strtok_r(NULL, "|", &save);
                if (proj_tok && sess_tok && strcmp(proj_tok, ledger_id) == 0) {
                    snprintf(peer_session_root, sizeof(peer_session_root), "%s", sess_tok);
                    break;
                }
            }
            pclose(pf);
        }
    }
#endif

    if (peer_session_root[0] && real_root[0]) {
        char peer_state_path[PATH_BUF];
        snprintf(peer_state_path, sizeof(peer_state_path), "%s/pieces/system/bv_state.txt", peer_session_root);
        write_kv(peer_state_path, "focused_project_id", host);
        write_kv(peer_state_path, "focused_project_root", real_root);
        snprintf(message, message_sz, "Board widget already open - refocusing on %s.", host);
        return;
    }

#ifdef _WIN32
    {
        /* Wide path check — ANSI access() fails on emoji house paths. */
        wchar_t wbtn[PATH_BUF];
        int has_btn = 0;
        if (MultiByteToWideChar(CP_UTF8, 0, widget_button, -1, wbtn, PATH_BUF) > 0 ||
            MultiByteToWideChar(CP_ACP, 0, widget_button, -1, wbtn, PATH_BUF) > 0) {
            has_btn = (GetFileAttributesW(wbtn) != INVALID_FILE_ATTRIBUTES);
        }
        if (house_root[0] && real_root[0] && has_btn) {
            wchar_t wcmd[PATH_BUF * 3];
            wchar_t wps1[PATH_BUF], wfocus[PATH_BUF];
            MultiByteToWideChar(CP_UTF8, 0, widget_button, -1, wps1, PATH_BUF);
            MultiByteToWideChar(CP_UTF8, 0, real_root, -1, wfocus, PATH_BUF);
            /* Prefer house-relative focus for board-viewer button.ps1 */
            _snwprintf(wcmd, PATH_BUF * 3,
                       L"powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\" run-widget \"%s\"",
                       wps1, wfocus);
            STARTUPINFOW si; PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si); ZeroMemory(&pi, sizeof(pi));
            SetEnvironmentVariableW(L"RUN_PROFILE", L"widget");
            if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE,
                               CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
                snprintf(message, message_sz, "Board widget launching (GL)...");
            } else {
                snprintf(message, message_sz, "Board widget launch failed (err=%lu).",
                         (unsigned long)GetLastError());
            }
        } else {
            snprintf(message, message_sz,
                     "Board widget missing (need &.widgits/board-viewer/button.ps1).");
        }
    }
#else
    if (house_root[0] && real_root[0] && access(widget_button, F_OK) == 0) {
        char cmd_buf[PATH_BUF * 2];
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "setsid env RUN_PROFILE=widget bash '%s' run-widget '%s' >/dev/null 2>&1 < /dev/null &",
                 widget_button, real_root);
        { int _rc = system(cmd_buf); (void)_rc; }
        snprintf(message, message_sz, "Board widget launching (separate GL window)...");
    } else {
        snprintf(message, message_sz,
                 "Board widget missing (need &.widgits/board-viewer/button.sh).");
    }
#endif
}

/* REAL FIX 2026-08-04, direct instruction ("give the chicken the
 * master-ledger AI mutaclysm zombies have, but just to walk
 * randomly") - real precedent checked directly (101.mutaclsym's own
 * ops/tick_monsters.c: one real step per hero-turn tick, logged to its
 * own real master_ledger.txt via append_ledger()). This is the
 * deliberately SIMPLER "just walk randomly" version - no chase/flee
 * decision_mode, no monster_types.txt registry lookup (piececraft-xyz has
 * neither yet) - just a real, honest random step per real world tick,
 * clamped to the debug map's own 16x16 bounds, logged through this
 * SAME ledger_append() every other real player action already uses
 * (now actor-parameterized so an animal's own entries say so
 * honestly, not "player"). */
static void resolve_real_root(const char *proj_root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", proj_root);
    char real_root_path[PATH_BUF];
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", proj_root);
    FILE *rf = fopen(real_root_path, "r");
    if (rf) {
        char buf[PATH_BUF];
        if (fgets(buf, sizeof(buf), rf)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            if (buf[0]) snprintf(out, out_sz, "%s", buf);
        }
        fclose(rf);
    }
}

static void tick_animals(const char *proj_root, int tick) {
    char real_root[PATH_BUF];
    resolve_real_root(proj_root, real_root, sizeof(real_root));
    char animals_path[PATH_BUF];
    snprintf(animals_path, sizeof(animals_path), "%s/pieces/world_01/animals.txt", real_root);
    FILE *f = fopen(animals_path, "r");
    if (!f) return;

    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL) ^ (unsigned)getpid()); seeded = 1; }

    char rewritten[64][MAX_LINE];
    int n = 0;
    char line[MAX_LINE];
    while (n < 64 && fgets(line, sizeof(line), f)) {
        char entity_id[64];
        int x, y, z;
        if (sscanf(line, "%63[^,],%d,%d,%d", entity_id, &x, &y, &z) == 4) {
            int dx = (rand() % 3) - 1; /* -1, 0, or 1 */
            int dy = (rand() % 3) - 1;
            int nx = x + dx, ny = y + dy;
            if (nx < 0) nx = 0; if (nx > 15) nx = 15;
            if (ny < 0) ny = 0; if (ny > 15) ny = 15;
            snprintf(rewritten[n], MAX_LINE, "%s,%d,%d,%d\n", entity_id, nx, ny, z);
            if (nx != x || ny != y) {
                char details[64];
                snprintf(details, sizeof(details), "x:%d,y:%d", nx, ny);
                ledger_append(real_root, tick, entity_id, "wander", details);
            }
        } else {
            snprintf(rewritten[n], MAX_LINE, "%s", line);
        }
        n++;
    }
    fclose(f);

    FILE *wf = fopen(animals_path, "w");
    if (wf) {
        for (int i = 0; i < n; i++) fputs(rewritten[i], wf);
        fclose(wf);
    }
}

/* advance_tick - real Phase 2 world-tick counter (design §5), replacing
 * the old civ-txt clone's own config.txt "turn" field for anything
 * gameplay-real (phase2-plan.md §6 step 1). "the world only advances
 * when the player acts... one real thing happens, then the world
 * catches up by exactly one step" - direct instruction confirmed both
 * movement AND the manual End Turn button should advance this same
 * real counter ("movement will end turn and so can button"), so this
 * is a real, shared helper both call into, not two separate counters
 * drifting apart. Returns the NEW tick value (post-increment) so the
 * caller can log/display it without a second read. */
static int advance_tick(const char *proj_root) {
    /* REAL FIX 2026-08-04, direct user report ("tick didn't change
     * time... autotick isn't moving time forward"): this used to write
     * pieces/world_01/state.txt via the raw EPHEMERAL session root -
     * fine for a file that already existed (and was symlinked) when
     * the session started, wrong for world_01/state.txt on a session
     * that launched BEFORE Confirm & Start ever generated it (pc_
     * generate_chunk.c's own real world-gen write already correctly
     * uses real_root, see that file's own header comment) - the two
     * ended up silently writing/reading two DIFFERENT files, real
     * "symlink omission" bug, same real class this house has hit
     * before. Real fix: resolve real_root here too, matching pc_
     * generate_chunk.c and tick_animals() above. */
    char root[PATH_BUF];
    resolve_real_root(proj_root, root, sizeof(root));
    char world_state_path[PATH_BUF];
    snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", root);
    int tick = read_kv_int(world_state_path, "tick", 0);
    tick += 1;
    write_kv_int(world_state_path, "tick", tick);
    return tick;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/piececraft-xyz/pieces/pc_menu/state.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    /* REAL BUG FIX 2026-08-20 (see sim-smell-fix.md's own widget_cmds/
     * inbox.txt writeup): under the copy-based symlink-elimination
     * strategy, PRISC_PROJECT_ROOT stays the disposable session dir, but
     * board-viewer writes real inbox commands to the REAL (non-session)
     * project root - see OPEN_BOARD_WIDGET's own real_root resolution
     * above, board_widget_bridge.txt's inbox_path is resolved against
     * board-viewer's own focused_project_root, which IS real_root, not
     * the session dir. This op must drain the SAME real location board-
     * viewer actually writes to, not its own session-local copy (which
     * would silently never see new commands at all). */
    char inbox_real_root[PATH_BUF];
    resolve_real_root(project_root, inbox_real_root, sizeof(inbox_real_root));
    char inbox_path[PATH_BUF];
    snprintf(inbox_path, sizeof(inbox_path), "%s/pieces/system/widget_cmds/inbox.txt", inbox_real_root);
    char inbox_cmd_buf[MAX_LINE] = "";
    {
        FILE *ibf = fopen(inbox_path, "r");
        if (ibf) {
            if (fgets(inbox_cmd_buf, sizeof(inbox_cmd_buf), ibf)) {
                inbox_cmd_buf[strcspn(inbox_cmd_buf, "\r\n")] = '\0';
            }
            fclose(ibf);
        }
        if (inbox_cmd_buf[0]) {
            FILE *tf = fopen(inbox_path, "w");
            if (tf) fclose(tf);
        }
    }

    int key = atoi(argv[1]);

    if (key == 0 && !inbox_cmd_buf[0]) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/pc_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    const char *cmd = NULL;
    if (inbox_cmd_buf[0]) {
        cmd = inbox_cmd_buf;
    } else if (resolved_item >= 1 && resolved_item <= item_count) {
        cmd = items[resolved_item - 1].command;
    }

    if (cmd) {
        if (strcmp(cmd, "CONFIRM_START") == 0) {
            /* REPLACED 2026-08-03, real Phase 1 divergence per
             * civ-vs-piece.md §2/§6: civ-txt's own Victory/Map/Combat
             * setup options and single-flat-board.txt generation are
             * gone - a voxel sandbox has neither concept. Real chunk
             * generation now: a random seed (civ-vs-piece.md's own
             * "auto-random on Confirm & Start" decision - manual seed
             * entry needs the house's real cli_io text-input mechanic,
             * flagged as separate later work, not blocking this), then
             * pc_generate_chunk.+x does the actual terrain/world/hero
             * state writing (see that op's own header comment for the
             * full writeup - seeded per-coordinate hash, not raw
             * srand(time()), since chunks must regenerate identically
             * later per design §2's decompression requirement). */
            write_kv(config_path, "game_state", "playing");

            unsigned int world_seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();

#ifdef _WIN32
            {
                char env_kv[PATH_BUF + 32];
                snprintf(env_kv, sizeof(env_kv), "PRISC_PROJECT_ROOT=%s", project_root);
                _putenv(env_kv);
                char args[64];
                snprintf(args, sizeof(args), "%u 0 0", world_seed);
                int ok = win_run_pe("ops\\+x\\pc_generate_chunk.+x", args, project_root, 60000, 0);
                if (!ok) {
                    snprintf(message, sizeof(message),
                             "CONFIRM_START: pc_generate_chunk failed (Win spawn/write).");
                }
            }
#else
            {
                char gen_cmd[PATH_BUF + 64];
                snprintf(gen_cmd, sizeof(gen_cmd),
                         "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/pc_generate_chunk.+x' %u 0 0 >/dev/null 2>&1",
                         project_root, project_root, world_seed);
                { int _rc = system(gen_cmd); (void)_rc; }
            }
#endif
            launch_clock_daemon_if_needed(project_root);

            snprintf(message, sizeof(message), "World generated (seed %u). Game started.", world_seed);
        } else if (strcmp(cmd, "CONFIRM_START_DEBUG") == 0) {
            /* Real premade "vanilla debug" flat map - see pc_generate_
             * chunk.c's own header comment on is_tree_column() for the
             * full writeup (direct instruction: "premade-map that is
             * mostly flat with just a few trees, as a vanilla debug
             * testing ground" + "i want to make sure he spawns on flat,
             * clearly visible ground level"). Same generation op, same
             * seed mechanism (still real/deterministic, just ignored
             * for height since flat mode fixes it) - only the extra
             * "flat" argv flag differs from CONFIRM_START above. */
            write_kv(config_path, "game_state", "playing");

            unsigned int world_seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();

#ifdef _WIN32
            {
                char env_kv[PATH_BUF + 32];
                snprintf(env_kv, sizeof(env_kv), "PRISC_PROJECT_ROOT=%s", project_root);
                _putenv(env_kv);
                char args[64];
                snprintf(args, sizeof(args), "%u 0 0 flat", world_seed);
                int ok = win_run_pe("ops\\+x\\pc_generate_chunk.+x", args, project_root, 120000, 0);
                if (!ok) {
                    snprintf(message, sizeof(message),
                             "CONFIRM_START_DEBUG: pc_generate_chunk failed to launch.");
                }
            }
#else
            {
                char gen_cmd[PATH_BUF + 64];
                snprintf(gen_cmd, sizeof(gen_cmd),
                         "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/pc_generate_chunk.+x' %u 0 0 flat >/dev/null 2>&1",
                         project_root, project_root, world_seed);
                { int _rc = system(gen_cmd); (void)_rc; }
            }
#endif
            launch_clock_daemon_if_needed(project_root);

            snprintf(message, sizeof(message), "Debug flat world generated. Game started.");
        } else if (strcmp(cmd, "END_TURN") == 0) {
            /* Manual tick-advance fallback (design §5, phase2-plan.md
             * §6 step 1) - the SAME real world_01/state.txt tick
             * counter real movement now also advances (see bv_menu_
             * input.c's own MOVE-notifies-host wiring), not a separate
             * civ-txt-leftover "turn" counter. */
            int tick = advance_tick(project_root);
            char details[128];
            snprintf(details, sizeof(details), "tick:%d", tick);
            ledger_append(project_root, tick, "player", "end_turn", details);
            tick_animals(project_root, tick);
            snprintf(message, sizeof(message), "Tick %d.", tick);
        } else if (strcmp(cmd, "TOGGLE_AUTOTICK") == 0) {
            /* REAL, NEW 2026-08-04, direct instruction ("] will start
             * or stop autotick"). The actual real-time advancement
             * (elapsed wall-clock -> game seconds, chicken wander on
             * each real game-minute crossed) lives in pc_compose_frame.
             * c's own advance_game_clock() - runs every real poll
             * regardless of which key was last pressed, so this
             * handler's ENTIRE job is flipping the one real flag that
             * gates it. */
            char real_root_ws[PATH_BUF];
            resolve_real_root(project_root, real_root_ws, sizeof(real_root_ws));
            char world_state_path[PATH_BUF];
            snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", real_root_ws);
            int enabled = read_kv_int(world_state_path, "autotick_enabled", 0);
            enabled = !enabled;
            write_kv_int(world_state_path, "autotick_enabled", enabled);
            if (enabled) write_kv_int(world_state_path, "autotick_last_real_ms", 0); /* real, fresh baseline - see advance_game_clock()'s own header comment on why 0 means "just starting" */
            snprintf(message, sizeof(message), "Autotick %s.", enabled ? "ON" : "off");
        } else if (strcmp(cmd, "CYCLE_TICK_SPEED") == 0) {
            /* REAL, NEW 2026-08-04, direct instruction ("[" cycles time
             * speeds) - real fixed cycle through every real rate
             * pc_compose_frame.c's own advance_game_clock() understands
             * (cent/sec/min/hour/day, same real names/order the user's
             * own game_centiseconds_per_real_millisecond table used). */
            char real_root_ws[PATH_BUF];
            resolve_real_root(project_root, real_root_ws, sizeof(real_root_ws));
            char world_state_path[PATH_BUF];
            snprintf(world_state_path, sizeof(world_state_path), "%s/pieces/world_01/state.txt", real_root_ws);
            char speed[16] = "min";
            read_kv_str_local(world_state_path, "autotick_speed", speed, sizeof(speed));
            const char *next_speed = "min";
            if (strcmp(speed, "cent") == 0) next_speed = "sec";
            else if (strcmp(speed, "sec") == 0) next_speed = "min";
            else if (strcmp(speed, "min") == 0) next_speed = "hour";
            else if (strcmp(speed, "hour") == 0) next_speed = "day";
            else if (strcmp(speed, "day") == 0) next_speed = "cent";
            write_kv(world_state_path, "autotick_speed", next_speed);
            snprintf(message, sizeof(message), "Tick speed: %s.", next_speed);
        } else if (strcmp(cmd, "MOVE") == 0) {
            /* Real movement-driven tick advance (design §5, direct
             * instruction "movement will end turn and so can button").
             * board-viewer's own bv_menu_input.c now appends this
             * action to this host's own real inbox every time the
             * xelector's real position actually changes (arrow keys) -
             * this handler doesn't move anything itself (the xelector's
             * own pos_x/y/z were already written directly by board-
             * viewer, cross-project, before this ever arrives here) -
             * it only advances the shared world tick, matching END_
             * TURN's own real counter exactly. */
            int tick = advance_tick(project_root);
            char details[128];
            snprintf(details, sizeof(details), "tick:%d", tick);
            ledger_append(project_root, tick, "player", "move", details);
            tick_animals(project_root, tick);
        } else if (strncmp(cmd, "JUMP", 4) == 0) {
            /* Real plumbing, honest stub mechanic (phase2-plan.md §6
             * step 1 - real jump PHYSICS is genuinely later work, this
             * wires the real key->inbox->dispatch path end-to-end so
             * Phase 2's own possession/constraint system has something
             * real to build on, per that doc's own §3 writeup). Jump
             * still advances the real world tick (a real action did
             * happen), and posts a real, honest ledger entry - not
             * pretending physics exists yet. */
            int tick = advance_tick(project_root);
            char details[128];
            snprintf(details, sizeof(details), "tick:%d", tick);
            ledger_append(project_root, tick, "player", "jump", details);
            tick_animals(project_root, tick);
            snprintf(message, sizeof(message), "Jump (tick %d) - physics not implemented yet.", tick);
        } else if (strncmp(cmd, "MINE", 4) == 0) {
            /* Same real-plumbing-honest-stub shape as JUMP above - real
             * block removal is Phase 2's own pc_break_block op
             * (civ-vs-piece.md §6, not built this pass). */
            int tick = advance_tick(project_root);
            char details[128];
            snprintf(details, sizeof(details), "tick:%d", tick);
            ledger_append(project_root, tick, "player", "mine", details);
            tick_animals(project_root, tick);
            snprintf(message, sizeof(message), "Mine (tick %d) - block removal not implemented yet.", tick);
        } else if (strncmp(cmd, "BUILD", 5) == 0) {
            /* Same real-plumbing-honest-stub shape as JUMP/MINE above -
             * real block placement is Phase 2's own pc_place_block op
             * (civ-vs-piece.md §6, not built this pass). */
            int tick = advance_tick(project_root);
            char details[128];
            snprintf(details, sizeof(details), "tick:%d", tick);
            ledger_append(project_root, tick, "player", "build", details);
            tick_animals(project_root, tick);
            snprintf(message, sizeof(message), "Build (tick %d) - block placement not implemented yet.", tick);
        } else if (strcmp(cmd, "OPEN_BOARD_WIDGET") == 0) {
            open_board_widget(project_root, message, sizeof(message));
        } else if (strcmp(cmd, "OPEN_VIEW_EDITOR") == 0) {
            /* REAL, NEW 2026-08-04, direct instruction - the new "View
             * Editor" menu entry added alongside "View Board". Honest
             * stub for now: the real editor widget (file-menu/maps/
             * tiles/db/plugins/play-pause-reset) doesn't exist yet -
             * this is real, working PLUMBING (the button, the command,
             * this handler) so the real widget has somewhere to plug
             * in later, not a placeholder that silently does nothing.
             * See piececraft-xyz-blueprint.md once written for the real
             * design this will eventually open. */
            snprintf(message, sizeof(message),
                     "View Editor - not built yet (see piececraft-xyz-blueprint.md).");
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/pc_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
