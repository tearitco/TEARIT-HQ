/* khtpm_taskbar_manager.c — single toolbar logic for all platforms
 * (renamed from khtpm_taskbar_core.c 2026-08-10 — "manager" naming to
 * match real CHTPM convention, see khtpm-refactor-plan.md §10). */
#ifndef _WIN32
#define _DEFAULT_SOURCE  /* kill()/pid_t under -std=c11 strict mode, matches tp_taskbar.c's own convention */
#endif
#include "khtpm_taskbar_manager.h"

/* macOS leg (2026-08-22): no `setsid` binary on macOS; every detached
 * spawn in this file uses the "setsid nohup ..." shell shape, which
 * silently fails (sh: setsid: command not found → /dev/null). nohup+&
 * already detaches for this launcher pattern (the mac start script
 * proves it), so the prefix becomes empty there — Linux byte-identical.
 * Same rationale as run_shortcut()'s KTB_SETSID in the manager driver. */
#ifdef __APPLE__
#define KTB_SETSID ""
#else
#define KTB_SETSID "setsid "
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>

/* Forward decl - ktb_init() (below) needs this before its own real
 * definition, further down this file (see that definition's own header
 * comment: cursword must always be running, checked/relaunched on every
 * taskbar-manager start). */
static void livedesk_ensure_cursword(const char *house_root);

/* Forward decls - ktb_init() (below) needs these before their own real
 * definitions, further down this file. livedesk_spawn_desk() is the core
 * real logic for spawning all entities listed in a desk's PDL; reused by
 * live desk switches and now also by livedesk_spawn_active_desk() on startup.
 * livedesk_spawn_active_desk() itself reads the currently-active session's
 * currently-active desk, then calls livedesk_spawn_desk() to relaunch its
 * entities. This ensures that when button.sh reset rebuilds the manager,
 * the active desk's real entity set (not a static autostart.pdl list that
 * can drift) gets reliably relaunched. */
static void livedesk_spawn_desk(const char *house_root, const char *sroot, const char *id, const char *desk);
static void livedesk_spawn_active_desk(const char *house_root);

/* REAL, NEW 2026-08-25 (direct request: a general "kill hq" menu row that
 * covers EVERYTHING the taskbar launches, not just a fixed -hq binary
 * name list — real live test proved the fixed-list kill_hq_windows.sh
 * killed db-hq but did nothing for a toys-cell launch, e.g.
 * mutaclysm-neo, because nothing ever recorded that launch's PID
 * anywhere; see au11-hq/TPMOS-COMPLIANCE-DEBT.md's own toys-teardown
 * gap note). Every real launch site in this file already ends its own
 * built `cmd`/`sh` string in a trailing `&` (backgrounds under `setsid`,
 * which makes that PID its OWN process-group leader — a real, existing
 * property, not something added here). Wrapping the SAME already-built
 * string with one more statement in the SAME shell invocation
 * (`; echo $! >> "<registry>"`) records that PID for free, with zero
 * change to how any individual site builds its own command. Swap
 * `system(cmd)`/`system(sh)` call sites to go through this instead —
 * see kill_hq_windows.sh's own now-generalized body for the reader side
 * (kills the whole recorded process GROUP via `kill -TERM -$pid`, not
 * just the one PID, since only killing the group leader itself would
 * leave real descendants like a toy's own window process still
 * running). Placed HERE (after <stdio.h>/<stdlib.h>, not before them -
 * an earlier version of this comment+function sat above those includes
 * and only "worked" via implicit int declarations of snprintf/system,
 * a real bug caught by a real compiler warning during this same pass,
 * not by inspection.
 *
 * SECOND real bug, also caught live (not by inspection) driving the real
 * HQ menu's "stats" row through the real relay after this landed:
 * `sh: 1: Syntax error: ";" unexpected`. Every `cmd` passed in already
 * ends in ` &` (backgrounding IS a statement separator, same role as
 * `;`) - the original `"%s; echo $! ..."` format put TWO separators back
 * to back (`& ; echo`), which is an empty-statement syntax error in
 * dash. Fix: no explicit `;` - a plain space after the caller's own
 * trailing `&` is a valid, single separator (`cmd & echo $! ...`). */
static int ktb_system_recorded(const char *house_root, const char *cmd) {
    char wrapped[KTB_PATH_BUF * 4];
    snprintf(wrapped, sizeof(wrapped),
             "%s echo $! >> \"%s/#.desktop/livedesk_launched_pids.txt\"",
             cmd, house_root);
    return system(wrapped);
}
#endif

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <tlhelp32.h>
#  include <io.h>
#  include <dirent.h>
#  include <direct.h>
#  include <sys/stat.h>
#  include <time.h>
#  ifndef S_ISDIR
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#  endif
#  define mkdir(path, mode) _mkdir(path)
static char *strsep(char **stringp, const char *delim) {
    char *s, *p;
    if (!stringp || !*stringp) return NULL;
    s = *stringp;
    p = s;
    while (*p && !strchr(delim, *p)) p++;
    if (*p) { *p = '\0'; *stringp = p + 1; }
    else *stringp = NULL;
    return s;
}
static int ktb_nanosleep_win(long nsec) {
    DWORD ms = (DWORD)(nsec / 1000000L);
    if (ms == 0) ms = 1;
    Sleep(ms);
    return 0;
}
#  define nanosleep(ts, rem) ktb_nanosleep_win((ts) ? (ts)->tv_nsec : 0)
static FILE *ktb_fopen(const char *path, const char *mode) {
    wchar_t wp[KTB_PATH_BUF], wm[16];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, KTB_PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, path, -1, wp, KTB_PATH_BUF))
        return fopen(path, mode);
    MultiByteToWideChar(CP_ACP, 0, mode, -1, wm, 16);
    return _wfopen(wp, wm);
}
static void win_star_alias(char *path) {
    char tmp[KTB_PATH_BUF];
    size_t o = 0;
    int at_comp = 1;
    const char *p;
    if (!path || !path[0]) return;
    for (p = path; *p && o + 1 < sizeof(tmp); p++) {
        if (at_comp && p[0] == '*' && p[1] == '.') {
            tmp[o++] = '_';
            at_comp = 0;
            continue;
        }
        tmp[o++] = *p;
        at_comp = (*p == '/' || *p == '\\');
    }
    tmp[o] = '\0';
    memcpy(path, tmp, o + 1);
}
static void win_exe_suffix(char *path) {
    size_t n = strlen(path);
    if (n > 3 && strcmp(path + n - 3, ".+x") == 0)
        memcpy(path + n - 3, ".exe", 5);
    else if (n > 4 && _stricmp(path + n - 4, ".exe") != 0)
        strcat(path, ".exe");
}
static int win_spawn_n(const char *exe, const char **args, int nargs) {
    char e[KTB_PATH_BUF];
    snprintf(e, sizeof(e), "%s", exe);
    win_star_alias(e);
    win_exe_suffix(e);
    for (char *p = e; *p; p++) if (*p == '/') *p = '\\';
    wchar_t wcmd[KTB_PATH_BUF * 4];
    wchar_t we[KTB_PATH_BUF];
    MultiByteToWideChar(CP_UTF8, 0, e, -1, we, KTB_PATH_BUF);
    wchar_t acc[KTB_PATH_BUF * 4];
    _snwprintf(acc, (KTB_PATH_BUF * 4) - 1, L"\"%s\"", we);
    for (int i = 0; i < nargs; i++) {
        char a[KTB_PATH_BUF];
        snprintf(a, sizeof(a), "%s", args[i] ? args[i] : ".");
        win_star_alias(a);
        for (char *p = a; *p; p++) if (*p == '/') *p = '\\';
        wchar_t wa[KTB_PATH_BUF];
        MultiByteToWideChar(CP_UTF8, 0, a, -1, wa, KTB_PATH_BUF);
        wchar_t more[KTB_PATH_BUF * 4];
        _snwprintf(more, (KTB_PATH_BUF * 4) - 1, L"%s \"%s\"", acc, wa);
        wcsncpy(acc, more, (KTB_PATH_BUF * 4) - 1);
        acc[(KTB_PATH_BUF * 4) - 1] = 0;
    }
    wcsncpy(wcmd, acc, (KTB_PATH_BUF * 4) - 1);
    wcmd[(KTB_PATH_BUF * 4) - 1] = 0;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS;
    BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, L".", &si, &pi);
    if (!ok) {
        flags = CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS;
        ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, L".", &si, &pi);
    }
    if (!ok) return 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}
static int win_spawn_cwd(const char *exe, const char *arg) {
    const char *a = arg ? arg : ".";
    return win_spawn_n(exe, &a, 1);
}
static void ktb_kill_by_exe(const char *stem) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (strstr(pe.szExeFile, stem)) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (h) { TerminateProcess(h, 0); CloseHandle(h); }
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}
#else
#  include <sys/types.h>
#  include <unistd.h>
#  include <signal.h>
#  include <sys/stat.h>
#  include <sys/file.h>
#  include <fcntl.h>
#  include <dirent.h>
#  include <time.h>
#  define ktb_fopen fopen
#endif

#ifdef _WIN32
static int ktb_my_pid(void) { return (int)GetCurrentProcessId(); }
#else
static int ktb_my_pid(void) { return (int)getpid(); }
#endif

static void path_join(char *out, size_t n, const char *a, const char *b) {
    if (!a || !a[0] || strcmp(a, ".") == 0) {
        snprintf(out, n, "%s", b ? b : "");
        return;
    }
    size_t al = strlen(a);
    if (a[al - 1] == '/' || a[al - 1] == '\\')
        snprintf(out, n, "%s%s", a, b);
    else
#ifdef _WIN32
        snprintf(out, n, "%s\\%s", a, b);
#else
        snprintf(out, n, "%s/%s", a, b);
#endif
}

int ktb_pid_alive(int pid) {
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    DWORD code = 0;
    int ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return ok;
#else
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
#endif
}

