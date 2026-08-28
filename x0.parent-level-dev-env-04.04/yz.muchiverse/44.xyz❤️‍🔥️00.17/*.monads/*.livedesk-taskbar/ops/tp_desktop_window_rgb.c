/* tp_desktop_window - the missing "live" half of tp_place_desktop.
 * Usage: tp_desktop_window.+x <package_dir>
 *
 * tp_place_desktop.c already writes a real package (glyph.txt + meta.pdl)
 * into #.desktop/tiles/<name>/, but nothing rendered it - it just sat on
 * disk. This is a small, borderless GLX window (same override_redirect
 * technique as 01.muchi-pals's egg_window.c, stripped down to the parts a
 * tile stamp actually needs: no sprite/vitals/self-tick, just "be a real,
 * draggable, closeable OS window that represents this one desktop
 * package"). Window title is the glyph itself so it's identifiable at a
 * glance without needing real glyph-texture rendering (deferred - see
 * aomorai-editor-blueprint.md open question 8 on glyph widening; this
 * window already reads glyph.txt as a whole line so widening it to UTF-8
 * later is a non-issue for this file specifically).
 *
 * Dragging repositions the window and writes the pixel position back to
 * <package_dir>/desktop_pos.txt (x=,y=) - the one deliberate state write,
 * same "recording user input, not deciding behavior" exception egg_window's
 * own header comment makes for its own position write-back.
 *
 * Polls every ~300ms for <package_dir>/glyph.txt still existing - if the
 * package directory has been removed (e.g. tp_import_from_desktop.+x with
 * a future --remove flag, or a human rm -rf), this window closes itself
 * instead of floating forever pointing at nothing.
 */
#ifndef _WIN32
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#else
#include "khtpm_strip_x11_win.h"
#include <shellapi.h>
#endif
/* RGB compose->present fork (2026-08-12, direct instruction: "we should
 * do it with a new entity... full GLX->Xlib rewrite for this one
 * entity"). tp_desktop_window.c's GLX usage was fully contained to
 * three spots (font-list glyph fallback, sprite texture quad, clear+
 * swap) - everything else (popups, drag, shape mask, relay) already
 * used plain Xlib/Xft-free Xcore drawing untouched here. Ported to the
 * same offscreen-Pixmap-compose + XGetImage/XPutImage-present pattern
 * already proven on db-hq and the taskbar (see khtpm_hq_render.c's own
 * g_frame_rgb header comment) instead of GLX's own double-buffer swap -
 * this is the ONE entity binary using this pattern, not a global
 * replacement of tp_desktop_window.c (that one's still GLX, unchanged,
 * still what every other entity spawns). No GL headers needed anymore. */
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <locale.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/file.h>
#include <dirent.h>
#include <sys/wait.h>
#ifdef __APPLE__
/* macOS leg (2026-08-22): no /proc/self/exe here — _NSGetExecutablePath()
 * is the Apple equivalent for the self-binary lookups below. */
#include <mach-o/dyld.h>
#endif
#else
#include <io.h>
#include <direct.h>
#include <sys/time.h>
static int flock(int fd, int op) { (void)fd; (void)op; return 0; }
#define LOCK_EX 2
#define LOCK_UN 8
#define LOCK_NB 4
#define strtok_r strtok_s
#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
static pid_t waitpid(pid_t p, int *st, int fl) {
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)p);
    if (!h) { if (st) *st = 0; return -1; }
    DWORD wait = (fl & WNOHANG) ? 0 : INFINITE;
    DWORD w = WaitForSingleObject(h, wait);
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    if (w == WAIT_TIMEOUT) return 0;
    if (st) *st = (int)code;
    return p;
}
static struct tm *localtime_r(const time_t *t, struct tm *out) {
    if (localtime_s(out, t) != 0) return NULL;
    return out;
}
#ifndef S_ISDIR
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#define mkdir(p, m) _mkdir(p)
static int ktb_usleep(unsigned usec) {
    DWORD ms = usec / 1000; if (!ms) ms = 1; Sleep(ms); return 0;
}
#define usleep ktb_usleep
#define getpid() ((int)GetCurrentProcessId())
static int kill(pid_t p, int sig) {
    (void)sig;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)p);
    if (!h) return -1;
    if (sig == 0) { DWORD c=0; GetExitCodeProcess(h,&c); CloseHandle(h); return c==STILL_ACTIVE?0:-1; }
    CloseHandle(h);
    h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)p);
    if (!h) return -1;
    TerminateProcess(h, 1); CloseHandle(h); return 0;
}
static char *basename(char *p) {
    char *s = strrchr(p, '\\'); char *f = strrchr(p, '/');
    if (f && (!s || f > s)) s = f;
    return s ? s + 1 : p;
}
static ssize_t readlink(const char *path, char *buf, size_t n) {
    (void)path;
    wchar_t w[4352];
    DWORD k = GetModuleFileNameW(NULL, w, 4352);
    if (!k) return -1;
    int m = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, (int)n, NULL, NULL);
    return m > 0 ? (ssize_t)(m - 1) : -1;
}
static void win_package_rel(char *path) {
    char *m = strstr(path, "xyzfs/");
    if (!m) m = strstr(path, "xyzfs\\");
    if (!m) m = strstr(path, "/#.desktop/");
    if (!m) m = strstr(path, "\\#.desktop\\");
    if (m && m != path) {
        if (*m == '/' || *m == '\\') m++;
        memmove(path, m, strlen(m) + 1);
    }
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
}
static char *dirname(char *p) {
    char *s = strrchr(p, '\\'); char *f = strrchr(p, '/');
    if (f && (!s || f > s)) s = f;
    if (!s) return ".";
    *s = '\0';
    return p;
}
#endif

/* ========================================================================
 * 2026-08-06 FOCUS-RECOVERY — option C (user chose):
 *   locks OFF; grabs ON; soft focus ON (this popup only).
 * Flip any flag to experiment. Nav [N] UI + NAV_KEY file relay stay ON.
 * ======================================================================== */
#ifndef LIVEDESK_USE_REGISTRY_LOCK
#define LIVEDESK_USE_REGISTRY_LOCK 1
#endif
#ifndef LIVEDESK_USE_POPUP_LOCK
#define LIVEDESK_USE_POPUP_LOCK 0
#endif
#ifndef LIVEDESK_USE_XGRAB_POINTER
#define LIVEDESK_USE_XGRAB_POINTER 1
#endif
#ifndef LIVEDESK_USE_XGRAB_KEYBOARD
#define LIVEDESK_USE_XGRAB_KEYBOARD 1
#endif
/* REAL 2026-08-07, direct instruction ("make them configurable via
 * config / .pdl file so i can easily experiment with them"): the
 * context-menu behavior guards are now RUNTIME-configurable, read from
 * the package's own meta.pdl by read_menu_config() below:
 *   STATE | menu_stay_open | 1    outside/repeat clicks keep menu open
 *   STATE | grab_pointer   | 1    pointer grab while a menu is open
 *   STATE | grab_keyboard  | 1    keyboard grab while a menu is open
 * menu_stay_open=1 also makes the menu NON-modal (no pointer grab), so
 * the toolbar and other windows stay clickable while it's open - see
 * open_context_menu()'s grab block. Missing rows keep these compile-time
 * defaults. Edit meta.pdl and the next right-click re-reads it - no
 * rebuild, no restart. */
static int g_menu_stay_open = 1;
static int g_grab_pointer = LIVEDESK_USE_XGRAB_POINTER;
static int g_grab_keyboard = LIVEDESK_USE_XGRAB_KEYBOARD;
/* Soft focus fallback (option C 2026-08-06): set input focus on THIS
 * popup only — not a house-wide raise-fight across entity tiles.
 * Used on open + header click so keys still work if grab fails. */
#ifndef LIVEDESK_POPUP_SOFT_FOCUS
#define LIVEDESK_POPUP_SOFT_FOCUS 1
#endif

/* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER's own real
 * monsters need a 2x2-desktop-grid-cell footprint - "these monsters
 * should take up 4 tiles instead of the previous 1 tile"): WIN_PX was
 * a compile-time #define, meaning every desktop entity was hardcoded
 * to the exact same 64px size. Converted to a real runtime variable,
 * set once early in main() from a real, optional "footprint_tiles"
 * STATE row in the package's own meta.pdl (same real SECTION|KEY|VALUE
 * parse convention every other real field there already uses) -
 * defaults to 1 (this exact same 64px value) when absent, so every
 * existing pet/asa/ava package is completely unaffected. Every one of
 * this file's own real call sites below still just reads "WIN_PX" -
 * only its OWN declaration changed, not the 9 real places it's used. */
static int WIN_PX = 64;
#define POLL_INTERVAL_USEC 300000
#define PATH_BUF 4352
/* REAL FIX 2026-08-04, direct instruction ("desk has a grid... egg-pets
 * snap to grid... make sure windows/procs are killed and don't render
 * more than 30fps, cpu is getting hot"): same GRID_CELL_PX egg_window.c
 * uses (01.muchi-pals-🥚️-13.01/system/egg_window.c) - tile stamps should
 * snap to the SAME desktop grid egg-pals already use, not a separate
 * one.
 *
 * REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a, direct instruction:
 * "this size should be set/read from a .pdl which can be changed"):
 * converted from a compile-time #define to a real runtime variable, set
 * once early in main() from an optional "GRID | cell_px | N" row in
 * #.desktop/desk_grid.pdl (see read_grid_cell_px() below) - same real
 * "compile-time constant -> runtime variable read from a real file,
 * safe default preserves existing behavior" pattern this file's own
 * WIN_PX/footprint_tiles conversion already established above. Defaults
 * to 80 (this file's own original hardcoded value) when the file/row is
 * absent, so every existing desktop is completely unaffected until
 * someone actually writes a real desk_grid.pdl. */
static int GRID_CELL_PX = 80;
#define MAX_FPS 30
#define MIN_FRAME_USEC (1000000 / MAX_FPS)

/* REAL FIX 2026-08-05, direct instruction ("this is where we will
 * refactor the xwindow to be chtpm/master ledger compliant" -
 * MUCHI_RANCHER's own work item 2, see MUCHI_RANCHER_DESIGN.md §5 and
 * TILE_PICKER_DESIGN.md §10's own 3-step parity plan): every other
 * CHTPM app in this house gets a real, plain-text history file
 * (auditability) and a real file an external writer - human, script,
 * or AI - can inject a command into, that this window's own event
 * loop actually polls (AI-injection power). Raw override_redirect
 * windows like this one had NEITHER until now. Scoped per-package
 * (own history.txt/interact_relay.txt inside package_dir, not a
 * shared house-wide file like pieces/keyboard/history.txt - this
 * window only ever represents ONE entity, so its own real audit trail
 * belongs right next to that entity's own other real state). */
static char g_history_path[PATH_BUF];
static char g_relay_path[PATH_BUF];

/* Stage 2c PROOF (2026-08-16, direct instruction: "we wanna wire that
 * new context to toolbar and right clik entity and get rid of legacy,
 * so i can check it") - ONE-ENTITY test, see local-2do-15.txt's own
 * entity-context-menu entry. Real integration point: every one of this
 * file's ~20 open_context_menu() call sites already funnels through
 * that ONE function - rather than touch all of them (real risk, this
 * popup engine is deeply coupled to lock/lifecycle bookkeeping every
 * caller assumes), open_context_menu() itself now HIDES the legacy
 * popup (XUnmapWindow, right after its own real creation/lock/grab
 * logic runs completely unchanged) and launches the new khtpm .chtpm-
 * based renderer as the VISIBLE replacement, only when this entity's
 * own <package_dir>/menu.chtpm exists. Every entity without a
 * menu.chtpm keeps the exact original behavior, zero risk. */
static int g_use_khtpm_menu = 0;
static char g_khtpm_menu_pkg_dir[PATH_BUF] = "";
static char g_khtpm_menu_house_root[PATH_BUF] = "";
static pid_t g_khtpm_menu_pid = -1;

static void launch_khtpm_menu(int px, int py) {
    /* kill-then-relaunch, same real single-instance convention every
     * khtpm app's own button.sh already uses - a page-nav GOTO could
     * call open_context_menu() again while a prior instance is still
     * up (real for objects.pdl-style multi-page menus, not exercised
     * by ava's own single-page menu.chtpm yet, but correct to guard
     * for now rather than after it's hit live). */
    if (g_khtpm_menu_pid > 0) {
        kill(g_khtpm_menu_pid, SIGTERM);
        waitpid(g_khtpm_menu_pid, NULL, WNOHANG);
        g_khtpm_menu_pid = -1;
    }
    char bin_path[PATH_BUF];
    snprintf(bin_path, sizeof(bin_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity_menu_render.+x", g_khtpm_menu_house_root);
    /* REAL Stage 5 step 3/4 (2026-08-16, khtpm-merge-how2.md §5d.3) -
     * real, unified <house_root> <chtpm_path> [x] [y] contract (was
     * <package_dir> <house_root> [x] [y]) - khtpm_entity_menu_render's
     * own main() now derives package_dir from dirname(chtpm_path)
     * itself, so this caller just needs to build the real chtpm path
     * once instead of passing the bare dir. */
    char chtpm_path[PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "%s/menu.chtpm", g_khtpm_menu_pkg_dir);
    char px_str[16], py_str[16];
    snprintf(px_str, sizeof(px_str), "%d", px);
    snprintf(py_str, sizeof(py_str), "%d", py);
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, g_khtpm_menu_house_root, chtpm_path, px_str, py_str, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        g_khtpm_menu_pid = pid;
    }
#else
    (void)bin_path;
    (void)chtpm_path;
    (void)px_str;
    (void)py_str;
    /* Entity-menu CHTPM renderer is a later Win pass; keep legacy popup. */
    g_khtpm_menu_pid = -1;
#endif
}

static void append_history(const char *fmt, ...) {
    FILE *f = fopen(g_history_path, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

/* Real, generic method dispatch - same three real cases the live
 * click-handling ButtonPress branch already handles (CLOSE/void/real
 * command), factored out here so interact_relay.txt injection (below,
 * in main()'s own poll loop) can dispatch a method exactly the same
 * way a real click does. OPEN_USER is deliberately NOT handled here -
 * it needs live popup-position context only the real click site has;
 * a relay-injected OPEN_USER is a real, small future gap, not silently
 * faked here. */
static void dispatch_action(const char *action, const char *package_dir, const char *house_root, int *running_ptr) {
    /* Real bug fix (2026-08-11, direct live report: "clicking enter on
     * book menu doesn't work but mouse click does" — turned out to be
     * about "Read" specifically, not "Dir"). This function used to only
     * ever pass ONE argument (package_dir) to the command — but the
     * mouse ButtonPress handler's own separate, DUPLICATED inline copy
     * of this same dispatch logic (added 2026-08-10, "REAL FIX 2026-08-10
     * ... pass it as a real second argument") passes TWO (package_dir,
     * house_root). Any METHOD line updated to rely on the second argument
     * (e.g. book-stack's real "Read" — `sh -c 'H="$1" && ... exec
     * "$H/.../prisc+x" ...'`) worked via mouse (real click path) but
     * silently failed via Enter/RUN_METHOD/ACTIVATE_NAV (all funnel
     * through this ONE function) — $1/$H came up empty, the exec target
     * became a malformed path, sh -c's own exec failed, stderr redirected
     * to /dev/null, zero visible symptom beyond "nothing happened".
     * Fixed: this function now ALWAYS passes both arguments, matching the
     * mouse handler's own (already-correct) convention — the single
     * source of truth for the calling contract, instead of two
     * independently-diverging copies. Every METHOD line's script must
     * tolerate BOTH being present regardless of trigger path now (already
     * true for "Read" and, after the sibling fix wrapping bare system
     * binaries like "Dir" in `sh -c 'exec CMD "$0"'`, true for those too —
     * a bare command that ONLY reads argv[1] and ignores extras, like
     * xdg-open, needs that wrapper; a real script using $0/$1 already
     * works either way). */
    if (strcmp(action, "CLOSE") == 0) {
        *running_ptr = 0;
    } else if (strcmp(action, "void") == 0) {
        /* intentional no-op */
    } else if (strcmp(action, "OPEN_USER") == 0) {
        /* not supported via relay injection - see comment above */
    } else {
#ifdef _WIN32
        /* Do not system() bash. Dir → Explorer. Read → prisc+x.exe + event.pal
         * (Choose-Read/Hear/Tao lives in that pal, not meta.pdl). */
        if (strstr(action, "xdg-open") || strstr(action, "explorer")) {
            ShellExecuteA(NULL, "open", package_dir, NULL, NULL, SW_SHOWNORMAL);
            return;
        }
        if (strstr(action, "prisc") || strstr(action, "pieces/reader")) {
            const char *hr = (house_root && house_root[0]) ? house_root : ".";
            wchar_t wpat[PATH_BUF], wprisc[PATH_BUF], wev[PATH_BUF];
            char ev_a[PATH_BUF];
            snprintf(ev_a, sizeof(ev_a), "%s\\_.monads\\_.book-stack\\pieces\\reader\\event_pkg\\pages\\page_1\\event.pal", hr);
            for (char *q = ev_a; *q; q++) if (*q == '/') *q = '\\';
            MultiByteToWideChar(CP_UTF8, 0, ev_a, -1, wev, PATH_BUF);
            _snwprintf(wpat, PATH_BUF, L"%hs\\101.mutaclsym*", hr);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(wpat, &fd);
            wprisc[0] = 0;
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    wchar_t cand[PATH_BUF];
                    _snwprintf(cand, PATH_BUF, L"%hs\\%s\\system\\prisc+x.exe", hr, fd.cFileName);
                    if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                        wcsncpy(wprisc, cand, PATH_BUF - 1);
                        wprisc[PATH_BUF - 1] = 0;
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            if (wprisc[0] && GetFileAttributesW(wev) != INVALID_FILE_ATTRIBUTES) {
                wchar_t cmd[PATH_BUF * 2];
                _snwprintf(cmd, PATH_BUF * 2 - 1, L"\"%s\" \"%s\"", wprisc, wev);
                STARTUPINFOW si; PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                               CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS,
                               NULL, NULL, &si, &pi);
                if (pi.hThread) CloseHandle(pi.hThread);
                if (pi.hProcess) CloseHandle(pi.hProcess);
            }
            return;
        }
        (void)house_root;
        return;
#else
        char cmd[PATH_BUF * 3];
        /* macOS leg (2026-08-22): METHOD actions are canonical Linux
         * shell strings (book-stack's Dir row is literally
         * `sh -c 'exec xdg-open "$0"'`); macOS has no xdg-open. Rewrite
         * occurrences to the native `open` at runtime — the same
         * translate-at-runtime-not-PDL shape as run_shortcut()'s
         * ktb_portable_darwin() in the manager driver. */
#ifdef __APPLE__
        {
            char fixed[PATH_BUF * 3];
            const char *rd = action;
            char *wr = fixed;
            size_t n = 0;
            while (*rd && n < sizeof(fixed) - 6) {
                if (strncmp(rd, "xdg-open", 8) == 0 &&
                    (rd == action || rd[-1] == ' ' || rd[-1] == '\'' || rd[-1] == '"')) {
                    memcpy((char *)wr, "open", 4); wr += 4; rd += 8; n += 4;
                } else { *wr++ = *rd++; n++; }
            }
            *wr = '\0';
            snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", fixed, package_dir, house_root);
        }
#else
        snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", action, package_dir, house_root);
#endif
        int rc = system(cmd);
        (void)rc;
#endif
    }
}

/* REAL, 2026-08-05, direct instruction ("we should be thinking about
 * adding open desk procs to a livedesk master ledger just for
 * practice... a taskbar-widget at bottom of the screen"): a house-wide
 * "livedesk" master ledger under #.desktop/ (the confirmed real
 * house-wide file-desktop root - @.apps/hikikomorai/hikikomorai-design.md
 * §0 coined "livedesk" for this exact concept) giving every generated
 * entity a real, STABLE index nav number the first time its window
 * ever opens - reused, not reassigned, on every later relaunch (so a
 * future "jump to entity N" convention has a real, permanent address to
 * jump to, matching the digit-jump shape cli_io's own field convention
 * already uses). A separate, live "currently open" registry
 * (#.desktop/livedesk_open.txt) is what the new taskbar widget (below,
 * ensure_taskbar_running()/tp_taskbar.c) actually polls - entries added
 * on window-open, removed on clean window-close, so its tabs track
 * which entities are ACTUALLY live right now, not full history. */
static void dirname_step(const char *in, char *out, size_t out_sz) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", in);
    char *d = dirname(tmp);
    snprintf(out, out_sz, "%s", d);
}

/* Portable self-binary path (macOS leg 2026-08-22): /proc/self/exe does
 * not exist on macOS; _NSGetExecutablePath() is the Apple equivalent and
 * may return a relative path when the binary was launched by bare name,
 * so resolve through realpath(). Linux/other keep readlink(/proc/self/exe)
 * byte-for-byte as before. Returns 1 on success. */
static int self_exe_path(char *out, size_t out_sz) {
#ifdef __APPLE__
    uint32_t sz = (uint32_t)out_sz;
    if (_NSGetExecutablePath(out, &sz) != 0) return 0;
    char resolved[PATH_BUF];
    if (realpath(out, resolved)) snprintf(out, out_sz, "%s", resolved);
    return 1;
#else
    ssize_t slen = readlink("/proc/self/exe", out, out_sz - 1);
    if (slen <= 0) return 0;
    out[slen] = '\0';
    return 1;
#endif
}

/* House-root discovery: walk UP from this binary's own real install dir
 * (found via /proc/self/exe) until a directory containing BOTH #.desktop/
 * and &.widgits/ is found - the same marker-walk khtpm_vars.sh uses, so it
 * survives any relocation (the binary used to live at
 * <house>/&.widgits/tile-picker/ops/+x/ and was consolidated into
 * *.monads/*.livedesk-taskbar/ops/+x/ - the fixed dirname-step climb is
 * gone, position no longer matters). */
static void resolve_livedesk_paths(char *ops_dir_out, size_t ops_sz, char *house_root_out, size_t house_sz) {
    ops_dir_out[0] = '\0';
    house_root_out[0] = '\0';
    char self_path[PATH_BUF];
    if (!self_exe_path(self_path, sizeof(self_path))) return;
    char step[PATH_BUF];
    dirname_step(self_path, step, sizeof(step)); /* .../ops/+x */
    snprintf(ops_dir_out, ops_sz, "%s", step);
    /* Walk up from ops_dir until a dir holds both #.desktop/ and &.widgits/. */
    for (;;) {
        char desk[PATH_BUF], widg[PATH_BUF];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
#ifdef _WIN32
        for (char *p = desk; *p; p++) if (*p == '/') *p = '\\';
        for (char *p = widg; *p; p++) if (*p == '/') *p = '\\';
#endif
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
            snprintf(house_root_out, house_sz, "%s", step);
            return;
        }
        char *slash = strrchr(step, '/');
#ifdef _WIN32
        char *bslash = strrchr(step, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
        if (!slash || slash == step) break; /* reached root, give up */
        *slash = '\0';
    }
#ifdef _WIN32
    if (!house_root_out[0] && access("#.desktop", F_OK) == 0 && access("&.widgits", F_OK) == 0)
        snprintf(house_root_out, house_sz, ".");
#endif
}

/* Real, stable index: scans the ledger for a prior ASSIGN row whose
 * PATH= matches this exact package_dir and reuses that INDEX if found;
 * only assigns a brand-new one (from a real, persisted counter file)
 * the first time this package_dir has ever opened. Also writes
 * <package_dir>/livedesk_index.txt so any tool can read "this entity's
 * own index" directly without re-scanning the whole ledger. */
static int ensure_livedesk_index(const char *package_dir, const char *house_root) {
    char ledger_path[PATH_BUF], counter_path[PATH_BUF], idx_path[PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/#.desktop/livedesk_master_ledger.txt", house_root);
    snprintf(counter_path, sizeof(counter_path), "%s/#.desktop/livedesk_next_index.txt", house_root);
    snprintf(idx_path, sizeof(idx_path), "%s/livedesk_index.txt", package_dir);

    FILE *lf = fopen(ledger_path, "r");
    if (lf) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), lf)) {
            char *pathmark = strstr(line, "PATH=");
            if (!pathmark) continue;
            char pval[PATH_BUF];
            snprintf(pval, sizeof(pval), "%s", pathmark + 5);
            pval[strcspn(pval, "\r\n")] = '\0';
            if (strcmp(pval, package_dir) == 0) {
                char *idxmark = strstr(line, "INDEX=");
                if (idxmark) {
                    int idx = atoi(idxmark + 6);
                    fclose(lf);
                    FILE *out = fopen(idx_path, "w");
                    if (out) { fprintf(out, "%d\n", idx); fclose(out); }
                    return idx;
                }
            }
        }
        fclose(lf);
    }

    int next = 1;
    FILE *cf = fopen(counter_path, "r");
    if (cf) { if (fscanf(cf, "%d", &next) != 1) next = 1; fclose(cf); }
    FILE *cw = fopen(counter_path, "w");
    if (cw) { fprintf(cw, "%d\n", next + 1); fclose(cw); }

    char pkgcopy[PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *ent_name = basename(pkgcopy);
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    FILE *la = fopen(ledger_path, "a");
    if (la) {
        fprintf(la, "[%s] ASSIGN INDEX=%d ENTITY=%s PATH=%s\n", ts, next, ent_name, package_dir);
        fclose(la);
    }
    FILE *out = fopen(idx_path, "w");
    if (out) { fprintf(out, "%d\n", next); fclose(out); }
    return next;
}

/* Real liveness probe (kill(pid,0)) - forward-declared here, defined
 * alongside nav_claim_rows() below, shared by both self-healing prune
 * passes (see that function's own header comment for the full 2026-08-06
 * diagnosis this exists to fix). */
static int pid_is_alive(int pid);

/* REAL FIX 2026-08-06, direct-caught regression during the SAME session
 * ("book-stack missing from livedesk_open.txt after a simultaneous
 * multi-launch"): the self-healing prune above turned a single atomic
 * O_APPEND write (safe under concurrent writers - POSIX guarantees
 * small appends don't interleave) into a read-prune-write-rename cycle,
 * which is NOT atomic across processes - two entities launching at once
 * can both read the same "before" state, and whichever renames last
 * silently discards the other's own addition. Real fix: a real
 * cross-process mutex (flock(), same real mechanism as the popup lock
 * above) around the whole read+write critical section in every
 * function that does this read-modify-write-rename dance. */
static int g_registry_lock_fd = -1;

static void registry_lock_acquire(const char *house_root) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd < 0) {
        char lock_path[PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_registry.lock", house_root);
        g_registry_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_EX);
}

static void registry_lock_release(void) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_UN);
}

/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13") - see nav_claim_rows()'s own header comment above for
 * the full diagnosis. Same self-healing shape here: prune any dead-PID
 * or malformed line as part of adding the new one, rather than trusting
 * append-only history that never gets checked against reality. */
static void livedesk_registry_add(const char *house_root, const char *package_dir, int index, pid_t pid) {
    char reg_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    char pkgcopy[PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *ent_name = basename(pkgcopy);

    registry_lock_acquire(house_root);
#ifdef _WIN32
    {
        FILE *w = fopen(reg_path, "a");
        if (w) {
            fprintf(w, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n", (int)pid, index, ent_name, package_dir);
            fclose(w);
        }
    }
#else
    FILE *f = fopen(reg_path, "r");
    FILE *w = fopen(tmp_path, "w");
    if (f && w) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *pp = strstr(line, "PID=");
            int line_pid = pp ? atoi(pp + 4) : 0;
            if (!pp || !pid_is_alive(line_pid)) continue;
            fputs(line, w);
        }
    }
    if (f) fclose(f);
    if (w) {
        fprintf(w, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n", (int)pid, index, ent_name, package_dir);
        fclose(w);
        rename(tmp_path, reg_path);
    }
#endif
    registry_lock_release();
}

static void livedesk_registry_remove(const char *house_root, pid_t pid) {
    char reg_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(reg_path, "r");
    if (!f) { registry_lock_release(); return; }
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); registry_lock_release(); return; }
    char marker[32];
    snprintf(marker, sizeof(marker), "PID=%d|", (int)pid);
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, marker, strlen(marker)) == 0) continue;
        fputs(line, w);
    }
    fclose(f);
    fclose(w);
    rename(tmp_path, reg_path);
    registry_lock_release();
}

/* Direct instruction: "it can open when livedesk using app is open, and
 * if one is already open just add the tab of that app to the taskbar" -
 * a real PID-file singleton check (kill(pid,0), same liveness-probe
 * convention this codebase already uses in spirit elsewhere), not a
 * naive "spawn every time" that would pile up duplicate bars.
 *
 * REAL FIX 2026-08-05, direct correction ("why dont i see task bar in
 * &.widgits dir? thats where its ment to be... its not a member of
 * tile-picker"): the taskbar is its own real widget
 * (*.monads/*.livedesk-taskbar/), matching every other real widget's own
 * top-level layout (event-editor/, event-ez/, tile-picker/ itself) -
 * NOT nested inside tile-picker/ops/ just because tp_desktop_window.c
 * happens to be the one that launches it. Located via house_root
 * (already resolved above) plus this fixed, real house-relative path,
 * not via ops_dir (which would have wrongly implied "lives next to
 * tp_desktop_window.c"). */
static void ensure_taskbar_running(const char *house_root) {
#ifdef _WIN32
    if (x11_process_running("khtpm_strip_parser")) return;
    char exe[PATH_BUF];
    DWORD n = GetModuleFileNameA(NULL, exe, PATH_BUF);
    if (!n) return;
    char *slash = strrchr(exe, '\\');
    if (!slash) return;
    snprintf(slash + 1, PATH_BUF - (size_t)(slash + 1 - exe), "khtpm_strip_parser.exe");
    x11_spawn_cwd(exe, house_root && house_root[0] ? house_root : ".");
    return;
#else
    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/#.desktop/livedesk_taskbar.pid", house_root);
    int alive = 0;
    FILE *f = fopen(pid_path, "r");
    if (f) {
        int pid = 0;
        if (fscanf(f, "%d", &pid) == 1 && pid > 0 && kill((pid_t)pid, 0) == 0) alive = 1;
        fclose(f);
    }
    /* REAL FIX 2026-08-06: pid-file race with $.crypts concurrent launch
     * spawned DUPLICATE taskbars (two processes, grab/focus chaos). Also
     * scan /proc for an already-running tp_taskbar for this house. */
    if (!alive) {
        DIR *pd = opendir("/proc");
        if (pd) {
            struct dirent *ent;
            while ((ent = readdir(pd)) != NULL) {
                if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
                char cpath[64];
                snprintf(cpath, sizeof(cpath), "/proc/%s/cmdline", ent->d_name);
                FILE *cf = fopen(cpath, "r");
                if (!cf) continue;
                char cmdbuf[PATH_BUF * 2];
                size_t n = fread(cmdbuf, 1, sizeof(cmdbuf) - 1, cf);
                fclose(cf);
                if (n == 0) continue;
                cmdbuf[n] = '\0';
                for (size_t i = 0; i < n; i++) if (cmdbuf[i] == '\0') cmdbuf[i] = ' ';
                /* REAL FIX 2026-08-11, direct live report "it opened both
                 * toolbars" (entities each relaunched legacy tp_taskbar
                 * even with the new khtpm strip taskbar already up): this
                 * scan only ever matched the literal substring
                 * "tp_taskbar", so it never recognized khtpm_strip_parser
                 * as "a taskbar is already running for this house" —
                 * every entity independently concluded none was running
                 * and launched legacy on top of it. Broadened to also
                 * match "khtpm_strip_parser", the taskbar's own process
                 * name (parser is the long-lived, user-visible half; its
                 * forked manager child living or dying tracks it 1:1).
                 * "tp_taskbar" kept as a harmless no-op safety net.
                 *
                 * REAL UPDATE 2026-08-11, same session, later: legacy
                 * tp_taskbar.c retired (archived to
                 * *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip,
                 * originals deleted) — khtpm_strip_parser.+x is the real,
                 * only taskbar now. The fallback launch command below
                 * used to hardcode tp_taskbar.+x's path, which no longer
                 * exists on disk at all; updated to launch khtpm's own
                 * (renamed, no "_test" suffix) binary instead. */
                if ((strstr(cmdbuf, "tp_taskbar") || strstr(cmdbuf, "khtpm_strip_parser"))
                    && strstr(cmdbuf, house_root)) {
                    alive = 1;
                    break;
                }
            }
            closedir(pd);
        }
    }
    if (!alive) {
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_strip_parser.+x' '%s' >/dev/null 2>&1 &",
                 house_root, house_root);
        int rc = system(cmd);
        (void)rc;
    }
#endif /* !_WIN32 */
}

/* REAL, 2026-08-05, direct correction (see TILE_PICKER_DESIGN.md §13 -
 * "brackets are ment for focuz not holding numbers... it should look
 * up first and increment indexes"): a SHARED, LIVE claim pool for
 * every real "[N]" shown on screen right now, house-wide -
 * `#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt` - deliberately separate from
 * ensure_livedesk_index()'s own PERMANENT ledger above (that one never
 * changes across relaunches; this one is pure live/ephemeral, numbers
 * free up and get reused the moment whatever held them closes). A
 * context menu claims one contiguous NAV range for its own rows the
 * moment it opens (nav_claim_rows()), releases that same range the
 * moment it closes (nav_release_pid()) - the taskbar (a separate real
 * process, *.monads/*.livedesk-taskbar/ops/tp_taskbar.c) claims its own
 * tab numbers from this exact same pool, so a tab and a menu row can
 * never show the same live number at once. */
/* action widened 2026-08-04, direct instruction (fo-menu-sys.md's real
 * convention: VALUE is a real, directly-executable command, e.g. a full
 * "gnome-terminal -- \"<long absolute house path>/chat.sh\"" line, not a
 * short keyword) - 64 bytes silently truncated/dropped real rows built
 * from this house's own long, emoji-heavy absolute paths.
 *
 * Moved up here (2026-08-05) from its original spot right before
 * load_methods() further down - nav_claim_rows() below needs the real
 * type, not just a forward declaration. */
typedef struct { char label[64]; char action[PATH_BUF]; } MethodItem;

/* REAL FIX 2026-08-06, direct report ("index of navs maybe off, because
 * its not reindexing if one context closes and another opens"): base
 * used to be max_nav+1 - a real, confirmed bug, monotonically growing
 * FOREVER across a whole session, never reclaiming numbers
 * nav_release_pid() already freed on close. Real fix: scan every
 * currently-claimed NAV into a used[] set, then take the lowest
 * contiguous run of n free slots starting from 1 - the same real
 * "smallest available" convention wraith-alpha's own focus_index
 * validation (initialize_focus()/is_navigable()) uses to never trust a
 * stale index, applied here as smallest-free-range allocation instead. */
#define NAV_TRACK_MAX 4096
/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13" - traced live: livedesk_nav_claims.txt/livedesk_open.txt
 * were full of entries for PIDs that no longer exist - some from
 * ordinary process churn, one line even structurally corrupted
 * (no "PID=" at all), proof two processes raced an unsynchronized
 * write to the same shared file at once). Neither file was ever
 * self-healing - a stale/corrupt line just sat there inflating every
 * future nav_claim_rows() count forever, exactly the "phantom index"
 * bug the report describes. Real fix: verify every claim's PID is
 * ACTUALLY alive (kill(pid,0)) as part of the same read pass, and
 * rewrite the file with only live, well-formed entries kept - so this
 * self-heals on every single popup open, regardless of how a stale
 * entry got there in the first place. */
static int pid_is_alive(int pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
}