void ktb_init(KtbState *s, const char *house_root) {
    memset(s, 0, sizeof(*s));
    snprintf(s->house_root, sizeof(s->house_root), "%s",
             (house_root && house_root[0]) ? house_root : ".");
    path_join(s->pid_path, sizeof(s->pid_path), s->house_root, "#.desktop/livedesk_taskbar.pid");
#ifdef _WIN32
    for (char *p = s->pid_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    snprintf(s->theme_bg, sizeof(s->theme_bg), "white");
    snprintf(s->theme_fg, sizeof(s->theme_fg), "black");
    s->tab_focus_idx = 0;
    s->strip_focus_cell = 0; /* matches tp_taskbar.c's own strip_focus_cell default (button 1 / HQ) */
    s->strip_user_cmd[0] = '\0';
    ktb_load_cell_ids(s);
    /* Real, new 2026-08-30, direct instruction ("cursword is an entity
     * that should always be open... its the users assistant. 1rst
     * entity."): every taskbar-manager start is a real opportunity for
     * cursword to have lapsed (first boot ever, a prior crash, having
     * been closed by hand last session) - see
     * livedesk_ensure_cursword()'s own header comment (declared later
     * in this same file, forward-declared alongside livedesk_close_all
     * near the top) for the full real design. */
    livedesk_ensure_cursword(s->house_root);
    /* Real, new 2026-08-31, direct instruction ("yes have the active desk
     * be run from autostart"): on taskbar-manager start (when button.sh reset
     * triggers), spawn the current active session's active desk entities
     * using the same proven real logic as live desk switches. This closes the
     * structural drift: instead of relying on a static, separately-maintained
     * autostart.pdl list (which can become out-of-sync when entities are
     * added/removed), we now relaunch the REAL, genuine active desk's entity
     * set, the exact one the user left last session. */
    livedesk_spawn_active_desk(s->house_root);
}

/* REAL, NEW 2026-08-16 - see KtbState's own cell_id_pos/cell_id_str
 * field comment for the full real cross-process reasoning. Read once
 * at startup; khtpm_strip_header.chtpm's own button order never
 * changes at runtime, so no need to re-read later. Missing file (e.g.
 * strip_parser hasn't run yet this session) is a real, harmless no-op
 * - every ktb_cell_id() lookup just returns "" and the existing
 * which==N dispatch chain keeps working exactly as before. */
void ktb_load_cell_ids(KtbState *s) {
    s->n_cell_ids = 0;
    char path[KTB_PATH_BUF];
    path_join(path, sizeof(path), s->house_root, "#.desktop/livedesk_header_cell_ids.txt");
    FILE *f = ktb_fopen(path, "r");
    if (!f) return;
    char line[128];
    while (s->n_cell_ids < 15 && fgets(line, sizeof(line), f)) {
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = '\0';
        int pos = atoi(line);
        char *id = bar + 1;
        id[strcspn(id, "\r\n")] = '\0';
        if (pos < 1 || !id[0]) continue;
        s->cell_id_pos[s->n_cell_ids] = pos;
        snprintf(s->cell_id_str[s->n_cell_ids], sizeof(s->cell_id_str[0]), "%s", id);
        s->n_cell_ids++;
    }
    fclose(f);
}

/* REAL, NEW 2026-08-16 - real position(which)->id lookup, "" if this
 * cell has no real declared id yet (every existing cell today, until
 * migrated one at a time - real, deliberate incremental adoption, not
 * a forced rewrite of the whole dispatch chain in one risky pass). */
const char *ktb_cell_id(const KtbState *s, int which) {
    for (int i = 0; i < s->n_cell_ids; i++)
        if (s->cell_id_pos[i] == which) return s->cell_id_str[i];
    return "";
}

void ktb_write_pidfile(KtbState *s, int pid) {
    FILE *f = ktb_fopen(s->pid_path, "w");
    if (f) { fprintf(f, "%d\n", pid); fclose(f); }
}

void ktb_unlink_pidfile(const KtbState *s) {
    remove(s->pid_path);
}

static int load_shortcuts(KtbState *s) {
    char path[KTB_PATH_BUF];
    path_join(path, sizeof(path), s->house_root, "#.desktop/livedesk_shortcuts.pdl");
#ifdef _WIN32
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(path, "r");
    s->n_shortcuts = 0;
    if (!f) return 0;
    char line[KTB_PATH_BUF];
    while (s->n_shortcuts < KTB_MAX_SHORTCUTS && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SHORTCUT", 8) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *ge = end;
        while (ge > p && ge[-1] == ' ') ge--;
        size_t glen = (size_t)(ge - p);
        if (glen == 0 || glen >= sizeof(s->shortcuts[0].glyph)) continue;
        memcpy(s->shortcuts[s->n_shortcuts].glyph, p, glen);
        s->shortcuts[s->n_shortcuts].glyph[glen] = 0;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = 0;
        snprintf(s->shortcuts[s->n_shortcuts].command,
                 sizeof(s->shortcuts[0].command), "%s", v);
        s->n_shortcuts++;
    }
    fclose(f);
    return s->n_shortcuts;
}

static void load_theme(KtbState *s) {
    snprintf(s->theme_bg, sizeof(s->theme_bg), "white");
    snprintf(s->theme_fg, sizeof(s->theme_fg), "black");
    char path[KTB_PATH_BUF];
    path_join(path, sizeof(path), s->house_root, "#.desktop/livedesk_theme.pdl");
#ifdef _WIN32
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(path, "r");
    if (!f) return;
    char line[KTB_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char key[16];
        size_t klen = (size_t)(end - p);
        while (klen && p[klen - 1] == ' ') klen--;
        if (klen >= sizeof(key)) continue;
        memcpy(key, p, klen); key[klen] = 0;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = 0;
        if (strcmp(key, "bg") == 0) snprintf(s->theme_bg, sizeof(s->theme_bg), "%s", v);
        else if (strcmp(key, "fg") == 0) snprintf(s->theme_fg, sizeof(s->theme_fg), "%s", v);
    }
    fclose(f);
}

/* Forward declarations - real bodies are below (registry_lock_acquire/
 * release need a live house_root/fd, defined near their other registry
 * helpers), but load_tabs() above that point is the actual unlocked
 * writer this cross-process lock exists to guard - see this fix's own
 * header comment at KTB_LIVEDESK_USE_REGISTRY_LOCK's definition in
 * khtpm_taskbar_manager.h for the real bug this closes. */
static void registry_lock_acquire(const char *house_root);
static void registry_lock_release(void);

/* load_tabs + prune dead (Linux design) */
static int load_tabs(KtbState *s) {
    char reg_path[KTB_PATH_BUF], tmp_path[KTB_PATH_BUF];
    path_join(reg_path, sizeof(reg_path), s->house_root, "#.desktop/livedesk_open.txt");
    path_join(tmp_path, sizeof(tmp_path), s->house_root, "#.desktop/livedesk_open.txt.tmp");
#ifdef _WIN32
    for (char *p = reg_path; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    /* Same cross-process flock() tp_desktop_window*.c's own
     * livedesk_registry_add() already takes around ITS read-prune-
     * write-rename cycle on this exact file - this critical section
     * needs the identical lock, not a separate/looser one, or the two
     * sides can still stomp each other even with both individually
     * "locked" (see KTB_LIVEDESK_USE_REGISTRY_LOCK's own header comment
     * in khtpm_taskbar_manager.h for the real bug this closes). */
    registry_lock_acquire(s->house_root);
    FILE *f = ktb_fopen(reg_path, "r");
    s->n_tabs = 0;
    if (!f) { registry_lock_release(); return 0; }
    FILE *w = ktb_fopen(tmp_path, "w");
    char line[KTB_PATH_BUF];
    while (s->n_tabs < KTB_MAX_TABS && fgets(line, sizeof(line), f)) {
        KtbTab t;
        memset(&t, 0, sizeof(t));
        char *p;
        if ((p = strstr(line, "PID="))) t.pid = atoi(p + 4);
        if ((p = strstr(line, "ENTITY="))) {
            char *e = p + 7;
            char *end = strchr(e, '|');
            size_t len = end ? (size_t)(end - e) : strcspn(e, "\r\n");
            if (len >= sizeof(t.entity)) len = sizeof(t.entity) - 1;
            memcpy(t.entity, e, len);
            t.entity[len] = 0;
        }
        if ((p = strstr(line, "PATH="))) {
            snprintf(t.path, sizeof(t.path), "%s", p + 5);
            t.path[strcspn(t.path, "\r\n")] = 0;
        }
        if (!t.entity[0] || !ktb_pid_alive(t.pid)) continue;
        if (w) fputs(line, w);
        s->tabs[s->n_tabs++] = t;
    }
    fclose(f);
#ifdef _WIN32
    /* Do not rewrite livedesk_open.txt here. remove+rename races rgb
     * append and Win rename-cannot-replace, leaving one tab (asa).
     * Prune is in-memory only; dead lines stay on disk until quit. */
    if (w) { fclose(w); remove(tmp_path); }
#else
    if (w) { fclose(w); remove(reg_path); rename(tmp_path, reg_path); }
#endif
    registry_lock_release();
    return s->n_tabs;
}

/* sync_tab_claims — Linux design, KIND=tab only; leave KIND=row alone */
static void sync_tab_claims(KtbState *s) {
    char claims_path[KTB_PATH_BUF], tmp_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt");
    path_join(tmp_path, sizeof(tmp_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    /* Real bug fix (2026-08-11, direct live report: "numbering is very
     * weird now beyond 12. it goes from 31 to 14... cant we get it to do
     * more uniform numbering?"). This used to derive each tab's number
     * from `max_nav+1`, where max_nav was the highest NAV= value seen
     * ANYWHERE in the shared claims file — including stale KIND=row
     * entries left behind by entity PIDs that died and got relaunched
     * during testing (confirmed directly: the live claims file had
     * KIND=row NAV=20-23 entries for a book-stack PID that no longer
     * exists, plus tab claims scattered as 14/15/17/18/19/31/32 — gapped
     * and monotonically growing across restarts, never pruned, never
     * uniform). The parser's own digit-jump math (khtpm_strip_parser.c's
     * unified_apply()/g_nav_focus) already ASSUMES tabs are simply
     * KTB_STRIP_N_CELLS + position + 1, contiguous, no gaps — the
     * claims-pool-derived numbers here had silently drifted out of sync
     * with that assumption too, not just looking "weird" cosmetically.
     * Fixed: tab numbers are now purely POSITIONAL (deterministic,
     * matches the parser's own model exactly, always 13,14,15... with no
     * gaps regardless of claims-file history) — the shared claims file is
     * still WRITTEN (so entities' own row-claiming still sees which
     * numbers the strip is using) but no longer READ to decide a tab's
     * own number. */
    FILE *w = ktb_fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = ktb_fopen(claims_path, "r");
    if (f) {
        char line[KTB_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "KIND=tab", 8) != 0) fputs(line, w); /* keep KIND=row/btn etc. */
        }
        fclose(f);
    }
    for (int i = 0; i < s->n_tabs; i++) {
        s->tabs[i].nav = KTB_STRIP_N_CELLS + i + 1;
        fprintf(w, "KIND=tab|PID=%d|NAV=%d|ENTITY=%s|PATH=%s\n",
                s->tabs[i].pid, s->tabs[i].nav, s->tabs[i].entity, s->tabs[i].path);
    }
    fclose(w);
    remove(claims_path);
    rename(tmp_path, claims_path);
}

/* Real bug fix (2026-08-11, direct live report: "the nav number is taking
 * digits but '>' isn't jumping to them"): tp_taskbar.c's own
 * sync_strip_claims() (line ~663) writes KIND=btn|PID=..|NAV=1..n_cells|
 * PATH=house_root claims into the SAME shared livedesk_nav_claims.txt
 * sync_tab_claims() above already writes tab entries into — fixed
 * nav 1..KTB_STRIP_N_CELLS, one per header cell, in the exact HQ/USER/
 * file/... order this port's own HQ_HEADER_LABELS already uses. Nothing
 * in this port ever wrote these before, so ktb_jump_nav() below could
 * never find a match for a header-cell number — only tab numbers ever
 * worked. Ported directly, same filter-old-then-write-fresh shape as
 * sync_tab_claims's own KIND=tab handling. */
static void sync_strip_claims(KtbState *s) {
    char claims_path[KTB_PATH_BUF], tmp_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt");
    path_join(tmp_path, sizeof(tmp_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    int my_pid = ktb_my_pid();
    FILE *w = ktb_fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = ktb_fopen(claims_path, "r");
    if (f) {
        char line[KTB_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "KIND=btn", 8) == 0) continue; /* rewritten fresh below */
            fputs(line, w); /* keep tab/row entries as-is */
        }
        fclose(f);
    }
    for (int i = 0; i < KTB_STRIP_N_CELLS; i++)
        fprintf(w, "KIND=btn|PID=%d|NAV=%d|PATH=%s\n", my_pid, i + 1, s->house_root);
    fclose(w);
    remove(claims_path);
    rename(tmp_path, claims_path);
}

/* Forward decl - real definition (shared kv/.pdl reader) lives further down
 * this file, already used by livedesk_build_hq_menu() etc. */
void read_key_value(const char *path, const char *key, char *out, size_t out_sz);

/* USER cell command, ported from tp_taskbar.c's load_strip_config() reading
 * of the "strip_user_cmd" key from #.desktop/livedesk_taskbar.pdl (same
 * SECTION-row .pdl the theme/hq-menu readers already use - read_key_value()
 * above already handles that shape). Empty when unset, matching the
 * legacy's own default. */
static void load_strip_user_cmd(KtbState *s) {
    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_taskbar.pdl", s->house_root);
    read_key_value(pdl, "strip_user_cmd", s->strip_user_cmd, sizeof(s->strip_user_cmd));
}

void ktb_reload(KtbState *s) {
    load_tabs(s);
    sync_tab_claims(s);
    sync_strip_claims(s);
    load_shortcuts(s);
    load_theme(s);
    load_strip_user_cmd(s);
    if (s->tab_focus_idx >= s->n_tabs) s->tab_focus_idx = s->n_tabs > 0 ? s->n_tabs - 1 : 0;
    if (s->strip_focus_cell >= KTB_STRIP_N_CELLS) s->strip_focus_cell = KTB_STRIP_N_CELLS - 1;
}

static void write_relay(const char *package_path, const char *cmd) {
    char relay[KTB_PATH_BUF];
    path_join(relay, sizeof(relay), package_path, "interact_relay.txt");
#ifdef _WIN32
    for (char *p = relay; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(relay, "w");
    if (f) { fprintf(f, "%s\n", cmd); fclose(f); }
}

void ktb_activate_tab(KtbState *s, int idx) {
    if (idx < 0 || idx >= s->n_tabs) return;
    /* Real bug fix (2026-08-11, direct live report: "enter on nav in
     * bottom bar isn't opening its context menu like it used to"). This
     * function used to write "ACTIVATE\nOPEN_CONTEXT\n" (two lines) — but
     * tp_desktop_window.c's own relay reader (its interact_relay.txt poll
     * block) does exactly ONE fgets() per tick and immediately truncates
     * the file, so only the FIRST line is ever read. "ACTIVATE" has no
     * handler at all there (grepped: only "OPEN_CONTEXT" is a recognized
     * bare command) — every activation silently consumed and discarded
     * the real command on line 2, which never got read. Legacy's own
     * taskbar_activate_tab() (tp_taskbar.c ~line 1174, "same as
     * right-click on the tile") writes ONLY "OPEN_CONTEXT" — no leading
     * "ACTIVATE" at all. Fixed to match exactly. (Legacy's function ALSO
     * calls taskbar_raise_tab() first, a real Xlib XRaiseWindow — this
     * manager has zero Xlib access by design, so there's no equivalent to
     * port; OPEN_CONTEXT's own handler is what actually shows the popup.) */
    write_relay(s->tabs[idx].path, "OPEN_CONTEXT");
    s->tab_focus_idx = idx;
}

void ktb_jump_nav(KtbState *s, int nav_n) {
    char claims_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(claims_path, "r");
    if (!f) {
        /* fallback: match tab.nav */
        for (int i = 0; i < s->n_tabs; i++)
            if (s->tabs[i].nav == nav_n) { ktb_activate_tab(s, i); return; }
        return;
    }
    char line[KTB_PATH_BUF];
    int found = 0;
    char kind[16] = "", path[KTB_PATH_BUF] = "";
    while (fgets(line, sizeof(line), f)) {
        char *navp = strstr(line, "NAV=");
        if (!navp || atoi(navp + 4) != nav_n) continue;
        /* Real bug fix (2026-08-11): "btn" (header cell, this port's own
         * sync_strip_claims() above) is genuinely different from "row"
         * (an entity's own context-menu row, claimed by tp_desktop_window.c
         * — the ACTIVATE_NAV relay-injection path below is real and
         * intentional for THAT case, confirmed as desired for
         * agents/harnesses driving those remote menus, not something to
         * remove). A header cell needs LOCAL activation instead — it's
         * this taskbar's own cell, not a remote package with its own
         * interact_relay.txt. */
        snprintf(kind, sizeof(kind), "%s",
                 strncmp(line, "KIND=tab", 8) == 0 ? "tab" :
                 strncmp(line, "KIND=btn", 8) == 0 ? "btn" : "row");
        char *pp = strstr(line, "PATH=");
        if (pp) {
            snprintf(path, sizeof(path), "%s", pp + 5);
            path[strcspn(path, "\r\n")] = 0;
        }
        found = 1;
        break;
    }
    fclose(f);
    if (!found) return;
    if (strcmp(kind, "tab") == 0) {
        for (int i = 0; i < s->n_tabs; i++)
            if (s->tabs[i].nav == nav_n) { ktb_activate_tab(s, i); return; }
    } else if (strcmp(kind, "btn") == 0) {
        /* Real bug fix (2026-08-11): header cell — nav_n IS the 1-based
         * header index (sync_strip_claims() writes it that way, matching
         * ktb_hq_open()'s own which=index+1 convention exactly), so this
         * is the SAME dispatch ktb_hq_header click-handling already uses
         * in khtpm_taskbar_manager_main.c's dispatch_code() — activate
         * locally, no relay file involved at all. nav_n==2 (USER) no
         * longer special-cased to ktb_strip_user_activate() as of
         * 2026-08-11 - USER has a real ktb_hq_open() submenu now, see
         * livedesk_build_user_menu(). */
        ktb_hq_open(s, nav_n);
    } else {
        /* menu row (KIND=row, an entity's own context menu, claimed by
         * tp_desktop_window.c): inject ACTIVATE_NAV into that package's
         * own interact_relay.txt — real, intentional, and desired as-is
         * for agent/harness-driven remote interaction, not touched here. */
        char relay[KTB_PATH_BUF];
        path_join(relay, sizeof(relay), path, "interact_relay.txt");
#ifdef _WIN32
        for (char *p = relay; *p; p++) if (*p == '/') *p = '\\';
#endif
        FILE *rf = ktb_fopen(relay, "w");
        if (rf) {
            fprintf(rf, "ACTIVATE_NAV:%d\n", nav_n);
            fclose(rf);
        }
    }
}

void ktb_digit_clear(KtbState *s) {
    s->digit_len = 0;
    s->digit_buf[0] = 0;
    s->nav_armed = 0;
}

/* Real bug fix (2026-08-11, direct live report: "digits still accumulating
 * too high and not jumping. legacy code has the answer, ur not copying it
 * right"): ktb_digit_push() used to be a naive append (any digit, up to 8
 * chars, never validated, never actually moved anything). tp_taskbar.c's
 * real digit-typing handler (main(), ~line 4037-4116, "chtpm_parser_pal
 * digit_accum") is a completely different algorithm: each new digit is
 * validated against the REAL currently-available nav range
 * (max_claimed_nav(), read fresh from the same shared claims file
 * sync_strip_claims()/sync_tab_claims() write into — not a flat 8-char
 * guess), the buffer is capped to exactly the digit-count that range
 * needs (never more), an out-of-range new value restarts from the bare
 * digit if THAT alone is valid, and — critically — every valid keystroke
 * immediately moves the [>] cursor to match (live "do_jump"), without
 * activating anything; only Enter (ktb_digit_enter → ktb_jump_nav, already
 * correct) actually activates. Ported faithfully below. */
static int max_claimed_nav(const KtbState *s) {
    char claims_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(claims_path, "r");
    if (!f) return 0;
    char line[KTB_PATH_BUF];
    int max_n = 0;
    while (fgets(line, sizeof(line), f)) {
        char *navp = strstr(line, "NAV=");
        if (!navp) continue;
        int v = atoi(navp + 4);
        if (v > max_n) max_n = v;
    }
    fclose(f);
    return max_n;
}

/* Live "do_jump": moves [>] to match the current digit_buf WITHOUT
 * activating anything (matches tp_taskbar.c's own explicit "Do NOT open
 * popup here - defer activation to Enter key" comment, which applies
 * equally to tabs in the real code — neither kind activates on a bare
 * digit keystroke, only Enter does). */
static void ktb_nav_digit_peek(KtbState *s) {
    if (!s->digit_buf[0]) return;
    int nav_n = atoi(s->digit_buf);
    char claims_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(claims_path, "r");
    if (!f) return;
    char line[KTB_PATH_BUF];
    char kind[16] = "";
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *navp = strstr(line, "NAV=");
        if (!navp || atoi(navp + 4) != nav_n) continue;
        snprintf(kind, sizeof(kind), "%s",
                 strncmp(line, "KIND=tab", 8) == 0 ? "tab" :
                 strncmp(line, "KIND=btn", 8) == 0 ? "btn" : "row");
        found = 1;
        break;
    }
    fclose(f);
    if (!found) return;
    if (strcmp(kind, "tab") == 0) {
        for (int i = 0; i < s->n_tabs; i++)
            if (s->tabs[i].nav == nav_n) { s->tab_focus_idx = i; s->strip_focus_cell = -1; return; }
    } else if (strcmp(kind, "btn") == 0) {
        if (nav_n >= 1 && nav_n <= KTB_STRIP_N_CELLS) s->strip_focus_cell = nav_n - 1;
    }
    /* "row" kind (an entity's own context-menu row) intentionally left as
     * a no-op here for now — that's a real, separate feature (live focus-
     * peek into a REMOTE package's open menu) this port doesn't render/
     * track locally yet, not something to guess-implement blind. */
}

void ktb_digit_push(KtbState *s, char c) {
    if (!isdigit((unsigned char)c)) return;
    int d = c - '0';
    int total_nav = max_claimed_nav(s);
    if (total_nav < 1) total_nav = s->n_tabs > 0 ? s->n_tabs : 9;
    int accum = atoi(s->digit_buf);
    int new_val = accum * 10 + d;
    if (new_val > 0 && new_val <= total_nav) {
        snprintf(s->digit_buf, sizeof(s->digit_buf), "%d", new_val);
    } else if (d > 0 && d <= total_nav) {
        snprintf(s->digit_buf, sizeof(s->digit_buf), "%d", d);
    } else if (d == 0 && accum == 0) {
        /* leading zeros ignored */
    } else {
        if (d > 0 && d <= total_nav)
            snprintf(s->digit_buf, sizeof(s->digit_buf), "%d", d);
        else
            s->digit_buf[0] = '\0';
    }
    /* hard cap: never more digits than needed for total_nav */
    {
        int max_digits = 1, tn = total_nav;
        while (tn >= 10) { max_digits++; tn /= 10; }
        if ((int)strlen(s->digit_buf) > max_digits)
            s->digit_buf[max_digits] = '\0';
    }
    s->digit_len = (int)strlen(s->digit_buf);
    s->nav_armed = 1;
    ktb_nav_digit_peek(s);
}

void ktb_digit_backspace(KtbState *s) {
    if (s->digit_len > 0) s->digit_buf[--s->digit_len] = 0;
    if (s->digit_len == 0) s->nav_armed = 0;
}

void ktb_digit_enter(KtbState *s) {
    if (s->digit_len <= 0) {
        /* Enter with empty buffer: activate focused tab */
        ktb_activate_tab(s, s->tab_focus_idx);
        return;
    }
    ktb_jump_nav(s, atoi(s->digit_buf));
    ktb_digit_clear(s);
}

void ktb_focus_delta(KtbState *s, int delta) {
    if (s->n_tabs <= 0) return;
    s->tab_focus_idx += delta;
    if (s->tab_focus_idx < 0) s->tab_focus_idx = s->n_tabs - 1;
    if (s->tab_focus_idx >= s->n_tabs) s->tab_focus_idx = 0;
    s->nav_armed = 1;
}

void ktb_action_portable(const char *in, char *out, size_t out_sz) {
    if (!in) { out[0] = 0; return; }
    const char *markers[] = {
        "/$.crypts/", "\\$.crypts\\",
        "/&.widgits/", "\\&.widgits\\",
        "/@.apps/", "\\@.apps\\",
        NULL
    };
    for (int i = 0; markers[i]; i++) {
        const char *m = strstr(in, markers[i]);
        if (m) { snprintf(out, out_sz, "%s", m + 1); return; }
    }
    snprintf(out, out_sz, "%s", in);
}

/* Forward decls - real definitions are further down (registry-close +
 * /proc hard-kill sweep, shared with player>reset's livedesk_reset_
 * entities()); needed here since ktb_quit_and_save() now calls both
 * (2026-08-11 fix, see that function's own header comment). */
static void livedesk_close_all(const char *house_root);
#ifndef _WIN32
static void livedesk_kill_stray_entities(const char *house_root);
#endif

void ktb_quit_and_save(KtbState *s) {
    /* Real bug fix (2026-08-11, found during a full legacy-vs-khtpm gap
     * sweep, direct instruction: "find any more nuanced or advanced
     * functional gaps"). This used to rewrite $.crypts/autostart.pdl's
     * LAUNCH rows on every quit — but that's not what legacy's real
     * quit_and_save_session() does (tp_taskbar.c ~line 1120-1131), and
     * legacy's OWN header comment there is explicit about why: "[X] will
     * quit and save session... The session-close part is real; the
     * destructive rewrite turned out to be wrong for this desk because it
     * erased the curated startup list and collapsed later resets. Keep
     * the close behavior, but do not rewrite $.crypts/autostart.pdl
     * here." Legacy USED TO do exactly what this function still did, hit
     * a real problem from it, and deliberately removed it — khtpm's port
     * had regressed back to the pre-fix behavior. This is also the actual
     * root cause of a symptom patched earlier tonight (the stale tool-bar
     * path kept reappearing in autostart.pdl after every relaunch/kill
     * cycle) — that patch fixed the WRONG path baked into this rewrite;
     * the real fix, per legacy's own history, is to not rewrite at all.
     * Fixed: this function now only closes entities and unlinks the
     * pidfile, matching legacy's real, corrected behavior exactly.
     *
     * Real fix (2026-08-11, direct live report: "there are entities still
     * on desk after killing tb. this isn't supposed to be"): this used to
     * message only s->tabs[] (whatever the taskbar currently shows as
     * tabs), not the full #.desktop/livedesk_open.txt registry — an
     * entity that never became a tab (or fell out of tabs[] some other
     * way) silently survived quit. Switched to the same registry-based
     * livedesk_close_all() + /proc hard-kill sweep already proven by
     * player>reset's livedesk_reset_entities() - see
     * livedesk_kill_stray_entities()'s own header comment for the
     * SIGTERM-then-SIGKILL shape this now guarantees on every quit, not
     * just when a human remembers to run EMERGENCY_CLOSE.sh by hand. */
    livedesk_close_all(s->house_root);
#ifndef _WIN32
    livedesk_kill_stray_entities(s->house_root);
#endif
    ktb_unlink_pidfile(s);
}

int ktb_close_x0(int screen_w) {
    return screen_w - KTB_CLOSE_W;
}

int ktb_shortcuts_x0(int screen_w, int n_shortcuts) {
    return ktb_close_x0(screen_w) - n_shortcuts * KTB_SHORTCUT_W;
}

int ktb_tab_index_at_x(int x, int n_tabs, int tabs_right) {
    /* Bug 2 fix (2026-08-11, live-test report "buttons sometimes don't
     * work"): re-read tp_taskbar.c's own real click handler (main(),
     * ~line 3754) — its bound is `(idx+1)*TAB_W <= tabs_right`, NOT
     * `x < tabs_right`. The two are different: draw_bar()'s own loop
     * (~line 1071) stops drawing a tab once `x0+8 >= tabs_right`, so a tab
     * can still be (partially) DRAWN while legacy's own hit-test already
     * rejects clicking it near the right margin. This port previously used
     * the looser `x < tabs_right` bound, which accepted clicks legacy
     * itself would silently swallow (drift between "what's clickable here"
     * and "what's clickable in the reference") — tightened to match
     * legacy's real bound exactly, cell-for-cell. */
    if (x < 0 || x >= tabs_right) return -1;
    int i = x / KTB_TAB_W;
    if (i < 0 || i >= n_tabs) return -1;
    if ((i + 1) * KTB_TAB_W > tabs_right) return -1;
    return i;
}

int ktb_shortcut_index_at_x(int x, int screen_w, int n_shortcuts) {
    int close_x0 = ktb_close_x0(screen_w);
    for (int i = 0; i < n_shortcuts; i++) {
        int sx0 = close_x0 - (i + 1) * KTB_SHORTCUT_W;
        if (x >= sx0 && x < sx0 + KTB_SHORTCUT_W) return i;
    }
    return -1;
}

/* ---------------------------------------------------------------------
 * livedesk_* registry/session/desk/pals business logic, ported from
 * tp_taskbar.c's pure-logic functions (see khtpm-refactor-plan.md §10,
 * task 3). Function names left as-is (livedesk_* not renamed to ktb_*,
 * matching how ktb_* names were also left alone during the earlier
 * file-level rename). PATH_BUF -> KTB_PATH_BUF and the LIVEDESK_* size
 * constants -> KTB_LIVEDESK_* throughout; pid_is_alive() calls into the
 * already-ported ktb_pid_alive() instead of duplicating it.
 * ------------------------------------------------------------------- */

/* Read one value for `key` from a kv or pdl file: "key=value" lines or
 * "SECTION | key | value" rows both work (the last | segment is the
 * value). Ported verbatim from tp_taskbar.c's read_key_value(). */
void read_key_value(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = ktb_fopen(path, "r");
    if (!f) return;
    char line[KTB_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        /* REAL BUG found and fixed 2026-08-13 (direct report: "wait.
         * i see neither settings nor statistics added to tb. where is
         * it?!"): this used to skip EVERY line starting with the
         * literal word "SECTION" - but in this house's real .pdl
         * convention (e.g. livedesk_taskbar.pdl), "SECTION" is the
         * ROW TAG every single real data row uses
         * ("SECTION | hq_menu_5_label | settings"), not a special
         * marker - only the true header/divider row ("SECTION | KEY |
         * VALUE", where the field right after SECTION is literally
         * the word "KEY") was ever meant to be skipped. The old blanket
         * check meant read_key_value() could NEVER successfully read
         * ANY hq_menu_N_label/cmd key from this file, ever - the HQ
         * menu was silently falling back to its 3-item hardcoded
         * default ($.restart/X.quit/cancel) this WHOLE TIME, not just
         * for newly-added rows. Confirmed live via strip_var_hqitems.txt
         * showing exactly that 3-item fallback content, even after a
         * fresh restart with a real, correct 7-row .pdl on disk. Fixed
         * to only skip the literal "SECTION | KEY | ..." header line,
         * not real "SECTION | <real_key> | <value>" data rows. */
        if (strncmp(line, "SECTION", 7) == 0) {
            char *after = line + 7;
            while (*after == ' ' || *after == '\t') after++;
            if (*after == '|') {
                after++;
                while (*after == ' ' || *after == '\t') after++;
                if (strncmp(after, "KEY", 3) == 0) continue; /* the real header row only */
            } else {
                continue; /* "SECTION" not followed by a real "| key | value" row at all - a true divider/junk line */
            }
        }
        if (strncmp(line, "META", 4) == 0) continue;
        char *p = strstr(line, key);
        if (!p) continue;
        char *eq = strchr(p, '=');
        char *bar = strrchr(p, '|');
        char *v = NULL;
        if (eq && (!bar || eq < bar)) v = eq + 1;
        else if (bar) v = bar + 1;
        if (!v) continue;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        size_t n = strlen(v);
        while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = '\0';
        if (v[0]) { snprintf(out, out_sz, "%s", v); break; }
    }
    fclose(f);
}

/* Resolve a launcher script path for a named window app (settings,
 * stats, ai, db, ...) from #.desktop/livedesk_launchers.pdl (house
 * standard - see that file's header comment; paths there are stored
 * RELATIVE to house_root, never absolute/hardcoded). Copies the fully
 * resolved path into `out` and returns 1 on success, 0 if the pdl is
 * missing or has no row for `app`. Built 2026-08-13 after the HQ menu's
 * "stats" row kept failing: its old pdl row shelled out to a relative
 * "&.hq-apps/..." path that resolved against THIS process's cwd (the
 * house's parent dir when the taskbar is launched by run_khtpm_strip.sh),
 * not house_root - so the shell could never find it. All HQ window rows
 * now go through here so they resolve against house_root regardless of
 * cwd, exactly like settings/open-hai's own dedicated branches. */
static int ktb_hq_launcher_path(const char *house_root, const char *app,
                                char *out, size_t out_sz) {
    out[0] = '\0';
    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_launchers.pdl", house_root);
    char key[64], rel[KTB_PATH_BUF] = "";
    snprintf(key, sizeof(key), "launcher_%s", app);
    read_key_value(pdl, key, rel, sizeof(rel));
    if (!rel[0]) return 0;
    if (rel[0] == '/') {
        snprintf(out, out_sz, "%s", rel);
    } else {
        snprintf(out, out_sz, "%s/%s", house_root, rel);
    }
    return 1;
}

/* Cross-process lock around the livedesk_open.txt registry read-prune-
 * write-rename cycle. Off by default (KTB_LIVEDESK_USE_REGISTRY_LOCK==0),
 * same as tp_taskbar.c's own LIVEDESK_USE_REGISTRY_LOCK default. Not
 * compiled on Windows (no flock()); the lock is a best-effort guard, not
 * required for correctness on a single-writer desktop). */
#ifndef _WIN32
static int g_ktb_registry_lock_fd = -1;
static void registry_lock_acquire(const char *house_root) {
    if (!KTB_LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_ktb_registry_lock_fd < 0) {
        char lock_path[KTB_PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_registry.lock", house_root);
        g_ktb_registry_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_ktb_registry_lock_fd >= 0) flock(g_ktb_registry_lock_fd, LOCK_EX);
}
static void registry_lock_release(void) {
    if (!KTB_LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_ktb_registry_lock_fd >= 0) flock(g_ktb_registry_lock_fd, LOCK_UN);
}
#else
static void registry_lock_acquire(const char *house_root) { (void)house_root; }
static void registry_lock_release(void) {}
#endif

/* input_mode for session/desk rename fields: [A-Za-z0-9_-] only, applied
 * BEFORE the buffer so a rejected char never reaches the editable text.
 * Ported verbatim from tp_taskbar.c's cliio_key_allowed(). */
static int cliio_key_allowed(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-') return 1;
    return 0;
}

static void livedesk_mkdir_p(const char *path) {
    char tmp[KTB_PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void livedesk_rel_path(const char *house_root, const char *abs, char *out, size_t sz) {
    size_t hl = strlen(house_root);
    if (strncmp(abs, house_root, hl) == 0 && abs[hl] == '/')
        snprintf(out, sz, "%s", abs + hl + 1);
    else
        snprintf(out, sz, "%s", abs);
}

static void livedesk_join_path(const char *house_root, const char *rel, char *out, size_t sz) {
#ifdef _WIN32
    const char *xyz = strstr(rel, "xyzfs/");
    if (!xyz) xyz = strstr(rel, "xyzfs\\");
    if (xyz)
        snprintf(out, sz, "%s/%s", house_root, xyz);
    else if (rel[0] == '/' || (rel[0] && rel[1] == ':'))
        snprintf(out, sz, "%s", rel);
    else
        snprintf(out, sz, "%s/%s", house_root, rel);
#else
    if (rel[0] == '/')
        snprintf(out, sz, "%s", rel);
    else
        snprintf(out, sz, "%s/%s", house_root, rel);
#endif
}

static int livedesk_login_root(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
#ifdef _WIN32
    {
        wchar_t pat[KTB_PATH_BUF];
        wchar_t wh[KTB_PATH_BUF];
        const char *hr = (house_root && house_root[0]) ? house_root : ".";
        if (!MultiByteToWideChar(CP_UTF8, 0, hr, -1, wh, KTB_PATH_BUF))
            MultiByteToWideChar(CP_ACP, 0, hr, -1, wh, KTB_PATH_BUF);
        if (wcscmp(wh, L".") == 0)
            _snwprintf(pat, KTB_PATH_BUF, L"0.user-pal*");
        else
            _snwprintf(pat, KTB_PATH_BUF, L"%s\\0.user-pal*", wh);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            char name_utf8[512];
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name_utf8, (int)sizeof(name_utf8), NULL, NULL);
            FindClose(h);
            if (!strcmp(hr, "."))
                snprintf(out, sz, "%s/00.login-signup", name_utf8);
            else
                snprintf(out, sz, "%s/%s/00.login-signup", hr, name_utf8);
            return out[0] != '\0';
        }
    }
#endif
    DIR *d = opendir(house_root && house_root[0] ? house_root : ".");
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "0.user-pal", 10) == 0) {
            snprintf(out, sz, "%s/%s/00.login-signup",
                     (house_root && house_root[0]) ? house_root : ".", e->d_name);
            break;
        }
    }
    closedir(d);
    return out[0] != '\0';
}

static void livedesk_user_uuid(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char login_root[KTB_PATH_BUF];
    if (!livedesk_login_root(house_root, login_root, sizeof(login_root))) return;
    char p[KTB_PATH_BUF];
    snprintf(p, sizeof(p), "%s/current_login.txt", login_root);
    read_key_value(p, "current_user_uuid", out, sz);
#ifdef _WIN32
    if (!out[0]) {
        char users[KTB_PATH_BUF];
        snprintf(users, sizeof(users), "%s/xyzfs/users", house_root);
        DIR *d = opendir(users);
        int best_n = -1;
        char best[128] = "";
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                char pals[KTB_PATH_BUF];
                snprintf(pals, sizeof(pals), "%s/%s/home/livedesk/pals", users, e->d_name);
                DIR *pd = opendir(pals);
                if (!pd) continue;
                int n = 0, has_self = 0;
                struct dirent *pe;
                while ((pe = readdir(pd))) {
                    if (pe->d_name[0] == '.') continue;
                    n++;
                    if (strcmp(pe->d_name, "self") == 0) has_self = 1;
                }
                closedir(pd);
                int score = n + (has_self ? 1000 : 0);
                if (score > best_n) {
                    best_n = score;
                    snprintf(best, sizeof(best), "%s", e->d_name);
                }
            }
            closedir(d);
        }
        if (best[0]) snprintf(out, sz, "%s", best);
    }
#endif
}

static int livedesk_sessions_root(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char uuid[128] = "";
    livedesk_user_uuid(house_root, uuid, sizeof(uuid));
    if (!uuid[0]) return 0;
    snprintf(out, sz, "%s/xyzfs/users/%s/home/livedesk/sessions", house_root, uuid);
    return 1;
}