static int nav_claim_rows(const char *house_root, pid_t pid, const char *package_dir, MethodItem *items, int n) {
    char claims_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp", house_root);
    static char used[NAV_TRACK_MAX];
    memset(used, 0, sizeof(used));
    registry_lock_acquire(house_root); /* covers BOTH writes below - the prune-rename and the append are one critical section */
    FILE *f = fopen(claims_path, "r");
    FILE *w = fopen(tmp_path, "w");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *pp = strstr(line, "PID=");
            int line_pid = pp ? atoi(pp + 4) : 0;
            if (!pp || !pid_is_alive(line_pid)) continue; /* drop malformed/dead - self-heal */
            if (w) fputs(line, w);
            char *p = strstr(line, "NAV=");
            if (p) {
                int v = atoi(p + 4);
                if (v > 0 && v < NAV_TRACK_MAX) used[v] = 1;
            }
        }
        fclose(f);
    }
    if (w) { fclose(w); rename(tmp_path, claims_path); }
    int base = 1;
    for (int cand = 1; cand < NAV_TRACK_MAX - n; cand++) {
        int fits = 1;
        for (int i = 0; i < n; i++) {
            if (used[cand + i]) { fits = 0; break; }
        }
        if (fits) { base = cand; break; }
    }
    FILE *a = fopen(claims_path, "a");
    if (a) {
        for (int i = 0; i < n; i++) {
            fprintf(a, "KIND=row|PID=%d|NAV=%d|ROW=%d|LABEL=%s|PATH=%s\n",
                    (int)pid, base + i, i, items[i].label, package_dir);
        }
        fclose(a);
    }
    registry_lock_release();
    return base;
}

static void nav_release_pid(const char *house_root, pid_t pid) {
    char claims_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) { registry_lock_release(); return; }
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); registry_lock_release(); return; }
    char marker[32];
    snprintf(marker, sizeof(marker), "|PID=%d|", (int)pid);
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, marker)) continue;
        fputs(line, w);
    }
    fclose(f);
    fclose(w);
    rename(tmp_path, claims_path);
    registry_lock_release();
}

static void read_glyph(const char *package_dir, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

/* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER monsters need
 * a real 2x2-tile footprint): reads a real, optional
 * "STATE | footprint_tiles | N" row from the package's own meta.pdl -
 * same SECTION|KEY|VALUE parse shape load_methods() below already
 * uses. Defaults to 1 (this file's own original, unconditional 64px
 * size) when the row is absent, so every existing pet/asa/ava package
 * is completely unaffected. */
/* REAL 2026-08-07, direct-caught bug ("muchi 4TSG has no Close
 * button"): m8's objects.pdl began with a UTF-8 BOM (EF BB BF), so its
 * first line "PAGE | main" failed strncmp(line,"PAGE",4) and the whole
 * main page (the one carrying Feed/Menu/Play/Close/Cancel) silently
 * vanished - the menu instead showed the NEXT page (activities), which
 * has no Close. Every PDL reader here opens package data files that are
 * often saved by Windows editors (which attach a BOM). This helper eats
 * a leading BOM right after fopen so the very first line parses like
 * any other. */
static FILE *pdl_open(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }
    return f;
}

static int read_footprint_tiles(const char *package_dir) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 1;
    char line[PATH_BUF];
    int result = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "footprint_tiles";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        int v = atoi(end + 1);
        if (v > 0) result = v;
        break;
    }
    fclose(f);
    return result;
}

/* REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a) - reads an optional
 * "GRID | cell_px | N" row from #.desktop/desk_grid.pdl (real,
 * house-wide - NOT per-package, since the desktop grid is one shared
 * thing every entity snaps to, unlike footprint_tiles which is
 * per-entity). Same SECTION|KEY|VALUE parse shape as
 * read_footprint_tiles() just above, adapted for a "GRID" section
 * instead of "STATE". Defaults to 80 (this file's own original
 * hardcoded GRID_CELL_PX value) when the file/row is absent. */
static int read_grid_cell_px(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = pdl_open(path);
    if (!f) return 80;
    char line[PATH_BUF];
    int result = 80;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "cell_px";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        int v = atoi(end + 1);
        if (v > 0) result = v;
        break;
    }
    fclose(f);
    return result;
}

/* REAL 2026-08-07, direct instruction ("make them configurable via
 * config / .pdl file so i can easily experiment with them"): reads the
 * context-menu guard rows from the package's own meta.pdl (same
 * SECTION|KEY|VALUE shape load_methods()/read_footprint_tiles() use).
 * Keys (STATE section):
 *   STATE | menu_stay_open | 1   outside/repeat clicks keep the menu open
 *   STATE | grab_pointer   | 1   modal pointer grab while a menu is open
 *   STATE | grab_keyboard  | 1   modal keyboard grab while a menu is open
 *   STATE | grab_pointer_while_stay_open | 0   see open_context_menu()'s
 *                                  own comment on this key — lets a user
 *                                  opt back INTO a pointer grab even with
 *                                  menu_stay_open=1, for entities whose
 *                                  row clicks aren't reliably reaching a
 *                                  non-grabbed override-redirect popup
 *                                  under this house's Wayland/XWayland
 *                                  setup (direct report 2026-08-11: "it
 *                                  works clicking enter, but not mouse
 *                                  clicking"). Trades away "rest of the
 *                                  desk stays clickable while this menu's
 *                                  open" (the ORIGINAL reason grabbing was
 *                                  disabled for stay-open menus,
 *                                  2026-08-07) — real tradeoff, exposed as
 *                                  a knob instead of picking one hardcoded
 *                                  answer for every entity.
 * Missing rows keep the compile-time defaults. Called on startup AND on
 * every right-click reload, so a human edits meta.pdl and the very next
 * menu open picks it up - no rebuild, no restart.
 *
 * REAL BUG FIX (2026-08-11, found while adding the new key above, same
 * off-by-one class hit repeatedly elsewhere this session): the
 * "menu_stay_open" length check was klen==13 — the string is genuinely
 * 14 characters (verified: `printf '%s' "menu_stay_open" | wc -c`). This
 * meant STATE|menu_stay_open|... NEVER matched, at all — the key was
 * permanently stuck at its compile-time default (1) no matter what a
 * human set in meta.pdl, exactly the "looks configurable but silently
 * isn't" trap this whole config system exists to avoid. Fixed to 14. */
static int g_grab_pointer_while_stay_open = 0;

static void read_menu_config(const char *package_dir) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        int v = atoi(end + 1);
        size_t klen = (size_t)(label_end - p);
        if (klen == 14 && strncmp(p, "menu_stay_open", 14) == 0)
            g_menu_stay_open = v ? 1 : 0;
        else if (klen == 12 && strncmp(p, "grab_pointer", 12) == 0)
            g_grab_pointer = v ? 1 : 0;
        else if (klen == 13 && strncmp(p, "grab_keyboard", 13) == 0)
            g_grab_keyboard = v ? 1 : 0;
        else if (klen == 28 && strncmp(p, "grab_pointer_while_stay_open", 28) == 0)
            g_grab_pointer_while_stay_open = v ? 1 : 0;
    }
    fclose(f);
}

/* REAL FIX 2026-08-05, direct instruction ("they should have a unique
 * alpha numerica 4digit combo"): piece_id alone (the package dir's own
 * basename) isn't guaranteed unique - two entities could share a name.
 * button.sh's own ensure_package() now generates a real, persistent
 * 4-char alphanumeric instance_id once (instance_id.txt, seed-once-
 * don't-clobber, same convention as glyph/created_at) - read it back
 * here the same way read_glyph() already does. */
static void read_instance_id(const char *package_dir, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/instance_id.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int package_still_exists(const char *package_dir) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    struct stat st;
    return stat(path, &st) == 0;
}

/* Cheap, deterministic glyph->color hash so different tile stamps are
 * visually distinct even before/without a loaded font. */
static void glyph_color(char g, float *r, float *gg, float *b) {
    unsigned int h = (unsigned int)(unsigned char)g * 2654435761u;
    *r = ((h >> 0) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *gg = ((h >> 8) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *b = ((h >> 16) & 0xFF) / 255.0f * 0.7f + 0.2f;
}

/* REAL, NEW 2026-08-04, direct instruction ("next test, it should be a
 * real tile" - i.e. the window must show the actual glyph, not just a
 * colored square). No sprite/texture pipeline exists for tile-picker yet
 * (that's the still-open glyph-widening/rendering question in both
 * design docs), so this uses the same glXUseXFont technique GLUT's own
 * bitmap-font helpers are built on: load a real X core font, convert its
 * glyphs into GL display lists, draw with glCallLists. "fixed"-family X
 * core fonts are always present on any X11 install (confirmed via
 * xlsfonts on this machine) - no extra font file dependency. Returns 0
 * and leaves *base_list unset if no such font can be loaded (caller falls
 * back to the plain color square, same as before this fix). */
static int g_font_loaded = 0;
static XFontStruct *g_font_info = NULL;

/* REAL FIX 2026-08-05, direct report ("its having problems with the
 * chinese"): all popup text (context menu rows, Show Text) was drawn
 * with plain XDrawString - Latin-1 only, real X core-font limitation,
 * so the Chinese half of book-stack's Bible verses rendered as boxes/
 * garbage. Real fix: Xutf8DrawString against a real multi-byte XFontSet
 * that includes a CJK-capable base font (confirmed present on this
 * house's system via `fc-list :lang=zh` / `xlsfonts` - GNU unifont's
 * own "-misc-fixed-*-iso10646-1" covers CJK, used here alongside a
 * plain Latin fallback so ASCII stays crisp). Built once in main(),
 * used by every popup draw site below instead of XDrawString. */
static XFontSet g_popup_fontset = NULL;

static void load_popup_fontset(Display *dpy) {
    char **missing = NULL;
    int n_missing = 0;
    char *def_str = NULL;
    const char *base =
        "-misc-fixed-medium-r-normal--18-120-100-100-c-90-iso10646-1,"
        "-*-fixed-medium-r-normal--18-*-*-*-*-*-iso10646-1,"
        "-*-*-medium-r-normal--*-*-*-*-*-*-iso10646-1";
    g_popup_fontset = XCreateFontSet(dpy, base, &missing, &n_missing, &def_str);
    if (missing) XFreeStringList(missing);
    if (!g_popup_fontset) {
        g_popup_fontset = XCreateFontSet(dpy, "fixed", &missing, &n_missing, &def_str);
        if (missing) XFreeStringList(missing);
    }
}

static void popup_draw_text(Display *dpy, Drawable d, GC gc, int x, int y, const char *s) {
    if (g_popup_fontset) {
        Xutf8DrawString(dpy, d, g_popup_fontset, gc, x, y, s, (int)strlen(s));
    } else {
        XDrawString(dpy, d, gc, x, y, s, (int)strlen(s));
    }
}

/* Pixel width of UTF-8 popup label text (same fontset as popup_draw_text). */
static int popup_text_px(Display *dpy, const char *s) {
    (void)dpy;
    if (!s || !*s) return 0;
    if (g_popup_fontset) {
        XRectangle ink, logical;
        Xutf8TextExtents(g_popup_fontset, s, (int)strlen(s), &ink, &logical);
        return logical.width > 0 ? logical.width : ink.width;
    }
    /* Fallback: ~9px/glyph for the 18px fixed face we load. */
    return (int)strlen(s) * 9;
}

static int load_glyph_font(Display *dpy) {
    g_font_info = XLoadQueryFont(dpy, "-sony-fixed-medium-r-normal--24-170-100-100-c-120-iso8859-1");
    if (!g_font_info) g_font_info = XLoadQueryFont(dpy, "fixed");
    if (!g_font_info) return 0;
    return 1;
}

/* Draws directly into the compose buffer via plain XDrawString (no GL
 * display lists needed once glXUseXFont is gone - XLoadQueryFont's own
 * XFontStruct is enough for a GC-based draw). */
static void draw_glyph_rgb(Display *dpy, Drawable buf, GC gc, char g) {
    if (!g_font_info) return;
    XSetFont(dpy, gc, g_font_info->fid);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    int cw = WIN_PX / 2, ch = (g_font_info->ascent + g_font_info->descent);
    int x = (WIN_PX - cw) / 2;
    int y = (WIN_PX + g_font_info->ascent - g_font_info->descent) / 2;
    (void)ch;
    XDrawString(dpy, buf, gc, x, y, &g, 1);
}

/* REAL FIX 2026-08-04, direct instruction ("do u see how egg-pal creates
 * the same emoji that user picked?"): load_sprite/upload_texture/
 * draw_sprite below are the same real technique 01.muchi-pals's
 * egg_window.c already uses to render a picked emoji as a real textured
 * quad, not text. tp_place_desktop.c now generates <package_dir>/
 * sprite.csv via the same emoji_gen_atlas.+x -> emoji_xtract.+x pipeline
 * hatch_egg.c uses; this window loads that CSV if present and draws the
 * real emoji texture instead of the glyph-hashed color square + font
 * fallback (which remains the fallback if sprite.csv is missing/failed
 * to generate). */
static int g_has_sprite = 0;
/* REAL FIX 2026-08-04, direct instruction ("asa's transparency isn't
 * fixed, just background is now red"): GL_BLEND alone only blends
 * within the GL scene against whatever this window itself already drew
 * (the glClearColor fill) - it does NOT make the real X11 WINDOW
 * transparent to the desktop behind it. Plain X11 windows are opaque
 * rectangles by default; real per-pixel desktop transparency needs the
 * X11 Shape Extension to cut the window's actual SHAPE to match the
 * sprite's alpha - exactly what egg_window.c's own build_shape_mask()
 * already does (ported verbatim below, POSIX branch only, matching this
 * file's own X11/GLX-only scope). Sprite pixels are now kept around
 * (not freed after texture upload) so the mask can be built from real
 * alpha data - same "kept around... so the shape mask can be rebuilt"
 * comment egg_window.c's own header already has for the identical
 * reason. */
static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;

static int load_sprite_csv(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }

    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return 0; }

    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int rr, gg, bb, aa;
        if (sscanf(line, "%d,%d,%d,%d", &rr, &gg, &bb, &aa) == 4) {
            pixels[count * 4 + 0] = (unsigned char)rr;
            pixels[count * 4 + 1] = (unsigned char)gg;
            pixels[count * 4 + 2] = (unsigned char)bb;
            pixels[count * 4 + 3] = (unsigned char)aa;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return 0; }

    g_sprite_pixels = pixels; /* kept - shape mask AND the RGB draw below both read straight from this, no GL texture upload needed */
    g_sprite_res = res;
    return 1;
}

/* Verbatim port of egg_window.c's own build_shape_mask() (POSIX/X11
 * branch) - builds the window's real clip shape from the sprite's own
 * alpha channel (upscaled nearest-neighbor to the window's pixel size),
 * so the desktop genuinely shows through transparent pixels instead of
 * this window's own opaque background fill. */
static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
    XSetForeground(dpy, mask_gc, 1);
    if (g_sprite_pixels) {
        for (int y = 0; y < WIN_PX; y++) {
            int sy = (y * g_sprite_res) / WIN_PX;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            for (int x = 0; x < WIN_PX; x++) {
                int sx = (x * g_sprite_res) / WIN_PX;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] > 127) {
                    XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
                }
            }
        }
    } else {
        XFillArc(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX, 0, 360 * 64);
    }
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

/* Direct alpha-blended pixel blit into the compose buffer, nearest-
 * neighbor scaled sprite->window exactly like build_shape_mask()'s own
 * loop above (same sx/sy math, so the drawn pixels and the window's
 * real clip shape can never drift apart) - replaces the GL texture-
 * quad upload/draw entirely. Ported technique: events-hq's own
 * draw_entity_glyph() (khtpm_events_hq_render.c) already does this
 * exact alpha-over-background blit for a real sprite.csv. bg_r/g/b are
 * 0-255 ints (glyph_color()'s own 0..1 floats *255, already computed by
 * the caller) - blended UNDER the sprite's own alpha, same as GL_BLEND/
 * GL_SRC_ALPHA did before. */
static void draw_sprite_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;
    for (int y = 0; y < WIN_PX; y++) {
        int sy = (y * g_sprite_res) / WIN_PX;
        if (sy >= g_sprite_res) sy = g_sprite_res - 1;
        for (int x = 0; x < WIN_PX; x++) {
            int sx = (x * g_sprite_res) / WIN_PX;
            if (sx >= g_sprite_res) sx = g_sprite_res - 1;
            unsigned char *p = &g_sprite_pixels[(sy * g_sprite_res + sx) * 4];
            int a = p[3];
            if (a <= 0) continue;
            int r = (p[0] * a + bg_r * (255 - a)) / 255;
            int g = (p[1] * a + bg_g * (255 - a)) / 255;
            int b = (p[2] * a + bg_b * (255 - a)) / 255;
            XSetForeground(dpy, gc, ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, x, y);
        }
    }
}

/* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever they
 * click... the phymoji will appear"): if tp_place_desktop.c already
 * wrote a desktop_pos.txt (a real click point resolved by
 * tp_arm_placer.c), spawn there instead of the fixed grid default -
 * "wherever they click" means the window's own life should start at
 * that point, not just be draggable to it afterward. */
static int read_initial_pos(const char *package_dir, int *out_x, int *out_y) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int x = -1, y = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) x = atoi(line + 2);
        else if (strncmp(line, "y=", 2) == 0) y = atoi(line + 2);
    }
    fclose(f);
    if (x < 0 || y < 0) return 0;
    *out_x = x; *out_y = y;
    return 1;
}

/* REAL, NEW 2026-08-04, direct instruction ("add the context menus that
 * already exist from egg-pal to these by default"): a real, data-driven
 * popup context menu, modeled directly on egg_window.c's own
 * open_context_menu()/draw_context_menu()/close_context_menu() (same
 * override_redirect popup + XGrabPointer-on-open technique), but
 * reading its item list from the package's own meta.pdl METHOD rows
 * instead of egg_window's single hardcoded "Close" - see
 * TILE_PICKER_DESIGN.md §4.5. tp_place_desktop.c writes a default
 * METHOD row ("Close") into every new package, so the default behavior
 * matches egg_window's own exactly - this just makes the list
 * extensible (more methods appended to meta.pdl later, e.g. "Open Event
 * Editor" once event-editor exists, need no renderer changes here). */
/* REAL FIX 2026-08-05: asa/ava's methods.pdl was already AT this cap
 * (8: Chat/Events/Events(mock)/Play/Stop/Ledger/Close/Cancel) before
 * adding "Events (ez)" - load_methods() silently drops anything past
 * MAX_METHODS with no error, so a 9th row would have been invisible
 * with zero warning. Bumped with real headroom, not just +1. */
#define MAX_METHODS 12
#define POPUP_ROW_H 28
/* REAL FIX 2026-08-06, user: "menu screen is too thin i cant see everything"
 * — fixed 160px clipped RPG Menu rows (XP / qolq / Level lines with nav
 * prefixes). Width is now content-measured (see measure_context_popup_w /
 * g_popup_w); these are floor/ceiling only. */
#define POPUP_W_MIN 180
#define POPUP_W_MAX 640
#define POPUP_W POPUP_W_MIN /* legacy alias: text-popup floor, input defaults */
static int g_popup_w = POPUP_W_MIN;

static int load_methods(const char *package_dir, MethodItem *items, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t llen = (size_t)(label_end - p);
        if (llen == 0 || llen >= sizeof(items[0].label)) continue;
        memcpy(items[n].label, p, llen);
        items[n].label[llen] = '\0';

        char *a = end + 1;
        while (*a == ' ') a++;
        char *a_end = a + strcspn(a, "\r\n");
        while (a_end > a && a_end[-1] == ' ') a_end--;
        size_t alen = (size_t)(a_end - a);
        if (alen == 0 || alen >= sizeof(items[0].action)) continue;
        memcpy(items[n].action, a, alen);
        items[n].action[alen] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

/* REAL, 2026-08-05, direct instruction ("context menus should be much
 * more customizable, robust, even having user input, href, back, etc
 * just like chtpm" - see TILE_PICKER_DESIGN.md §11 for the wraith-alpha
 * research this is modeled on, scoped down to v1: real multi-PAGE
 * navigation + real text input, reusing MethodItem's own label/action
 * shape rather than inventing a parallel one - free positioning (x/y/w/h/z)
 * is deliberately deferred, not needed while every menu here is still a
 * single-column row list). Optional <package_dir>/objects.pdl:
 *   PAGE | <name>
 *   OBJECT | label=<text> | action=<value>
 * action is exactly methods.pdl's own real convention (a real command,
 * "CLOSE", "void") PLUS two new reserved forms: "GOTO:<page>" (push
 * current page, switch) and "BACK" (pop the page stack) for href/back
 * navigation, and "STATE:<key>" for a real input row (activates text
 * entry, committed value written to <package_dir>/<key>.txt on Escape -
 * same click-to-activate/Escape-to-commit shape this house's own cli_io
 * field convention already uses, not a new one). Absent entirely =
 * existing single-page methods.pdl behavior, completely unchanged. */
#define MAX_PAGES 8
typedef struct {
    char name[32];
    MethodItem items[MAX_METHODS];
    int n_items;
} ObjPage;

static int load_objects(const char *package_dir, ObjPage *pages, int max_pages) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/objects.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[PATH_BUF];
    int n_pages = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PAGE", 4) == 0) {
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = p + strcspn(p, "\r\n");
            while (end > p && end[-1] == ' ') end--;
            size_t nlen = (size_t)(end - p);
            if (nlen == 0 || n_pages >= max_pages) continue;
            if (nlen >= sizeof(pages[n_pages].name)) nlen = sizeof(pages[n_pages].name) - 1;
            memcpy(pages[n_pages].name, p, nlen);
            pages[n_pages].name[nlen] = '\0';
            pages[n_pages].n_items = 0;
            n_pages++;
        } else if (strncmp(line, "OBJECT", 6) == 0 && n_pages > 0) {
            ObjPage *pg = &pages[n_pages - 1];
            if (pg->n_items >= MAX_METHODS) continue;
            MethodItem *item = &pg->items[pg->n_items];
            item->label[0] = '\0';
            item->action[0] = '\0';
            char *tok = line;
            while ((tok = strchr(tok, '|')) != NULL) {
                tok++;
                while (*tok == ' ') tok++;
                char *tok_end = strchr(tok, '|');
                size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
                while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
                if (strncmp(tok, "label=", 6) == 0) {
                    size_t l = tlen - 6;
                    if (l >= sizeof(item->label)) l = sizeof(item->label) - 1;
                    memcpy(item->label, tok + 6, l);
                    item->label[l] = '\0';
                } else if (strncmp(tok, "action=", 7) == 0) {
                    size_t l = tlen - 7;
                    if (l >= sizeof(item->action)) l = sizeof(item->action) - 1;
                    memcpy(item->action, tok + 7, l);
                    item->action[l] = '\0';
                }
                tok = tok_end ? tok_end : tok + strlen(tok);
            }
            if (item->label[0]) pg->n_items++;
        }
    }
    fclose(f);
    /* REAL FIX 2026-08-05, direct correction ("it needs a cancel button
     * so context can clear, close removes entity" - caught mid-test
     * when the first objects.pdl page shipped with Close but no
     * Cancel): every page needs a real, discoverable no-op dismiss
     * distinct from Close (which really ends the process). Rather than
     * relying on every future objects.pdl author to remember this,
     * auto-append one when a page doesn't already declare its own. */
    for (int pi = 0; pi < n_pages; pi++) {
        int has_cancel = 0;
        for (int i = 0; i < pages[pi].n_items; i++) {
            if (strcmp(pages[pi].items[i].label, "Cancel") == 0) { has_cancel = 1; break; }
        }
        if (!has_cancel && pages[pi].n_items < MAX_METHODS) {
            snprintf(pages[pi].items[pages[pi].n_items].label, sizeof(pages[pi].items[0].label), "Cancel");
            snprintf(pages[pi].items[pages[pi].n_items].action, sizeof(pages[pi].items[0].action), "void");
            pages[pi].n_items++;
        }
    }
    return n_pages;
}

/* REAL, 2026-08-05, direct instruction ("book stack emoji... entirely
 * with khtpm / eventscript page, and pal"): a minimal, single-page
 * loader for a real Show Choices popup - a flat list of real
 * "OBJECT | label=.. | action=.." rows, no PAGE header needed (a choice
 * prompt is always exactly one page). Same real parse shape
 * load_objects() already uses for its own OBJECT rows, just without
 * the page-boundary bookkeeping. Used by the SHOW_PAGE relay command
 * below - the file is a real, externally-generated (by whatever op
 * asked for the choice) objects-style file, arbitrary full path, not
 * package_dir-relative. */
static int load_flat_objects(const char *full_path, MethodItem *items, int max) {
    FILE *f = pdl_open(full_path);
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "OBJECT", 6) != 0) continue;
        items[n].label[0] = '\0';
        items[n].action[0] = '\0';
        char *tok = line;
        while ((tok = strchr(tok, '|')) != NULL) {
            tok++;
            while (*tok == ' ') tok++;
            char *tok_end = strchr(tok, '|');
            size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
            while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
            if (strncmp(tok, "label=", 6) == 0) {
                size_t l = tlen - 6;
                if (l >= sizeof(items[0].label)) l = sizeof(items[0].label) - 1;
                memcpy(items[n].label, tok + 6, l);
                items[n].label[l] = '\0';
            } else if (strncmp(tok, "action=", 7) == 0) {
                size_t l = tlen - 7;
                if (l >= sizeof(items[0].action)) l = sizeof(items[0].action) - 1;
                memcpy(items[n].action, tok + 7, l);
                items[n].action[l] = '\0';
            }
            tok = tok_end ? tok_end : tok + strlen(tok);
        }
        if (items[n].label[0]) n++;
    }
    fclose(f);
    return n;
}

/* REAL FIX 2026-08-05, direct instruction ("context window should have
 * name/id of entity, so its addressable by others" + "i dont see those
 * in context window?"): the entity's real id (piece_id-instance_id) was
 * only ever written into the invisible X11 window title - the actual
 * VISIBLE popup menu had no id at all. A non-clickable header row (row
 * 0) now shows it, so a human right-clicking an entity can read/copy
 * its real address directly, not just infer it from the glyph. */
static char g_full_id[96] = "";

/* Size the next context menu to its longest drawn row (header + labels,
 * including worst-case chtpm nav prefix "[>] 99. "). Result lives in
 * g_popup_w; open/draw/hit-test/submenu offset all share that. */
static int measure_context_popup_w(Display *dpy, MethodItem *items, int n) {
    const int pad = 28; /* left text x (12) + right margin + border */
    int maxw = popup_text_px(dpy, g_full_id);
    if (items) {
        for (int i = 0; i < n; i++) {
            char buf[192];
            snprintf(buf, sizeof(buf), "[>] 99. %s", items[i].label);
            int tw = popup_text_px(dpy, buf);
            if (tw > maxw) maxw = tw;
            tw = popup_text_px(dpy, items[i].label);
            if (tw > maxw) maxw = tw;
        }
    }
    int w = maxw + pad;
    if (w < POPUP_W_MIN) w = POPUP_W_MIN;
    if (w > POPUP_W_MAX) w = POPUP_W_MAX;
    return w;
}

/* REAL FIX 2026-08-05, direct-caught bug ("its been having issues"
 * investigation - a plain `kill <pid>` left a stale livedesk_open.txt/
 * nav_claims.txt entry behind forever, since this process had no
 * SIGTERM handler and this file's own real cleanup code (registry/
 * nav-claim removal, right before the real return at the end of
 * main()) only ever ran on a normal exit - a bare SIGTERM (the default
 * for plain `kill`, and what most process-manager/session shutdowns
 * send) skipped it entirely. Real fix: catch SIGTERM/SIGINT, set this
 * real sig_atomic_t flag (async-signal-safe per POSIX - nothing more
 * elaborate is safe to do inside a signal handler), the main loop's own
 * condition checks it every iteration so the SAME real cleanup path
 * always runs. */
static volatile sig_atomic_t g_shutdown_requested = 0;
static void handle_shutdown_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

/* Real Show Text popup content - see the SHOW_TEXT_FILE relay handler
 * in main() and its own Expose-draw branch below. */
static char g_text_popup_lines[64][256];
static int g_text_popup_n_lines = 0;

/* REAL FIX 2026-08-06, direct report ("book and monster are both having
 * focus control problems"): XGrabKeyboard/XGrabPointer are DISPLAY-WIDE
 * EXCLUSIVE resources - only one process can hold either at a time.
 * Every entity (book-stack, each monster, asa, ava...) is its own
 * separate OS process, each independently calling XGrabKeyboard the
 * moment ITS OWN popup opens, with zero coordination between them. If
 * two entities' popups happen to open around the same time, whichever
 * grabs last wins - the OTHER process's retry loop (open_context_menu's
 * own 5-attempt retry above) silently exhausts and returns a popup with
 * NO real keyboard grab at all, no error, no log - exactly this report.
 * Real fix: a real, house-wide, cross-process mutex via flock() on a
 * single shared lockfile (#.desktop/livedesk_popup.lock) - any process
 * about to open a popup blocks (real, not spin-polled - flock(LOCK_EX)
 * blocks in the kernel) until it's the only one holding it, so grab
 * contention structurally can't happen anymore. g_popup_lock_depth
 * makes this reentrant for a SINGLE process's own nested popups (e.g.
 * the main context menu plus its own input_popup_win at once) - only
 * the outermost open/close pair actually touches the lock. */
static int g_popup_lock_fd = -1;
static int g_popup_lock_depth = 0;
static char g_house_root_for_lock[PATH_BUF] = "";

/* REAL FIX/REVERT 2026-08-06, direct report ("not clicking buttons with
 * the mouse will run the button... serious focus issues that doing
 * khtpm introduced"): flock(LOCK_EX) BLOCKS THE ENTIRE PROCESS - this
 * whole event loop froze while waiting, and any real input event that
 * arrived at the X server during that freeze (a leftover click, a
 * queued Enter from earlier) sat buried in the queue and fired the
 * INSTANT the lock finally freed, indistinguishable from "it ran
 * itself." A real, un-clicked action. Real fix: bounded, non-blocking
 * wait (LOCK_NB in a short poll loop, ~1s total) instead of blocking
 * forever - worst case behaves like the pre-lock code (a grab might
 * still race), but the process is NEVER frozen long enough to build up
 * a dangerous input backlog. */