static void livedesk_root_read(const char *sroot, char *active, size_t asz, char *last, size_t lsz) {
    char p[KTB_PATH_BUF];
    snprintf(p, sizeof(p), "%s/session.pdl", sroot);
    if (active) read_key_value(p, "active_session", active, asz);
    if (last) read_key_value(p, "last_session", last, lsz);
}

static int livedesk_current_session_name(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char active[64] = "";
    livedesk_root_read(sroot, active, sizeof(active), NULL, 0);
    if (!active[0]) return 0;
    char sp[KTB_PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sroot, active);
    char name[256] = "";
    read_key_value(sp, "name", name, sizeof(name));
    if (name[0]) snprintf(out, sz, "%s", name);
    else snprintf(out, sz, "%s", active);
    return 1;
}

static int livedesk_current_desk_name(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char active[64] = "";
    livedesk_root_read(sroot, active, sizeof(active), NULL, 0);
    if (!active[0]) return 0;
    char sp[KTB_PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sroot, active);
    char desk[64] = "";
    read_key_value(sp, "active_desk", desk, sizeof(desk));
    if (!desk[0]) return 0;
    snprintf(out, sz, "%s", desk);
    return 1;
}

/* Real gap fix (2026-08-11, direct request: "the button vars for
 * user/file/desk"). Ported verbatim from tp_taskbar.c's real username
 * lookup (~line 3480-3506, direct historical correction: "it shows user
 * of linux not user of livedesk" — getenv("USER") is the WRONG, OS-level
 * identity; the real one lives in this house's own login system,
 * 0.user-pal👤️/00.login-signup's userpal_whoami.+x). "none" (nobody
 * logged in) falls back to "guest", never to an OS username. Non-static
 * (declared in khtpm_taskbar_manager.h) so khtpm_taskbar_manager_main.c
 * can call it to publish a strip_var_username.txt fragment, same pattern
 * as publish_var_fragments()'s existing strip_tabs/strip_shortcuts/
 * strip_hq_items vars. */
void ktb_get_username(const KtbState *s, char *out, size_t sz) {
    /* Real perf fix: this is a real shell-out (popen), and
     * publish_var_fragments() (khtpm_taskbar_manager_main.c) calls this on
     * EVERY dispatch_code() — every keystroke/click reaching the manager,
     * not just once a second like tp_taskbar.c's own poll loop. A login
     * essentially never changes mid-session, so cache the result and only
     * re-check every 5s (time(), not a redraw-tick counter — this file
     * has no concept of "ticks", only of individual dispatched codes). */
    static char cached[128] = "";
    static time_t cached_at = 0;
    time_t now = time(NULL);
    if (cached[0] && now - cached_at < 5) { snprintf(out, sz, "%s", cached); return; }
    cached_at = now;
    snprintf(cached, sizeof(cached), "guest");
    snprintf(out, sz, "guest");
    /* Win: do not popen userpal_whoami.+x (bash). Read current_login.txt. */
    {
        char login_root[KTB_PATH_BUF], login_file[KTB_PATH_BUF], uid[128] = "";
        if (livedesk_login_root(s->house_root, login_root, sizeof(login_root))) {
            snprintf(login_file, sizeof(login_file), "%s/current_login.txt", login_root);
            read_key_value(login_file, "current_user_id", uid, sizeof(uid));
        }
#ifdef _WIN32
        if (!uid[0]) {
            wchar_t pat[KTB_PATH_BUF];
            WIN32_FIND_DATAW fd;
            _snwprintf(pat, KTB_PATH_BUF, L"0.user-pal*");
            HANDLE h = FindFirstFileW(pat, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                wchar_t wlogin[KTB_PATH_BUF];
                _snwprintf(wlogin, KTB_PATH_BUF, L"%s\\00.login-signup\\current_login.txt", fd.cFileName);
                FindClose(h);
                FILE *lf = _wfopen(wlogin, L"r");
                if (lf) {
                    char line[256];
                    while (fgets(line, sizeof(line), lf)) {
                        if (strncmp(line, "current_user_id=", 16) == 0) {
                            snprintf(uid, sizeof(uid), "%s", line + 16);
                            uid[strcspn(uid, "\r\n")] = '\0';
                            break;
                        }
                    }
                    fclose(lf);
                }
            }
        }
#endif
        if (uid[0] && strcmp(uid, "none") != 0) {
            snprintf(out, sz, "%s", uid);
            snprintf(cached, sizeof(cached), "%s", uid);
            return;
        }
    }
#ifndef _WIN32
    char userpal_root[KTB_PATH_BUF];
    snprintf(userpal_root, sizeof(userpal_root), "%s/0.user-pal👤️/00.login-signup", s->house_root);
    char whoami_cmd[KTB_PATH_BUF * 2];
    snprintf(whoami_cmd, sizeof(whoami_cmd),
             "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/userpal_whoami.+x' 2>/dev/null",
             userpal_root, userpal_root);
    FILE *wp = popen(whoami_cmd, "r");
    if (wp) {
        char line[128];
        if (fgets(line, sizeof(line), wp)) {
            char *sp = strchr(line, ' ');
            if (sp) *sp = '\0';
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] && strcmp(line, "none") != 0) {
                snprintf(out, sz, "%s", line);
                snprintf(cached, sizeof(cached), "%s", line);
            }
        }
        pclose(wp);
    }
#endif
}

/* Real gap fix, same request: live "file:<session>"/"desks:<desk>" labels
 * (tp_taskbar.c's own g_file_label/g_desks_label convention, ~line
 * 1367-1368), replacing the layout's static "file"/"desks" placeholder
 * text. Thin wrappers over the already-ported livedesk_current_session_
 * name()/livedesk_current_desk_name() (both static, task-3 ported earlier
 * this session, never wired to anything until now) — non-static so
 * khtpm_taskbar_manager_main.c can call them. */
void ktb_get_file_label(const KtbState *s, char *out, size_t sz) {
    char name[256];
    if (livedesk_current_session_name(s->house_root, name, sizeof(name)) && name[0])
        snprintf(out, sz, "file:%s", name);
    else
        snprintf(out, sz, "file");
}

/* Real gap fix (2026-08-11, direct request: "user should have a sprite at
 * least"). Ported verbatim from tp_taskbar.c's active_avatar_dir() (~line
 * 2019-2031) — the SAME real login-system lookup ktb_get_username() above
 * uses, extended to also resolve the active avatar directory: reads
 * current_user_uuid from 0.user-pal👤️/00.login-signup/current_login.txt,
 * active_avatar_uuid from that same dir's xyzfs/session.pdl, then builds
 * <house_root>/xyzfs/users/<uuid>/home/avatars/<avatar_uuid> — a directory
 * that (like any tab's entity dir) may contain its own sprite.csv, read by
 * the SAME tab_sprite()/blit_tab_sprite() mechanism tabs already use (confirmed
 * directly: tp_taskbar.c's own USER cell reuses tab_sprite(), it does not
 * have a second sprite-loading function). Empty output (no avatar) is a
 * real, valid legacy outcome too — not an error to guard against here. */
void ktb_get_avatar_dir(const KtbState *s, char *out, size_t sz) {
    out[0] = '\0';
    char login_root[KTB_PATH_BUF];
    snprintf(login_root, sizeof(login_root), "%s/0.user-pal👤️/00.login-signup", s->house_root);
    char login_path[KTB_PATH_BUF], sess_path[KTB_PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
    snprintf(sess_path, sizeof(sess_path), "%s/xyzfs/session.pdl", login_root);
    char user_uuid[128] = "", avatar_uuid[128] = "";
    read_key_value(login_path, "current_user_uuid", user_uuid, sizeof(user_uuid));
    read_key_value(sess_path, "active_avatar_uuid", avatar_uuid, sizeof(avatar_uuid));
    if (!user_uuid[0] || !avatar_uuid[0]) return;
    snprintf(out, sz, "%s/xyzfs/users/%s/home/avatars/%s", s->house_root, user_uuid, avatar_uuid);
}

void ktb_get_desks_label(const KtbState *s, char *out, size_t sz) {
    char name[64];
    if (livedesk_current_desk_name(s->house_root, name, sizeof(name)) && name[0])
        snprintf(out, sz, "desks:%s", name);
    else
        snprintf(out, sz, "desks");
}

static void livedesk_root_write(const char *sroot, const char *active, const char *last) {
    char p[KTB_PATH_BUF];
    snprintf(p, sizeof(p), "%s/session.pdl", sroot);
    livedesk_mkdir_p(sroot);
    char keep[8][KTB_PATH_BUF];
    int n_keep = 0;
    FILE *f = fopen(p, "r");
    if (f) {
        char line[KTB_PATH_BUF];
        while (n_keep < 8 && fgets(line, sizeof(line), f)) {
            if (strstr(line, "active_session") || strstr(line, "last_session")) continue;
            snprintf(keep[n_keep], sizeof(keep[n_keep]), "%s", line);
            n_keep++;
        }
        fclose(f);
    }
    FILE *w = fopen(p, "w");
    if (!w) return;
    for (int i = 0; i < n_keep; i++) fputs(keep[i], w);
    if (active && active[0]) fprintf(w, "STATE | active_session | %s\n", active);
    if (last && last[0]) fprintf(w, "STATE | last_session | %s\n", last);
    fclose(w);
}

static void livedesk_session_dir(const char *sroot, const char *id, char *out, size_t sz) {
    snprintf(out, sz, "%s/%s", sroot, id);
}

static int livedesk_next_id(const char *sroot, char *out, size_t sz) {
    int best = 0;
    DIR *d = opendir(sroot);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == 's' && isdigit((unsigned char)e->d_name[1])) {
                int n = atoi(e->d_name + 1);
                if (n > best) best = n;
            }
        }
        closedir(d);
    }
    snprintf(out, sz, "s%d", best + 1);
    return best + 1;
}

static void livedesk_ensure_session(const char *sroot, const char *id, const char *name) {
    char sdir[KTB_PATH_BUF], desks[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    livedesk_mkdir_p(sdir);
    livedesk_mkdir_p(desks);
    char sp[KTB_PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    char existing[256] = "";
    read_key_value(sp, "name", existing, sizeof(existing));
    if (!existing[0]) {
        FILE *f = fopen(sp, "w");
        if (f) { fprintf(f, "STATE | name | %s\n", name); fclose(f); }
    }
    char d1[KTB_PATH_BUF];
    snprintf(d1, sizeof(d1), "%s/desk_01.pdl", desks);
    if (access(d1, F_OK) != 0) {
        FILE *f = fopen(d1, "w");
        if (f) fclose(f);
    }
}

static void livedesk_session_name(const char *sroot, const char *id, char *out, size_t sz) {
    out[0] = '\0';
    char sdir[KTB_PATH_BUF], sp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    read_key_value(sp, "name", out, sz);
    if (!out[0]) snprintf(out, sz, "%s", id);
}

static void livedesk_active_desk(const char *sroot, const char *id, char *out, size_t sz) {
    out[0] = '\0';
    char sdir[KTB_PATH_BUF], sp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    read_key_value(sp, "active_desk", out, sz);
    if (!out[0]) snprintf(out, sz, "desk_01");
}

static void livedesk_write_active_desk(const char *sroot, const char *id, const char *desk) {
    char sdir[KTB_PATH_BUF], sp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    char name[256] = "";
    read_key_value(sp, "name", name, sizeof(name));
    FILE *f = fopen(sp, "w");
    if (!f) return;
    if (name[0]) fprintf(f, "STATE | name | %s\n", name);
    fprintf(f, "STATE | active_desk | %s\n", desk);
    fclose(f);
}

static int livedesk_desk_list(const char *sroot, const char *id, char out[][64], int max) {
    char sdir[KTB_PATH_BUF], desks[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    int n = 0;
#ifdef _WIN32
    {
        wchar_t wdir[KTB_PATH_BUF], wpat[KTB_PATH_BUF];
        if (!MultiByteToWideChar(CP_UTF8, 0, desks, -1, wdir, KTB_PATH_BUF))
            MultiByteToWideChar(CP_ACP, 0, desks, -1, wdir, KTB_PATH_BUF);
        _snwprintf(wpat, KTB_PATH_BUF, L"%s\\*.pdl", wdir);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(wpat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (n >= max) break;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                char name[64];
                WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, 64, NULL, NULL);
                char *dot = strrchr(name, '.');
                if (dot && strcmp(dot, ".pdl") == 0) {
                    *dot = '\0';
                    snprintf(out[n], 64, "%s", name);
                    n++;
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    if (n > 0) {
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(out[j], out[i]) < 0) {
                    char t[64];
                    snprintf(t, sizeof(t), "%s", out[i]);
                    snprintf(out[i], sizeof(out[i]), "%s", out[j]);
                    snprintf(out[j], sizeof(out[j]), "%s", t);
                }
        return n;
    }
#endif
    DIR *d = opendir(desks);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (n >= max) break;
            const char *dot = strrchr(e->d_name, '.');
            if (dot && strcmp(dot, ".pdl") == 0 && dot != e->d_name) {
                size_t len = (size_t)(dot - e->d_name);
                if (len >= 64) len = 63;
                memcpy(out[n], e->d_name, len);
                out[n][len] = '\0';
                n++;
            }
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(out[j], out[i]) < 0) {
                    char t[64];
                    snprintf(t, sizeof(t), "%s", out[i]);
                    snprintf(out[i], sizeof(out[i]), "%s", out[j]);
                    snprintf(out[j], sizeof(out[j]), "%s", t);
                }
    }
    return n;
}

static int livedesk_next_desk(const char *sroot, const char *id, char *out, size_t sz) {
    char sdir[KTB_PATH_BUF], desks[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    int best = 0;
    DIR *d = opendir(desks);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "desk_", 5) == 0 && isdigit((unsigned char)e->d_name[5])) {
                int n = atoi(e->d_name + 5);
                if (n > best) best = n;
            }
        }
        closedir(d);
    }
    snprintf(out, sz, "desk_%02d", best + 1);
    return best + 1;
}

static int livedesk_read_open(const char *house_root, int *pids, char ents[][128],
                              char paths[][KTB_PATH_BUF], int *indexes, int max) {
    char reg[KTB_PATH_BUF];
    snprintf(reg, sizeof(reg), "%s/#.desktop/livedesk_open.txt", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(reg, "r");
    if (!f) { registry_lock_release(); return 0; }
    int n = 0;
    char line[KTB_PATH_BUF];
    while (n < max && fgets(line, sizeof(line), f)) {
        int pid = 0, idx = 0;
        char ent[128] = "", path[KTB_PATH_BUF] = "";
        char *p;
        if ((p = strstr(line, "PID="))) pid = atoi(p + 4);
        if ((p = strstr(line, "INDEX="))) idx = atoi(p + 6);
        if ((p = strstr(line, "ENTITY="))) {
            char *e = p + 7, *end = strchr(e, '|');
            size_t len = end ? (size_t)(end - e) : strcspn(e, "\r\n");
            if (len >= sizeof(ent)) len = sizeof(ent) - 1;
            memcpy(ent, e, len);
            ent[len] = '\0';
        }
        if ((p = strstr(line, "PATH="))) {
            snprintf(path, sizeof(path), "%s", p + 5);
            path[strcspn(path, "\r\n")] = '\0';
        }
        if (!ent[0] || !ktb_pid_alive(pid)) continue;
        if (pids) pids[n] = pid;
        if (indexes) indexes[n] = idx;
        snprintf(ents[n], 128, "%s", ent);
        snprintf(paths[n], KTB_PATH_BUF, "%s", path);
        n++;
    }
    fclose(f);
    registry_lock_release();
    return n;
}

static void livedesk_glyph(const char *package_dir, char *out, size_t sz) {
    out[0] = '\0';
    char p[KTB_PATH_BUF];
    snprintf(p, sizeof(p), "%s/glyph.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    if (fgets(out, (int)sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int livedesk_read_pos(const char *package_dir, int *x, int *y) {
    *x = -1;
    *y = -1;
    char p[KTB_PATH_BUF];
    snprintf(p, sizeof(p), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) *x = atoi(line + 2);
        else if (strncmp(line, "y=", 2) == 0) *y = atoi(line + 2);
    }
    fclose(f);
    return (*x >= 0 && *y >= 0);
}

static void livedesk_base_name(const char *p, char *out, size_t sz) {
    const char *s = strrchr(p, '/');
    snprintf(out, sz, "%s", s ? s + 1 : p);
}

static void livedesk_copy_full(const char *src, const char *dst) {
    char parent[KTB_PATH_BUF], cmd[KTB_PATH_BUF * 2];
    snprintf(parent, sizeof(parent), "%s", dst);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    snprintf(cmd, sizeof(cmd),
             "mkdir -p '%s' && rm -rf '%s' && cp -r '%s' '%s' 2>/dev/null",
             parent, dst, src, dst);
    int rc = system(cmd);
    (void)rc;
}

/* REAL FIX 2026-08-30, direct live report ("tb shows emojis and pngs
 * already for user, and pid (clock) see? why cant they use same?"):
 * non-static (declared in khtpm_taskbar_manager.h) so khtpm_taskbar_
 * manager_main.c's publish_var_fragments() can resolve each pals-dropdown
 * row's real sprite.csv path - same real pattern ktb_get_username()
 * already uses to cross this same file boundary. See that function's own
 * header comment for the precedent. */
int livedesk_pals_root(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char uuid[128] = "";
    livedesk_user_uuid(house_root, uuid, sizeof(uuid));
    if (!uuid[0]) return 0;
    snprintf(out, sz, "%s/xyzfs/users/%s/home/livedesk/pals", house_root, uuid);
    return 1;
}

static void livedesk_pals_rel(const char *house_root, const char *name, char *out, size_t sz) {
    char pr[KTB_PATH_BUF];
    if (livedesk_pals_root(house_root, pr, sizeof(pr))) {
        size_t hl = strlen(house_root);
        snprintf(out, sz, "%s/%s", pr + hl + 1, name);
    } else {
        snprintf(out, sz, "%s", name);
    }
}

static void livedesk_hash_dir(const char *dir, char *out, size_t sz) {
    out[0] = '\0';
    char cmd[KTB_PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "(cd '%s' && find . -type f -print0 2>/dev/null | sort -z | "
             "xargs -0 sha256sum 2>/dev/null) 2>/dev/null | sha256sum", dir);
    FILE *p = popen(cmd, "r");
    if (!p) return;
    if (fgets(out, (int)sz, p)) out[strcspn(out, "\r\n")] = '\0';
    pclose(p);
    size_t n = strlen(out);
    if (n > 0 && out[n - 1] == '-') out[--n] = '\0';  /* "hash  -" -> "hash " */
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) out[--n] = '\0';
}

static void livedesk_ensure_pal(const char *pals_root, const char *name, const char *live_path) {
    if (!name[0] || !pals_root[0]) return;
    char pal[KTB_PATH_BUF], mp[KTB_PATH_BUF];
    snprintf(pal, sizeof(pal), "%s/%s", pals_root, name);
    snprintf(mp, sizeof(mp), "%s/pal.pdl", pal);
    if (access(pal, F_OK) != 0 && live_path[0] && strcmp(live_path, pal) != 0 &&
        access(live_path, F_OK) == 0)
        livedesk_copy_full(live_path, pal);
    if (access(pal, F_OK) != 0) return;
    char hash[128] = "", glyph[64] = "";
    livedesk_hash_dir(pal, hash, sizeof(hash));
    if (live_path[0] && access(live_path, F_OK) == 0)
        livedesk_glyph(live_path, glyph, sizeof(glyph));
    else
        livedesk_glyph(pal, glyph, sizeof(glyph));
    FILE *f = fopen(mp, "w");
    if (!f) return;
    fprintf(f, "PAL | name | %s\n", name);
    fprintf(f, "PAL | hash | %s\n", hash);
    fprintf(f, "PAL | glyph | %s\n", glyph);
    fclose(f);
}

static void livedesk_snapshot_desk(const char *house_root, const char *sroot, const char *id) {
    char active[64] = "";
    livedesk_active_desk(sroot, id, active, sizeof(active));
    char sdir[KTB_PATH_BUF], desks[KTB_PATH_BUF], sp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    livedesk_mkdir_p(sdir);
    livedesk_mkdir_p(desks);
    snprintf(sp, sizeof(sp), "%s/%s.pdl", desks, active);
    int pids[KTB_LIVEDESK_MAX_OPEN], indexes[KTB_LIVEDESK_MAX_OPEN];
    char ents[KTB_LIVEDESK_MAX_OPEN][128], paths[KTB_LIVEDESK_MAX_OPEN][KTB_PATH_BUF];
    int n = livedesk_read_open(house_root, pids, ents, paths, indexes, KTB_LIVEDESK_MAX_OPEN);
    /* REAL FIX 2026-08-30, direct live incident: switching desks while
     * the live entity registry was (transiently) empty - e.g. right
     * after a taskbar restart, before entities re-register - silently
     * overwrote a real, populated desk .pdl with zero rows, wiping
     * real user data (office.pdl's 7 real entities were lost this
     * exact way during live testing). A snapshot with n==0 is
     * indistinguishable from "the desk was already empty" vs "the
     * registry just hasn't caught up yet" - the safe assumption is the
     * latter. If the live registry is empty AND the existing file on
     * disk already has real DESK rows, skip the write entirely rather
     * than clobber it - an outgoing desk with genuinely zero real
     * entities was already an empty file before this call, so this
     * only changes behavior for the dangerous case, never the normal
     * one. */
    if (n == 0) {
        FILE *check = fopen(sp, "r");
        if (check) {
            char cline[256];
            int has_real_rows = 0;
            while (fgets(cline, sizeof(cline), check)) {
                if (strncmp(cline, "DESK", 4) == 0) { has_real_rows = 1; break; }
            }
            fclose(check);
            if (has_real_rows) return;
        }
    }
    FILE *w = fopen(sp, "w");
    if (!w) return;
    for (int i = 0; i < n; i++) {
        int x = -1, y = -1;
        if (!livedesk_read_pos(paths[i], &x, &y)) continue;
        char glyph[64] = "";
        livedesk_glyph(paths[i], glyph, sizeof(glyph));
        char rel[KTB_PATH_BUF];
        livedesk_rel_path(house_root, paths[i], rel, sizeof(rel));
        fprintf(w, "DESK | %s | %s | %d | %d | %d | %d | %s | %d\n",
                ents[i], rel, x, y, x / KTB_LIVEDESK_GRID_PX, y / KTB_LIVEDESK_GRID_PX,
                glyph, indexes[i]);
    }
    fclose(w);
    /* §4.8/§4.9: register every live entity into the user's pals registry.
     * New-model live entities already RUN from the pal copy, so this is a
     * no-op (self-copy guard); dev-authored/legacy sources get copied into
     * pals here (migration). Sessions no longer own per-session copies -
     * placements reference the single pal. */
    char pr[KTB_PATH_BUF];
    if (livedesk_pals_root(house_root, pr, sizeof(pr))) {
        for (int i = 0; i < n; i++) {
            char base[64];
            livedesk_base_name(paths[i], base, sizeof(base));
            if (base[0]) livedesk_ensure_pal(pr, base, paths[i]);
        }
    }
}