static void popup_lock_acquire(void) {
    if (!LIVEDESK_USE_POPUP_LOCK) return;
    if (g_popup_lock_depth++ > 0) return;
    if (!g_house_root_for_lock[0]) return;
    if (g_popup_lock_fd < 0) {
        char lock_path[PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_popup.lock", g_house_root_for_lock);
        g_popup_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_popup_lock_fd >= 0) {
        for (int attempt = 0; attempt < 50; attempt++) {
            if (flock(g_popup_lock_fd, LOCK_EX | LOCK_NB) == 0) break;
            usleep(20000); /* 20ms * 50 = ~1s total bounded wait, never indefinite */
        }
    }
}

static void popup_lock_release(void) {
    if (!LIVEDESK_USE_POPUP_LOCK) return;
    if (g_popup_lock_depth <= 0) return;
    if (--g_popup_lock_depth > 0) return;
    if (g_popup_lock_fd >= 0) flock(g_popup_lock_fd, LOCK_UN);
}


/* Soft focus for a single popup window (option C). Only touches THIS
 * window — no XRaiseWindow on other entities, no global focus war. */
static void popup_soft_focus(Display *dpy, Window popup) {
    if (!LIVEDESK_POPUP_SOFT_FOCUS) return;
    if (!popup) return;
    XRaiseWindow(dpy, popup); /* raise the menu itself so it's not buried */
    XSetInputFocus(dpy, popup, RevertToParent, CurrentTime);
    XFlush(dpy);
}

static void clamp_popup_to_screen(Display *dpy, int *x, int *y, int w, int h) {
    /* REAL 2026-08-06, user: context windows near bottom/side should open
     * in empty space (stay fully on-screen above taskbar, not clipped). */
    int scr = DefaultScreen(dpy);
    int sw = DisplayWidth(dpy, scr);
    int sh = DisplayHeight(dpy, scr);
    const int margin = 4;
    const int taskbar_reserve = 40; /* livedesk bar ~32px + padding */
    int usable_h = sh - taskbar_reserve;
    if (usable_h < h + margin) usable_h = sh - margin;
    int px = *x, py = *y;
    if (px + w + margin > sw) px = sw - w - margin;
    if (px < margin) px = margin;
    if (py + h + margin > usable_h) py = usable_h - h - margin;
    if (py < margin) py = margin;
    /* Prefer flipping above-left of anchor if we still overflow badly */
    if (py + h > usable_h) py = margin;
    if (px + w > sw) px = margin;
    *x = px;
    *y = py;
}

static Window open_context_menu(Display *dpy, GC gc, int *root_x, int *root_y, int nitems, MethodItem *items) {
    popup_lock_acquire();
    /* +1 row for the id header (see g_full_id comment above). */
    int h = POPUP_ROW_H * ((nitems > 0 ? nitems : 1) + 1);
    /* Content-aware width: measure labels when caller passes items;
     * bare input/placeholder menus get a slightly roomier default. */
    if (items && nitems > 0)
        g_popup_w = measure_context_popup_w(dpy, items, nitems);
    else
        g_popup_w = 320; /* input / empty: wider than the old 160 floor */
    int px = root_x ? *root_x : 0;
    int py = root_y ? *root_y : 0;
    clamp_popup_to_screen(dpy, &px, &py, g_popup_w, h);
    if (root_x) *root_x = px;
    if (root_y) *root_y = py;
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    /* REAL, 2026-08-05, direct instruction ("try some stuff with digit
     * jump / [] bracket nav"): KeyPressMask + a real keyboard grab
     * added alongside the existing pointer grab, so digit keys reach
     * this popup regardless of which window nominally has focus - same
     * reasoning as the existing XGrabPointer just above. */
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  px, py, (unsigned)g_popup_w, h, 1,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    /* REAL FIX 2026-08-06, direct root cause found ("keystrokes went
     * INTO the terminal"): this whole session's "focus problems" were
     * never really about the grab retry logic - Mutter's Wayland
     * compositor (org.gnome.mutter.wayland xwayland-allow-grabs)
     * restricts XGrabKeyboard from XWayland clients by default, real,
     * intentional Wayland security policy, not a bug in this code.
     * xwayland-grab-access-rules allowlists by WM_CLASS - these windows
     * never set one, so they could never be allowlisted. Real fix: a
     * real WM_CLASS ("MuchiverseLivedesk"), matched by
     * $.crypts/enable_xwayland_grabs.sh (house-wide, re-runnable). */
    XClassHint *class_hint = XAllocClassHint();
    if (class_hint) {
        class_hint->res_name = (char *)"MuchiverseLivedesk";
        class_hint->res_class = (char *)"MuchiverseLivedesk";
        XSetClassHint(dpy, popup, class_hint);
        XFree(class_hint);
    }
    XMapRaised(dpy, popup);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    /* REAL FIX 2026-08-05, direct report ("it definately has a problem
     * getting focus"): neither grab's own return value was ever
     * checked. A real, well-known X11 race: closing one popup
     * (XUngrabKeyboard/XUngrabPointer) and immediately opening another
     * (exactly what a SHOW_PAGE Show-Choices transition does - close
     * the old popup, open the new one, same event-handling tick) can
     * race the X server's own internal grab-release processing, so the
     * new XGrabKeyboard/XGrabPointer call can genuinely fail
     * (AlreadyGrabbed) even though nothing else actually wants the
     * grab. Real fix: retry a few times with a short real wait,
     * confirmed via `man XGrabKeyboard`'s own documented AlreadyGrabbed
     * return - not a made-up mitigation. */
    /* REAL FIX 2026-08-07, direct report ("toolbar is not taking clicks",
     * "x didn't close them but closed toolbar"): when menu_stay_open is
     * on, the menu must NOT hold a modal pointer grab - a grab redirects
     * EVERY pointer event (toolbar clicks, other windows, everything) to
     * this popup, so with the menu kept open the whole desk becomes
     * unclickable. Stay-open menus are deliberately NON-modal by default:
     * no pointer grab, so clicks reach their real targets; the menu
     * simply stays open until the user clicks a row/Cancel or presses
     * Escape/Enter. Arrow nav still works - the keyboard grab below is
     * kept. menu_stay_open=0 restores the old modal grab + dismiss-on-
     * any-outside-click behavior (grab_pointer still toggles it there).
     *
     * REAL, 2026-08-11, direct instruction ("add a config to switch that
     * on and off w/o hardcoding... user can tweak it w/o changing code,
     * and find optimal solution"): the "no grab while stay-open" choice
     * above is a genuine, real tradeoff (some entities' row clicks may
     * not reliably reach a non-grabbed override-redirect popup under
     * this house's Wayland/XWayland setup — direct report: "it works
     * clicking enter, but not mouse clicking"), not a universally-correct
     * answer for every entity. grab_pointer_while_stay_open (meta.pdl,
     * default 0 = unchanged prior behavior) lets a human opt back INTO
     * grabbing per-entity to test whether that fixes click delivery for
     * THEM specifically, accepting the "rest of the desk goes unclickable
     * while this menu is open" tradeoff as a deliberate choice instead of
     * it being permanently unavailable. Don't grab pointer if menu stays
     * open — allows user to click other windows (e.g., browser) while
     * keeping the menu open for relay-based navigation. */
    if (g_grab_pointer && !g_menu_stay_open) {
        for (int attempt = 0; attempt < 5; attempt++) {
            int rc = XGrabPointer(dpy, popup, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                                   None, None, CurrentTime);
            if (rc == GrabSuccess) break;
            XSync(dpy, False);
            usleep(5000);
        }
    }
    if (g_grab_keyboard) {
        for (int attempt = 0; attempt < 5; attempt++) {
            int rc = XGrabKeyboard(dpy, popup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            if (rc == GrabSuccess) break;
            XSync(dpy, False);
            usleep(5000);
        }
    }
    /* REAL FIX 2026-08-05, direct report ("clicking read just closes
     * context"): X11 can and does recycle a destroyed window's own
     * resource ID for the very next XCreateWindow call on the same
     * display (confirmed - this is exactly the close-old/open-new
     * sequence SHOW_PAGE does every time). Any ButtonPress/KeyPress
     * already sitting in the X server's queue, addressed to the OLD
     * popup's ID before it was destroyed, gets redelivered to this NEW
     * popup the instant it's mapped - read as an immediate phantom
     * click on row 0, dismissing the menu before the user ever saw it.
     * Real fix: drain any already-queued Button/KeyPress events
     * targeting this exact window id before returning, so only input
     * that arrives AFTER this popup genuinely existed can select a row. */
    XSync(dpy, False);
    XEvent stale_ev;
    while (XCheckWindowEvent(dpy, popup, ButtonPressMask | KeyPressMask, &stale_ev)) {
        /* discard - see comment above */
    }
    /* OPTION C 2026-08-06: grabs re-enabled (flags=1); locks stay off.
     * Soft focus on THIS popup only as fallback when grab fails or
     * XWayland ignores grab — not the old multi-entity focus war
     * (we never XSetInputFocus other processes' tile windows here). */
    popup_soft_focus(dpy, popup);

    /* Stage 2c PROOF (2026-08-16) - see launch_khtpm_menu()'s own header
     * comment. Everything above this point (window creation, lock
     * acquire, grabs) runs completely unchanged - the legacy popup is a
     * real, fully-alive X window, just never shown, so every downstream
     * caller's lock/lifecycle bookkeeping keeps working exactly as
     * before. Only the VISIBLE result changes. */
#ifdef _WIN32
    /* khtpm_entity_menu_render.exe is not a Win PE yet. Linux hides the
     * legacy popup and forks that binary; doing the same here destroyed
     * the menu and launched nothing. Keep the Xlib menu until that app
     * has its own shim. */
    (void)0;
#else
    if (g_use_khtpm_menu) {
        /* REAL FIX 2026-08-16, direct live report ("it also pops up
         * instead of context menu when i rightclick ava" - the "Chat"
         * item fired immediately instead of the menu showing): the
         * XGrabPointer above (still active on this now-hidden popup) was
         * delivering the initiating click's own trailing event straight
         * through to whatever mapped next - the new renderer's window,
         * landing on its first item. Explicitly release the grab before
         * hiding the legacy popup, same real cause class as the
         * window-ID-recycle phantom click fixed just above. */
        XUngrabPointer(dpy, CurrentTime);
        XUngrabKeyboard(dpy, CurrentTime);
        XSync(dpy, False);
        /* REAL FIX 2026-08-16, direct live report ("seems random" - Chat
         * firing on ~every other right-click with no menu visible first):
         * merely hiding (XUnmapWindow) left this a real, non-zero,
         * still-alive X window - every ~20 call sites' own `popup_win`
         * variable stayed set to it. The main event loop's real
         * right-click handler is gated on `if (!popup_win)` (line ~2868)
         * - a still-nonzero popup_win made the NEXT right-click get
         * treated as "click INSIDE the already-open menu" (hit-tested
         * against this stale, invisible popup's old geometry) instead of
         * opening fresh, landing on whatever nav row happened to be
         * nearest. Real fix: actually destroy the legacy popup (not just
         * hide it) and return None/0 here, so the caller's popup_win
         * genuinely goes back to 0 and the next right-click's own guard
         * re-arms correctly - matching real "menu closed" semantics
         * instead of a fake hidden-but-still-open one. Also release the
         * lock acquired at the top of this function (popup_lock_acquire)
         * since close_context_menu() - which normally balances it - is
         * never reached on this path. */
        XDestroyWindow(dpy, popup);
        popup_lock_release();
        launch_khtpm_menu(px, py);
        return None;
    }
#endif

    return popup;
}

/* REAL FIX 2026-08-05, direct correction ("im still seeing numbers in
 * brackets instead of [>] [] empty brackets like chtpm"): real chtpm
 * format (confirmed via direct example, a real live yahoo-broker frame)
 * is "[ ] N. Label" - an EMPTY bracket is a real focus-cursor marker
 * (becomes "[>]" for whichever row currently has focus), completely
 * separate from the plain "N." row number that follows it. The
 * previous "[N] Label" render put the shared live nav number INSIDE
 * the bracket - wrong shape entirely, not just a style nit. nav_base is
 * this popup's own claimed starting number (see nav_claim_rows()), so
 * row i's real number is nav_base+i; nav_base<=0 (the small
 * user_popup_win/input_popup_win submenus, which don't claim nav
 * numbers) suppresses both the bracket and the number rather than
 * showing a misleading unclaimed one.
 *
 * REAL FIX, same day, follow-up: real up/down focus-cursor tracking now
 * exists (focus_row, driven by real Up/Down KeyPress in main()'s own
 * popup_win branch) - whichever row equals focus_row shows "[>]", every
 * other row shows "[ ]", matching real chtpm's own single-row cursor
 * convention exactly (not a highlight/fill effect - just the bracket
 * glyph itself changes, same as the real captured yahoo-broker frame). */
static void draw_context_menu(Display *dpy, Window popup, GC gc, MethodItem *items, int n, int nav_base, int focus_row) {
    XClearWindow(dpy, popup);
    int h = POPUP_ROW_H * ((n > 0 ? n : 1) + 1);
    int pw = g_popup_w > 0 ? g_popup_w : POPUP_W_MIN;
    XDrawRectangle(dpy, popup, gc, 0, 0, pw - 1, h - 1);
    /* Row 0: non-clickable id header. */
    popup_draw_text(dpy, popup, gc, 12, POPUP_ROW_H / 2 + 4, g_full_id);
    XDrawLine(dpy, popup, gc, 0, POPUP_ROW_H, pw, POPUP_ROW_H);
    for (int i = 0; i < n; i++) {
        int row_y = (i + 1) * POPUP_ROW_H;
        if (i > 0) XDrawLine(dpy, popup, gc, 0, row_y, pw, row_y);
        char labeled[160];
        const char *cursor = (i == focus_row) ? "[>]" : "[ ]";
        if (nav_base > 0) snprintf(labeled, sizeof(labeled), "%s %d. %s", cursor, nav_base + i, items[i].label);
        else snprintf(labeled, sizeof(labeled), "%s", items[i].label);
        popup_draw_text(dpy, popup, gc, 12, row_y + POPUP_ROW_H / 2 + 4, labeled);
    }
}

/* REAL, 2026-08-05: the range-finder grid used to be drawn HERE as an
 * opaque popup (real, but covered the dog/nearby tiles - direct
 * correction: "it should just be a transparent outline like a png").
 * Moved to a real standalone binary, ops/tp_range_grid.c, using the
 * X11 Shape Extension (same real transparency technique this file's
 * own sprite rendering already uses) - launched via system() from the
 * "OPEN_RANGE_GRID" dispatch below instead of drawn inline. */

static void close_context_menu(Display *dpy, Window popup) {
    if (g_grab_pointer) XUngrabPointer(dpy, CurrentTime);
    if (g_grab_keyboard) XUngrabKeyboard(dpy, CurrentTime);
    XDestroyWindow(dpy, popup);
    popup_lock_release();
}

/* REAL, NEW 2026-08-04, direct instruction ("allow user editing of
 * asset... place an asset in asset folder of entity, and in .pal
 * specify emoji other than default, or path of asset (jpg/png), it will
 * use that instead of default emoji"). Real, simple convention:
 * <package_dir>/asset.pal, key=value lines:
 *   glyph=<emoji>          - regenerate sprite.csv from a DIFFERENT
 *                            emoji than glyph.txt's own default, via the
 *                            same real emoji_gen_atlas/emoji_xtract
 *                            pipeline tp_place_desktop.c already uses.
 *   asset_path=<path>      - use a real, arbitrary user image (PNG/JPG,
 *                            any size - NOT required to be pre-cropped
 *                            to 64x64) instead of any emoji at all, via
 *                            the NEW tp_asset_to_sprite.+x (see that
 *                            file's own header for why emoji_xtract.+x
 *                            alone isn't the right tool for an arbitrary
 *                            user image). Relative paths resolve against
 *                            package_dir/assets/ (the real "asset
 *                            folder" named in the instruction); absolute
 *                            paths are used as-is (e.g. pointing
 *                            directly at a file elsewhere, without
 *                            moving it).
 * Regenerates sprite.csv in place, once, at window startup - editing
 * asset.pal and relaunching the window picks up the change. */
static void apply_asset_override(const char *package_dir, const char *ops_dir) {
    char asset_pal[PATH_BUF];
    snprintf(asset_pal, sizeof(asset_pal), "%s/asset.pal", package_dir);
    FILE *f = fopen(asset_pal, "r");
    if (!f) return;

    char glyph_override[64] = "", asset_path_raw[PATH_BUF] = "";
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "glyph=", 6) == 0) snprintf(glyph_override, sizeof(glyph_override), "%s", line + 6);
        else if (strncmp(line, "asset_path=", 11) == 0) snprintf(asset_path_raw, sizeof(asset_path_raw), "%s", line + 11);
    }
    fclose(f);

    char sprite_path[PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);

    if (asset_path_raw[0]) {
        char resolved[PATH_BUF];
        if (asset_path_raw[0] == '/') {
            snprintf(resolved, sizeof(resolved), "%s", asset_path_raw);
        } else {
            snprintf(resolved, sizeof(resolved), "%s/assets/%s", package_dir, asset_path_raw);
        }
#ifndef _WIN32
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/tp_asset_to_sprite.+x' '%s' 64 '%s' >/dev/null 2>&1",
                 ops_dir, resolved, sprite_path);
        int rc = system(cmd);
        (void)rc;
#else
        (void)ops_dir;
        (void)resolved;
        /* Existing sprite.csv is used. Do not system() Linux .+x via cmd.exe
         * (0x800700E8 / pipe closed) — that was killing rgb at startup. */
#endif
    } else if (glyph_override[0]) {
#ifndef _WIN32
        char atlas_path[PATH_BUF], cmd[PATH_BUF * 2];
        snprintf(atlas_path, sizeof(atlas_path), "%s/atlas_override.png", package_dir);
        snprintf(cmd, sizeof(cmd),
                 "'%s/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1 && "
                 "'%s/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
                 ops_dir, glyph_override, atlas_path, ops_dir, atlas_path, sprite_path);
        int rc = system(cmd);
        (void)rc;
#else
        (void)ops_dir;
#endif
    }
}

static void write_pos(const char *package_dir, int x, int y) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "x=%d\ny=%d\n", x, y);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_desktop_window.+x <package_dir>\n");
        return 1;
    }
    signal(SIGTERM, handle_shutdown_signal);
    signal(SIGINT, handle_shutdown_signal);
    char package_buf[PATH_BUF];
    snprintf(package_buf, sizeof(package_buf), "%s", argv[1]);
#ifdef _WIN32
    win_package_rel(package_buf);
#endif
    const char *package_dir = package_buf;
    snprintf(g_history_path, sizeof(g_history_path), "%s/history.txt", package_dir);
    snprintf(g_relay_path, sizeof(g_relay_path), "%s/interact_relay.txt", package_dir);
    append_history("WINDOW_OPEN");
    char g_ops_dir[PATH_BUF], g_house_root[PATH_BUF];
    resolve_livedesk_paths(g_ops_dir, sizeof(g_ops_dir), g_house_root, sizeof(g_house_root));
#ifdef _WIN32
    if (!g_house_root[0]) snprintf(g_house_root, sizeof(g_house_root), ".");