static void livedesk_close_all(const char *house_root) {
    int pids[KTB_LIVEDESK_MAX_OPEN], idx[KTB_LIVEDESK_MAX_OPEN];
    char ents[KTB_LIVEDESK_MAX_OPEN][128], paths[KTB_LIVEDESK_MAX_OPEN][KTB_PATH_BUF];
    int n = livedesk_read_open(house_root, pids, ents, paths, idx, KTB_LIVEDESK_MAX_OPEN);
    for (int i = 0; i < n; i++) {
        /* REAL, NEW 2026-08-30, direct instruction ("cursword is an
         * entity that should always be open... its the users
         * assistant. 1rst entity"): cursword is exempt from every
         * "close all" sweep - a desk switch, session load, or reset
         * must never close it, only the real entities the DESK file
         * itself owns. See livedesk_ensure_cursword() (which re-spawns
         * it if it's ever found missing) for the other half of this. */
        char base[64];
        livedesk_base_name(paths[i], base, sizeof(base));
        if (strcmp(base, "cursword") == 0) continue;
        char relay[KTB_PATH_BUF];
        snprintf(relay, sizeof(relay), "%s/interact_relay.txt", paths[i]);
        FILE *cf = fopen(relay, "w");
        if (cf) { fprintf(cf, "CLOSE\n"); fclose(cf); }
    }
    if (n > 0) {
        struct timespec ts = {0, 450 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
}

/* REAL, NEW 2026-08-30, direct instruction ("cursword is an entity that
 * should always be open. even if others aren't. its the users assistant.
 * 1rst entity. how do we make that so?"): cursword is the one entity that
 * lives OUTSIDE the desk-file/session model entirely - it's not "on" any
 * particular desk, it's always there regardless of which desk/session is
 * active. This checks the real live registry (same source of truth
 * livedesk_spawn_desk's own dedup check already uses) for a real, alive
 * process whose pal dir basename is "cursword"; if none is found - first
 * boot, a crash, or the entity having been closed by hand - it spawns one
 * fresh from the user's own pals/cursword copy (same exe + nohup pattern
 * livedesk_spawn_desk uses). Callers: main() at startup, and right after
 * every livedesk_spawn_desk() call (desk switch, session load, new
 * session/desk, reset) - anywhere entities get (re)launched or torn down
 * is a place cursword's own presence could have lapsed.
 *
 * REAL, NEW 2026-08-30, direct instruction ("it should always start in
 * the upper top left, where it used to auto start. see where it is right
 * now?"): unlike every other entity, cursword's spawn position is
 * pinned, not remembered - every fresh (re)spawn here forces
 * desktop_pos.txt back to CURSWORD_HOME_X/Y regardless of wherever it
 * was last dragged/nudged/placed to, so "always open" also means
 * "always findable in the same spot," not wherever testing or a prior
 * session happened to leave it. */
#define CURSWORD_HOME_X 0
#define CURSWORD_HOME_Y 0
static void livedesk_ensure_cursword(const char *house_root) {
    char pr[KTB_PATH_BUF];
    if (!livedesk_pals_root(house_root, pr, sizeof(pr))) return;
    char pal[KTB_PATH_BUF];
    snprintf(pal, sizeof(pal), "%s/cursword", pr);
    if (access(pal, F_OK) != 0) return; /* no cursword pal provisioned for this user - nothing to ensure */

    int pids[KTB_LIVEDESK_MAX_OPEN], idx[KTB_LIVEDESK_MAX_OPEN];
    char ents[KTB_LIVEDESK_MAX_OPEN][128], paths[KTB_LIVEDESK_MAX_OPEN][KTB_PATH_BUF];
    int n = livedesk_read_open(house_root, pids, ents, paths, idx, KTB_LIVEDESK_MAX_OPEN);
    for (int i = 0; i < n; i++) {
        char base[64];
        livedesk_base_name(paths[i], base, sizeof(base));
        if (strcmp(base, "cursword") == 0 && ktb_pid_alive(pids[i])) return; /* already alive */
    }

    {
        char posp[KTB_PATH_BUF];
        snprintf(posp, sizeof(posp), "%s/desktop_pos.txt", pal);
        FILE *pw = fopen(posp, "w");
        if (pw) { fprintf(pw, "x=%d\ny=%d\n", CURSWORD_HOME_X, CURSWORD_HOME_Y); fclose(pw); }
    }

    char exe[KTB_PATH_BUF];
    snprintf(exe, sizeof(exe), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", house_root);
#ifdef _WIN32
    win_star_alias(exe);
    win_exe_suffix(exe);
    for (char *p = exe; *p; p++) if (*p == '/') *p = '\\';
#endif
    if (access(exe, F_OK) != 0) return;
#ifdef _WIN32
    win_spawn_cwd(exe, pal);
#else
    char cmd[KTB_PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), KTB_SETSID "nohup '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe, pal);
    int rc = ktb_system_recorded(house_root, cmd);
    (void)rc;
#endif
}

/* Real fix, 2026-08-31: reads the currently-active session and desk,
 * then reuses the proven livedesk_spawn_desk() logic to launch its real
 * entity set. This replaces the prior static autostart.pdl-based approach
 * with a real, drift-free mechanism: on button.sh reset, the ACTUAL active
 * desk's ACTUAL entity list (whatever session/desk is genuinely current,
 * not hardcoded) gets relaunched, the same way a live desk switch does.
 * Missing/unreadable session metadata is a silent no-op (preserves prior
 * behavior: if there's no active desk metadata, nothing spawns on reset,
 * which is safer than trying to guess). */
static void livedesk_spawn_active_desk(const char *house_root) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;

    char active_session[64] = "";
    livedesk_root_read(sroot, active_session, sizeof(active_session), NULL, 0);
    if (!active_session[0]) return; /* no active session - nothing to spawn */

    char active_desk[64] = "";
    livedesk_active_desk(sroot, active_session, active_desk, sizeof(active_desk));
    if (!active_desk[0]) return; /* no active desk - nothing to spawn */

    /* Now spawn the real active desk using the same proven logic as
     * livedesk_switch_desk() — the real, production-proven path. */
    livedesk_spawn_desk(house_root, sroot, active_session, active_desk);
}

static void livedesk_spawn_desk(const char *house_root, const char *sroot, const char *id, const char *desk) {
    char sdir[KTB_PATH_BUF], dp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(dp, sizeof(dp), "%s/desks/%s.pdl", sdir, desk);
#ifdef _WIN32
    /* desk_01.pdl is empty on this checkout; office.pdl holds the pals. */
    {
        FILE *probe = fopen(dp, "r");
        int has = 0;
        if (probe) {
            char ln[KTB_PATH_BUF];
            while (fgets(ln, sizeof(ln), probe))
                if (strncmp(ln, "DESK", 4) == 0) { has = 1; break; }
            fclose(probe);
        }
        if (!has) {
            char alt[KTB_PATH_BUF];
            snprintf(alt, sizeof(alt), "%s/desks/office.pdl", sdir);
            if (access(alt, F_OK) == 0) {
                snprintf(dp, sizeof(dp), "%s", alt);
                desk = "office";
            }
        }
    }
#endif
    FILE *f = fopen(dp, "r");
    if (!f) return;
    /* REAL FIX 2026-08-12, direct instruction ("we may also get rid of
     * the fallback at this point. its just more context to confuse
     * future agents"): the GLX tp_desktop_window.+x escape hatch
     * (renderer.txt="gl") is gone - RGB (tp_desktop_window_rgb.+x) is
     * the ONLY entity renderer this function launches now, no opt-out,
     * no missing-binary fallback. The old GLX source/binary itself is
     * untouched on disk (see !.deprecated-2026-08-12/ for what actually
     * got archived - this one deliberately was NOT, per the same
     * instruction that put it there originally: it's still real,
     * working code, just no longer reachable from this dispatch path). */
    /* 2026-08-14 consolidation: the entity renderer moved OUT of
     * tile-picker into this runtime folder - path below now points at the
     * tb ops/+x/ dir instead of &.widgits/tile-picker/ops/+x/. */
    char exe[KTB_PATH_BUF];
    snprintf(exe, sizeof(exe), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", house_root);
#ifdef _WIN32
    win_star_alias(exe);
    win_exe_suffix(exe);
    for (char *p = exe; *p; p++) if (*p == '/') *p = '\\';
#endif
    if (access(exe, F_OK) != 0) {
        fclose(f);
        return;
    }
    /* Real bug fix (2026-08-11, found live while testing the new
     * player>reset feature — "self" launched TWICE from one reset).
     * desks/<name>.pdl is an append-only HISTORY of saves (confirmed
     * directly: office.pdl genuinely has two real "self" DESK rows, an
     * early real position and a later 0,0 one from a different save) —
     * this loop spawned every DESK row unconditionally, with no dedup by
     * entity at all. Fixed: skip a row if its entity was already spawned
     * earlier in this same pass (keep-first, i.e. whichever position wins
     * the position-write below is irrelevant since it's a stat write, not
     * the reason for the extra process — the bug was purely "launched
     * twice," not "wrong position"). */
    char spawned[64][64];
    int n_spawned = 0;
    char line[KTB_PATH_BUF * 2];

    /* REAL FIX 2026-08-12, direct instruction ("i saw u double render
     * all entities in toolbar on accident. we should have a guard so
     * that never happens"): the in-pass dedup below (spawned[]) only
     * catches duplicate ROWS within THIS one desk-file read - it does
     * nothing if this whole function gets invoked twice (e.g. by an
     * accidentally-duplicated manager process, the actual cause traced
     * live this session, now separately closed by a real pidfile
     * singleton check in khtpm_taskbar_manager_main.c's main()). Second,
     * independent layer here: snapshot who's ALREADY alive in the real
     * registry before spawning anything, and skip any row whose pal is
     * already running - so even if this function somehow runs twice
     * concurrently (a second bug, a stuck lock, anything), it can never
     * itself be the thing that launches two processes for one entity. */
    int live_pids[KTB_LIVEDESK_MAX_OPEN], live_idx[KTB_LIVEDESK_MAX_OPEN];
    char live_ents[KTB_LIVEDESK_MAX_OPEN][128], live_paths[KTB_LIVEDESK_MAX_OPEN][KTB_PATH_BUF];
    int n_live = livedesk_read_open(house_root, live_pids, live_ents, live_paths, live_idx, KTB_LIVEDESK_MAX_OPEN);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        char *ent = NULL, *path = NULL, *xs = NULL, *ys = NULL;
        p++;
        ent = strtok(p, "|");
        path = strtok(NULL, "|");
        xs = strtok(NULL, "|");
        ys = strtok(NULL, "|");
        (void)ent;
        if (!path) continue;
        while (*path == ' ') path++;
        path[strcspn(path, "\r\n")] = '\0';
        char *pe = path + strlen(path);
        while (pe > path && pe[-1] == ' ') *--pe = '\0';
        char full[KTB_PATH_BUF];
        livedesk_join_path(house_root, path, full, sizeof(full));
        {
            char rowbase[64];
            livedesk_base_name(path, rowbase, sizeof(rowbase));
            int already = 0;
            for (int i = 0; i < n_spawned; i++) if (strcmp(spawned[i], rowbase) == 0) { already = 1; break; }
            if (already) continue;
            if (n_spawned < 64) snprintf(spawned[n_spawned++], sizeof(spawned[0]), "%s", rowbase);
        }
        /* §4.8/§4.9: the launch dir is the CANONICAL PAL, resolved by
         * basename (placements reference the pal; the pal copy is the real
         * location). A missing dev folder must NOT drop the entity - only a
         * missing PAL does. Legacy rows (path pointing at a dev folder or a
         * session entities copy) register their package into pals first. */
        char base[64];
        livedesk_base_name(path, base, sizeof(base));
        /* REAL FIX 2026-08-31, direct live report ("cursword now shows
         * up twice") - same real reason livedesk_close_all() already
         * skips cursword by basename: it has its own dedicated spawn+
         * dedup path (livedesk_ensure_cursword(), called right before
         * livedesk_spawn_active_desk() in ktb_init()) - a plain DESK row
         * for it in the active desk's own .pdl would otherwise race
         * that just-backgrounded spawn (its own real "already_live"
         * check below reads the registry BEFORE cursword's own process
         * has necessarily self-registered yet), producing a real
         * duplicate. */
        if (strcmp(base, "cursword") == 0) continue;
        char pr[KTB_PATH_BUF];
        if (!livedesk_pals_root(house_root, pr, sizeof(pr))) continue;
        char pal[KTB_PATH_BUF];
        snprintf(pal, sizeof(pal), "%s/%s", pr, base);
        if (access(pal, F_OK) != 0) {
            if (access(full, F_OK) != 0) continue;   /* not owned - skip */
            livedesk_ensure_pal(pr, base, full);      /* migrate into pals */
        }
        {
            int already_live = 0;
            for (int i = 0; i < n_live; i++)
                if (strcmp(live_paths[i], pal) == 0 && ktb_pid_alive(live_pids[i])) { already_live = 1; break; }
            if (already_live) continue; /* real process already running for this pal - never double-spawn it */
        }
        /* REAL FIX 2026-08-31, direct live report ("asa/ava/book-stack/
         * tile all silently self-close within a second of spawning") -
         * root cause: crypt_autostart.c's own quit_current_livedesk()
         * writes a plain "CLOSE\n" into every registered pal's real
         * interact_relay.txt as its graceful-shutdown attempt, BEFORE
         * the hard-kill sweep that follows it. If the OLD process for
         * this exact pal got killed before it ever polled and consumed
         * that line (a real, confirmed race - not hypothetical, caught
         * live via each closed pal's own history.txt: WINDOW_OPEN then
         * INJECTED: CLOSE one second later), the stale CLOSE is still
         * sitting in the file when THIS brand-new process starts - its
         * own first poll tick reads it and dutifully closes itself,
         * since interact_relay.txt is keyed by package PATH, not PID,
         * with zero way for a fresh process to tell a genuinely-new
         * command from a stale leftover one. Real, deterministic fix
         * (direct instruction: "we should use a marker file that is
         * house std" - not a timing delay, which would just narrow the
         * race, not close it): truncate the relay file right here,
         * immediately before this exact spawn, so any stale command
         * from a prior incarnation of this same pal is cleared before
         * the new process's own first poll ever runs. Matches this
         * relay file's own already-existing real contract elsewhere in
         * this house (write once, consumed once, truncated) - this is
         * just enforcing that contract at the one real point it could
         * otherwise be violated (a killed-before-consuming process). */
        {
            char relay[KTB_PATH_BUF];
            snprintf(relay, sizeof(relay), "%s/interact_relay.txt", pal);
            FILE *rf = fopen(relay, "w");
            if (rf) fclose(rf);
        }
        int x = xs ? atoi(xs) : -1, y = ys ? atoi(ys) : -1;
        if (x >= 0 && y >= 0) {
            char posp[KTB_PATH_BUF];
            snprintf(posp, sizeof(posp), "%s/desktop_pos.txt", pal);
            FILE *pw = fopen(posp, "w");
            if (pw) { fprintf(pw, "x=%d\ny=%d\n", x, y); fclose(pw); }
        }
#ifdef _WIN32
        win_spawn_cwd(exe, pal);
#else
        char cmd[KTB_PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), KTB_SETSID "nohup '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe, pal);
        int rc = ktb_system_recorded(house_root, cmd);
        (void)rc;
#endif
    }
    fclose(f);
    /* Real, new 2026-08-30: whatever desk just (re)spawned its own real
     * entities above, cursword is never one of the DESK rows read from
     * it - it lives outside the desk model entirely (see
     * livedesk_ensure_cursword()'s own header comment) - so it needs its
     * own explicit re-check every time this function runs. */
    livedesk_ensure_cursword(house_root);
}

static void livedesk_default_session(const char *house_root, const char *sroot, char *out, size_t sz) {
    char active[KTB_PATH_BUF] = "";
    livedesk_root_read(sroot, active, sizeof(active), NULL, 0);
    if (!active[0]) {
        snprintf(out, sz, "s1");
        livedesk_ensure_session(sroot, out, "pre-design");
        /* Check if there are live entities before snapshot. If not, the desk
         * will stay empty until entities actually launch and we resync. */
        int pids[KTB_LIVEDESK_MAX_OPEN];
        char ents[KTB_LIVEDESK_MAX_OPEN][128], paths[KTB_LIVEDESK_MAX_OPEN][KTB_PATH_BUF];
        int n = livedesk_read_open(house_root, pids, ents, paths, NULL, KTB_LIVEDESK_MAX_OPEN);
        if (n > 0) livedesk_snapshot_desk(house_root, sroot, out);
        livedesk_root_write(sroot, out, "");
    } else {
        snprintf(out, sz, "%s", active);
    }
}

static void livedesk_switch_desk(const char *house_root, const char *sroot, const char *id, const char *desk) {
    char active[64] = "";
    livedesk_active_desk(sroot, id, active, sizeof(active));
    /* Auto-save the OUTGOING desk only - never the desk we're switching TO
     * (snapshotting the incoming desk from the live registry could wipe its
     * saved entity set when the live desktop is empty/dead). */
    if (strcmp(active, desk) != 0)
        livedesk_snapshot_desk(house_root, sroot, id);
    livedesk_close_all(house_root);
    livedesk_write_active_desk(sroot, id, desk);
    livedesk_spawn_desk(house_root, sroot, id, desk);
}

#ifndef _WIN32
/* Hard-kill sweep for entities NOT tracked in #.desktop/livedesk_open.txt
 * — real fix, 2026-08-11, direct report: "some lingered last time and
 * that caused problems." livedesk_close_all() only ever sends CLOSE to
 * whatever's currently listed in the registry file; an entity that's
 * running but never made it into (or fell out of) that registry — e.g.
 * one hand-launched outside the normal spawn path, or left over from a
 * prior crash/registry-write race — silently survives any "close
 * everything" pass, no matter how many times it runs. This scans /proc
 * directly (same technique tp_desktop_window.c's own
 * ensure_taskbar_running() already uses) for any tp_desktop_window
 * process whose cmdline references THIS house_root, and SIGTERMs it,
 * catching stragglers the registry-based close can't see. */
/* SIGTERM-only sweep, no SIGKILL escalation. Extended 2026-08-11 (direct
 * live report: "there are entities still on desk after killing tb. this
 * isn't supposed to be" — reproduced by killing khtpm's own processes
 * directly instead of going through the in-app quit row, which skipped
 * ktb_quit_and_save() entirely) — SIGTERM alone isn't enough if the
 * target's own signal handling is wedged/slow; EMERGENCY_CLOSE.sh (this
 * house's existing manual fallback script, kept as-is for humans) already
 * proved the right shape: collect PIDs, SIGTERM all, wait, SIGKILL any
 * survivor. That 3-phase shape is folded in here now so the NORMAL quit
 * path (ktb_quit_and_save(), below) gets the same guarantee automatically
 * — the manual script should no longer be load-bearing for routine quits,
 * only a true last-resort if khtpm itself is unresponsive. */
static void livedesk_kill_stray_entities(const char *house_root) {
    DIR *pd = opendir("/proc");
    if (!pd) return;
    pid_t pids[256];
    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(pd)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        char cpath[64];
        snprintf(cpath, sizeof(cpath), "/proc/%s/cmdline", ent->d_name);
        FILE *cf = fopen(cpath, "r");
        if (!cf) continue;
        char cmdbuf[KTB_PATH_BUF * 2];
        size_t nb = fread(cmdbuf, 1, sizeof(cmdbuf) - 1, cf);
        fclose(cf);
        if (nb == 0) continue;
        cmdbuf[nb] = '\0';
        for (size_t i = 0; i < nb; i++) if (cmdbuf[i] == '\0') cmdbuf[i] = ' ';
        /* Kill both legacy entities (tp_desktop_window) and khtpm subwindows
         * (open-hai, db-hq, events-hq, etc.) that reference this house_root.
         * REAL, NEW 2026-08-30: cursword is exempt (see
         * livedesk_ensure_cursword()'s own header comment) - it should
         * survive this sweep exactly like a normal close, never get
         * hard-killed as a "stray". */
        if (strstr(cmdbuf, house_root) && !strstr(cmdbuf, "/pals/cursword") &&
            (strstr(cmdbuf, "tp_desktop_window") || strstr(cmdbuf, "khtpm_open_hai_render") ||
             strstr(cmdbuf, "khtpm_hq_render"))) {
            int pid = atoi(ent->d_name);
            if (pid > 0 && n < (int)(sizeof(pids) / sizeof(pids[0]))) pids[n++] = (pid_t)pid;
        }
    }
    closedir(pd);
    if (n == 0) return;
    for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
    struct timespec ts = {1, 0};
    nanosleep(&ts, NULL);
    for (int i = 0; i < n; i++) {
        if (kill(pids[i], 0) == 0) kill(pids[i], SIGKILL);
    }
}
#endif

/* Real "kill all entities, then reload the current desk fresh" — wires up
 * the player-cell "reset" row, which was a genuinely inert placeholder in
 * BOTH legacy and this port until now (see livedesk_build_player_menu()'s
 * own comment). Deliberately does NOT snapshot the live desk first (unlike
 * livedesk_switch_desk()) — the whole point of a reset is to discard
 * whatever mess is currently live and reload from the last SAVED desk
 * state, not persist the mess. Graceful close (registry-based CLOSE relay)
 * first, then the /proc hard-kill sweep above for anything the registry
 * missed, THEN respawn — so a genuinely stuck/orphaned entity from last
 * time can't survive this even if it couldn't survive a plain close. */
static void livedesk_reset_entities(const char *house_root, const char *sroot, const char *id) {
    char desk[64] = "";
    livedesk_active_desk(sroot, id, desk, sizeof(desk));
    if (!desk[0]) return;
    livedesk_close_all(house_root);
#ifndef _WIN32
    livedesk_kill_stray_entities(house_root);
#endif
    livedesk_spawn_desk(house_root, sroot, id, desk);
}

static void livedesk_load_session(const char *house_root, const char *sroot, const char *id) {
    char cur[KTB_PATH_BUF] = "";
    livedesk_root_read(sroot, cur, sizeof(cur), NULL, 0);
    if (cur[0] && strcmp(cur, id) != 0)
        livedesk_snapshot_desk(house_root, sroot, cur);   /* don't lose outgoing work */
    livedesk_close_all(house_root);
    char desk[64] = "";
    livedesk_active_desk(sroot, id, desk, sizeof(desk));
    livedesk_spawn_desk(house_root, sroot, id, desk);
    livedesk_root_write(sroot, id, cur[0] ? cur : id);
}

static void livedesk_new_session(const char *house_root) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[KTB_PATH_BUF] = "";
    livedesk_root_read(sroot, cur, sizeof(cur), NULL, 0);
    if (cur[0]) livedesk_snapshot_desk(house_root, sroot, cur);
    char id[64];
    int num = livedesk_next_id(sroot, id, sizeof(id));
    char name[64];
    snprintf(name, sizeof(name), "session%d", num);
    livedesk_ensure_session(sroot, id, name);
    livedesk_close_all(house_root);
    livedesk_write_active_desk(sroot, id, "desk_01");
    livedesk_root_write(sroot, id, cur[0] ? cur : id);
}

static void livedesk_new_desk(const char *house_root) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[KTB_PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    char sdir[KTB_PATH_BUF], desks[KTB_PATH_BUF], nd[64], dp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, cur, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    livedesk_mkdir_p(desks);
    livedesk_next_desk(sroot, cur, nd, sizeof(nd));
    snprintf(dp, sizeof(dp), "%s/%s.pdl", desks, nd);
    FILE *f = fopen(dp, "w");
    if (f) fclose(f);
    livedesk_snapshot_desk(house_root, sroot, cur);   /* auto-save current desk */
    livedesk_close_all(house_root);
    livedesk_write_active_desk(sroot, cur, nd);
    livedesk_root_write(sroot, cur, "");
}

static void livedesk_save(const char *house_root) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[KTB_PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    livedesk_snapshot_desk(house_root, sroot, cur);
    livedesk_root_write(sroot, cur, "");
}

static void livedesk_save_as_with_name(const char *house_root, const char *sroot, const char *newname) {
    if (!newname[0]) return;
    for (const char *p = newname; *p; p++)
        if (!cliio_key_allowed(*p)) return;
    char cur[KTB_PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    livedesk_snapshot_desk(house_root, sroot, cur);
    char nid[64];
    (void)livedesk_next_id(sroot, nid, sizeof(nid));
    char srcd[KTB_PATH_BUF], dst[KTB_PATH_BUF];
    livedesk_session_dir(sroot, cur, srcd, sizeof(srcd));
    livedesk_session_dir(sroot, nid, dst, sizeof(dst));
    livedesk_ensure_session(sroot, nid, newname);
    char cmd[KTB_PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/desks' && cp -r '%s/desks' '%s/' 2>/dev/null", dst, srcd, dst);
    int rc = system(cmd);
    (void)rc;
    char src_ad[64] = "";
    livedesk_active_desk(sroot, cur, src_ad, sizeof(src_ad));
    livedesk_write_active_desk(sroot, nid, src_ad);
    livedesk_root_write(sroot, nid, "");
}

static int livedesk_build_session_menu(const char *house_root, HQMenuItem *menu, int max) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char ids[KTB_LIVEDESK_DYN_MAX][64];
    int n = 0;
#ifdef _WIN32
    {
        wchar_t wroot[KTB_PATH_BUF], wpat[KTB_PATH_BUF];
        if (!MultiByteToWideChar(CP_UTF8, 0, sroot, -1, wroot, KTB_PATH_BUF))
            MultiByteToWideChar(CP_ACP, 0, sroot, -1, wroot, KTB_PATH_BUF);
        _snwprintf(wpat, KTB_PATH_BUF, L"%s\\*", wroot);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(wpat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (n >= max || n >= KTB_LIVEDESK_DYN_MAX) break;
                if (fd.cFileName[0] == L'.') continue;
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                char id[64];
                WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, id, 64, NULL, NULL);
                wchar_t wsp[KTB_PATH_BUF];
                _snwprintf(wsp, KTB_PATH_BUF, L"%s\\%s\\session.pdl", wroot, fd.cFileName);
                if (GetFileAttributesW(wsp) == INVALID_FILE_ATTRIBUTES) continue;
                snprintf(ids[n], sizeof(ids[n]), "%s", id);
                n++;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
#endif
    DIR *d = (n > 0) ? NULL : opendir(sroot);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (n >= max || n >= KTB_LIVEDESK_DYN_MAX) break;
            if (e->d_name[0] == '.') continue;
            char sp[KTB_PATH_BUF];
            snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sroot, e->d_name);
            if (access(sp, F_OK) != 0) continue;
            snprintf(ids[n], sizeof(ids[n]), "%s", e->d_name);
            n++;
        }
        closedir(d);
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(ids[j], ids[i]) < 0) {
                char t[64];
                snprintf(t, sizeof(t), "%s", ids[i]);
                snprintf(ids[i], sizeof(ids[i]), "%s", ids[j]);
                snprintf(ids[j], sizeof(ids[j]), "%s", t);
            }
    for (int i = 0; i < n && i < max; i++) {
        char name[256] = "";
        livedesk_session_name(sroot, ids[i], name, sizeof(name));
        snprintf(menu[i].label, sizeof(menu[i].label), "%s", name);
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:open-session:%s", ids[i]);
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Cancel");
        menu[n].command[0] = '\0';
        n++;
    }
    return n;
}

/* REAL FIX 2026-08-30, same pass as livedesk_build_file_menu() above,
 * direct instruction ("maybe we should fix tb file/desk first"). The
 * desk NAME list itself stays a real directory scan
 * (livedesk_desk_list()) - that's genuinely data (real desk files on
 * disk), not a hardcoded-C violation, so it's correct as-is (matching
 * TASKBAR-MENU-ARCHITECTURE.md's own note: "directory-scanning
 * builders keep the scan"). What WAS the anti-pattern is the trailing
 * static action rows (edit/+new-desk/cancel) being baked into C -
 * converted those to the same PDL-driven shape
 * (desk_menu_action_N_label/_cmd), falling back to the previous
 * hardcoded 3 rows if the .pdl defines none. */
static int livedesk_build_desk_menu(const char *house_root, HQMenuItem *menu, int max) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char cur[KTB_PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    char desks[KTB_LIVEDESK_DYN_MAX][64];
    int n = livedesk_desk_list(sroot, cur, desks, KTB_LIVEDESK_DYN_MAX);
    int i = 0;
    for (; i < n && i < max - 3; i++) {
        snprintf(menu[i].label, sizeof(menu[i].label), "%s", desks[i]);
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:switch-desk:%s/%s", cur, desks[i]);
    }

    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    int action_count = 0;
    HQMenuItem actions[8];
    for (int k = 1; k <= 8 && action_count < 8; k++) {
        char lkey[40], ckey[40];
        snprintf(lkey, sizeof(lkey), "desk_menu_action_%d_label", k);
        snprintf(ckey, sizeof(ckey), "desk_menu_action_%d_cmd", k);
        char lab[64] = "", cmd[KTB_PATH_BUF] = "";
        read_key_value(pdl, lkey, lab, sizeof(lab));
        read_key_value(pdl, ckey, cmd, sizeof(cmd));
        if (!lab[0]) continue;
        snprintf(actions[action_count].label, sizeof(actions[action_count].label), "%s", lab);
        snprintf(actions[action_count].command, sizeof(actions[action_count].command), "%s", cmd);
        action_count++;
    }
    if (action_count == 0) {
        /* K11: dedicated accessibility entry - always the row right after the
         * desk list (claims index N+1); opens properties for the focused row. */
        snprintf(actions[0].label, sizeof(actions[0].label), "edit");
        snprintf(actions[0].command, sizeof(actions[0].command), "livedesk:edit-desk");
        snprintf(actions[1].label, sizeof(actions[1].label), "+new-desk");
        snprintf(actions[1].command, sizeof(actions[1].command), "livedesk:new-desk");
        snprintf(actions[2].label, sizeof(actions[2].label), "cancel");
        actions[2].command[0] = '\0';
        action_count = 3;
    }
    for (int k = 0; k < action_count && i < max; k++, i++)
        menu[i] = actions[k];
    return i;
}

static int livedesk_build_pals_menu(const char *house_root, HQMenuItem *menu, int max) {
    char pr[KTB_PATH_BUF];
    if (!livedesk_pals_root(house_root, pr, sizeof(pr))) return 0;
    char names[KTB_LIVEDESK_DYN_MAX][64];
    int n = 0;
    DIR *d = opendir(pr);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (n >= max || n >= KTB_LIVEDESK_DYN_MAX) break;
            if (e->d_name[0] == '.') continue;
            char mp[KTB_PATH_BUF];
            snprintf(mp, sizeof(mp), "%s/%s/pal.pdl", pr, e->d_name);
            char gp[KTB_PATH_BUF];
            snprintf(gp, sizeof(gp), "%s/%s/glyph.txt", pr, e->d_name);
            if (access(mp, F_OK) != 0 && access(gp, F_OK) != 0) continue;
            snprintf(names[n], sizeof(names[n]), "%s", e->d_name);
            n++;
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(names[j], names[i]) < 0) {
                    char t[64];
                    snprintf(t, sizeof(t), "%s", names[i]);
                    snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                    snprintf(names[j], sizeof(names[j]), "%s", t);
                }
    }
    for (int i = 0; i < n && i < max; i++) {
        char mp[KTB_PATH_BUF], glyph[64] = "", hash[128] = "";
        snprintf(mp, sizeof(mp), "%s/%s/pal.pdl", pr, names[i]);
        read_key_value(mp, "glyph", glyph, sizeof(glyph));
        read_key_value(mp, "hash", hash, sizeof(hash));
        char short_hash[16] = "";
        snprintf(short_hash, sizeof(short_hash), "%s", hash);
        short_hash[10] = '\0';
        snprintf(menu[i].label, sizeof(menu[i].label), "%s %s #%s",
                 glyph[0] ? glyph : "•", names[i], short_hash);
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:pal:%s", names[i]);
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Cancel");
        menu[n].command[0] = '\0';
        n++;
    }
    return n;
}

/* REAL, NEW 2026-08-24 - the "6.palettes" cell's own dropdown, direct
 * instruction ("theres supposed to go under '6.palettes' see that on
 * tb?"). Reads #.desktop/livedesk_taskbar.pdl's
 * "SECTION | palettes_menu_N_label | value" / "_cmd" rows - deliberately
 * the SAME PDL-driven pattern as livedesk_build_hq_menu() directly below,
 * NOT the C-hardcoded cell-14 anti-pattern (see TASKBAR-MENU-ARCHITECTURE.md
 * standing-debt section). Each row opens that palette category's own
 * db-style tile window via &.widgits/palettes/palettes_menu.sh
 * (design: #.ref/menu/palletes/pallette-design.txt). */
static int livedesk_build_palettes_menu(const char *house_root, HQMenuItem *menu, int max) {
    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    int count = 0;
    for (int i = 1; i <= max; i++) {
        char lkey[40], ckey[40];
        snprintf(lkey, sizeof(lkey), "palettes_menu_%d_label", i);
        snprintf(ckey, sizeof(ckey), "palettes_menu_%d_cmd", i);
        char lab[64] = "", cmd[KTB_PATH_BUF] = "";
        read_key_value(pdl, lkey, lab, sizeof(lab));
        read_key_value(pdl, ckey, cmd, sizeof(cmd));
        if (!lab[0]) continue;
        snprintf(menu[count].label, sizeof(menu[count].label), "%s", lab);
        snprintf(menu[count].command, sizeof(menu[count].command), "%s", cmd);
        count++;
    }
    return count;
}

/* REAL, NEW 2026-08-31 - the "network" cell (positional 13, click code
 * 4000+13 per NETWORK-CELL-HQ-WINDOWS-DESIGN.md §2), wiring the real
 * next step that doc's own "REAL HANDOFF STATUS" section documents:
 * opencode had already built and tested the real launcher scripts
 * (open_network_app.sh/open_network_browser.sh under &.hq-apps/
 * network/) but never wired the taskbar menu itself. Same real
 * PDL-driven pattern as livedesk_build_palettes_menu() right above
 * (NOT the C-hardcoded livedesk_build_ai_menu() anti-pattern) -
 * answers that doc's own still-open §11 reviewer question: the
 * canonical row-writer is this dedicated-prefix PDL shape
 * (`network_menu_N_label`/`_cmd`), same as palettes, not a literal
 * `strip_btn_13_menu_N` guess. */
static int livedesk_build_network_menu(const char *house_root, HQMenuItem *menu, int max) {
    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    int count = 0;
    for (int i = 1; i <= max; i++) {
        char lkey[40], ckey[40];
        snprintf(lkey, sizeof(lkey), "network_menu_%d_label", i);
        snprintf(ckey, sizeof(ckey), "network_menu_%d_cmd", i);
        char lab[64] = "", cmd[KTB_PATH_BUF] = "";
        read_key_value(pdl, lkey, lab, sizeof(lab));
        read_key_value(pdl, ckey, cmd, sizeof(cmd));
        if (!lab[0]) continue;
        snprintf(menu[count].label, sizeof(menu[count].label), "%s", lab);
        snprintf(menu[count].command, sizeof(menu[count].command), "%s", cmd);
        count++;
    }
    return count;
}

/* Real HQ button's own menu ($.restart / X.quit / cancel), ported from
 * tp_taskbar.c's load_hq_config(): reads #.desktop/livedesk_taskbar.pdl's
 * "SECTION | hq_menu_N_label | value" / "SECTION | hq_menu_N_cmd | value"
 * rows (1-based N, compacted to 0-based here exactly like the legacy does),
 * falls back to the documented defaults when the .pdl defines no rows.
 * hq_label itself (default "HQ") is read by the strip cell layer, not here -
 * this only builds the row list. */
static int livedesk_build_hq_menu(const char *house_root, HQMenuItem *menu, int max) {
    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    int count = 0;
    /* cap history: 8 -> 9 for the cursword row (2026-08-24, AU24-oc-handon.md
     * §4.4), 9 -> KTB_LIVEDESK_DYN_MAX same day for the palettes rows
     * (direct instruction "pallets not opening drop down yet. look at hq
     * and do the same"): the .pdl now defines 20 real rows (10 palette
     * categories, cancel moved last to 20). The artificial literal cap
     * silently dropped every row past it - the dynamic max is the real
     * bound the caller already passes. */
    for (int i = 1; i <= max; i++) {
        char lkey[32], ckey[32];
        snprintf(lkey, sizeof(lkey), "hq_menu_%d_label", i);
        snprintf(ckey, sizeof(ckey), "hq_menu_%d_cmd", i);
        char lab[64] = "", cmd[KTB_PATH_BUF] = "";
        read_key_value(pdl, lkey, lab, sizeof(lab));
        read_key_value(pdl, ckey, cmd, sizeof(cmd));
        if (!lab[0]) continue;
        snprintf(menu[count].label, sizeof(menu[count].label), "%s", lab);
        snprintf(menu[count].command, sizeof(menu[count].command), "%s", cmd);
        count++;
    }
    if (count > 0) return count;
    /* Real bug fix (2026-08-11, direct live report: "when i click restart
     * in new toolbar sub-button, it seems to be restarting legacy as well
     * ... completely inappropriate"): the default command here was copied
     * verbatim from tp_taskbar.c's own legacy default ("setsid nohup
     * $.crypts/button.sh run") — correct for LEGACY (that's genuinely how
     * legacy's own restart is meant to work), but this is a SEPARATE
     * default in a SEPARATE, parallel, not-yet-deployed system, and
     * ktb_action_portable() only rewrites ABSOLUTE-path commands, so this
     * bare relative one passed straight through to button.sh — the
     * house-wide autostart control, which relaunches everything including
     * legacy via autostart.pdl's own LAUNCH|tool-bar row. Fixed: build the
     * FULL ABSOLUTE path to this system's own scoped restart script
     * (run_khtpm_strip.sh's "new" action — kills/rebuilds/relaunches ONLY
     * this khtpm_strip_parser/manager pair, never touches legacy or
     * anything else in autostart.pdl) using house_root, so it can never
     * resolve to the wrong thing regardless of this process's own cwd. */
    snprintf(menu[0].label, sizeof(menu[0].label), "$.restart");
    snprintf(menu[0].command, sizeof(menu[0].command),
             KTB_SETSID "nohup sh '%s/*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh' new",
             house_root);
    snprintf(menu[1].label, sizeof(menu[1].label), "X.quit");
    snprintf(menu[1].command, sizeof(menu[1].command), "quit");
    snprintf(menu[2].label, sizeof(menu[2].label), "cancel");
    menu[2].command[0] = '\0';
    return 3;
}

static void livedesk_place_pal(const char *house_root, const char *name) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[KTB_PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    char pr[KTB_PATH_BUF];
    if (!livedesk_pals_root(house_root, pr, sizeof(pr))) return;
    char pal[KTB_PATH_BUF];
    snprintf(pal, sizeof(pal), "%s/%s", pr, name);
    if (access(pal, F_OK) != 0) return;          /* must be owned */
    livedesk_ensure_pal(pr, name, pal);           /* refresh manifest only */
    char prel[KTB_PATH_BUF];
    livedesk_pals_rel(house_root, name, prel, sizeof(prel));
    char ad[64] = "";
    livedesk_active_desk(sroot, cur, ad, sizeof(ad));
    char sdir[KTB_PATH_BUF], dp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, cur, sdir, sizeof(sdir));
    snprintf(dp, sizeof(dp), "%s/desks/%s.pdl", sdir, ad);
    char glyph[64] = "", mp[KTB_PATH_BUF];
    snprintf(mp, sizeof(mp), "%s/pal.pdl", pal);
    read_key_value(mp, "glyph", glyph, sizeof(glyph));
    int max_idx = -1, used[KTB_LIVEDESK_MAX_OPEN][2], n_used = 0;
    /* Live bug 2026-08-24: a mouse-click (5000+row) and a relay digit+Enter
     * racing the same spawn both passed the open-registry check and each
     * appended a DESK row - this function had no dedupe at all. A pal is one
     * desktop instance: if its row is already on this desk, reuse that row's
     * own grid slot/pos, skip ONLY the append, and still run everything below
     * (desktop_pos.txt refresh + entity launch - a fresh spawn after a crash
     * or manual close must still come up even though its old row persists). */
    int have_dup = 0, dup_x = 0, dup_y = 0, dup_gx = 0, dup_gy = 0;
    FILE *f = fopen(dp, "r");
    if (f) {
        char line[KTB_PATH_BUF * 2];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "DESK", 4) != 0) continue;
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            char *tok[8];
            for (int k = 0; k < 8 && p; k++) {
                while (*p == ' ' || *p == '|') p++;
                tok[k] = p;
                p = strchr(p, '|');
                if (p) {
                    char *e = p;
                    while (e > tok[k] && (e[-1] == ' ' || e[-1] == '\t')) e--;
                    *e = '\0';
                    p++;
                }
            }
            int idx = tok[7] ? atoi(tok[7]) : -1;
            if (!have_dup && tok[1] && strcmp(tok[1], prel) == 0) {
                have_dup = 1;
                dup_x = tok[2] ? atoi(tok[2]) : 0;
                dup_y = tok[3] ? atoi(tok[3]) : 0;
                dup_gx = tok[4] ? atoi(tok[4]) : 0;
                dup_gy = tok[5] ? atoi(tok[5]) : 0;
            }
            if (idx > max_idx) max_idx = idx;
            if (tok[4] && tok[5] && n_used < KTB_LIVEDESK_MAX_OPEN) {
                used[n_used][0] = atoi(tok[4]);
                used[n_used][1] = atoi(tok[5]);
                n_used++;
            }
        }
        fclose(f);
    }
    int gx = 0, gy = 0, found = 0;
    for (gy = 0; gy < 16 && !found; gy++)
        for (gx = 0; gx < 32 && !found; gx++) {
            found = 1;
            for (int k = 0; k < n_used; k++)
                if (used[k][0] == gx && used[k][1] == gy) { found = 0; break; }
        }
    if (!found) { gx = 0; gy = 0; }
    gx--; gy--;
    int idx = max_idx + 1;
    int x = gx * KTB_LIVEDESK_GRID_PX, y = gy * KTB_LIVEDESK_GRID_PX;
    if (have_dup) {
        /* Row already on this desk - keep its exact slot, no second row. */
        x = dup_x; y = dup_y; gx = dup_gx; gy = dup_gy;
    } else {
        FILE *w = fopen(dp, "a");
        if (w) {
            fprintf(w, "DESK | %s | %s | %d | %d | %d | %d | %s | %d\n",
                    name, prel, x, y, gx, gy, glyph, idx);
            fclose(w);
        }
    }
    char posp[KTB_PATH_BUF];
    snprintf(posp, sizeof(posp), "%s/desktop_pos.txt", pal);
    FILE *pw = fopen(posp, "w");
    if (pw) { fprintf(pw, "x=%d\ny=%d\n", x, y); fclose(pw); }
    char exe[KTB_PATH_BUF];
    /* 2026-08-14 consolidation: entity renderer now lives in this runtime
     * folder (moved out of tile-picker); GLX tp_desktop_window.+x name
     * retired 2026-08-12 - RGB is the only renderer. */
    snprintf(exe, sizeof(exe), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", house_root);
#ifdef _WIN32
    win_star_alias(exe);
    win_exe_suffix(exe);
    for (char *p = exe; *p; p++) if (*p == '/') *p = '\\';
    if (access(exe, F_OK) == 0)
        win_spawn_cwd(exe, pal);
#else
    if (access(exe, F_OK) == 0) {
        char cmd[KTB_PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), KTB_SETSID "nohup '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe, pal);
        int rc = ktb_system_recorded(house_root, cmd);
        (void)rc;
    }
#endif
}

static int livedesk_desk_entity_count(const char *sroot, const char *id, const char *desk) {
    char sdir[KTB_PATH_BUF], dp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(dp, sizeof(dp), "%s/desks/%s.pdl", sdir, desk);
    FILE *f = fopen(dp, "r");
    if (!f) return 0;
    char line[KTB_PATH_BUF];
    int n = 0;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "DESK", 4) == 0) n++;
    fclose(f);
    return n;
}

static void livedesk_delete_desk(const char *house_root, const char *sroot,
                                 const char *id, const char *desk) {
    char list[KTB_LIVEDESK_DYN_MAX][64];
    int n = livedesk_desk_list(sroot, id, list, KTB_LIVEDESK_DYN_MAX);
    if (n <= 1) return;                     /* never delete the only desk */
    char sdir[KTB_PATH_BUF], dp[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(dp, sizeof(dp), "%s/desks/%s.pdl", sdir, desk);
    if (access(dp, F_OK) != 0) return;
    char active[64] = "";
    livedesk_active_desk(sroot, id, active, sizeof(active));
    if (strcmp(active, desk) == 0) {
        const char *next = NULL;
        for (int i = 0; i < n; i++)
            if (strcmp(list[i], desk) != 0) { next = list[i]; break; }
        if (!next) return;
        livedesk_switch_desk(house_root, sroot, id, next);
    }
    unlink(dp);
}

static void livedesk_rename_desk(const char *house_root, const char *sroot,
                                 const char *id, const char *desk, const char *newname) {
    (void)house_root;
    if (!newname[0] || strcmp(newname, desk) == 0) return;
    for (const char *p = newname; *p; p++)
        if (!cliio_key_allowed(*p)) return;
    char sdir[KTB_PATH_BUF], desks[KTB_PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    char oldp[KTB_PATH_BUF], newp[KTB_PATH_BUF];
    snprintf(oldp, sizeof(oldp), "%s/%s.pdl", desks, desk);
    snprintf(newp, sizeof(newp), "%s/%s.pdl", desks, newname);
    if (access(oldp, F_OK) != 0 || access(newp, F_OK) == 0) return;
    char active[64] = "";
    livedesk_active_desk(sroot, id, active, sizeof(active));
    if (rename(oldp, newp) != 0) return;
    if (strcmp(active, desk) == 0)
        livedesk_write_active_desk(sroot, id, newname);
}

/* ========================================================================
 * HQ popup menu + cli-io modal wrappers (2026-08-11) - the khtpm_strip_
 * parser port of tp_taskbar.c's top-left HQ/user/file/desks window and its
 * cli-io save-as/rename-desk text-input modal. Every function below is a
 * thin driver over the livedesk_* logic already defined above in this
 * file (build_session_menu/build_desk_menu/build_pals_menu/switch_desk/
 * new_desk/rename_desk/save_as_with_name/place_pal) - none of that logic
 * is duplicated here, only exposed via the KtbState fields declared in
 * khtpm_taskbar_manager.h. See run_popup_row()/agent_relay_dispatch()'s
 * cli-io branch in tp_taskbar.c for the exact input-side behavior this
 * mirrors (Enter arms typing / submits, Esc backs out one level, digits
 * jump focus).
 * ======================================================================== */

void ktb_hq_close(KtbState *s) {
    s->hq_open = 0;
    s->hq_n_menu = 0;
    s->hq_focus = -1;
    s->hq_digit_accum = 0;
    /* Live bug report 2026-08-13: "windows that get opened (like settings)
     * ... cant be manually cancled. id like to make sure it closes on
     * 'cancel' of 1.hq". The settings window is its own detached X11
     * process (khtpm_taskbar_settings_render.+x, launched by
     * button_taskbar_settings.sh) - closing the HQ menu here does not
     * touch it. Kill it here too (same pgrep -f full-cmdline match
     * button_taskbar_settings.sh already uses for its own single-instance
     * guard, not a new pattern) so ANY HQ-menu close - cancel, picking
     * another row, Esc - also closes the stray window. No-op if none
     * running. */
#ifdef _WIN32
    ktb_kill_by_exe("khtpm_taskbar_settings_render");
#else
    int rc = system("pgrep -f 'khtpm_taskbar_settings_render\\.\\+x' >/dev/null 2>&1 "
                     "&& pgrep -f 'khtpm_taskbar_settings_render\\.\\+x' | xargs -r kill -TERM "
                     ">/dev/null 2>&1");
    (void)rc;
#endif
}

/* Static file-cell submenu (new-desk/save/save-as/load), ported verbatim
 * from tp_taskbar.c's load_strip_config() defaults for btns[0] ("file") -
 * always exactly these 4 rows in the legacy (no .pdl override support for
 * strip button submenus was ever wired up there beyond the hq_menu_N_*
 * keys, so none is added here either - see this function's own header
 * comment on the which=3/8 static tables). */
/* REAL FIX 2026-08-30, direct instruction ("maybe we should fix tb
 * file/desk first then? if ur saying there not done yet?") - this was
 * C-hardcoded, the exact real, documented anti-pattern
 * TASKBAR-MENU-ARCHITECTURE.md's "Standing refactor debt" section
 * calls out (2026-08-24 update: "user confirmed none of the cells'
 * builders are supposed to be C-hardcoded"). Converted to the SAME
 * real, correct PDL-driven shape livedesk_build_hq_menu()/
 * livedesk_build_palettes_menu() already use - reads
 * #.desktop/livedesk_taskbar.pdl's "file_menu_N_label"/"_cmd" rows at
 * cell-open time, no recompile needed to add/edit/reorder a row. Falls
 * back to the previous hardcoded 4 rows only if the .pdl defines none
 * (same fallback-if-empty shape hq_menu's own builder uses), so this
 * is a pure superset - nothing regresses if the .pdl rows are never
 * added. */
static int livedesk_build_file_menu(const char *house_root, HQMenuItem *menu, int max) {
    char pdl[KTB_PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    int count = 0;
    for (int i = 1; i <= max; i++) {
        char lkey[32], ckey[32];
        snprintf(lkey, sizeof(lkey), "file_menu_%d_label", i);
        snprintf(ckey, sizeof(ckey), "file_menu_%d_cmd", i);
        char lab[64] = "", cmd[KTB_PATH_BUF] = "";
        read_key_value(pdl, lkey, lab, sizeof(lab));
        read_key_value(pdl, ckey, cmd, sizeof(cmd));
        if (!lab[0]) continue;
        snprintf(menu[count].label, sizeof(menu[count].label), "%s", lab);
        snprintf(menu[count].command, sizeof(menu[count].command), "%s", cmd);
        count++;
    }
    if (count > 0) return count;
    int n = 0;
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "new-desk"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:new-desk"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "save"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:save"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "save-as"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:save-as"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "load"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:load"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

/* Static player-cell submenu (play/pause/reset). play/pause were ported
 * verbatim from tp_taskbar.c's load_strip_config() defaults for btns[5]
 * ("player") and are genuinely inert placeholders in legacy itself (only
 * menu[i].label is ever set there, command left "" and never assigned) —
 * not a gap in this port. "reset" was ALSO inert like that until direct
 * request 2026-08-11 ("wire up player > reset [to] close all entities
 * then relaunch them fresh") — real, new functionality added here, not
 * present in legacy at all. See livedesk_reset_entities() for what it
 * does. */
static int livedesk_build_player_menu(HQMenuItem *menu, int max) {
    int n = 0;
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "play"); menu[n].command[0] = '\0'; n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "pause"); menu[n].command[0] = '\0'; n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "reset"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:reset-entities"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

/* ai cell (14) - real, wired 2026-08-12 (direct instruction: "get
 * started" on OPEN-HAI-GUI-DESIGN.md).
 *
 * REAL BUG FOUND LIVE (2026-08-12, direct report: "tb still doesn't
 * open ai window... need to wire that up asap"): a single-row menu
 * REAL ROOT CAUSE FOUND (2026-08-12): the h-ai button in
 * khtpm_strip_header.chtpm was self-closing with no child <row> element.
 * ACTIVATE buttons require nested <row> children for their menu items to
 * be discoverable by is_descendant() — a self-closing button has no
 * children, so the menu was never found. Fixed by making the button
 * non-self-closing and adding the child row, exactly like db/player/etc.
 * already do. Confirmed working: relay sequence `nav.sh nav 14` / `row 1`
 * now reliably launches khtpm_open_hai_render.+x.
 *
 * The 2-row shape (Open h-ai + Cancel) is INTENTIONAL, not a workaround.
 * Cancel row provides standard close-without-action UX, matching other
 * menus that need to be dismissible. */
static int livedesk_build_ai_menu(HQMenuItem *menu, int max) {
    int n = 0;
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Open h-ai");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:open-open-hai");
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Chat-h-ai");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:open-chat-hai");
        n++;
    }
    if (n < max) {
        /* Empty command, on purpose - ktb_hq_activate()'s own final
         * `else` branch (non-empty-but-unrecognized commands instead
         * get shelled out via system() as a real fallback for actual
         * shell-command menu rows elsewhere in this file - an empty
         * command is the actual "just close, no action" contract). */
        snprintf(menu[n].label, sizeof(menu[n].label), "Cancel");
        menu[n].command[0] = '\0';
        n++;
    }
    return n;
}