#endif
    snprintf(g_house_root_for_lock, sizeof(g_house_root_for_lock), "%s", g_house_root);
    /* REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a) - read the real,
     * optional, house-wide grid cell size as early as possible (right
     * after g_house_root resolves, before anything below uses
     * GRID_CELL_PX). */
    GRID_CELL_PX = read_grid_cell_px(g_house_root);
    /* Stage 2c PROOF - see launch_khtpm_menu()'s own header comment. */
    {
        char menu_chtpm_path[PATH_BUF];
        snprintf(menu_chtpm_path, sizeof(menu_chtpm_path), "%s/menu.chtpm", package_dir);
        if (access(menu_chtpm_path, F_OK) == 0) {
            g_use_khtpm_menu = 1;
            snprintf(g_khtpm_menu_pkg_dir, sizeof(g_khtpm_menu_pkg_dir), "%s", package_dir);
            snprintf(g_khtpm_menu_house_root, sizeof(g_khtpm_menu_house_root), "%s", g_house_root);
        }
    }
    int g_livedesk_index = 0;
    if (g_house_root[0]) {
        g_livedesk_index = ensure_livedesk_index(package_dir, g_house_root);
        livedesk_registry_add(g_house_root, package_dir, g_livedesk_index, getpid());
        ensure_taskbar_running(g_house_root);
        append_history("LIVEDESK_INDEX=%d", g_livedesk_index);
    }
    /* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER monsters
     * need a real 2x2-desktop-grid-cell footprint): only override the
     * original flat 64px WIN_PX when a package's own meta.pdl actually
     * declares footprint_tiles > 1. footprint_tiles defaults to 1 (see
     * read_footprint_tiles()), so every existing pet/asa/ava package
     * (no such STATE row) renders at EXACTLY the original 64px,
     * unchanged - only footprint_tiles>1 packages (monsters) get
     * footprint_tiles * GRID_CELL_PX (e.g. 2*80=160). */
    {
        int footprint_tiles = read_footprint_tiles(package_dir);
        if (footprint_tiles > 1) WIN_PX = footprint_tiles * GRID_CELL_PX;
    }
    read_menu_config(package_dir);
    /* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
     * tho"): glyph.txt can now hold a real multi-byte UTF-8 emoji -
     * kept as a full string, not truncated to its first byte. Real
     * texture rendering of that emoji (via emoji_gen_atlas.+x, this
     * house's own FreeType+NotoColorEmoji pipeline, already used by
     * chtpm_rgb_render) is NOT done here yet - this window still only
     * shows a glyph-hashed color + the glyph as its window title, same
     * as before this fix. Flagged as still-open, tracked alongside the
     * tp_menu_input.c/tp_compose_frame.c glyph-widening this session
     * already did for the picker's own chtpm-rendered path (which DOES
     * show real emoji, via that same already-working renderer). */
    char glyph_line[64];
    read_glyph(package_dir, glyph_line, sizeof(glyph_line));
    char glyph = glyph_line[0] ? glyph_line[0] : '?';
    const char *glyph_str = glyph_line[0] ? glyph_line : "?";

    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_desktop_window: cannot open display\n");
        return 1;
    }
    load_popup_fontset(dpy);

    int screen_num = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, screen_num);
    int depth = DefaultDepth(dpy, screen_num);

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, screen_num), vis, AllocNone);
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask;
    swa.override_redirect = True;

    Window win = XCreateWindow(dpy, RootWindow(dpy, screen_num), 3 * GRID_CELL_PX, 3 * GRID_CELL_PX, WIN_PX, WIN_PX,
                                0, depth, InputOutput, vis,
                                CWColormap | CWEventMask | CWOverrideRedirect, &swa);
    XMapWindow(dpy, win);

    /* RGB compose buffer - all real drawing (background, glyph/sprite)
     * happens into this offscreen Pixmap, presented each frame via
     * XGetImage+XPutImage below, same as db-hq/taskbar. */
    Pixmap g_buf = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, (unsigned)depth);
    GC g_buf_gc = XCreateGC(dpy, g_buf, 0, NULL);
    /* REAL FIX 2026-08-05, direct instruction ("context window should
     * have name/id of entity, so its addressable by others"): the
     * window title used to be "tile:<glyph>" only - real piece_id
     * (basename of package_dir, the same id every methods.pdl/meta.pdl/
     * event_pkg already uses) was nowhere in it, so any OTHER process
     * that needs to find a SPECIFIC entity's own window (a future AI
     * tick loop targeting "chase target=cat", a test harness, etc) had
     * no reliable way to address it - glyph alone can collide (two
     * entities could share an emoji) and isn't the entity's own real
     * identity. basename() needs a mutable copy since POSIX basename()
     * may modify its argument. */
    char pkg_copy[PATH_BUF];
    snprintf(pkg_copy, sizeof(pkg_copy), "%s", package_dir);
    const char *piece_id = basename(pkg_copy);
    char instance_id[32];
    read_instance_id(package_dir, instance_id, sizeof(instance_id));
    if (instance_id[0]) snprintf(g_full_id, sizeof(g_full_id), "%s-%s", piece_id, instance_id);
    else snprintf(g_full_id, sizeof(g_full_id), "%s", piece_id);
    char title[192];
    snprintf(title, sizeof(title), "tile:%s:%s", g_full_id, glyph_str);
    XStoreName(dpy, win, title);

    /* Alpha blending against the background is now done per-pixel in
     * draw_sprite_rgb() itself (no GL_BLEND state to set up). */
    float r, g, b;
    glyph_color(glyph, &r, &g, &b);
    g_font_loaded = load_glyph_font(dpy);

    /* Resolve ops_dir (same /proc/self/exe technique tp_place_desktop.c
     * already uses) so apply_asset_override() can find tp_asset_to_
     * sprite.+x/emoji_gen_atlas.+x/emoji_xtract.+x next to this binary. */
    {
        char self_path[PATH_BUF];
        if (self_exe_path(self_path, sizeof(self_path))) {
            char *ops_dir = dirname(self_path);
            apply_asset_override(package_dir, ops_dir);
        }
    }

    char sprite_path[PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);
    g_has_sprite = load_sprite_csv(sprite_path);

    /* Real window shape from the sprite's own alpha, if we have one -
     * see build_shape_mask()'s own header comment for why GL_BLEND
     * alone wasn't enough. */
    if (g_has_sprite) {
#ifndef _WIN32
        Pixmap shape_mask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
        GC shape_gc = XCreateGC(dpy, shape_mask, 0, NULL);
        build_shape_mask(dpy, win, shape_gc, shape_mask);
        XFreeGC(dpy, shape_gc);
        XFreePixmap(dpy, shape_mask);
#endif
        /* Win: per-pixel alpha via UpdateLayeredWindow in XPutImage (XShape shim). */
    }

    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));
    int max_col = (screen_w / GRID_CELL_PX) - 1;
    int max_row = (screen_h / GRID_CELL_PX) - 1;
    if (max_col < 0) max_col = 0;
    if (max_row < 0) max_row = 0;

    int xfd = ConnectionNumber(dpy);
    int win_x = 3 * GRID_CELL_PX, win_y = 3 * GRID_CELL_PX; /* grid-aligned spawn, matching egg_window.c's own default */
    {
        int ix, iy;
        if (read_initial_pos(package_dir, &ix, &iy)) {
            int gx = (ix + GRID_CELL_PX / 2) / GRID_CELL_PX;
            int gy = (iy + GRID_CELL_PX / 2) / GRID_CELL_PX;
            if (gx < 0) gx = 0; if (gx > max_col) gx = max_col;
            if (gy < 0) gy = 0; if (gy > max_row) gy = max_row;
            win_x = gx * GRID_CELL_PX;
            win_y = gy * GRID_CELL_PX;
        }
#ifdef _WIN32
        /* Linux pos can sit past this monitor. Keep on the primary work
         * area, below the strip. */
        {
            int pad_top = 40, g = 8;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
            if (win_x + WIN_PX > screen_w - g) win_x = screen_w - WIN_PX - g;
            if (win_y + WIN_PX > screen_h - g) win_y = screen_h - WIN_PX - g;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
        }
#endif
#ifdef __APPLE__
        /* macOS leg (2026-08-22): mirror of the _WIN32 work-area clamp.
         * Saved Linux grid positions can sit past this display's right
         * edge (live: tiles parked at x=1600 on a 1680px screen, mostly
         * invisible). XQuartz rootless maps y=0 to just under the macOS
         * menu bar, so pad_top only needs to clear the taskbar strip. */
        {
            int pad_top = 40, g = 8;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
            if (win_x + WIN_PX > screen_w - g) win_x = screen_w - WIN_PX - g;
            if (win_y + WIN_PX > screen_h - g) win_y = screen_h - WIN_PX - g;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
        }
#endif
        XMoveWindow(dpy, win, win_x, win_y);
        /* macOS leg (2026-08-22): persist the CLAMPED position - the
         * saved Linux grid value can sit past this display's edge, and
         * downstream consumers (khtpm_show_choices.c's picker spawn
         * reads this same file) must not inherit an off-screen x/y. */
        write_pos(package_dir, win_x, win_y);
    }
    MethodItem methods[MAX_METHODS];
    int n_methods = load_methods(package_dir, methods, MAX_METHODS);
    if (n_methods == 0) {
        snprintf(methods[0].label, sizeof(methods[0].label), "Close");
        snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
        n_methods = 1;
    }
    /* REAL, 2026-08-05: optional multi-page objects.pdl overrides the
     * single-page methods.pdl list above - see load_objects()'s own
     * header comment. methods[]/n_methods become "whichever page is
     * currently open"'s own item list when objects.pdl exists, so every
     * existing render/dispatch/relay code below that already reads
     * methods[]/n_methods keeps working completely unchanged. */
    ObjPage obj_pages[MAX_PAGES];
    int n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
    int using_objects = (n_obj_pages > 0);
    int cur_page = 0;
    int page_stack[MAX_PAGES];
    int page_stack_n = 0;
    if (using_objects) {
        n_methods = obj_pages[0].n_items;
        for (int i = 0; i < n_methods; i++) methods[i] = obj_pages[0].items[i];
    }
    int input_active = 0;
    char input_key[64] = "";
    char input_buffer[256] = "";
    Window input_popup_win = 0;
    Window popup_win = 0;
    int popup_nav_base = 0;
    int popup_focus_row = 0;
    int popup_digit_accum = 0; /* chtpm: digits move [>] before Enter */
    /* REAL, 2026-08-05, direct instruction ("book stack emoji... give
     * me option to generate a random verse... entirely with khtpm /
     * eventscript page, and pal"): real Show Choices support - a
     * SEPARATE process (e.g. a real event.pal's own exec'd op) can ask
     * THIS already-running window to present a choice popup via a new
     * SHOW_PAGE relay command, reusing the exact same popup_win/
     * methods[]/n_methods machinery the entity's own normal context
     * menu already uses. choice_mode gates the row-activation dispatch
     * sites (mouse click / RUN_METHOD / ACTIVATE_NAV / Enter) to write
     * the picked row's action to choice_result_path instead of running
     * the normal CLOSE/void/GOTO/etc dispatch. */
    int choice_mode = 0;
    char choice_result_path[PATH_BUF] = "";
    Window text_popup_win = 0;
    GC popup_gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);

    /* REAL, 2026-08-05, direct instruction ("lets just open an adjacent
     * contextmenu (with cancel button) beside the first one for now, so
     * we can test the range finder visual"): quick, real submenu using
     * the SAME already-working raw X11 popup mechanism (open_context_
     * menu/draw_context_menu), not a new CHTPM screen (see hikkikomorai/
     * x11-mouse-2do.txt for why the CHTPM route was rolled back -
     * gl_mirror.c has no mouse-click support and no configurable
     * window size). A second, fixed-content popup opens ADJACENT to
     * the first when "User" is clicked; "Move" inside it opens a third
     * small popup drawing a real range-finder grid. */
    Window user_popup_win = 0;
    int user_popup_x = 0, user_popup_y = 0;
    MethodItem user_methods[4];
    snprintf(user_methods[0].label, sizeof(user_methods[0].label), "Move");
    snprintf(user_methods[0].action, sizeof(user_methods[0].action), "OPEN_RANGE_GRID");
    snprintf(user_methods[1].label, sizeof(user_methods[1].label), "Inventory");
    snprintf(user_methods[1].action, sizeof(user_methods[1].action), "void");
    snprintf(user_methods[2].label, sizeof(user_methods[2].label), "Skill");
    snprintf(user_methods[2].action, sizeof(user_methods[2].action), "void");
    snprintf(user_methods[3].label, sizeof(user_methods[3].label), "Cancel");
    snprintf(user_methods[3].action, sizeof(user_methods[3].action), "void");

    int popup_x = 0, popup_y = 0; /* where the MAIN popup itself opened, for real submenu adjacency */
    int dragging = 0, drag_start_x = 0, drag_start_y = 0;
    int running = 1;
    struct timeval last_frame = { 0, 0 };

    while (running && !g_shutdown_requested) {
#ifdef _WIN32
        x11_wait(dpy, POLL_INTERVAL_USEC);
        (void)xfd;
#else
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);
#endif

        int need_redraw = 0;
#ifdef _WIN32
        if (last_frame.tv_sec == 0) need_redraw = 1;