/* ---------------------------------------------------------------- */
/* Cell 15 (date/time) clock menu (15.clock-design.md §5.2).          */
/*                                                                   */
/* The clock system lives under <house>/#.desktop/clocks/ (owned by   */
/* the headless lc_clock daemon, &.widgits/livedesk-clock):            */
/*   clocks.pdl    CLOCK|<id>|scope=<s>|desc=<d>   (registry)          */
/*   <id>.pdl      SECTION rows: rate/running/tick/game_time_epoch_ms  */
/*   reminders.pdl r<id>|clock=<id>|at=<ms>|text=<s>|event=<s>|note=<s>*/
/*                 |enabled=1|fired=<ms>|repeat=<s>                   */
/*   daemon.pid    running daemon pid (menu rows shell out to lc_clock */
/*                 subcommands; the daemon does the flock-protected    */
/*                 writes, see lc_clock.c's own header comment).       */
/*                                                                   */
/* Level selection: the same "internal which" trick the db cell uses  */
/* (100/101/102): which==15 = clock root, 151 = clocks&cals list,      */
/* 152 = reminders list, 153 = game-clock controls, 154 = calendar     */
/* view of a chosen clock (selected id carried via g_clock_sel_id).    */
/* ---------------------------------------------------------------- */

#define CLOCK_MENU_CLOCKS   151
#define CLOCK_MENU_REMINDERS 152
#define CLOCK_MENU_GAME      153
#define CLOCK_MENU_CAL       154

static char g_clock_sel_id[64] = ""; /* clock chosen for the cal view */

static int livedesk_clock_rows(const char *house_root, HQMenuItem *menu, int max,
                               const char *row_kind, const char *extra_cmd) {
    int n = 0;
    char dir[KTB_PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/#.desktop/clocks", house_root);
    if (access(dir, F_OK) != 0) mkdir(dir, 0755);

    char fname[KTB_PATH_BUF];
    snprintf(fname, sizeof(fname), "%s/%s.pdl", dir, row_kind); /* clocks|reminders */
    FILE *f = fopen(fname, "r");
    if (f) {
        char line[KTB_PATH_BUF];
        while (fgets(line, sizeof(line), f) && n < max) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!line[0]) continue;
            char label[KTB_PATH_BUF], cmd[KTB_PATH_BUF];
            /* each record: first token = id, renderer formats per kind */
            if (strcmp(row_kind, "clocks") == 0) {
                char id[64] = "", scope[64] = "", desc[128] = "";
                char *p = line;
                if (strncmp(p, "CLOCK|", 6) == 0) p += 6;
                char *tok = strsep(&p, "|");
                if (tok) snprintf(id, sizeof(id), "%s", tok);
                while (p && *p) {
                    tok = strsep(&p, "|");
                    if (strncmp(tok, "scope=", 6) == 0) snprintf(scope, sizeof(scope), "%s", tok + 6);
                    else if (strncmp(tok, "desc=", 5) == 0) snprintf(desc, sizeof(desc), "%s", tok + 5);
                }
                if (!id[0]) continue;
                if (strcmp(extra_cmd, "cal") == 0) {
                    snprintf(label, sizeof(label), "%s  [%s]", id, scope[0] ? scope : "?");
                    snprintf(cmd, sizeof(cmd), "livedesk:clock:open:%s", id);
                } else if (strcmp(extra_cmd, "game") == 0) {
                    char st[KTB_PATH_BUF];
                    snprintf(st, sizeof(st), "%s/%s.pdl", dir, id);
                    char rate[32] = "off", running[8] = "1";
                    FILE *sf = fopen(st, "r");
                    if (sf) {
                        char sl[256];
                        while (fgets(sl, sizeof(sl), sf)) {
                            if (sscanf(sl, "SECTION | rate | %31s", rate) == 1) continue;
                            if (sscanf(sl, "SECTION | running | %7s", running) == 1) continue;
                        }
                        fclose(sf);
                    }
                    snprintf(label, sizeof(label), "%s  [%s %s]", id,
                             strcmp(running, "0") == 0 ? "paused" : "running", rate);
                    snprintf(cmd, sizeof(cmd), "livedesk:clock:open:%s", id);
                } else {
                    snprintf(label, sizeof(label), "%s", id);
                    if (scope[0]) { strncat(label, "  [", sizeof(label) - strlen(label) - 1); strncat(label, scope, sizeof(label) - strlen(label) - 1); strncat(label, "]", sizeof(label) - strlen(label) - 1); }
                    snprintf(cmd, sizeof(cmd), "livedesk:clock:open:%s", id);
                }
            } else { /* reminders */
                char rid[64] = "", clock[64] = "", at[64] = "", ev[128] = "", note[128] = "";
                char *p = line;
                while (p && *p) {
                    char *tok = strsep(&p, "|");
                    char *eq = strchr(tok, '=');
                    if (!eq) { /* bare token = record id (r1, r2, ...) */
                        snprintf(rid, sizeof(rid), "%s", tok);
                        continue;
                    }
                    if (strncmp(tok, "clock=", 6) == 0) snprintf(clock, sizeof(clock), "%s", tok + 6);
                    else if (strncmp(tok, "text=", 5) == 0) snprintf(at, sizeof(at), "%s", tok + 5);
                    else if (strncmp(tok, "event=", 6) == 0) snprintf(ev, sizeof(ev), "%s", tok + 6);
                    else if (strncmp(tok, "note=", 5) == 0) snprintf(note, sizeof(note), "%s", tok + 5);
                }
                if (!rid[0]) continue;
                snprintf(label, sizeof(label), "%s  %s@%s %s%s", rid, clock,
                         at[0] ? at : "?", ev[0] ? "→ " : "", ev[0] ? ev : (note[0] ? note : ""));
                snprintf(cmd, sizeof(cmd), "livedesk:clock:reminder-del:%s", rid);
            }
            if (n < max) {
                snprintf(menu[n].label, sizeof(menu[n].label), "%s", label);
                snprintf(menu[n].command, sizeof(menu[n].command), "%s", cmd);
                n++;
            }
        }
        fclose(f);
    }
    return n;
}

static int livedesk_build_clock_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = 0;
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "clocks & cals"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:clocks"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "reminders"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:reminders"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "game clocks"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:game"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "clock as event…"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:event-ez:%s", g_clock_sel_id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

static int livedesk_build_clock_cals_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = livedesk_clock_rows(house_root, menu, max, "clocks", "cal");
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "new game clock…"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:new-clock"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "back"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:back"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

static int livedesk_build_clock_reminders_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = livedesk_clock_rows(house_root, menu, max, "reminders", "");
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "new reminder…"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:new-reminder"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "back"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:back"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

static int livedesk_build_clock_game_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = livedesk_clock_rows(house_root, menu, max, "clocks", "game");
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "back"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:back"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

static int livedesk_build_clock_cal_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = 0;
    char id[64];
    snprintf(id, sizeof(id), "%s", g_clock_sel_id);
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "gamedate: %s", id); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:gamedate:%s", id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "advance (endturn)"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:endturn:%s", id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "ticker on/off"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:ticker:%s:on", id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "rate: cent|sec|min|hour|day"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:rate:%s:min", id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "pause/resume"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:pause:%s", id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "delete"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:delete:%s", id); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "back"); snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:clock:back"); n++; }
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

static int livedesk_build_db_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = 0;
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char cur[KTB_PATH_BUF] = "";
    livedesk_root_read(sroot, cur, sizeof(cur), NULL, 0);
    if (!cur[0]) return 0;

    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "db-ez");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:db-ez-sections");
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "db-hq");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:open-common-events-hq:%s", cur);
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Cancel");
        menu[n].command[0] = '\0';
        n++;
    }
    return n;
}

/* db-ez top level: RPG-Maker-style 14 section rows (todo-a12.txt Phase A).
 * Only "Common Events" is wired (see livedesk_build_db_common_events_menu());
 * the other 13 are inert placeholders with an empty command, matching the
 * existing convention (e.g. livedesk_build_player_menu()'s "play"/"pause")
 * which already falls through to the harmless default-close path in
 * ktb_hq_activate() - no dispatch case needed for them, so they can't crash. */
static int livedesk_build_db_ez_sections_menu(HQMenuItem *menu, int max) {
    static const char *sections[] = {
        "Actors", "Classes", "Skills", "Items", "Weapons", "Armors",
        "Enemies", "Troops", "States", "Animations", "Tilesets",
        "System", "Types"
    };
    int n = 0;
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]) && n < max; i++) {
        snprintf(menu[n].label, sizeof(menu[n].label), "%s", sections[i]);
        menu[n].command[0] = '\0';
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Common Events");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:db-ez-common-events");
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "back");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:db-ez-back");
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Cancel");
        menu[n].command[0] = '\0';
        n++;
    }
    return n;
}

/* Common Events list - GLOBAL (house_root-wide), not session-scoped, per
 * direct instruction: "wired to function globally instead of locally, just
 * like rpg maker" (Common Events are game-wide, unlike per-map events).
 * Deliberately bypasses livedesk_sessions_root()/livedesk_root_read(). */
static int livedesk_build_db_common_events_menu(const char *house_root, HQMenuItem *menu, int max) {
    char ce_root[KTB_PATH_BUF];
    snprintf(ce_root, sizeof(ce_root), "%s/common_events", house_root);
    if (access(ce_root, F_OK) != 0) mkdir(ce_root, 0755);

    char names[KTB_LIVEDESK_DYN_MAX][64];
    int n = 0;
    DIR *d = opendir(ce_root);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (n >= max || n >= KTB_LIVEDESK_DYN_MAX) break;
            if (e->d_name[0] == '.') continue;
            char ep[KTB_PATH_BUF];
            snprintf(ep, sizeof(ep), "%s/%s", ce_root, e->d_name);
            struct stat st;
            if (stat(ep, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(names[n], sizeof(names[n]), "%s", e->d_name);
            n++;
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(names[j], names[i]) < 0) {
                    char t[64];
                    snprintf(t, sizeof(t), "%s", names[i]);
                    snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                    snprintf(names[j], sizeof(names[j]), "%s", t);
                }
    }

    int i = 0;
    for (; i < n && i < max; i++) {
        snprintf(menu[i].label, sizeof(menu[i].label), "%s", names[i]);
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:open-common-event:%s", names[i]);
    }
    if (i < max) {
        snprintf(menu[i].label, sizeof(menu[i].label), "+ new common event");
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:new-common-event");
        i++;
    }
    if (i < max) {
        snprintf(menu[i].label, sizeof(menu[i].label), "back");
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:db-ez-common-events-back");
        i++;
    }
    if (i < max) {
        snprintf(menu[i].label, sizeof(menu[i].label), "Cancel");
        menu[i].command[0] = '\0';
        i++;
    }
    return i;
}

/* USER cell (which==2) submenu: New User... + one switch-user row per
 * existing account under 0.user-pal's users/ dir + Logout. Real, new
 * functionality (2026-08-11, direct instruction: "there is no way to
 * create new user in 2. user tab yet ... going forward i want to use only
 * livedesk as the interface, unique user accounts") - USER was a
 * direct-action no-submenu cell in the legacy (see ktb_strip_user_activate()
 * and this function's own former header comment), never had account
 * management wired to it at all.
 *
 * Design note (see au11-hq/USER_CREATION.md for the full writeup): the
 * genesis tpmos reference (1.TPMOS_c_+rmmp.0103.0001/projects/user/layouts/
 * user_signup.chtpm) proves simultaneous multi-field <cli_io> as the real
 * CHTPM standard for signup forms, but khtpm's own cli_io is a single
 * special-cased LEAF per screen (see khtpm_strip_header.chtpm's own header
 * comment) - genuinely one text buffer, not tpmos's multi-widget layout
 * model. Two fields are collected SEQUENTIALLY instead (op "new-user-id"
 * then "new-user-name", see ktb_cliio_submit()) as the closest faithful
 * match given that real constraint, not a wholesale re-architecture. */
static int livedesk_build_user_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = 0;
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "New User..."); snprintf(menu[n].command, sizeof(menu[n].command), "user:new"); n++; }

    char login_root[KTB_PATH_BUF];
    if (livedesk_login_root(house_root, login_root, sizeof(login_root))) {
        char users_dir[KTB_PATH_BUF];
        snprintf(users_dir, sizeof(users_dir), "%s/users", login_root);
        char cur_login[KTB_PATH_BUF];
        snprintf(cur_login, sizeof(cur_login), "%s/current_login.txt", login_root);
        char cur_id[128] = "";
        read_key_value(cur_login, "current_user_id", cur_id, sizeof(cur_id));

        char ids[KTB_LIVEDESK_DYN_MAX][128];
        int nu = 0;
#ifdef _WIN32
        {
            wchar_t wpat[KTB_PATH_BUF], wud[KTB_PATH_BUF];
            if (!MultiByteToWideChar(CP_UTF8, 0, users_dir, -1, wud, KTB_PATH_BUF))
                MultiByteToWideChar(CP_ACP, 0, users_dir, -1, wud, KTB_PATH_BUF);
            _snwprintf(wpat, KTB_PATH_BUF, L"%s\\*", wud);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(wpat, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (nu >= KTB_LIVEDESK_DYN_MAX - 2) break;
                    if (fd.cFileName[0] == L'.') continue;
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    char idutf[128];
                    WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, idutf, (int)sizeof(idutf), NULL, NULL);
                    wchar_t wprof[KTB_PATH_BUF];
                    _snwprintf(wprof, KTB_PATH_BUF, L"%s\\%s\\profile.txt", wud, fd.cFileName);
                    if (GetFileAttributesW(wprof) == INVALID_FILE_ATTRIBUTES) continue;
                    snprintf(ids[nu], sizeof(ids[nu]), "%s", idutf);
                    nu++;
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
#endif
        DIR *d = (nu > 0) ? NULL : opendir(users_dir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (nu >= KTB_LIVEDESK_DYN_MAX - 2) break;
                if (e->d_name[0] == '.') continue;
                char prof[KTB_PATH_BUF];
                snprintf(prof, sizeof(prof), "%s/%s/profile.txt", users_dir, e->d_name);
                if (access(prof, F_OK) != 0) continue;
                snprintf(ids[nu], sizeof(ids[nu]), "%s", e->d_name);
                nu++;
            }
            closedir(d);
        }
        for (int i = 0; i < nu - 1; i++)
            for (int j = i + 1; j < nu; j++)
                if (strcmp(ids[j], ids[i]) < 0) {
                    char t[128];
                    snprintf(t, sizeof(t), "%s", ids[i]);
                    snprintf(ids[i], sizeof(ids[i]), "%s", ids[j]);
                    snprintf(ids[j], sizeof(ids[j]), "%s", t);
                }
        for (int i = 0; i < nu && n < max; i++) {
            char prof[KTB_PATH_BUF], dname[128] = "";
            snprintf(prof, sizeof(prof), "%s/%s/profile.txt", users_dir, ids[i]);
            read_key_value(prof, "display_name", dname, sizeof(dname));
            if (!dname[0]) snprintf(dname, sizeof(dname), "%s", ids[i]);
            int is_cur = (cur_id[0] && strcmp(cur_id, ids[i]) == 0);
            snprintf(menu[n].label, sizeof(menu[n].label), "%s%s (%s)", is_cur ? "* " : "", ids[i], dname);
            snprintf(menu[n].command, sizeof(menu[n].command), "user:switch:%s", ids[i]);
            n++;
        }
        if (cur_id[0] && n < max) {
            snprintf(menu[n].label, sizeof(menu[n].label), "Logout");
            snprintf(menu[n].command, sizeof(menu[n].command), "user:logout");
            n++;
        }
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "Cancel");
        menu[n].command[0] = '\0';
        n++;
    }
    return n;
}

/* which is (cell index + 1), matching KtbState's own header-comment cell
 * order: 1=HQ, 2=USER, 3=file, 4=desks, 5=pals, 6=palettes, 7=edit,
 * 8=player, 9=db, 10=plugins, 11=store, 12=network, 13=ai. USER (2) now has a real
 * submenu (New User/Switch/Logout, see livedesk_build_user_menu() above) -
 * direct instruction 2026-08-11 to stop routing USER through
 * ktb_strip_user_activate()'s legacy-matching no-submenu path. Cells
 * 6/7/9/10/11/12 (palettes/edit/db/plugins/store/network) are confirmed
 * inert placeholders even in the legacy (load_strip_config() sets a label
 * and nothing else for each) - clicking one just closes whatever popup was
 * open, mirroring open_cell_popup()'s unconditional close_popups() call
 * followed by a no-op n_menu==0/cmd-empty cell. Cell 13 ("ai") is a NEW
 * inert placeholder added 2026-08-12, same pattern - no submenu, falls into
 * the same catch-all branch below. Cell 14 (date/time) WAS display-only,
 * not an ACTIVATE cell at all (see khtpm_strip_header.chtpm - it had no
 * onClick); since 2026-08-13 it IS an ACTIVATE:15 cell with a real clock
 * menu (see livedesk_build_clock_menu() + this function's which==15 branch
 * + the "livedesk:clock:*" dispatch cases in ktb_hq_activate()). */
/* REAL, NEW 2026-08-16, khtpm-merge-how2.md §5c.2 - real "toys" cell
 * population, scans for a real, new, minimal `toy.pdl` identity file
 * (META|title, META|launch - a real button.sh-relative action) rather
 * than retrofitting either of the house's two existing, real, but
 * differently-shaped identity conventions (meta.pdl's category=Toys,
 * or TPMOS-native project.pdl, which has no category field at all -
 * see khtpm-merge-how2.md §5c.2's own real finding). Opt-in by FILE
 * PRESENCE only (a directory with no toy.pdl is silently skipped, so
 * this can't produce a false-positive no matter how many real,
 * unrelated top-level directories exist) - scans house_root's own
 * direct children AND house_root/@.apps's own direct children (one
 * level each), matching where the 4 real, direct-instruction-named
 * candidates (mutaclysm, my-chara, my-lawyer, piececraft) actually
 * live (mutaclysm at house_root's own top level; the other 3 under
 * @.apps/). Live, in-process directory scan every time this cell
 * opens - same real, established shape as livedesk_build_pals_menu()/
 * livedesk_build_desk_menu() above, not a new pattern. */
static void toys_scan_add(HQMenuItem *menu, int max, int *n,
                         const char *root, const char *d_name) {
    if (*n >= max - 1) return;
    if (!d_name || d_name[0] == '.' || d_name[0] == '\0') return;
    char toy_pdl[KTB_PATH_BUF];
    snprintf(toy_pdl, sizeof(toy_pdl), "%s/%s/toy.pdl", root, d_name);
    char title[128] = "", launch[256] = "";
    read_key_value(toy_pdl, "title", title, sizeof(title));
    read_key_value(toy_pdl, "launch", launch, sizeof(launch));
    if (!title[0]) snprintf(title, sizeof(title), "%s", d_name);
    if (!launch[0]) snprintf(launch, sizeof(launch), "button.sh");
    snprintf(menu[*n].label, sizeof(menu[*n].label), "%s", title);
    snprintf(menu[*n].command, sizeof(menu[*n].command), "livedesk:open-toy:%s/%s/%s", root, d_name, launch);
    (*n)++;
}

static void toys_scan_one_root(const char *root, HQMenuItem *menu, int max, int *n) {
#ifdef _WIN32
    /* MinGW opendir/readdir/_access are ANSI. House path has emoji, so
     * readdir(".") returns 0 entries and toys stays Cancel-only.
     * Same FindFirstFileW pattern as livedesk_build_user_menu(). */
    {
        char root_n[KTB_PATH_BUF];
        wchar_t wroot[KTB_PATH_BUF], wpat[KTB_PATH_BUF];
        snprintf(root_n, sizeof(root_n), "%s", (root && root[0]) ? root : ".");
        win_star_alias(root_n);
        for (char *p = root_n; *p; p++) if (*p == '/') *p = '\\';
        if (!MultiByteToWideChar(CP_UTF8, 0, root_n, -1, wroot, KTB_PATH_BUF))
            MultiByteToWideChar(CP_ACP, 0, root_n, -1, wroot, KTB_PATH_BUF);
        _snwprintf(wpat, KTB_PATH_BUF, L"%s\\*", wroot);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(wpat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (*n >= max - 1) break;
                if (fd.cFileName[0] == L'.') continue;
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                wchar_t wtoy[KTB_PATH_BUF];
                _snwprintf(wtoy, KTB_PATH_BUF, L"%s\\%s\\toy.pdl", wroot, fd.cFileName);
                if (GetFileAttributesW(wtoy) == INVALID_FILE_ATTRIBUTES) continue;
                char nameutf[256];
                WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nameutf, (int)sizeof(nameutf), NULL, NULL);
                toys_scan_add(menu, max, n, root, nameutf);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
            return;
        }
    }
#endif
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *e;
    while (*n < max - 1 && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char toy_pdl[KTB_PATH_BUF];
        snprintf(toy_pdl, sizeof(toy_pdl), "%s/%s/toy.pdl", root, e->d_name);
        if (access(toy_pdl, F_OK) != 0) continue;
        toys_scan_add(menu, max, n, root, e->d_name);
    }
    closedir(d);
}

static int livedesk_build_toys_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = 0;
    toys_scan_one_root(house_root, menu, max, &n);
    char apps_root[KTB_PATH_BUF];
    snprintf(apps_root, sizeof(apps_root), "%s/@.apps", house_root);
    toys_scan_one_root(apps_root, menu, max, &n);
    if (n < max) { snprintf(menu[n].label, sizeof(menu[n].label), "Cancel"); menu[n].command[0] = '\0'; n++; }
    return n;
}

void ktb_hq_open(KtbState *s, int which) {
    int n = 0;
    /* REAL, NEW 2026-08-16, direct correction ("the cells aren't
     * supposed to be hardcoded... that's an oversight") - real,
     * data-declared cell identity, checked FIRST, before the existing
     * purely-positional which==N chain below (kept as-is for every
     * cell not yet migrated - real, deliberate incremental adoption).
     * "" (no real id declared for this position yet) falls through
     * unchanged to the existing chain. */
    const char *cid = ktb_cell_id(s, which);
    if (strcmp(cid, "toys") == 0) { n = livedesk_build_toys_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX); }
    /* palettes (positional 6, "6.palettes") wired 2026-08-24 - cid branch
     * first like toys; positional fallback while incremental adoption
     * continues (livedesk_header_cell_ids.txt now declares 6|palettes). */
    else if (strcmp(cid, "palettes") == 0 || which == 6) n = livedesk_build_palettes_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 2) n = livedesk_build_user_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 4) n = livedesk_build_desk_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 5) n = livedesk_build_pals_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 1) n = livedesk_build_hq_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 3) n = livedesk_build_file_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 8) n = livedesk_build_player_menu(s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    /* db (9) restored 2026-08-12 - was parked as an inert placeholder
     * while the real bug (header-click codes swallowed whenever ANY
     * cell's menu was already open, see dispatch_code()'s hq_open branch
     * in khtpm_taskbar_manager_main.c) got found and fixed; that bug
     * hit every cell, not just db, so db itself was never the problem.
     * See au11-hq/DB-HQ-HANDOFF.md for db-hq's still-placeholder status. */
    else if (which == 9) n = livedesk_build_db_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    /* ai (14) - real, wired 2026-08-12, see livedesk_build_ai_menu()'s
     * own header comment. Was one of the bare inert cells (6/7/10/11/
     * 12/13/14) this same catch-all comment below used to include. */
    else if (which == 14) n = livedesk_build_ai_menu(s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    /* date/time (15) - clock menu, wired 2026-08-13 (au11-hq/15.clock-
     * design.md §5.2): root + internal sublevels 151 (clocks&cals) / 152
     * (reminders) / 153 (game-clock controls) / 154 (calendar view). The
     * header click itself routes here generically via KSC_HQ_HEADER_BASE
     * (see khtpm_strip_codes.h: which = cell index + 1, 15 = date/time). */
    else if (which == 15) n = livedesk_build_clock_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == CLOCK_MENU_CLOCKS) n = livedesk_build_clock_cals_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == CLOCK_MENU_REMINDERS) n = livedesk_build_clock_reminders_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == CLOCK_MENU_GAME) n = livedesk_build_clock_game_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == CLOCK_MENU_CAL) n = livedesk_build_clock_cal_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else if (which == 100) n = livedesk_build_session_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX); /* 100 = internal-only "session picker", reached from the file cell's "load" row (livedesk:load), never a header click directly - see ktb_hq_activate() */
    else if (which == 101) n = livedesk_build_db_ez_sections_menu(s->hq_menu, KTB_LIVEDESK_DYN_MAX); /* 101 = internal-only db-ez 14-section list, reached from db cell's "db-ez" row */
    else if (which == 102) n = livedesk_build_db_common_events_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX); /* 102 = internal-only Common Events list (global, house_root-wide), reached from db-ez's "Common Events" row */
    /* network (13) - real, wired 2026-08-31, see livedesk_build_network_
     * menu()'s own header comment. Was one of the bare inert cells this
     * same catch-all comment below used to include. */
    else if (which == 13) n = livedesk_build_network_menu(s->house_root, s->hq_menu, KTB_LIVEDESK_DYN_MAX);
    else { ktb_hq_close(s); return; } /* inert cell (6/7/10/11/12) or unknown - close any open popup, no-op otherwise, matching the legacy exactly */
    if (n <= 0) {
        snprintf(s->hq_menu[0].label, sizeof(s->hq_menu[0].label), "(empty)");
        s->hq_menu[0].command[0] = '\0';
        n = 1;
    }
    s->hq_n_menu = n;
    s->hq_open = which;
    s->hq_focus = 0;
    s->hq_digit_accum = 0;
}

void ktb_hq_focus_delta(KtbState *s, int delta) {
    if (s->hq_n_menu <= 0) return;
    s->hq_focus += delta;
    if (s->hq_focus < 0) s->hq_focus = s->hq_n_menu - 1;
    if (s->hq_focus >= s->hq_n_menu) s->hq_focus = 0;
}

/* Simplified in spirit from tp_taskbar.c's popup_digit(): accumulate
 * digits typed while the popup is open and jump focus to the matching
 * 1-based row the moment the accumulated number is a valid row index. */
void ktb_hq_digit(KtbState *s, int d) {
    if (s->hq_n_menu <= 0) return;
    int accum = s->hq_digit_accum * 10 + d;
    if (accum >= 1 && accum <= s->hq_n_menu) {
        s->hq_focus = accum - 1;
        s->hq_digit_accum = accum;
    } else {
        s->hq_digit_accum = d;
        if (d >= 1 && d <= s->hq_n_menu) s->hq_focus = d - 1;
    }
}

/* REAL, dynamic path discovery (2026-08-17, direct instruction: "we
 * dont hardcode, see how tpmos's button.sh does dynamic path
 * discovery" - live report after muchi-pet/livedesk-clock moved out of
 * xyzfs/bin/ and every hardcoded "%s/xyzfs/bin/<app>/..." string
 * silently broke). Same real precedent as this house's own
 * play_event.sh (upward "101.mutaclsym... / system" landmark search) and
 * this very file's own toys_scan_one_root() (scans known root dirs for
 * an app by name) - not invented fresh. Scans a short list of known
 * real app-root directories under house_root for a subdirectory whose
 * name contains app_name, so the next move doesn't need a source edit
 * here again. */
static int find_app_dir(const char *house_root, const char *app_name, char *out, size_t outsz) {
    static const char *roots[] = { "*.monads", "&.widgits", "&.hq-apps", "@.apps", NULL };
    for (int i = 0; roots[i]; i++) {
        char parent[KTB_PATH_BUF];
        snprintf(parent, sizeof(parent), "%s/%s", house_root, roots[i]);
#ifdef _WIN32
        win_star_alias(parent);
#endif
        DIR *d = opendir(parent);
        if (!d) continue;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strstr(ent->d_name, app_name)) {
                snprintf(out, outsz, "%s/%s", parent, ent->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    out[0] = '\0';
    return 0;
}

void ktb_hq_activate(KtbState *s, int row) {
    if (row < 0 || row >= s->hq_n_menu) { ktb_hq_close(s); return; }
    HQMenuItem *m = &s->hq_menu[row];
    char sroot[KTB_PATH_BUF];
    int have_sroot = livedesk_sessions_root(s->house_root, sroot, sizeof(sroot));

    /* USER cell rows (2026-08-11, see livedesk_build_user_menu()'s own
     * header comment for why these exist / the tpmos-vs-khtpm cli_io
     * design note). "user:switch:" is genuinely 12 characters (verified:
     * printf '%s' "user:switch:" | wc -c) - checked directly after the
     * off-by-one bugs found elsewhere in this same function on
     * "livedesk:switch-desk:"/"livedesk:open-session:", not assumed. */
    if (strcmp(m->command, "user:new") == 0) {
        ktb_cliio_open_new_user(s);
        return;
    } else if (strncmp(m->command, "user:switch:", 12) == 0) {
        char login_root[KTB_PATH_BUF];
        if (livedesk_login_root(s->house_root, login_root, sizeof(login_root))) {
            /* Real bug, found live via relay testing (2026-08-11, not
             * hypothetical): userpal_*.+x binaries resolve their OWN
             * users//current_login.txt relative to PRISC_PROJECT_ROOT (or
             * "." if unset - see userpal_create_account.c's resolve_root())
             * — invoking them by absolute path alone leaves that "."
             * pointing at THIS MANAGER PROCESS's cwd (house_root), not
             * login_root, so they'd read/write a totally disconnected
             * users//current_login.txt at house_root that
             * livedesk_user_uuid()/ktb_get_avatar_dir() (which explicitly
             * read login_root's copy) never sees — accounts would appear
             * to switch but nothing else in khtpm would agree who's
             * logged in. Fixed the same way run_capture() in user-pal's
             * own userpal_menu_input.c already does it: `cd` into
             * login_root first so "." resolves there, no env var needed. */
#ifndef _WIN32
            char sh[KTB_PATH_BUF * 2];
            snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'cd \"%s\" && \"./ops/+x/userpal_login.+x\" \"%s\"' >/dev/null 2>&1 &",
                     login_root, m->command + 12);
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
#else
            (void)login_root;
#endif
        }
        ktb_hq_close(s);
        return;
    } else if (strcmp(m->command, "user:logout") == 0) {
        char login_root[KTB_PATH_BUF];
        if (livedesk_login_root(s->house_root, login_root, sizeof(login_root))) {
#ifndef _WIN32
            char sh[KTB_PATH_BUF * 2];
            snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'cd \"%s\" && \"./ops/+x/userpal_logout.+x\"' >/dev/null 2>&1 &", login_root);
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
#else
            (void)login_root;
#endif
        }
        ktb_hq_close(s);
        return;
    }

    if (strncmp(m->command, "livedesk:switch-desk:", 21) == 0) {
        /* Real bug fix (2026-08-11, same off-by-one class as
         * "livedesk:open-session:" above, found immediately after fixing
         * that one — checked systematically this time instead of one at
         * a time: "livedesk:switch-desk:" is genuinely 21 characters
         * (verified: `printf '%s' "livedesk:switch-desk:" | wc -c`), not
         * 22. Same failure mode: this branch could never match at all,
         * so switching desks within a session (the "desks" cell's own
         * rows) has been completely non-functional this whole session.
         * Grepped every other strncmp(m->command, "livedesk:...", N) in
         * this file after finding this to confirm no third instance —
         * "livedesk:pal:" (13) was already correct. */
        if (have_sroot) {
            char rest[KTB_PATH_BUF];
            snprintf(rest, sizeof(rest), "%s", m->command + 21);
            char *slash = strchr(rest, '/');
            if (slash) {
                *slash = '\0';
                livedesk_switch_desk(s->house_root, sroot, rest, slash + 1);
            }
        }
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:new-desk") == 0) {
        livedesk_new_desk(s->house_root);
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:edit-desk") == 0) {
        ktb_cliio_open_rename_desk(s); /* replaces the row list with the rename modal */
    } else if (strncmp(m->command, "livedesk:open-session:", 22) == 0) {
        /* Real bug fix (2026-08-11, direct live report/question: "if we
         * load a session it should auto load the desk with stuff on it
         * if there is one, right?" — confirmed yes, and confirmed broken
         * TWICE over, found live via the new save/load harness:
         * 1) This branch only ever wrote the active_session pointer file,
         *    never actually closed the outgoing session's entities or
         *    spawned the new session's own desk. livedesk_load_session()
         *    (already fully ported, ~line 1426 — snapshot outgoing, close
         *    all, spawn the target session's active desk, write the
         *    pointer) was sitting completely unused. Legacy's own real
         *    handler calls exactly this (tp_taskbar.c ~line 3155). Fixed
         *    below: call it directly instead of the bare pointer write.
         * 2) SEPARATE, deeper, pre-existing bug found while debugging fix
         *    #1 live (added temporary debug tracing, confirmed via direct
         *    evidence, not guessed): the strncmp() length here was 23,
         *    but "livedesk:open-session:" is genuinely 22 characters
         *    (verified: `printf '%s' "livedesk:open-session:" | wc -c`).
         *    Comparing 23 bytes meant byte 23 was compared too — the
         *    literal's own '\0' terminator against the real command's
         *    's' (start of the appended session id, e.g.
         *    "livedesk:open-session:s1") — a guaranteed mismatch. This
         *    branch could NEVER match, at all, regardless of fix #1 —
         *    session-switching via the picker has been completely
         *    non-functional this whole session, not just missing the
         *    entity-reload. The `+23` extraction offset had the same
         *    off-by-one (would have chopped the first character off the
         *    target id even once the strncmp was fixed). Both corrected
         *    to 22. */
        if (have_sroot) livedesk_load_session(s->house_root, sroot, m->command + 22);
        ktb_hq_close(s);
    } else if (strncmp(m->command, "livedesk:pal:", 13) == 0) {
        livedesk_place_pal(s->house_root, m->command + 13);
        /* §4.9 (mirrored): keep the pals popup open so several pals can be
         * placed in a row - only Esc/cancel closes it. */
    } else if (strncmp(m->command, "livedesk:open-palette:", 22) == 0) {
        /* palettes cell rows (2026-08-24, "6.palettes" dropdown): opens
         * that category's db-style tile window via
         * &.widgits/palettes/palettes_menu.sh (design:
         * #.ref/menu/palletes/pallette-design.txt). Dispatch string +
         * C-side quoted absolute path, the open-chat-hai/open_db_hq.sh
         * proven shape - NOT a raw PDL shell command, because the
         * house's literal "&.widgits/" dirs cannot survive unquoted in
         * the generic sh -c fallback ('&' is a control operator: found
         * live today via TB_PROOF touch test - raw row silently ran
         * ".widgits/..." after backgrounding). Prefix length verified:
         * printf '%s' "livedesk:open-palette:" | wc -c = 22. */
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh),
                 KTB_SETSID "nohup sh -c 'sh \"%s/&.widgits/palettes/palettes_menu.sh\" \"%s\"' >/dev/null 2>&1 &",
                 s->house_root, m->command + 22);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
        ktb_hq_close(s);
    } else if (strncmp(m->command, "livedesk:open-network:", 22) == 0) {
        /* network cell rows (2026-08-31, "13.network" dropdown, see
         * livedesk_build_network_menu()'s own header comment) - SAME
         * real reason as livedesk:open-palette: right above: the real
         * launcher scripts live under the house's literal "&.hq-apps/"
         * dir, which cannot survive unquoted in the generic sh -c
         * fallback ('&' is a control operator). Dispatch string +
         * C-side quoted absolute path, not a raw PDL shell command.
         * "browser" opens the real cli-io stub (open_network_browser.sh,
         * no extra arg); irc/forum/chain open the matching app via
         * open_network_app.sh's own real <app> key. Both scripts
         * already exist, tested, under &.hq-apps/network/ - built by
         * opencode, only the menu wiring itself was missing (see
         * NETWORK-CELL-HQ-WINDOWS-DESIGN.md's own "REAL HANDOFF
         * STATUS"). Prefix length verified: printf '%s'
         * "livedesk:open-network:" | wc -c = 22. */
        const char *key = m->command + 22;
        char sh[KTB_PATH_BUF * 3];
        if (strcmp(key, "browser") == 0) {
            snprintf(sh, sizeof(sh),
                     KTB_SETSID "nohup sh \"%s/&.hq-apps/network/open_network_browser.sh\" \"%s\" >/dev/null 2>&1 &",
                     s->house_root, s->house_root);
        } else {
            const char *title = strcmp(key, "irc") == 0 ? "IRC Chat"
                               : strcmp(key, "forum") == 0 ? "Forum"
                               : strcmp(key, "chain") == 0 ? "Chain" : key;
            snprintf(sh, sizeof(sh),
                     KTB_SETSID "nohup sh \"%s/&.hq-apps/network/open_network_app.sh\" \"%s\" \"%s\" \"%s\" >/dev/null 2>&1 &",
                     s->house_root, s->house_root, key, title);
        }
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:spawn-cursword") == 0) {
        /* CURSword personal-assistant entity (AU24-oc-handon.md §4.4),
         * HQ-menu row "cursword" (#.desktop/livedesk_taskbar.pdl
         * hq_menu_8_*). First click: copy the template entity
         * (<house_root>/*.monads/*.cursword/entities/cursword - house-
         * relative literal star-dir path, same convention as every other
         * *.monads/*. reference in this file, no absolute paths) into the
         * logged-in user's pals root, then place/spawn on the active desk
         * via the exact same livedesk_place_pal() path the pals cell uses
         * (DESK row append + livedesk_index assignment + desktop_pos.txt +
         * tp_desktop_window_rgb spawn all inherited, nothing new invented).
         *
         * SINGLE-INSTANCE (direct instruction 2026-08-24: "clicking
         * cursword shouldn't create a double. should only allow open of
         * the 1 , or focus too"): a re-click while an instance is already
         * open NEVER places again - it detects the live instance through
         * the house-standard open-entities registry (livedesk_read_open(),
         * which already filters dead PIDs) and writes RAISE into the
         * entity's own interact_relay.txt; the running entity window
         * raises itself to the top of the stack (see RAISE handling in
         * tp_desktop_window_rgb.c's relay poll). Relay-only action, no
         * new IPC, no Xlib here (this manager is deliberately pure
         * logic). */
        char pr[KTB_PATH_BUF];
        if (livedesk_pals_root(s->house_root, pr, sizeof(pr))) {
            char pal[KTB_PATH_BUF];
            snprintf(pal, sizeof(pal), "%s/cursword", pr);
            if (access(pal, F_OK) != 0) {
                char tpl[KTB_PATH_BUF];
                snprintf(tpl, sizeof(tpl), "%s/*.monads/*.cursword/entities/cursword", s->house_root);
                if (access(tpl, F_OK) == 0)
                    livedesk_copy_full(tpl, pal);
            }
            if (access(pal, F_OK) == 0) {
                int pids[KTB_LIVEDESK_MAX_OPEN], idxs[KTB_LIVEDESK_MAX_OPEN];
                char ents[KTB_LIVEDESK_MAX_OPEN][128];
                char paths[KTB_LIVEDESK_MAX_OPEN][KTB_PATH_BUF];
                int n = livedesk_read_open(s->house_root, pids, ents, paths, idxs, KTB_LIVEDESK_MAX_OPEN);
                int already_open = 0;
                for (int i = 0; i < n; i++) {
                    if (strcmp(ents[i], "cursword") == 0) { already_open = 1; break; }
                }
                if (already_open) {
                    char rp[KTB_PATH_BUF];
                    snprintf(rp, sizeof(rp), "%s/interact_relay.txt", pal);
                    FILE *w = fopen(rp, "w");
                    if (w) { fputs("RAISE\n", w); fclose(w); }
                } else {
                    livedesk_place_pal(s->house_root, "cursword");
                }
            }
        }
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:db-ez-sections") == 0) {
        /* db cell's "db-ez" row - drill into the 14-section list
         * (todo-a12.txt Phase A). */
        ktb_hq_open(s, 101);
    } else if (strcmp(m->command, "livedesk:db-ez-back") == 0) {
        ktb_hq_open(s, 9);
    } else if (strcmp(m->command, "livedesk:db-ez-common-events") == 0) {
        ktb_hq_open(s, 102);
    } else if (strcmp(m->command, "livedesk:db-ez-common-events-back") == 0) {
        ktb_hq_open(s, 101);
    } else if (strncmp(m->command, "livedesk:open-toy:", 18) == 0) {
        /* REAL, NEW 2026-08-16, khtpm-merge-how2.md 5c.2 - launches a
         * real toy project's own button.sh "run" action. m->command + 18
         * is the full path already resolved by livedesk_build_toys_menu().
         * Win: button.sh -> button.ps1 via CreateProcessW (no bash). */
#ifdef _WIN32
        {
            char launch[KTB_PATH_BUF];
            snprintf(launch, sizeof(launch), "%s", m->command + 18);
            win_star_alias(launch);
            size_t ln = strlen(launch);
            if (ln > 3 && strcmp(launch + ln - 3, ".sh") == 0)
                memcpy(launch + ln - 3, ".ps1", 5);
            for (char *p = launch; *p; p++) if (*p == '/') *p = '\\';
            wchar_t wfile[KTB_PATH_BUF], wdir[KTB_PATH_BUF], wcmd[KTB_PATH_BUF * 2];
            if (!MultiByteToWideChar(CP_UTF8, 0, launch, -1, wfile, KTB_PATH_BUF))
                MultiByteToWideChar(CP_ACP, 0, launch, -1, wfile, KTB_PATH_BUF);
            wcsncpy(wdir, wfile, KTB_PATH_BUF - 1);
            wdir[KTB_PATH_BUF - 1] = 0;
            wchar_t *slash = wcsrchr(wdir, L'\\');
            if (slash) *slash = 0;
            _snwprintf(wcmd, (KTB_PATH_BUF * 2) - 1,
                       L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\" run", wfile);
            STARTUPINFOW si; PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | CREATE_NO_WINDOW;
            BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL,
                                     wdir[0] ? wdir : NULL, &si, &pi);
            if (!ok) {
                flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW;
                ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL,
                                     wdir[0] ? wdir : NULL, &si, &pi);
            }
            if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
        }
#else
        char sh[KTB_PATH_BUF * 2];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s\" run' >/dev/null 2>&1 &", m->command + 18);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