#endif

        /* REAL, 2026-08-05: poll interact_relay.txt for an injected
         * command - the "AI-injection power" half of this window's own
         * new CHTPM-parity work (see this file's header comment on
         * g_relay_path/append_history). A human, script, or AI agent
         * writes one command line into <package_dir>/interact_relay.txt;
         * this loop (already running every POLL_INTERVAL_USEC via the
         * select() above) picks it up, dispatches it exactly like a
         * real click would, logs it to history.txt, then truncates the
         * relay file so the same command isn't re-consumed next tick -
         * the same real "write once, consumed once" shape this house's
         * other relay files already use. */
        {
            struct stat relay_st;
            if (stat(g_relay_path, &relay_st) == 0 && relay_st.st_size > 0) {
                char line[PATH_BUF];
                line[0] = '\0';
                FILE *rf = fopen(g_relay_path, "r");
                if (rf) {
                    if (!fgets(line, sizeof(line), rf)) line[0] = '\0';
                    fclose(rf);
                }
                FILE *tf = fopen(g_relay_path, "w");
                if (tf) fclose(tf);
                line[strcspn(line, "\n")] = '\0';
                if (line[0]) {
                    append_history("INJECTED: %s", line);
                    if (strcmp(line, "RAISE") == 0) {
                        /* Single-instance spawn support (2026-08-24,
                         * cursword HQ row): the taskbar writes RAISE into
                         * this relay instead of spawning a second
                         * tp_desktop_window_rgb when this entity is
                         * already open. Raise own main window to the top
                         * of the stack - stacking manipulation from the
                         * entity's own X connection, no cross-process
                         * window plumbing. Deliberately NO
                         * XSetInputFocus/XGrabKeyboard here: this is an
                         * override_redirect window, exempt from WM focus
                         * handling entirely (see db-hq's main() header
                         * comment for the full Mutter investigation) -
                         * real keyboard focus was never achievable for
                         * these windows; raise-to-top IS the whole
                         * observable "focus" behavior, and a human click
                         * still lands keyboard where Mutter allows it. */
                        XRaiseWindow(dpy, win);
                        XFlush(dpy);
                    } else if (strncmp(line, "RUN_METHOD:", 11) == 0) {
                        const char *label = line + 11;
                        for (int i = 0; i < n_methods; i++) {
                            if (strcmp(methods[i].label, label) != 0) continue;
                            /* Same GOTO:/BACK/STATE: reserved forms the
                             * real click path handles (see the
                             * ButtonPress branch below) - relay
                             * injection dispatches a page navigation or
                             * input-activation row exactly like a real
                             * click would, not just plain commands. */
                            if (using_objects && strncmp(methods[i].action, "GOTO:", 5) == 0) {
                                const char *target = methods[i].action + 5;
                                for (int pi = 0; pi < n_obj_pages; pi++) {
                                    if (strcmp(obj_pages[pi].name, target) == 0) {
                                        if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                        cur_page = pi;
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        /* Same real fix as the mouse-click
                                         * path above: make the page switch
                                         * actually visible, don't just
                                         * silently update methods[]. */
                                        if (popup_win) close_context_menu(dpy, popup_win);
                                        else { popup_x = win_x; popup_y = win_y; }
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                        break;
                                    }
                                }
                            } else if (using_objects && strcmp(methods[i].action, "BACK") == 0) {
                                if (page_stack_n > 0) {
                                    cur_page = page_stack[--page_stack_n];
                                    n_methods = obj_pages[cur_page].n_items;
                                    for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                    if (popup_win) close_context_menu(dpy, popup_win);
                                    else { popup_x = win_x; popup_y = win_y; }
                                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                }
                            } else if (using_objects && strncmp(methods[i].action, "STATE:", 6) == 0) {
                                snprintf(input_key, sizeof(input_key), "%s", methods[i].action + 6);
                                input_buffer[0] = '\0';
                                input_active = 1;
                                append_history("INPUT_ACTIVATE key=%s", input_key);
                                if (!input_popup_win) {
                                    input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                }
                            } else {
                                dispatch_action(methods[i].action, package_dir, g_house_root, &running);
                            }
                            break;
                        }
                    } else if (strncmp(line, "ACTIVATE_NAV:", 13) == 0) {
                        /* REAL, 2026-08-05 (TILE_PICKER_DESIGN.md §13):
                         * the REMOTE half of the shared live nav pool -
                         * the taskbar's own terminal input writes this
                         * exact command into whichever window's
                         * interact_relay.txt actually owns the typed
                         * number (looked up from #.desktop/
                         * livedesk_nav_claims.txt), so pressing Enter on
                         * a number can activate a row inside a DIFFERENT
                         * window's currently-open menu, not just this
                         * process's own. Only meaningful while popup_win
                         * is open and N falls in this popup's own
                         * currently-claimed range - otherwise a stale/
                         * mistargeted command is silently ignored. */
                        int nav_n = atoi(line + 13);
                        if (popup_win && nav_n >= popup_nav_base && nav_n < popup_nav_base + n_methods) {
                            int row = nav_n - popup_nav_base;
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                            append_history("CLICK(nav) method=%s action=%s", methods[row].label, methods[row].action);
                            if (strcmp(methods[row].action, "CLOSE") == 0) {
                                running = 0;
                            } else if (strcmp(methods[row].action, "void") == 0) {
                                /* no-op */
                            } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                                const char *target = methods[row].action + 5;
                                for (int pi = 0; pi < n_obj_pages; pi++) {
                                    if (strcmp(obj_pages[pi].name, target) == 0) {
                                        if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                        cur_page = pi;
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                        break;
                                    }
                                }
                            } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                                if (page_stack_n > 0) {
                                    cur_page = page_stack[--page_stack_n];
                                    n_methods = obj_pages[cur_page].n_items;
                                    for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                }
                            } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                                snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                                input_buffer[0] = '\0';
                                input_active = 1;
                                append_history("INPUT_ACTIVATE key=%s", input_key);
                                if (!input_popup_win) {
                                    input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                }
                            } else {
                                dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                            }
                        }
                    } else if (strncmp(line, "FOCUS_NAV:", 10) == 0) {
                        /* Move [>] only (chtpm digit-jump). No activate. */
                        int nav_n = atoi(line + 10);
                        if (popup_win && nav_n >= popup_nav_base && nav_n < popup_nav_base + n_methods) {
                            popup_focus_row = nav_n - popup_nav_base;
                            popup_digit_accum = nav_n;
                            draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                            append_history("FOCUS_NAV %d -> row %d", nav_n, popup_focus_row);
                        }
                    } else if (strncmp(line, "NAV_KEY:", 8) == 0) {
                        /* REAL, 2026-08-06, direct instruction ("what if
                         * we logged keybord input when toolbar is on,
                         * and it will be sent to our khtpm from
                         * master-ledger for desktop to our own .txt...
                         * focus giving us our own control of this"):
                         * the real fix for XWayland's grab restrictions
                         * turned out incomplete in practice - so instead
                         * of fighting the compositor for a real
                         * XGrabKeyboard, the taskbar's own input box
                         * (which DOES reliably hold real X focus, no
                         * grab needed - see tp_taskbar.c) becomes a
                         * remote keyboard for whichever entity currently
                         * has a popup open, relaying Up/Down/Enter/
                         * Escape here exactly like a real local KeyPress
                         * would - same dispatch logic as the popup_win
                         * KeyPress branch below, just triggered
                         * remotely instead of from this process's own
                         * X queue. Only meaningful while popup_win is
                         * actually open here - otherwise silently
                         * ignored (stale/mistargeted, same rule
                         * ACTIVATE_NAV already follows). */
                        char navkey[16];
                        snprintf(navkey, sizeof(navkey), "%s", line + 8);
                        navkey[strcspn(navkey, "\r\n")] = '\0';
                        if (popup_win) {
                            if (strcmp(navkey, "Escape") == 0) {
                                close_context_menu(dpy, popup_win);
                                popup_win = 0;
                                nav_release_pid(g_house_root, getpid());
                                need_redraw = 1;
                            } else if (strcmp(navkey, "Up") == 0) {
                                if (n_methods > 0) {
                                    popup_focus_row = (popup_focus_row - 1 + n_methods) % n_methods;
                                    popup_digit_accum = 0;
                                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                                }
                            } else if (strcmp(navkey, "Down") == 0) {
                                if (n_methods > 0) {
                                    popup_focus_row = (popup_focus_row + 1) % n_methods;
                                    popup_digit_accum = 0;
                                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                                }
                            } else if (strcmp(navkey, "Enter") == 0) {
                                int row = popup_focus_row;
                                close_context_menu(dpy, popup_win);
                                popup_win = 0;
                                nav_release_pid(g_house_root, getpid());
                                if (choice_mode) {
                                    FILE *rf5 = fopen(choice_result_path, "w");
                                    if (rf5) { fprintf(rf5, "%s\n", methods[row].action); fclose(rf5); }
                                    append_history("SHOW_PAGE_PICK(navkey) action=%s -> %s", methods[row].action, choice_result_path);
                                    choice_mode = 0;
                                    need_redraw = 1;
                                    goto skip_navkey_enter_dispatch;
                                }
                                append_history("CLICK(navkey) method=%s action=%s", methods[row].label, methods[row].action);
                                if (strcmp(methods[row].action, "CLOSE") == 0) {
                                    running = 0;
                                } else if (strcmp(methods[row].action, "void") == 0) {
                                    /* no-op */
                                } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                                    user_popup_x = popup_x + g_popup_w + 4;
                                    user_popup_y = popup_y;
                                    user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                                } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                                    const char *target = methods[row].action + 5;
                                    for (int pi = 0; pi < n_obj_pages; pi++) {
                                        if (strcmp(obj_pages[pi].name, target) == 0) {
                                            if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                            cur_page = pi;
                                            n_methods = obj_pages[cur_page].n_items;
                                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                            popup_focus_row = 0; popup_digit_accum = 0;
                                            break;
                                        }
                                    }
                                } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                                    if (page_stack_n > 0) {
                                        cur_page = page_stack[--page_stack_n];
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                    }
                                } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                                    snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                                    input_buffer[0] = '\0';
                                    input_active = 1;
                                    append_history("INPUT_ACTIVATE key=%s", input_key);
                                    if (!input_popup_win) {
                                        input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                    }
                                } else {
                                    dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                                }
                                need_redraw = 1;
                                skip_navkey_enter_dispatch: ;
                            }
                        }
                    } else if (strncmp(line, "SHOW_PAGE:", 10) == 0) {
                        /* REAL, 2026-08-05: real Show Choices, injected
                         * into THIS already-running window by a SEPARATE
                         * process (a real event.pal's own exec'd op) -
                         * "<objects_file>|<result_file>". Reuses the
                         * exact same popup_win/methods[]/n_methods
                         * machinery the entity's own normal context menu
                         * already uses - choice_mode gates every row-
                         * activation site below to write the picked
                         * row's action to choice_result_path instead of
                         * running the normal dispatch. */
                        char *sep = strchr(line + 10, '|');
                        if (sep) {
                            *sep = '\0';
                            char *objpath = line + 10;
                            char *respath = sep + 1;
                            respath[strcspn(respath, "\r\n")] = '\0';
                            n_methods = load_flat_objects(objpath, methods, MAX_METHODS);
                            if (n_methods > 0) {
                                snprintf(choice_result_path, sizeof(choice_result_path), "%s", respath);
                                choice_mode = 1;
                                if (popup_win) close_context_menu(dpy, popup_win);
                                popup_x = win_x;
                                popup_y = win_y + WIN_PX + 4;
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                /* REAL FIX 2026-08-06, direct report ("they
                                 * hav nav in main but not book choices"):
                                 * SHOW_PAGE popups never claimed real
                                 * shared live nav numbers (nav_base was
                                 * hardcoded 0, which draw_context_menu
                                 * treats as "suppress the index entirely"
                                 * - meant to avoid showing an unclaimed
                                 * number, but it just reads as a missing
                                 * feature next to every other menu, which
                                 * DOES claim from nav_claim_rows()). Real
                                 * fix: claim rows here too, same as every
                                 * other popup_win open site in this file. */
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                popup_focus_row = 0; popup_digit_accum = 0;
                                append_history("SHOW_PAGE objects=%s result=%s", objpath, choice_result_path);
                            }
                        }
                    } else if (strncmp(line, "SHOW_TEXT_FILE:", 15) == 0) {
                        /* REAL, 2026-08-05: real Show Text - displays a
                         * real text file's own content (already real-
                         * line-wrapped by whichever op generated it) in
                         * a small popup, dismissed by click/Enter/Escape.
                         * Real, separate window from the choice/normal
                         * popups (text_popup_win), so a choice result and
                         * its own follow-up text display can coexist
                         * without fighting over popup_win/methods[]. */
                        char textpath[PATH_BUF];
                        snprintf(textpath, sizeof(textpath), "%s", line + 15);
                        textpath[strcspn(textpath, "\r\n")] = '\0';
                        FILE *tf2 = fopen(textpath, "r");
                        if (tf2) {
                            char tline[256];
                            int n_lines = 0;
                            int max_px = 0;
                            char text_lines[64][256];
                            while (n_lines < 64 && fgets(tline, sizeof(tline), tf2)) {
                                tline[strcspn(tline, "\r\n")] = '\0';
                                snprintf(text_lines[n_lines], sizeof(text_lines[0]), "%s", tline);
                                int lw = popup_text_px(dpy, tline);
                                if (lw > max_px) max_px = lw;
                                n_lines++;
                            }
                            fclose(tf2);
                            if (text_popup_win) close_context_menu(dpy, text_popup_win);
                            /* REAL FIX 2026-08-07: width used to be
                             * max_w*7 - a hardcoded 7px/char guess -
                             * but the real popup face is the 18px fixed
                             * fontset (~9px/glyph; CJK/emoji wider), so
                             * long bible_text verses were silently
                             * clipped at the right edge. Sized now from
                             * the REAL Xutf8TextExtents of the widest
                             * line (popup_text_px), + padding. */
                            int pop_w = (max_px * 115) / 100 + 24;
                            if (pop_w < POPUP_W) pop_w = POPUP_W;
                            if (pop_w > 900) pop_w = 900;
                            int pop_h = (n_lines + 1) * POPUP_ROW_H;
                            int tpx = win_x, tpy = win_y + WIN_PX + 4;
                            clamp_popup_to_screen(dpy, &tpx, &tpy, pop_w, pop_h);
                            XSetWindowAttributes swa2;
                            swa2.override_redirect = True;
                            swa2.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
                            swa2.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
                            text_popup_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                                            tpx, tpy, pop_w, pop_h, 1,
                                                            CopyFromParent, InputOutput, CopyFromParent,
                                                            CWOverrideRedirect | CWBackPixel | CWEventMask, &swa2);
                            {
                                XClassHint *ch2 = XAllocClassHint();
                                if (ch2) {
                                    ch2->res_name = (char *)"MuchiverseLivedesk";
                                    ch2->res_class = (char *)"MuchiverseLivedesk";
                                    XSetClassHint(dpy, text_popup_win, ch2);
                                    XFree(ch2);
                                }
                            }
                            XMapRaised(dpy, text_popup_win);
                            /* REAL FIX 2026-08-24, direct user report ("i
                             * want text to stay on screen till clicked...
                             * it closes very aggressively, when i press a
                             * key when its open"): this popup used to copy
                             * the context-menu's input policy wholesale -
                             * XGrabPointer/XGrabKeyboard whenever the pal's
                             * own STATE rows say grab_pointer/grab_keyboard
                             * (cursword's do) plus popup_soft_focus().
                             * Consequences: every keystroke ANYWHERE landed
                             * in this process and the old dismiss-on-any-
                             * KeyPress branch ate it (user's screenshot
                             * shortcuts died while a verse was up), every
                             * click anywhere was both swallowed by the grab
                             * AND dismissed the popup, and focused apps lost
                             * their keys while it stayed open. A Show Text
                             * box is not a modal menu: NO grabs, NO input
                             * focus. It now receives exactly what falls on
                             * it - a click directly on the box (its own
                             * ButtonPressMask; override_redirect keeps it
                             * topmost under the cursor so the click lands
                             * here without any grab) - everything else
                             * passes through to whatever the user actually
                             * aimed at. Dismissal itself is tightened in
                             * the event loop: ButtonPress ON THIS WINDOW
                             * only, keys never dismiss. */
                            /* REAL FIX 2026-08-06, direct-caught bug (a
                             * NAV_KEY-opened SHOW_TEXT_FILE popup
                             * self-dismissed a few seconds after opening,
                             * with no real input): text_popup_win never
                             * got the SAME stale-queued-event drain
                             * open_context_menu() already has (see that
                             * function's own header comment - X11 window
                             * ID reuse can redeliver an old
                             * Button/KeyPress meant for a JUST-destroyed
                             * window to this brand new one). This is a
                             * separate window creation path (not routed
                             * through open_context_menu()), so it never
                             * got the fix. Same real fix here. */
                            XSync(dpy, False);
                            {
                                XEvent stale_ev2;
                                while (XCheckWindowEvent(dpy, text_popup_win, ButtonPressMask | KeyPressMask, &stale_ev2)) {
                                    /* discard - see comment above */
                                }
                            }
                            for (int li = 0; li < n_lines; li++) {
                                strncpy(g_text_popup_lines[li], text_lines[li], sizeof(g_text_popup_lines[0]) - 1);
                            }
                            g_text_popup_n_lines = n_lines;
                            append_history("SHOW_TEXT_FILE %s (%d lines)", textpath, n_lines);
                        }
                    } else if (strncmp(line, "OPEN_PAGE:", 10) == 0) {
                        /* REAL 2026-08-06: open a named objects.pdl page
                         * after reload (e.g. RPG Menu rewritten by
                         * open_rp_menu.sh with live Level/Gold/HP). */
                        char page_name[64];
                        snprintf(page_name, sizeof(page_name), "%s", line + 10);
                        page_name[strcspn(page_name, "\r\n")] = '\0';
                        if (popup_win) {
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                        }
                        n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                        using_objects = (n_obj_pages > 0);
                        cur_page = 0;
                        page_stack_n = 0;
                        if (using_objects) {
                            int found = -1;
                            for (int pi = 0; pi < n_obj_pages; pi++) {
                                if (strcmp(obj_pages[pi].name, page_name) == 0) {
                                    found = pi;
                                    break;
                                }
                            }
                            if (found >= 0) {
                                cur_page = found;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            } else {
                                cur_page = 0;
                                n_methods = obj_pages[0].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[0].items[k];
                            }
                        } else {
                            n_methods = load_methods(package_dir, methods, MAX_METHODS);
                            if (n_methods == 0) {
                                snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                                snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                                n_methods = 1;
                            }
                        }
                        popup_x = win_x;
                        popup_y = win_y + WIN_PX + 4;
                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                        popup_focus_row = 0;
                        append_history("OPEN_PAGE:%s", page_name);
                    } else if (strcmp(line, "OPEN_CONTEXT") == 0) {
                        /* REAL 2026-08-06: taskbar nav Enter on a tab
                         * writes this to open the entity context menu
                         * (same reload+open path as right-click). */
                        if (popup_win) {
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                        }
                        n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                        using_objects = (n_obj_pages > 0);
                        if (using_objects) {
                            cur_page = 0;
                            page_stack_n = 0;
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                        } else {
                            n_methods = load_methods(package_dir, methods, MAX_METHODS);
                            if (n_methods == 0) {
                                snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                                snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                                n_methods = 1;
                            }
                        }
                        popup_x = win_x;
                        popup_y = win_y + WIN_PX + 4;
                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                        popup_focus_row = 0; popup_digit_accum = 0;
                        append_history("OPEN_CONTEXT");
                    } else if (strcmp(line, "CLOSE") == 0) {
                        running = 0;
                    }
                    need_redraw = 1;
                }
            }
        }

        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (popup_win && xev.type == Expose && xev.xany.window == popup_win) {
                draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
            } else if (popup_win && xev.type == ButtonPress) {
                /* Pointer is grabbed to popup_win while it's open (see
                 * open_context_menu) - every button press anywhere
                 * arrives here regardless of which window it physically
                 * landed on, same real technique egg_window.c's own
                 * popup uses. */
                /* Row 0 is the non-clickable id header (see g_full_id
                 * comment) - a real click there must not dispatch
                 * methods[0]. */
                int raw_row = xev.xbutton.y / POPUP_ROW_H;
                int row = raw_row - 1;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w &&
                             row >= 0 && row < n_methods;
                int header_click = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w && raw_row == 0;
                /* REAL FIX 2026-08-05, direct report ("context nav
                 * arrows and index #'s should get autofocus while its
                 * open or esp if 'headerbar' is touch[ed]"): a click on
                 * the non-clickable id header used to unconditionally
                 * close the whole menu with zero effect, easy to mistake
                 * for "the window just closed"/lost focus. Now a no-op
                 * that keeps the menu open (real keyboard grab already
                 * held the whole time - see open_context_menu()'s own
                 * real grab-retry fix above) instead of silently
                 * dismissing. */
                if (header_click) {
                    /* OPTION C: header click re-asserts soft focus on this
                     * menu (was pure no-op when grab alone was assumed). */
                    popup_soft_focus(dpy, popup_win);
                    need_redraw = 1;
                    continue;
                }
                if (!inside) {
                    /* REAL FIX 2026-08-07, direct instruction: clicking
                     * the same anchor again / clicking elsewhere must NOT
                     * close the menu - it stays open until the user
                     * clicks a real row or Cancel. Configurable via the
                     * package's meta.pdl: STATE | menu_stay_open | 0
                     * restores the old dismiss-on-any-click behavior. */
                    if (g_menu_stay_open) {
                        popup_soft_focus(dpy, popup_win);
                        need_redraw = 1;
                        continue;
                    }
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    need_redraw = 1;
                    continue;
                }
                close_context_menu(dpy, popup_win);
                popup_win = 0;
                nav_release_pid(g_house_root, getpid());
                if (choice_mode) {
                    /* REAL, 2026-08-05: a real Show Choices pick - write
                     * the chosen row's real action (a branch id) to the
                     * result file the waiting op is polling, instead of
                     * running the normal CLOSE/void/GOTO/etc dispatch. */
                    FILE *rf3 = fopen(choice_result_path, "w");
                    if (rf3) { fprintf(rf3, "%s\n", methods[row].action); fclose(rf3); }
                    append_history("SHOW_PAGE_PICK action=%s -> %s", methods[row].action, choice_result_path);
                    choice_mode = 0;
                } else {
                    append_history("CLICK method=%s action=%s", methods[row].label, methods[row].action);
                    /* REAL FIX 2026-08-04, direct instruction: this house
                     * already has a real, canonical convention for
                     * method dispatch - #.haiku+/tpmos-re-dox/fo-menu-
                     * sys.md ("Fuzz-Op Dynamic Menu System"): a
                     * METHOD's VALUE is a real, directly-executable op
                     * path (e.g. "projects/fuzz-op/ops/+x/feed_op.+x
                     * xlector"), or the literal keyword "void" for an
                     * internal-only no-op - NOT an abstract action
                     * keyword string-matched in C (the previous CLOSE/
                     * CANCEL/PLAY/STOP scheme this replaces). "CLOSE"
                     * stays a single reserved, internal keyword (closing
                     * THIS renderer's own event loop is not something an
                     * external process can do on this process's behalf)
                     * - everything else, including "void", is real
                     * fo-menu-sys.md-style dispatch: void = do nothing,
                     * any other VALUE = a real command line, executed
                     * with this package's own dir as argv[1] (same
                     * "pass the piece_id" convention fo-menu-sys.md's own
                     * examples show). */
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* Intentional no-op - e.g. "Cancel" - discoverable
                         * way to dismiss the menu with zero effect. */
                    } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                        /* Second reserved internal keyword, same class as
                         * CLOSE - see this file's own 2026-08-05
                         * comment on user_popup_win.
                         * REAL FIX 2026-08-05, direct instruction
                         * ("sub context menu is way offset, it could
                         * be right next to the other"): was positioned
                         * off the CLICK coordinate (wherever "User"
                         * happened to sit in the row list, often far
                         * down), not the main popup itself. Now
                         * adjacent to popup_win's own real position -
                         * right next to it, same top edge, regardless
                         * of which row was clicked. */
                        user_popup_x = popup_x + g_popup_w + 4;
                        user_popup_y = popup_y;
                        user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                    } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                        /* REAL, 2026-08-05: objects.pdl href navigation -
                         * see load_objects()'s own header comment.
                         * REAL FIX, same day, caught while wiring nav
                         * claims: this used to only update methods[]/
                         * n_methods without ever reopening popup_win -
                         * the page switch was invisible until the NEXT
                         * right-click. Now reopens at the same real
                         * position and re-claims nav numbers for the
                         * new page's own rows. */
                        const char *target = methods[row].action + 5;
                        for (int pi = 0; pi < n_obj_pages; pi++) {
                            if (strcmp(obj_pages[pi].name, target) == 0) {
                                if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                cur_page = pi;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                break;
                            }
                        }
                    } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                        if (page_stack_n > 0) {
                            cur_page = page_stack[--page_stack_n];
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                        }
                    } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                        /* REAL, 2026-08-05: objects.pdl real text-input
                         * row - same click-to-activate/Escape-to-commit
                         * shape this house's own cli_io field convention
                         * already uses. A small floating popup shows the
                         * live buffer (input_popup_win, drawn below);
                         * committing writes <package_dir>/<key>.txt. */
                        snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                        input_buffer[0] = '\0';
                        input_active = 1;
                        append_history("INPUT_ACTIVATE key=%s", input_key);
                        if (!input_popup_win) {
                            input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                        }
                    } else {
                        /* REAL FIX 2026-08-10, direct report ("bookstack no
                         * longer shows verse, event-ez button no longer
                         * opens event editor - path issue"): pals migration
                         * moved entities out of the dev-tree's fixed nesting
                         * depth (*.monads/*.widget/entities/<name>, always
                         * 4 levels under house_root) into
                         * xyzfs/users/<uuid>/home/livedesk/pals/<name>
                         * (a different depth entirely). METHOD/OBJECT
                         * actions that derived house_root by climbing a
                         * FIXED number of ".." from package_dir (argv[1])
                         * broke silently for any entity now living at pals'
                         * depth - not a bug in this dispatch call itself,
                         * but this is the one place that CAN fix it for
                         * every action at once: house_root is already
                         * known here (g_house_root), so pass it as a real
                         * second argument instead of making every
                         * downstream script/METHOD line re-derive it via
                         * fragile directory-climbing. Backward compatible -
                         * existing METHOD/OBJECT lines that only read $1
                         * (package_dir) are unaffected; only ones updated
                         * to also read $2 (sh scripts) / $1-after-$0 (sh -c
                         * lines, since $0 is already package_dir there)
                         * gain house_root. */
                        dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                    }
                }
                need_redraw = 1;
            } else if (popup_win && xev.type == KeyPress) {
                /* REAL, 2026-08-05, direct correction (see this file's
                 * own nav_claim_rows()/TILE_PICKER_DESIGN.md §13): the
                 * earlier local-digit-selects-a-row idea was wrong -
                 * "[N]" is now a real, GLOBAL, shared live address (see
                 * nav_claim_rows()), not a local 1-9 shortcut, so a bare
                 * digit keypress here no longer means anything - real
                 * jump-by-number now only happens remotely, via the
                 * taskbar's own terminal input writing an
                 * "ACTIVATE_NAV:<N>" command into interact_relay.txt
                 * (see the relay-poll block above). Escape still just
                 * closes the menu locally, same as "Cancel". */
                /* REAL FIX, same day, follow-up ("just wanna make sure
                 * the fundamentals are in place"): real Up/Down moves a
                 * real focus cursor (popup_focus_row, wraps both ways)
                 * matching real chtpm's own "[>]" convention - see
                 * draw_context_menu()'s own comment. Enter activates
                 * whichever row currently holds the cursor - same real
                 * dispatch logic the mouse-click/ACTIVATE_NAV paths use. */
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (klen < 0) klen = 0;
                if (klen >= (int)sizeof(kbuf)) klen = (int)sizeof(kbuf) - 1;
                kbuf[klen] = '\0';
                if (ks == XK_Escape) {
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    popup_digit_accum = 0;
                    need_redraw = 1;
                } else if (ks == XK_Up) {
                    if (n_methods > 0) {
                        popup_focus_row = (popup_focus_row - 1 + n_methods) % n_methods;
                        popup_digit_accum = 0;
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (ks == XK_Down) {
                    if (n_methods > 0) {
                        popup_focus_row = (popup_focus_row + 1) % n_methods;
                        popup_digit_accum = 0;
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                    /* chtpm digit_accum: jump [>] to global nav index in this menu */
                    int d = kbuf[0] - '0';
                    int lo = popup_nav_base;
                    int hi = popup_nav_base + n_methods - 1; /* inclusive */
                    if (n_methods <= 0) { /* no-op */ }
                    else {
                        int new_val = popup_digit_accum * 10 + d;
                        int jumped = 0;
                        if (new_val >= lo && new_val <= hi) {
                            popup_digit_accum = new_val;
                            popup_focus_row = new_val - popup_nav_base;
                            jumped = 1;
                        } else if (d >= lo && d <= hi) {
                            /* out of range as append — restart with d if valid address */
                            popup_digit_accum = d;
                            popup_focus_row = d - popup_nav_base;
                            jumped = 1;
                        } else if (d >= 1 && d <= n_methods && lo <= d && d <= hi) {
                            popup_digit_accum = d;
                            popup_focus_row = d - popup_nav_base;
                            jumped = 1;
                        } else if (d >= 1 && d <= n_methods) {
                            /* local 1..N when global range doesn't include small digits */
                            /* only if nav numbers are lo..hi; if lo>9, d alone won't match */
                            popup_digit_accum = 0;
                        } else {
                            popup_digit_accum = 0;
                        }
                        if (jumped)
                            draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    popup_digit_accum = 0;
                    int row = popup_focus_row;
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    if (choice_mode) {
                        FILE *rf4 = fopen(choice_result_path, "w");
                        if (rf4) { fprintf(rf4, "%s\n", methods[row].action); fclose(rf4); }
                        append_history("SHOW_PAGE_PICK(cursor) action=%s -> %s", methods[row].action, choice_result_path);
                        choice_mode = 0;
                        need_redraw = 1;
                        goto skip_enter_dispatch;
                    }
                    append_history("CLICK(cursor) method=%s action=%s", methods[row].label, methods[row].action);
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* no-op */
                    } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                        user_popup_x = popup_x + g_popup_w + 4;
                        user_popup_y = popup_y;
                        user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                    } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                        const char *target = methods[row].action + 5;
                        for (int pi = 0; pi < n_obj_pages; pi++) {
                            if (strcmp(obj_pages[pi].name, target) == 0) {
                                if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                cur_page = pi;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                popup_focus_row = 0; popup_digit_accum = 0;
                                break;
                            }
                        }
                    } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                        if (page_stack_n > 0) {
                            cur_page = page_stack[--page_stack_n];
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                            popup_focus_row = 0; popup_digit_accum = 0;
                        }
                    } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                        snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                        input_buffer[0] = '\0';
                        input_active = 1;
                        append_history("INPUT_ACTIVATE key=%s", input_key);
                        if (!input_popup_win) {
                            input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                        }
                    } else {
                        dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                    }
                    need_redraw = 1;
                    skip_enter_dispatch: ;
                }
            } else if (user_popup_win && xev.type == Expose && xev.xany.window == user_popup_win) {
                draw_context_menu(dpy, user_popup_win, popup_gc, user_methods, 4, -1, -1);
            } else if (user_popup_win && xev.type == ButtonPress) {
                int raw_row = xev.xbutton.y / POPUP_ROW_H;
                int row = raw_row - 1;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w &&
                             row >= 0 && row < 4;
                if (!inside) {
                    /* REAL FIX 2026-08-07: same "menus stay open until
                     * Cancel" rule as the main menu - an outside click
                     * must not dismiss this submenu either. Toggle with
                     * STATE | menu_stay_open | 0 in meta.pdl. */
                    if (g_menu_stay_open) {
                        popup_soft_focus(dpy, user_popup_win);
                        need_redraw = 1;
                        continue;
                    }
                    close_context_menu(dpy, user_popup_win);
                    user_popup_win = 0;
                    need_redraw = 1;
                    continue;
                }
                close_context_menu(dpy, user_popup_win);
                user_popup_win = 0;
                if (strcmp(user_methods[row].action, "OPEN_RANGE_GRID") == 0) {
                    /* REAL FIX 2026-08-05: this used to draw an OPAQUE
                     * popup right here (real, but covered up the dog
                     * and nearby tiles - direct correction: "it should
                     * just be a transparent outline like a png").
                     * Consolidated into tp_range_grid.+x, a real
                     * standalone binary using the X11 Shape Extension
                     * (same technique this file's own sprite
                     * transparency already uses) so only the outline
                     * strokes are opaque - launched here instead of
                     * duplicating that logic. Centered on THIS
                     * window's own real position (win_x/win_y, the
                     * same coords XMoveWindow already uses), not the
                     * submenu popup's location - direct correction
                     * ("the range finder should be around the dog
                     * tho, its off center"). */
                    int grid_size = 5 * GRID_CELL_PX;
                    int gx = win_x + WIN_PX / 2 - grid_size / 2;
                    int gy = win_y + WIN_PX / 2 - grid_size / 2;
                    char self_path[PATH_BUF];
                    if (self_exe_path(self_path, sizeof(self_path))) {
                        char *ops_dir = dirname(self_path);
                        char cmd[PATH_BUF * 2];
                        snprintf(cmd, sizeof(cmd), "'%s/tp_range_grid.+x' %d %d >/dev/null 2>&1 &",
                                 ops_dir, gx, gy);
                        int rc = system(cmd);
                        (void)rc;
                    }
                }
                need_redraw = 1;
            } else if (input_popup_win && xev.type == Expose && xev.xany.window == input_popup_win) {
                char disp[300];
                snprintf(disp, sizeof(disp), "%s: %s_", input_key, input_buffer);
                XClearWindow(dpy, input_popup_win);
                XDrawRectangle(dpy, input_popup_win, popup_gc, 0, 0, g_popup_w - 1, POPUP_ROW_H * 2 - 1);
                popup_draw_text(dpy, input_popup_win, popup_gc, 8, POPUP_ROW_H, disp);
            } else if (input_active && xev.type == KeyPress) {
                /* REAL, 2026-08-05: real text-input for an objects.pdl
                 * "STATE:<key>" row - see the ButtonPress branch above
                 * for how input_active/input_key get set. Escape commits
                 * (matching this house's own cli_io field convention:
                 * activate, type, Escape deactivates/persists - NOT
                 * Enter), Backspace edits, any other printable key
                 * appends. */
                char kbuf[32];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Escape) {
                    char statepath[PATH_BUF];
                    snprintf(statepath, sizeof(statepath), "%s/%s.txt", package_dir, input_key);
                    FILE *sf = fopen(statepath, "w");
                    if (sf) { fprintf(sf, "%s\n", input_buffer); fclose(sf); }
                    append_history("INPUT_COMMIT key=%s value=%s", input_key, input_buffer);
                    input_active = 0;
                    if (input_popup_win) { close_context_menu(dpy, input_popup_win); input_popup_win = 0; }
                } else if (ks == XK_BackSpace) {
                    size_t l = strlen(input_buffer);
                    if (l > 0) input_buffer[l - 1] = '\0';
                } else if (klen > 0) {
                    size_t l = strlen(input_buffer);
                    if (l + (size_t)klen < sizeof(input_buffer)) {
                        memcpy(input_buffer + l, kbuf, (size_t)klen);
                        input_buffer[l + klen] = '\0';
                    }
                }
                if (input_popup_win) {
                    char disp[300];
                    snprintf(disp, sizeof(disp), "%s: %s_", input_key, input_buffer);
                    XClearWindow(dpy, input_popup_win);
                    XDrawRectangle(dpy, input_popup_win, popup_gc, 0, 0, g_popup_w - 1, POPUP_ROW_H * 2 - 1);
                    XDrawString(dpy, input_popup_win, popup_gc, 8, POPUP_ROW_H,
                                disp, (int)strlen(disp));
                }
            } else if (text_popup_win && xev.type == Expose && xev.xany.window == text_popup_win) {
                XClearWindow(dpy, text_popup_win);
                int pop_w2 = 0, pop_h2 = 0;
                { Window root_r; int x_r, y_r; unsigned int w_r, h_r, bw_r, depth_r;
                  XGetGeometry(dpy, text_popup_win, &root_r, &x_r, &y_r, &w_r, &h_r, &bw_r, &depth_r);
                  pop_w2 = (int)w_r; pop_h2 = (int)h_r; }
                XDrawRectangle(dpy, text_popup_win, popup_gc, 0, 0, pop_w2 - 1, pop_h2 - 1);
                for (int li = 0; li < g_text_popup_n_lines; li++) {
                    popup_draw_text(dpy, text_popup_win, popup_gc, 8, (li + 1) * POPUP_ROW_H - 6, g_text_popup_lines[li]);
                }
            } else if (text_popup_win && xev.type == ButtonPress &&
                       xev.xany.window == text_popup_win) {
                /* REAL FIX 2026-08-24, direct user report ("i dont want it
                 * to close unless i click it exactly (not even if i click
                 * something else)"): this used to be
                 * `(ButtonPress || KeyPress)` with NO window check - any
                 * key anywhere or any click anywhere dismissed the box
                 * (and, while its input grabs were still in play, ate the
                 * event out from under whichever app it belonged to).
                 * Real RPG Maker "Show Text" waits for a confirm press,
                 * but this house's real use is screenshot-and-inspect:
                 * the box is now a passive overlay. ONLY a ButtonPress
                 * delivered ON the popup window itself closes it; keys of
                 * any kind never do; clicks on other windows are none of
                 * this process's business (no grab is taken at creation -
                 * see the creation-site fix above). */
                close_context_menu(dpy, text_popup_win);
                text_popup_win = 0;
                append_history("SHOW_TEXT_DISMISSED");
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
            } else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                /* REAL FIX 2026-08-04, direct instruction ("egg-pets
                 * snap to grid... do u see that logic"): same
                 * round-to-nearest-cell technique egg_window.c's own
                 * ButtonRelease handler uses, same GRID_CELL_PX (80),
                 * so a placed tile lands on the identical desktop grid
                 * egg-pals already use, not a separate/incompatible one. */
                int grid_x = (win_x + GRID_CELL_PX / 2) / GRID_CELL_PX;
                int grid_y = (win_y + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (grid_x < 0) grid_x = 0;
                if (grid_x > max_col) grid_x = max_col;
                if (grid_y < 0) grid_y = 0;
                if (grid_y > max_row) grid_y = max_row;
                win_x = grid_x * GRID_CELL_PX;
                win_y = grid_y * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_x, win_y);
                write_pos(package_dir, win_x, win_y);
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                /* REAL, NEW 2026-08-04: right-click now opens the real
                 * data-driven context menu (see load_methods() above)
                 * instead of closing immediately - matches the ask
                 * ("add the context menus that already exist from
                 * egg-pal to these by default"). */
                if (!popup_win) {
                    /* REAL FIX 2026-08-06: always reload base menu from DISK
                     * (objects.pdl if present, else meta.pdl). In-memory
                     * obj_pages went stale after SHOW_PAGE AND after user
                     * edited objects.pdl/meta.pdl while the process lived
                     * (Events (ez) missing until restart). */
                    read_menu_config(package_dir); /* re-read menu guard toggles too */
                    n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                    using_objects = (n_obj_pages > 0);
                    if (using_objects) {
                        cur_page = 0;
                        page_stack_n = 0;
                        n_methods = obj_pages[cur_page].n_items;
                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                    } else {
                        n_methods = load_methods(package_dir, methods, MAX_METHODS);
                        if (n_methods == 0) {
                            snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                            snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                            n_methods = 1;
                        }
                    }
                    popup_x = xev.xbutton.x_root;
                    popup_y = xev.xbutton.y_root;
                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                    popup_focus_row = 0; popup_digit_accum = 0;
                    if (popup_win)
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                }
            } else if (xev.type == MotionNotify && dragging) {
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_x += dx; win_y += dy;
                XMoveWindow(dpy, win, win_x, win_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
                need_redraw = 1;
            } else if (xev.type == KeyPress) {
                if (popup_win || user_popup_win || input_popup_win || text_popup_win || input_active) {
                    /* REAL FIX 2026-08-07, direct instruction ("print
                     * screen closes the context"): with a menu/popup open,
                     * an unhandled key (Print Screen, media keys, etc.) is
                     * NOT a close signal. The old fallback here set
                     * running = 0 on ANY key, so pressing Print Screen
                     * while a bible-verse / context menu was up silently
                     * killed the whole tile window. Menus stay open until
                     * the user clicks Cancel or presses Escape/Enter. */
                } else {
#ifndef _WIN32
                    running = 0; /* no menu open: any key closes the tile, as before */
#endif
                }
            }
        }
        if (!running) break;
#ifndef _WIN32
        if (!package_still_exists(package_dir)) break; /* package gone -> stop pointing at nothing */
#endif

        /* REAL FIX 2026-08-04, direct instruction ("dont render more
         * than 30fps etc, cpu is getting hot"): explicit, measured frame
         * pacing (not just relying on select()'s 300ms timeout, which
         * doesn't bound how often this loop redraws once real X events
         * start arriving, e.g. during a drag). Skips the redraw entirely
         * if nothing changed AND less than one frame interval has
         * passed since the last real swap - a static, undragged tile
         * does zero GL work between polls. */
        struct timeval now;
        gettimeofday(&now, NULL);
        /* Win long is 32-bit: (tv_sec * 1000000) overflows, goes negative,
         * and this skip never presents a frame. Linux long is 64-bit. */
        long long elapsed_usec =
            ((long long)now.tv_sec - last_frame.tv_sec) * 1000000LL +
            ((long long)now.tv_usec - last_frame.tv_usec);
        if (!need_redraw) {
            continue;
        }
        if (last_frame.tv_sec != 0 && elapsed_usec < MIN_FRAME_USEC) {
            long rem = (long)(MIN_FRAME_USEC - elapsed_usec);
            if (rem > 0 && rem < 1000000L) usleep((useconds_t)rem);
            continue;
        }

        {
            int bg_r = (int)(r * 255.0f), bg_g = (int)(g * 255.0f), bg_b = (int)(b * 255.0f);
            XSetForeground(dpy, g_buf_gc, ((unsigned long)bg_r << 16) | ((unsigned long)bg_g << 8) | (unsigned long)bg_b);
            XFillRectangle(dpy, g_buf, g_buf_gc, 0, 0, WIN_PX, WIN_PX);
            if (g_has_sprite) draw_sprite_rgb(dpy, g_buf, g_buf_gc, bg_r, bg_g, bg_b);
            else if (g_font_loaded) draw_glyph_rgb(dpy, g_buf, g_buf_gc, glyph);

            /* Present: same compose->present pattern as db-hq/taskbar -
             * one XGetImage capture off the buffer, XPutImage onto the
             * real window (proven pixel-identical in the original Phase
             * 0 test). Falls back to a direct XCopyArea if XGetImage
             * ever fails, matching the same safety fallback used there. */
            XSync(dpy, False);
            XImage *frame = XGetImage(dpy, g_buf, 0, 0, WIN_PX, WIN_PX, AllPlanes, ZPixmap);
            if (frame) {
                XPutImage(dpy, win, g_buf_gc, frame, 0, 0, 0, 0, WIN_PX, WIN_PX);
                XDestroyImage(frame);
            } else {
                XCopyArea(dpy, g_buf, win, g_buf_gc, 0, 0, WIN_PX, WIN_PX, 0, 0);
            }
        }
        last_frame = now;
    }

    if (popup_win) close_context_menu(dpy, popup_win); /* e.g. closed via keypress while menu was still open */
    if (user_popup_win) close_context_menu(dpy, user_popup_win);
    if (input_popup_win) close_context_menu(dpy, input_popup_win);
    /* Stage 2c PROOF - don't orphan the khtpm menu child if this entity's
     * own window closes while the menu is still up. */
    if (g_khtpm_menu_pid > 0) { kill(g_khtpm_menu_pid, SIGTERM); waitpid(g_khtpm_menu_pid, NULL, WNOHANG); }
    if (g_house_root[0]) {
        livedesk_registry_remove(g_house_root, getpid());
        nav_release_pid(g_house_root, getpid());
    }
    XFreeGC(dpy, popup_gc);
    free(g_sprite_pixels);
    XFreeGC(dpy, g_buf_gc);
    XFreePixmap(dpy, g_buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