#endif
        ktb_hq_close(s);
        } else if (strncmp(m->command, "livedesk:open-common-event:", 28) == 0) {
        /* GLOBAL (house_root-wide) common event, not session-scoped - see
         * livedesk_build_db_common_events_menu()'s own header comment. */
        char ce_path[KTB_PATH_BUF];
        snprintf(ce_path, sizeof(ce_path), "%s/common_events/%s", s->house_root, m->command + 28);
        if (access(ce_path, F_OK) != 0) mkdir(ce_path, 0755);
        char muchi_pet_dir[KTB_PATH_BUF];
        find_app_dir(s->house_root, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s/ops/open_event_ez.sh\" \"%s\" \"%s\"' >/dev/null 2>&1 &",
                 muchi_pet_dir, ce_path, s->house_root);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:new-common-event") == 0) {
        char ce_root[KTB_PATH_BUF];
        snprintf(ce_root, sizeof(ce_root), "%s/common_events", s->house_root);
        if (access(ce_root, F_OK) != 0) mkdir(ce_root, 0755);
        char ce_path[KTB_PATH_BUF];
        int idx = 0;
        for (;;) {
            snprintf(ce_path, sizeof(ce_path), "%s/event_%d", ce_root, idx);
            if (access(ce_path, F_OK) != 0) break;
            idx++;
        }
        mkdir(ce_path, 0755);
        char muchi_pet_dir[KTB_PATH_BUF];
        find_app_dir(s->house_root, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s/ops/open_event_ez.sh\" \"%s\" \"%s\"' >/dev/null 2>&1 &",
                 muchi_pet_dir, ce_path, s->house_root);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
        ktb_hq_close(s);
    } else if (strncmp(m->command, "livedesk:open-common-events-hq:", 31) == 0) {
        /* db-hq: first real proof of the HQML CSS layer (au11-hq/
         * HQML-DESIGN+PLANS.md), own separate X11 window process (see
         * khtpm_hq_render.c) - launches exactly like event-ez's own
         * setsid nohup pattern above, just against open_db_hq.sh.
         * PREFIX LENGTH BUG (found live via k9 relay testing, 2026-08-12):
         * this branch used 32 here, but `printf '%s' "livedesk:open-
         * common-events-hq:" | wc -c` is 31 - the literal's own '\0'
         * terminator (byte 32) was being compared against the real
         * command's 's' (start of the appended session id, e.g.
         * "livedesk:open-common-events-hq:s1"), a guaranteed mismatch.
         * Same off-by-one class as the "livedesk:open-session:" bug
         * documented elsewhere in this file - this branch could NEVER
         * match, at all, which is why clicking db-hq from the real
         * taskbar did nothing (silently fell through to the final
         * catch-all, see ktb_hq_activate()'s own closing branches). */
        char muchi_pet_dir[KTB_PATH_BUF];
        find_app_dir(s->house_root, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
#ifdef _WIN32
        char bin[KTB_PATH_BUF], chtpm[KTB_PATH_BUF];
        snprintf(bin, sizeof(bin), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_hq_render.+x", s->house_root);
        snprintf(chtpm, sizeof(chtpm), "%s/&.hq-apps/db-hq/dashboard.chtpm", s->house_root);
        const char *aa[2] = { s->house_root, chtpm };
        win_spawn_n(bin, aa, 2);
        (void)muchi_pet_dir;
#else
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s/ops/open_db_hq.sh\" \"%s\"' >/dev/null 2>&1 &",
                 muchi_pet_dir, s->house_root);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
#endif
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:open-open-hai") == 0) {
        /* ai cell (14) - real, wired 2026-08-12. Own separate X11
         * window process (khtpm_open_hai_render.c), same setsid nohup
         * launch pattern as db-hq/event-ez above, via open-hai's own
         * button.sh (mirrors open_db_hq.sh's own build-if-missing +
         * launch shape). */
#ifdef _WIN32
        char bin[KTB_PATH_BUF];
        snprintf(bin, sizeof(bin), "%s/&.widgits/open-hai/ops/+x/khtpm_open_hai_render.+x", s->house_root);
        const char *ha = s->house_root;
        win_spawn_n(bin, &ha, 1);
#else
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s/&.widgits/open-hai/button.sh\" \"%s\"' >/dev/null 2>&1 &",
                 s->house_root, s->house_root);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
#endif
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:open-chat-hai") == 0) {
        /* chat-hai cell (14) - standalone X11 window for multi-model ambient chat.
         * Launched via button.sh (mirrors open-hai pattern exactly). */
#ifdef _WIN32
        char bin[KTB_PATH_BUF], chtpm[KTB_PATH_BUF];
        snprintf(bin, sizeof(bin), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x", s->house_root);
        snprintf(chtpm, sizeof(chtpm), "%s/&.hq-apps/chat-hai/chat-hai.chtpm", s->house_root);
        const char *aa[2] = { s->house_root, chtpm };
        win_spawn_n(bin, aa, 2);
#else
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s/&.hq-apps/chat-hai/button.sh\" \"%s\"' >/dev/null 2>&1 &",
                 s->house_root, s->house_root);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
#endif
        ktb_hq_close(s);
    } else if (strncmp(m->command, "livedesk:clock:", 15) == 0) {
        /* Cell 15 clock menu dispatch (15.clock-design.md §5.3). All
         * writers shell out to lc_clock (control plane) which does the
         * flock-protected read-modify-write; the daemon is the only
         * writer of game_time_epoch_ms/tick/fired. */
        char lc_dir[KTB_PATH_BUF];
        find_app_dir(s->house_root, "livedesk-clock", lc_dir, sizeof(lc_dir));
        char lcbin[KTB_PATH_BUF];
        snprintf(lcbin, sizeof(lcbin), "%s/ops/+x/lc_clock.+x", lc_dir);
        const char *rest = m->command + 15;
        if (strcmp(rest, "clocks") == 0) {
            ktb_hq_open(s, CLOCK_MENU_CLOCKS);
            return;
        } else if (strcmp(rest, "reminders") == 0) {
            ktb_hq_open(s, CLOCK_MENU_REMINDERS);
            return;
        } else if (strcmp(rest, "game") == 0) {
            ktb_hq_open(s, CLOCK_MENU_GAME);
            return;
        } else if (strcmp(rest, "back") == 0) {
            ktb_hq_open(s, 15);
            return;
        } else if (strncmp(rest, "open:", 5) == 0) {
            snprintf(g_clock_sel_id, sizeof(g_clock_sel_id), "%s", rest + 5);
            ktb_hq_open(s, CLOCK_MENU_CAL);
            return;
        } else if (strcmp(rest, "new-clock") == 0 || strcmp(rest, "new-reminder") == 0) {
            /* cli_io flows are P3 (15.clock-design.md §5.4); placeholder
             * just reopens the list for now so the row isn't a no-op. */
            ktb_hq_open(s, rest[4] == 'c' ? CLOCK_MENU_CLOCKS : CLOCK_MENU_REMINDERS);
            return;
        } else if (strncmp(rest, "gamedate:", 9) == 0 ||
                   strncmp(rest, "endturn:", 8) == 0 ||
                   strncmp(rest, "ticker:", 7) == 0 ||
                   strncmp(rest, "rate:", 5) == 0 ||
                   strncmp(rest, "pause:", 6) == 0 ||
                   strncmp(rest, "delete:", 7) == 0) {
            char sh[KTB_PATH_BUF * 3];
            if (strncmp(rest, "gamedate:", 9) == 0) {
                snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '\"%s\" \"%s\" gamedate %s' >/dev/null 2>&1 &",
                         lcbin, s->house_root, rest + 9);
            } else if (strncmp(rest, "endturn:", 8) == 0) {
                snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '\"%s\" \"%s\" endturn %s' >/dev/null 2>&1 &",
                         lcbin, s->house_root, rest + 8);
            } else if (strncmp(rest, "ticker:", 7) == 0) {
                /* toggle: read current running/ticker state -> on|off.
                 * lc_clock has no "toggle" op; default to "on" once, and
                 * let the next click toggle off via the state file. KISS:
                 * the ticker state = <id>.pdl rate != off (see daemon). */
                char id[64]; snprintf(id, sizeof(id), "%s", rest + 7);
                char *colon = strchr(id, ':'); if (colon) *colon = '\0';
                snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '\"%s\" \"%s\" ticker %s on' >/dev/null 2>&1 &",
                         lcbin, s->house_root, id);
            } else if (strncmp(rest, "rate:", 5) == 0) {
                /* row is a fixed picker for now: cycle min->hour->day->off
                 * is P3; default to "min" (per design §4.1 default). */
                char id[64]; snprintf(id, sizeof(id), "%s", rest + 5);
                char *colon = strchr(id, ':'); if (colon) *colon = '\0';
                snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '\"%s\" \"%s\" rate %s min' >/dev/null 2>&1 &",
                         lcbin, s->house_root, id);
            } else if (strncmp(rest, "pause:", 6) == 0) {
                snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '\"%s\" \"%s\" ticker %s off' >/dev/null 2>&1 &",
                         lcbin, s->house_root, rest + 6);
            } else if (strncmp(rest, "delete:", 7) == 0) {
                snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '\"%s\" \"%s\" del %s' >/dev/null 2>&1 &",
                         lcbin, s->house_root, rest + 7);
            }
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
            ktb_hq_close(s);
        } else if (strncmp(rest, "event-ez:", 9) == 0) {
            /* P4 (15.clock-design.md §5.3): open this clock's event
             * package in event-ez. */
            char muchi_pet_dir[KTB_PATH_BUF];
            find_app_dir(s->house_root, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
            char sh[KTB_PATH_BUF * 3];
            snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'sh \"%s/ops/open_event_ez.sh\" \"%s/#.desktop/clocks/%s\" \"%s\"' >/dev/null 2>&1 &",
                     muchi_pet_dir, s->house_root, rest + 9, s->house_root);
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
            ktb_hq_close(s);
        } else {
            ktb_hq_close(s);
        }
    } else if (strcmp(m->command, "livedesk:open-settings") == 0) {
        /* HQ menu's "settings" row (2026-08-13, direct instruction) -
         * opens a palette picker for primary(theme_bg)/secondary
         * (theme_fg), writes #.desktop/livedesk_theme.pdl, then
         * triggers the same scoped restart "$.restart" already uses
         * so the new theme takes effect live. Own separate X11 window
         * process (khtpm_taskbar_settings_render.c), same setsid
         * nohup + button-script launch pattern as open-hai above.
         * Launcher script path now comes from the house-standard
         * livedesk_launchers.pdl registry, not hardcoded here. */
        char launcher[KTB_PATH_BUF];
        if (ktb_hq_launcher_path(s->house_root, "settings", launcher, sizeof(launcher))) {
            char sh[KTB_PATH_BUF * 3];
            snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'bash \"%s\" \"%s\"' >/dev/null 2>&1 &",
                     launcher, s->house_root);
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
        }
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:open-stats") == 0) {
        /* HQ menu's "stats" row - REAL FIX 2026-08-13 (direct live
         * report: "stats window still not opening... why cant it just
         * copy how hai opens?"). Two separate bugs, both fixed here:
         *
         * 1) The old pdl row (livedesk_taskbar.pdl hq_menu_6_cmd) was
         *    `sh "&.hq-apps/stats-hq/open_stats_hq.sh" .` - it fell
         *    through to the generic shell-out fallback, where the
         *    relative "&.hq-apps/..." path resolved against THIS
         *    process's cwd (the house's PARENT dir), not house_root,
         *    so the shell silently couldn't find the script. The stats
         *    row now uses the same reserved "livedesk:open-*" command
         *    shape as settings/open-hai/db-hq above.
         * 2) button_taskbar_stats.sh (which this used to shell to)
         *    hardcoded a stale path "xyzfs/bin/stats-hq/ops/
         *    open_stats_hq.sh" that does not exist - the real script
         *    lives at "&.hq-apps/stats-hq/open_stats_hq.sh". The whole
         *    window is now launched the exact same way open-hai opens
         *    (dedicated setsid nohup + button-script/launcher pattern),
         *    with the launcher script path read from the house-standard
         *    livedesk_launchers.pdl registry. */
        char launcher[KTB_PATH_BUF];
        if (ktb_hq_launcher_path(s->house_root, "stats", launcher, sizeof(launcher))) {
            char sh[KTB_PATH_BUF * 3];
            snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c 'bash \"%s\" \"%s\"' >/dev/null 2>&1 &",
                     launcher, s->house_root);
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
        }
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:reset-entities") == 0) {
        /* player cell's "reset" row - see livedesk_reset_entities()'s own
         * header comment for the full contract (graceful close, then a
         * /proc hard-kill sweep for stragglers, then respawn the current
         * desk fresh from its saved .pdl). */
        if (have_sroot) {
            char cur[KTB_PATH_BUF] = "";
            livedesk_default_session(s->house_root, sroot, cur, sizeof(cur));
            livedesk_reset_entities(s->house_root, sroot, cur);
        }
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:save") == 0) {
        /* file cell's "save" row - mirrors tp_taskbar.c's
         * livedesk_dispatch()'s own `strcmp(c, "save") == 0 ->
         * livedesk_save(house_root)` branch (an immediate action, no
         * replacement popup). */
        livedesk_save(s->house_root);
        ktb_hq_close(s);
    } else if (strcmp(m->command, "livedesk:save-as") == 0) {
        /* file cell's "save-as" row - mirrors livedesk_dispatch()'s
         * `livedesk_save_as()` branch, which opens the cli-io text-input
         * modal (same real target as the standalone which==4 header used
         * to be before this pass's 12-cell renumbering - see
         * ktb_cliio_open_save_as()). */
        ktb_cliio_open_save_as(s);
    } else if (strcmp(m->command, "livedesk:load") == 0) {
        /* file cell's "load" row - mirrors livedesk_dispatch()'s
         * `livedesk_open_sessions_popup()` branch: replaces the current
         * (file) row list in place with the session picker, same
         * "keep_open" shape run_popup_row() uses for load->sessions. */
        ktb_hq_open(s, 100); /* session picker (13 is an inert cell and would close the popup) */
    } else if (strcmp(m->command, "quit") == 0) {
        /* Real HQ menu's "X.quit" row (which==1, see ktb_hq_open()) -
         * mirrors tp_taskbar.c's agent_relay_dispatch() "quit" branch. Can't
         * call ktb_quit_and_save()+stop the event loop from here (no access
         * to the caller's g_running) - raise the flag instead, see this
         * struct field's own header comment in khtpm_taskbar_manager.h. */
        s->hq_quit_requested = 1;
        ktb_hq_close(s);
    } else if (m->command[0]) {
        /* Any other non-empty, non-livedesk-prefixed, non-"quit" command
         * (e.g. the real HQ menu's "$.restart" row) is a real shell command
         * - mirrors tp_taskbar.c's agent_relay_dispatch() "setsid nohup ..."
         * &" fallback for any HQ/strip menu row that isn't a reserved
         * command. This file already shells out directly elsewhere
         * (livedesk_copy_full/livedesk_spawn_desk etc, see this file's own
         * read_key_value() header comment), so this is consistent, not new. */
        char portable[KTB_PATH_BUF];
        ktb_action_portable(m->command, portable, sizeof(portable));
        char cmd[KTB_PATH_BUF * 3];
        if (strstr(portable, " .")) {
            char *p = portable;
            char *out = cmd;
            while (*p) {
                if (p[0] == ' ' && p[1] == '.' && (p[2] == '\0' || p[2] == ' ' || p[2] == '"')) {
                    out += snprintf(out, sizeof(cmd) - (size_t)(out - cmd), " %s", s->house_root);
                    p += 2;
                } else {
                    *out++ = *p++;
                }
            }
            *out = '\0';
        } else {
            snprintf(cmd, sizeof(cmd), "%s", portable);
        }
#ifdef _WIN32
        /* No bash. dir -> Explorer. cli -> open_cli.ps1 (visible console). */
        if (strstr(portable, "xdg-open") || strstr(m->command, "xdg-open")) {
            char abs[KTB_PATH_BUF];
            snprintf(abs, sizeof(abs), "%s", s->house_root[0] ? s->house_root : ".");
            win_star_alias(abs);
            for (char *p = abs; *p; p++) if (*p == '/') *p = '\\';
            ShellExecuteA(NULL, "open", abs, NULL, NULL, SW_SHOWNORMAL);
        } else if (strstr(portable, "open_cli") || strstr(m->command, "open_cli")) {
            char cli[KTB_PATH_BUF];
            snprintf(cli, sizeof(cli), "%s/*.monads/*.livedesk-taskbar/ops/open_cli.ps1", s->house_root);
            win_star_alias(cli);
            for (char *p = cli; *p; p++) if (*p == '/') *p = '\\';
            wchar_t wfile[KTB_PATH_BUF], wh[KTB_PATH_BUF], wcmd[KTB_PATH_BUF * 2];
            if (!MultiByteToWideChar(CP_UTF8, 0, cli, -1, wfile, KTB_PATH_BUF))
                MultiByteToWideChar(CP_ACP, 0, cli, -1, wfile, KTB_PATH_BUF);
            if (!MultiByteToWideChar(CP_UTF8, 0, s->house_root, -1, wh, KTB_PATH_BUF))
                MultiByteToWideChar(CP_ACP, 0, s->house_root, -1, wh, KTB_PATH_BUF);
            _snwprintf(wcmd, (KTB_PATH_BUF * 2) - 1,
                       L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\" \"%s\"",
                       wfile, wh);
            STARTUPINFOW si; PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | CREATE_NEW_CONSOLE;
            BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, wh[0] ? wh : NULL, &si, &pi);
            if (!ok) {
                flags = CREATE_NEW_CONSOLE;
                ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, flags, NULL, wh[0] ? wh : NULL, &si, &pi);
            }
            if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
        }
        (void)cmd;
#else
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '%s' >/dev/null 2>&1 &", cmd);
        int rc = ktb_system_recorded(s->house_root, sh);
        (void)rc;
#endif
        ktb_hq_close(s);
    } else {
        ktb_hq_close(s); /* "cancel" row or empty command */
    }
}

void ktb_cliio_close(KtbState *s) {
    s->cliio_active = 0;
    s->cliio_typing = 0;
    s->cliio_focus = 0;
    s->cliio_buffer[0] = '\0';
    s->cliio_sroot[0] = '\0';
    s->cliio_id[0] = '\0';
    s->cliio_desk[0] = '\0';
    s->cliio_op[0] = '\0';
}

/* Additive fix (2026-08-11, submenu/cli-io keyboard nav pass): tp_taskbar.c's
 * real cli-io key handling (cliio_active && !cliio_typing "nav mode" branch,
 * ~line 3829-3839) toggles g_cliio_focus between its 2 rows (0=field,
 * 1=cancel) on Up/Down — this port had no equivalent function at all, so
 * cli-io's row focus could only ever move via a mouse click, never the
 * keyboard, even though dispatch_code() already reuses KSC_FOCUS_LEFT/RIGHT
 * for exactly this purpose everywhere else (bottom bar, HQ popup). Named/
 * shaped to match ktb_hq_focus_delta() exactly. CLI_IO_ROWS is always 2
 * (field + cancel), matching tp_taskbar.c's own #define. */
void ktb_cliio_focus_delta(KtbState *s, int delta) {
    if (!s->cliio_active || s->cliio_typing) return; /* legacy: Up/Down only move focus in nav mode */
    s->cliio_focus += delta;
    if (s->cliio_focus < 0) s->cliio_focus = 1;
    if (s->cliio_focus > 1) s->cliio_focus = 0;
}

void ktb_cliio_open_save_as(KtbState *s) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(s->house_root, sroot, sizeof(sroot))) return;
    ktb_hq_close(s);
    snprintf(s->cliio_sroot, sizeof(s->cliio_sroot), "%s", sroot);
    s->cliio_id[0] = '\0';
    s->cliio_desk[0] = '\0';
    snprintf(s->cliio_op, sizeof(s->cliio_op), "save-as");
    s->cliio_buffer[0] = '\0';
    s->cliio_focus = 0;
    s->cliio_typing = 0;
    s->cliio_active = 1;
}

/* New-user signup, stage 1 of 2 (2026-08-11, see livedesk_build_user_menu()'s
 * header comment for the tpmos-vs-khtpm cli_io design note). Sequential
 * two-op flow ("new-user-id" then "new-user-name") since khtpm's cli_io is
 * one leaf/one buffer per screen, not tpmos's simultaneous multi-field
 * layout. cliio_id (normally scratch for rename-desk's "which session")
 * is reused here to stash the user_id collected in stage 1 for stage 2 to
 * read back - see ktb_cliio_submit()'s "new-user-id" branch. */
void ktb_cliio_open_new_user(KtbState *s) {
    ktb_hq_close(s);
    s->cliio_sroot[0] = '\0';
    s->cliio_id[0] = '\0';
    s->cliio_desk[0] = '\0';
    snprintf(s->cliio_op, sizeof(s->cliio_op), "new-user-id");
    s->cliio_buffer[0] = '\0';
    s->cliio_focus = 0;
    s->cliio_typing = 0;
    s->cliio_active = 1;
}

void ktb_cliio_open_rename_desk(KtbState *s) {
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(s->house_root, sroot, sizeof(sroot))) return;
    char cur[KTB_PATH_BUF] = "";
    livedesk_default_session(s->house_root, sroot, cur, sizeof(cur));
    char desk[64] = "";
    livedesk_active_desk(sroot, cur, desk, sizeof(desk));
    ktb_hq_close(s);
    snprintf(s->cliio_sroot, sizeof(s->cliio_sroot), "%s", sroot);
    snprintf(s->cliio_id, sizeof(s->cliio_id), "%s", cur);
    snprintf(s->cliio_desk, sizeof(s->cliio_desk), "%s", desk);
    snprintf(s->cliio_op, sizeof(s->cliio_op), "rename-desk");
    snprintf(s->cliio_buffer, sizeof(s->cliio_buffer), "%s", desk); /* seeded with current name */
    s->cliio_focus = 0;
    s->cliio_typing = 0;
    s->cliio_active = 1;
}

void ktb_cliio_start_typing(KtbState *s) { s->cliio_typing = 1; }
void ktb_cliio_stop_typing(KtbState *s) { s->cliio_typing = 0; }

void ktb_cliio_type(KtbState *s, char c) {
    if (!cliio_key_allowed(c)) return;
    size_t l = strlen(s->cliio_buffer);
    if (l < sizeof(s->cliio_buffer) - 1) {
        s->cliio_buffer[l] = c;
        s->cliio_buffer[l + 1] = '\0';
    }
}

void ktb_cliio_backspace(KtbState *s) {
    size_t l = strlen(s->cliio_buffer);
    if (l > 0) s->cliio_buffer[l - 1] = '\0';
}

void ktb_cliio_submit(KtbState *s) {
    if (!s->cliio_buffer[0]) return;
    if (strcmp(s->cliio_op, "rename-desk") == 0)
        livedesk_rename_desk(s->house_root, s->cliio_sroot, s->cliio_id, s->cliio_desk, s->cliio_buffer);
    else if (strcmp(s->cliio_op, "save-as") == 0)
        livedesk_save_as_with_name(s->house_root, s->cliio_sroot, s->cliio_buffer);
    else if (strcmp(s->cliio_op, "new-user-id") == 0) {
        /* Stage 1 -> 2: stash the id, re-open the SAME modal for the
         * display name instead of closing - see ktb_cliio_open_new_user()'s
         * header comment for why this is sequential, not simultaneous. */
        snprintf(s->cliio_id, sizeof(s->cliio_id), "%s", s->cliio_buffer);
        s->cliio_buffer[0] = '\0';
        snprintf(s->cliio_op, sizeof(s->cliio_op), "new-user-name");
        s->cliio_typing = 1; /* stay in typing mode - smoother than forcing another Enter first */
        return;
    } else if (strcmp(s->cliio_op, "new-user-name") == 0) {
        char login_root[KTB_PATH_BUF];
        if (livedesk_login_root(s->house_root, login_root, sizeof(login_root))) {
            /* Real bug, found live via relay testing (2026-08-11): same
             * cwd/PRISC_PROJECT_ROOT issue as user:switch:/user:logout
             * above - `cd` into login_root first so the binary's OWN
             * users//current_login.txt resolution lands in the SAME place
             * livedesk_user_uuid() reads, not a disconnected copy at
             * house_root. First live test of this flow created a real
             * account (users/claude-0001/profile.txt, xyzfs/users/<uuid>/
             * fully provisioned, auto-login wrote current_login.txt) —
             * but at house_root, invisible to the rest of khtpm. Verified
             * cleaned up before this fix; see au11-hq/USER_CREATION.md's
             * test log for the full trace. */
            char sh[KTB_PATH_BUF * 2];
            snprintf(sh, sizeof(sh),
                     KTB_SETSID "nohup sh -c 'cd \"%s\" && \"./ops/+x/userpal_create_account.+x\" \"%s\" \"%s\" && \"./ops/+x/userpal_login.+x\" \"%s\"' >/dev/null 2>&1 &",
                     login_root, s->cliio_id, s->cliio_buffer, s->cliio_id);
            int rc = ktb_system_recorded(s->house_root, sh);
            (void)rc;
        }
    }
    ktb_cliio_close(s);
}

/* ========================================================================
 * Full 12-cell strip wrappers, pass 2 (2026-08-11) - USER cell activation
 * and the unified header+tab nav cursor. See khtpm_taskbar_manager.h's
 * KtbState comment for the cell/which mapping this whole section assumes.
 * ======================================================================== */

/* Orphaned as of 2026-08-11 - no dispatch path calls this anymore now that
 * USER (which==2) routes through ktb_hq_open()/livedesk_build_user_menu()
 * like every other cell. Left in place (not deleted) since strip_user_cmd
 * was an explicit direct-instruction placeholder ("no-op until wired to a
 * user-switcher", 2026-08-08) that this function still reads - a future
 * agent may want to re-expose it as a configurable fallback action rather
 * than have it silently vanish. */
void ktb_strip_user_activate(KtbState *s) {
    /* Mirrors open_cell_popup()'s cmd-branch for a cell with no submenu:
     * close_popups() runs UNCONDITIONALLY first (even for a cell with
     * nothing to open), then the cell's own (possibly empty) command runs.
     * strip_user_cmd is a raw shell command in the legacy's own convention
     * (NOT "livedesk:"-prefixed - load_strip_config() never special-cases
     * it), so this fires it the exact same "setsid nohup sh -c" way
     * ktb_hq_activate()'s own generic fallback and khtpm_taskbar_manager_
     * main.c's run_shortcut() already do. */
    ktb_hq_close(s);
    if (!s->strip_user_cmd[0]) return; /* real no-op, matching the legacy's empty default */
    char portable[KTB_PATH_BUF];
    ktb_action_portable(s->strip_user_cmd, portable, sizeof(portable));
    char sh[KTB_PATH_BUF * 2];
    snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '%s' >/dev/null 2>&1 &", portable);
    int rc = ktb_system_recorded(s->house_root, sh);
    (void)rc;
}

/* Ported from tp_taskbar.c's nav_focus_step(): steps a SINGLE unified
 * cursor over [12 strip cells][s->n_tabs tabs], wrapping at both ends -
 * same shape as that function's own header comment on why two independent
 * cursors (strip_focus_cell/tab_focus_idx) must never be tracked
 * separately again. All 12 strip cells are navigable (none is_static, same
 * as every real StripCell in tp_taskbar.c's own cells[] assembly), so no
 * skip-logic is needed for the strip half of the range, only n_tabs==0
 * collapsing the tab half to nothing. */
void ktb_nav_focus_delta(KtbState *s, int delta) {
    int total = KTB_STRIP_N_CELLS + s->n_tabs;
    if (total <= 0) return;
    int focus = (s->strip_focus_cell >= 0) ? s->strip_focus_cell : KTB_STRIP_N_CELLS + s->tab_focus_idx;
    focus += delta;
    if (focus < 0) focus = total - 1;
    if (focus >= total) focus = 0;
    if (focus < KTB_STRIP_N_CELLS) {
        s->strip_focus_cell = focus;
    } else {
        s->strip_focus_cell = -1;
        s->tab_focus_idx = focus - KTB_STRIP_N_CELLS;
    }
    s->nav_armed = 1; /* matches ktb_focus_delta()'s own arm-on-move behavior */
}

/* REAL FIX, bug 1 (2026-08-11, direct live-test report: "right-click
 * doesn't arm nav focus" — user called this a big deal). Ports
 * tp_taskbar.c's button==3 ButtonPress branch on both strip_win and win
 * verbatim: close whatever popup/menu is open, arm nav, force the unified
 * cursor to index 0 (header gets priority [>] focus per that function's own
 * comment), and clear any in-progress digit buffer. */
void ktb_nav_arm(KtbState *s) {
    ktb_hq_close(s); /* this port's only "popup" — mirrors close_popups() */
    s->nav_armed = 1;
    s->strip_focus_cell = 0;
    s->digit_len = 0;
    s->digit_buf[0] = 0;
}

void ktb_nav_enter(KtbState *s) {
    if (s->digit_len > 0) { ktb_digit_enter(s); return; } /* typed-number jump, unchanged path */
    if (s->strip_focus_cell < 0) { ktb_digit_enter(s); return; } /* no strip cell focused - falls to tab-activate */
    int which = s->strip_focus_cell + 1;
    /* which==2 (USER) no longer special-cased to ktb_strip_user_activate()
     * as of 2026-08-11 - USER has a real ktb_hq_open() submenu now, see
     * livedesk_build_user_menu(). */
    ktb_hq_open(s, which);
}