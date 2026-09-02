/* crypt_autostart - house-wide autostart daemon.
 * Reads $.crypts/autostart.pdl (SECTION|KEY|VALUE):
 *   STATE   | enabled    | 1|0
 *   MOUNT   | uuid       | <disk UUID>     (Linux only)
 *   MOUNT   | mountpoint | <path>          (Linux only)
 *   LAUNCH  | <name>     | <command string>
 *
 * LAUNCH values should be HOUSE-RELATIVE (no /home/no/... absolutes):
 *   '&.widgits/.../tp_taskbar.+x' '.'
 *   '&.widgits/.../tp_desktop_window.+x' '#.desktop/entities/ava'
 * Absolute paths still work if present (legacy); relative preferred.
 *
 * Usage: crypt_autostart.+x [pdl_path]
 *   default pdl: next to this binary under $.crypts/autostart.pdl
 *
 * Windows: #ifdef _WIN32 (GetModuleFileName, CreateProcessW, name kill).
 * Linux: POSIX path (/proc, setsid nohup, udisksctl).
 */
#ifndef _WIN32
#  define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <tlhelp32.h>
#  include <process.h>
#  include <direct.h>
#  include <io.h>
#  define PATH_SEP '\\'
#  define access _access
#  ifndef F_OK
#    define F_OK 0
#  endif
#else
#  include <unistd.h>
#  include <signal.h>
#  include <dirent.h>
#  define PATH_SEP '/'
#endif

#define PATH_BUF 4352
#define MAX_LINE 4352
#define MAX_LAUNCH 64
#define MAX_ARGS 16

/* ---------- path helpers (both OS) ---------- */

static void path_norm_slashes(char *s) {
#ifdef _WIN32
    for (; *s; s++) if (*s == '/') *s = '\\';
#else
    (void)s;
#endif
}

#ifdef _WIN32
/* Linux dir names "*.monads" are "_.monads" on this Windows checkout
 * (* is illegal in an NTFS component). Alias at resolve time so PDL
 * keeps the Linux spelling. Do not call this on Linux. */
static void win_star_alias(char *path) {
    char tmp[PATH_BUF];
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
#endif

static void dirname_step(const char *in, char *out, size_t out_sz) {
    char tmp[PATH_BUF];
    size_t n = strlen(in);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    memcpy(tmp, in, n);
    tmp[n] = '\0';
    /* strip trailing separators */
    while (n > 1 && (tmp[n - 1] == '/' || tmp[n - 1] == '\\')) {
        tmp[--n] = '\0';
    }
    char *slash = strrchr(tmp, '/');
#ifdef _WIN32
    char *bslash = strrchr(tmp, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    if (!slash) {
        snprintf(out, out_sz, ".");
        return;
    }
    if (slash == tmp) {
        /* root */
        snprintf(out, out_sz, "%c", tmp[0] == '\\' ? '\\' : '/');
        return;
    }
    *slash = '\0';
    snprintf(out, out_sz, "%s", tmp);
}

static int is_abs_path(const char *p) {
    if (!p || !p[0]) return 0;
    if (p[0] == '/' || p[0] == '\\') return 1;
#ifdef _WIN32
    if (isalpha((unsigned char)p[0]) && p[1] == ':') return 1;
#endif
    return 0;
}

static void join_path(char *out, size_t out_sz, const char *a, const char *b) {
    if (!b || !b[0] || strcmp(b, ".") == 0) {
        snprintf(out, out_sz, "%s", a && a[0] ? a : ".");
        return;
    }
    if (is_abs_path(b)) {
        snprintf(out, out_sz, "%s", b);
        return;
    }
    /* house_root "." + relative => just relative (no ".\\" prefix noise) */
    if (!a || !a[0] || strcmp(a, ".") == 0) {
        snprintf(out, out_sz, "%s", b);
        path_norm_slashes(out);
        return;
    }
    size_t alen = strlen(a);
    if (alen > 0 && (a[alen - 1] == '/' || a[alen - 1] == '\\'))
        snprintf(out, out_sz, "%s%s", a, b);
    else
        snprintf(out, out_sz, "%s%c%s", a, PATH_SEP, b);
    path_norm_slashes(out);
}

/* This binary lives at <house_root>/$.crypts/ops/+x/crypt_autostart.+x */
static void resolve_default_pdl(char *out, size_t out_sz) {
    char self_path[PATH_BUF];
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, self_path, (DWORD)sizeof(self_path));
    if (n == 0 || n >= sizeof(self_path)) {
        snprintf(out, out_sz, "autostart.pdl");
        return;
    }
#else
    ssize_t slen = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (slen <= 0) {
        snprintf(out, out_sz, "autostart.pdl");
        return;
    }
    self_path[slen] = '\0';
#endif
    char step[PATH_BUF];
    dirname_step(self_path, step, sizeof(step)); /* .../ops/+x */
    dirname_step(step, step, sizeof(step));      /* .../ops */
    dirname_step(step, step, sizeof(step));      /* .../$.crypts */
    snprintf(out, out_sz, "%s%cautostart.pdl", step, PATH_SEP);
}

static void resolve_house_root_from_pdl(const char *pdl_path, char *out, size_t out_sz) {
    char step[PATH_BUF];
    dirname_step(pdl_path, step, sizeof(step)); /* .../$.crypts */
    dirname_step(step, out, out_sz);             /* house_root */
}

/* If path still starts with a known Linux house abs prefix, strip to relative
 * remainder (compat for unfixed PDL copies). Also strip house_root prefix. */
static void make_rel_to_house(const char *house_root, const char *in, char *out, size_t out_sz) {
    if (!in || !in[0]) {
        out[0] = '\0';
        goto done;
    }
    /* Already relative */
    if (!is_abs_path(in)) {
        snprintf(out, out_sz, "%s", in);
        goto done;
    }
    /* Strip house_root if absolute path is under it */
    size_t hlen = strlen(house_root);
    if (hlen > 0 && strncmp(in, house_root, hlen) == 0) {
        const char *rest = in + hlen;
        while (*rest == '/' || *rest == '\\') rest++;
        snprintf(out, out_sz, "%s", rest[0] ? rest : ".");
        goto done;
    }
    /* After yz.muchiverse/<house-folder>/ keep the rest (xyzfs/..., *.monads/...). */
    {
        const char *yz = strstr(in, "/yz.muchiverse/");
        if (!yz) yz = strstr(in, "\\yz.muchiverse\\");
        if (yz) {
            const char *after = yz + 15; /* strlen("/yz.muchiverse/") */
            while (*after && *after != '/' && *after != '\\') after++;
            if (*after == '/' || *after == '\\') after++;
            if (*after) {
                snprintf(out, out_sz, "%s", after);
                goto done;
            }
        }
    }
    /* Strip trailing ".../44.xyz...00.10/" style: find "/$.crypts" or "/&.widgits"
     * or "/#.desktop" or "/@.apps" in the absolute path and keep from there+1. */
    const char *markers[] = {
        "/&.widgits/", "\\&.widgits\\",
        "/#.desktop/", "\\#.desktop\\",
        "/@.apps/", "\\@.apps\\",
        "/$.crypts/", "\\$.crypts\\",
        "/xyzfs/", "\\xyzfs\\",
        "/*.monads/", "\\*.monads\\",
        "/_.monads/", "\\_.monads\\",
        "/101.", "\\101.",
        NULL
    };
    for (int i = 0; markers[i]; i++) {
        const char *m = strstr(in, markers[i]);
        if (m) {
            /* keep from after the leading slash of marker, so "&.widgits/..." */
            snprintf(out, out_sz, "%s", m + 1);
            goto done;
        }
    }
    /* Give up: keep absolute (will likely fail on wrong machine) */
    snprintf(out, out_sz, "%s", in);
done:
#ifdef _WIN32
    win_star_alias(out);
#endif
    return;
}

static int path_exists(const char *p) {
    return p && p[0] && access(p, F_OK) == 0;
}

/* Resolve a binary path for this OS: try as-is, then .exe, then .+x.exe */
static int resolve_binary(const char *house_root, const char *token, char *out, size_t out_sz) {
    char rel[PATH_BUF];
    char cand[PATH_BUF];
    make_rel_to_house(house_root, token, rel, sizeof(rel));
    path_norm_slashes(rel);

    if (is_abs_path(rel)) {
        snprintf(cand, sizeof(cand), "%s", rel);
    } else {
        join_path(cand, sizeof(cand), house_root, rel);
    }
    path_norm_slashes(cand);

#ifdef _WIN32
    /* Prefer .exe ALWAYS on Windows. Linux ELF files named *.+x often
     * sit beside Win builds; running them yields ERROR_BAD_EXE_FORMAT (193). */
    {
        size_t clen = strlen(cand);
        char alt[PATH_BUF];
        if (clen > 3 && strcmp(cand + clen - 3, ".+x") == 0) {
            if (clen - 3 + 4 < sizeof(alt)) {
                memcpy(alt, cand, clen - 3);
                memcpy(alt + clen - 3, ".exe", 5);
                if (path_exists(alt)) {
                    snprintf(out, out_sz, "%s", alt);
                    return 1;
                }
            }
        }
        if (clen + 4 < sizeof(alt)) {
            snprintf(alt, sizeof(alt), "%s.exe", cand);
            if (path_exists(alt)) {
                snprintf(out, out_sz, "%s", alt);
                return 1;
            }
        }
        /* last resort: path as given if it is already .exe */
        if (clen > 4 && _stricmp(cand + clen - 4, ".exe") == 0 && path_exists(cand)) {
            snprintf(out, out_sz, "%s", cand);
            return 1;
        }
        /* Do NOT return bare . +x (ELF) on Windows */
        snprintf(out, out_sz, "%s", cand);
        /* rewrite display path to .exe for clearer error even if missing */
        if (clen > 3 && strcmp(cand + clen - 3, ".+x") == 0 && clen - 3 + 4 < out_sz) {
            memcpy(out, cand, clen - 3);
            memcpy(out + clen - 3, ".exe", 5);
        }
        return 0;
    }
#else
    if (path_exists(cand)) {
        snprintf(out, out_sz, "%s", cand);
        return 1;
    }
    if (!strchr(token, '/') && !strchr(token, '\\')) {
        snprintf(out, out_sz, "%s", token);
        return 0;
    }
    snprintf(out, out_sz, "%s", cand);
    return 0;
#endif
}

static void resolve_arg(const char *house_root, const char *token, char *out, size_t out_sz) {
    char rel[PATH_BUF];
    if (!token || !token[0] || strcmp(token, ".") == 0) {
        snprintf(out, out_sz, "%s", house_root);
        return;
    }
    make_rel_to_house(house_root, token, rel, sizeof(rel));
    if (is_abs_path(rel)) {
        snprintf(out, out_sz, "%s", rel);
        path_norm_slashes(out);
        return;
    }
    if (!strchr(rel, '/') && !strchr(rel, '\\') &&
        !(rel[0] == '&' || rel[0] == '#' || rel[0] == '@' ||
          rel[0] == '$' || rel[0] == '*' || rel[0] == '!')) {
        snprintf(out, out_sz, "%s", rel);
        return;
    }
#ifdef _WIN32
    /* Keep house-relative so pal dirs stay ASCII (xyzfs/...) under CWD=house.
     * Joining the emoji house folder makes MinGW fopen miss glyph/sprite. */
    snprintf(out, out_sz, "%s", rel);
    path_norm_slashes(out);
#else
    join_path(out, out_sz, house_root, rel);
#endif
}

/* Parse 'a' 'b' "c" or unquoted tokens from LAUNCH value */
static int tokenize_cmd(const char *cmd, char args[][PATH_BUF], int max_args) {
    int n = 0;
    const char *p = cmd;
    while (*p && n < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char quote = 0;
        if (*p == '\'' || *p == '"') {
            quote = *p++;
        }
        const char *start = p;
        if (quote) {
            while (*p && *p != quote) p++;
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        size_t len = (size_t)(p - start);
        if (len >= PATH_BUF) len = PATH_BUF - 1;
        memcpy(args[n], start, len);
        args[n][len] = '\0';
        n++;
        if (quote && *p == quote) p++;
    }
    return n;
}

/* ---------- mount / quit / launch: platform-specific ---------- */

#ifndef _WIN32
static int is_mounted(const char *mountpoint) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char dev[PATH_BUF], mp[PATH_BUF];
        if (sscanf(line, "%s %s", dev, mp) == 2 && strcmp(mp, mountpoint) == 0) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static int pid_alive(pid_t pid) {
    if (pid <= 1) return 0;
    return kill(pid, 0) == 0 || errno != ESRCH;
}
#endif

#ifdef _WIN32
static int pid_alive_win(DWORD pid) {
    if (pid <= 0) return 0;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    DWORD code = 0;
    int alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}

static void kill_pid_win(DWORD pid) {
    if (pid <= 0) return;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return;
    TerminateProcess(h, 1);
    CloseHandle(h);
}

/* Kill by image name via Toolhelp — never system(taskkill) (console flash)
 * and never enumerate Process.Path (hang risk). */
static int exe_name_match(const char *exe, const char *want) {
    if (!exe || !want || !want[0]) return 0;
    size_t wl = strlen(want);
    if (_stricmp(exe, want) == 0) return 1;
    if (wl > 4 && _stricmp(want + wl - 4, ".exe") == 0)
        return _stricmp(exe, want) == 0;
    char with[160];
    snprintf(with, sizeof(with), "%s.exe", want);
    return _stricmp(exe, with) == 0;
}

static void kill_by_name_win(const char *name) {
    DWORD self = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == self) continue;
            if (!exe_name_match(pe.szExeFile, name)) continue;
            kill_pid_win(pe.th32ProcessID);
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

/* Match PowerShell Start-Process: no DETACHED, no CREATE_NO_WINDOW, no
 * SW_HIDE. DETACHED+parent-exit was dropping rgb while strip survived. */
#define SPAWN_GUI (CREATE_BREAKAWAY_FROM_JOB | CREATE_UNICODE_ENVIRONMENT)

/* Prefer relative paths + CreateProcessW so emoji house roots work. */
static int to_rel_under_house(const char *house_root, const char *abs_or_rel,
                              char *out, size_t out_sz) {
    if (!is_abs_path(abs_or_rel)) {
        snprintf(out, out_sz, "%s", abs_or_rel);
        path_norm_slashes(out);
        return 1;
    }
    size_t hlen = strlen(house_root);
    if (hlen > 0 && _strnicmp(abs_or_rel, house_root, (int)hlen) == 0) {
        const char *rest = abs_or_rel + hlen;
        while (*rest == '/' || *rest == '\\') rest++;
        snprintf(out, out_sz, "%s", rest[0] ? rest : ".");
        path_norm_slashes(out);
        return 1;
    }
    snprintf(out, out_sz, "%s", abs_or_rel);
    path_norm_slashes(out);
    return 0;
}

static int launch_detached_win(const char *exe, char args[][PATH_BUF], int nargs, const char *cwd) {
    char rel_exe[PATH_BUF];
    char rel_args[MAX_ARGS][PATH_BUF];
    to_rel_under_house(cwd, exe, rel_exe, sizeof(rel_exe));
    for (int i = 0; i < nargs; i++) {
        if (i == 0) snprintf(rel_args[0], sizeof(rel_args[0]), "%s", rel_exe);
        else to_rel_under_house(cwd, args[i], rel_args[i], sizeof(rel_args[i]));
    }

    /* Build UTF-16 command line */
    char cmdline_a[PATH_BUF * 4];
    size_t pos = 0;
    cmdline_a[0] = '\0';
    for (int i = 0; i < nargs && pos + 8 < sizeof(cmdline_a); i++) {
        const char *a = rel_args[i];
        int need_q = 1; /* always quote for safety with special chars */
        if (i > 0) {
            cmdline_a[pos++] = ' ';
            cmdline_a[pos] = '\0';
        }
        if (need_q)
            pos += (size_t)snprintf(cmdline_a + pos, sizeof(cmdline_a) - pos, "\"%s\"", a);
        else
            pos += (size_t)snprintf(cmdline_a + pos, sizeof(cmdline_a) - pos, "%s", a);
    }

    wchar_t wcmd[PATH_BUF * 4];
    wchar_t wcwd[PATH_BUF];
    wchar_t wexe[PATH_BUF];
    MultiByteToWideChar(CP_UTF8, 0, cmdline_a, -1, wcmd, (int)(sizeof(wcmd) / sizeof(wcmd[0])));
    MultiByteToWideChar(CP_UTF8, 0, cwd, -1, wcwd, (int)(sizeof(wcwd) / sizeof(wcwd[0])));
    /* App name: relative exe under cwd, or full if strip failed */
    MultiByteToWideChar(CP_UTF8, 0, rel_exe, -1, wexe, (int)(sizeof(wexe) / sizeof(wexe[0])));

    /* Explorer-like launch (same survival as PowerShell Start-Process).
     * Parameters = args after the exe. */
    wchar_t wparams[PATH_BUF * 4];
    wparams[0] = 0;
    {
        char rest[PATH_BUF * 4];
        rest[0] = 0;
        size_t rp = 0;
        for (int i = 1; i < nargs && rp + 8 < sizeof(rest); i++) {
            if (i > 1) rest[rp++] = ' ';
            rp += (size_t)snprintf(rest + rp, sizeof(rest) - rp, "\"%s\"", rel_args[i]);
        }
        rest[rp] = 0;
        MultiByteToWideChar(CP_UTF8, 0, rest, -1, wparams, (int)(sizeof(wparams) / sizeof(wparams[0])));
    }
    MultiByteToWideChar(CP_UTF8, 0, rel_exe, -1, wexe, (int)(sizeof(wexe) / sizeof(wexe[0])));

    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = wexe;
    sei.lpParameters = wparams[0] ? wparams : NULL;
    sei.lpDirectory = wcwd;
    sei.nShow = SW_SHOWNOACTIVATE;
    BOOL ok = ShellExecuteExW(&sei);
    if (!ok || (INT_PTR)sei.hInstApp <= 32) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, 0, NULL, wcwd, &si, &pi);
        if (ok) {
            if (pi.hThread) CloseHandle(pi.hThread);
            if (pi.hProcess) CloseHandle(pi.hProcess);
        } else {
            fprintf(stderr, "crypt_autostart: spawn failed (%lu) exe=%s cmdline=%s cwd=%s\n",
                    (unsigned long)GetLastError(), exe, cmdline_a, cwd);
            return -1;
        }
    } else if (sei.hProcess) {
        WaitForInputIdle(sei.hProcess, 1500);
        CloseHandle(sei.hProcess);
    }
    Sleep(200);
    return 0;
}
#endif

static void quit_current_livedesk(const char *house_root) {
    char open_path[PATH_BUF];
    char claims_path[PATH_BUF];
    char tbar_pid_path[PATH_BUF];
    join_path(open_path, sizeof(open_path), house_root, "#.desktop/livedesk_open.txt");
    join_path(claims_path, sizeof(claims_path), house_root, "#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt");
    join_path(tbar_pid_path, sizeof(tbar_pid_path), house_root, "#.desktop/livedesk_taskbar.pid");
    path_norm_slashes(open_path);
    path_norm_slashes(claims_path);
    path_norm_slashes(tbar_pid_path);

    printf("crypt_autostart: quitting current livedesk before re-launch\n");

    /* 1) Graceful CLOSE to every registered entity package. */
    FILE *of = fopen(open_path, "r");
    if (of) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), of)) {
            char *pp = strstr(line, "PATH=");
            if (pp) {
                char path_raw[PATH_BUF];
                char path[PATH_BUF];
                snprintf(path_raw, sizeof(path_raw), "%s", pp + 5);
                path_raw[strcspn(path_raw, "\r\n")] = '\0';
                resolve_arg(house_root, path_raw, path, sizeof(path));
                path_norm_slashes(path);
                char relay[PATH_BUF];
                join_path(relay, sizeof(relay), path, "interact_relay.txt");
                path_norm_slashes(relay);
                FILE *rf = fopen(relay, "w");
                if (rf) {
                    fprintf(rf, "CLOSE\n");
                    fclose(rf);
                }
                printf("crypt_autostart: CLOSE -> %s\n", path);
            }
        }
        fclose(of);
    }

    /* 2) Taskbar pid file. */
    {
        FILE *pf = fopen(tbar_pid_path, "r");
        if (pf) {
            int tpid = 0;
            if (fscanf(pf, "%d", &tpid) == 1 && tpid > 1) {
#ifdef _WIN32
                if (pid_alive_win((DWORD)tpid)) {
                    kill_pid_win((DWORD)tpid);
                    printf("crypt_autostart: TERMINATE taskbar pid=%d\n", tpid);
                }
#else
                if (pid_alive((pid_t)tpid)) {
                    kill((pid_t)tpid, SIGTERM);
                    printf("crypt_autostart: SIGTERM taskbar pid=%d\n", tpid);
                }
#endif
            }
            fclose(pf);
        }
    }

#ifdef _WIN32
    Sleep(400);
#else
    usleep(400000);
#endif

    /* 3) SIGTERM / Terminate entity PIDs from open registry. */
    of = fopen(open_path, "r");
    if (of) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), of)) {
            char *pidp = strstr(line, "PID=");
            if (!pidp) continue;
            int pid = atoi(pidp + 4);
#ifdef _WIN32
            if (pid_alive_win((DWORD)pid)) {
                kill_pid_win((DWORD)pid);
                printf("crypt_autostart: TERMINATE entity pid=%d\n", pid);
            }
#else
            if (pid_alive((pid_t)pid)) {
                kill((pid_t)pid, SIGTERM);
                printf("crypt_autostart: SIGTERM entity pid=%d\n", pid);
            }
#endif
        }
        fclose(of);
    }

    /* 4) Sweep by process name (Windows: taskkill; Linux: /proc cmdline). */
#ifdef _WIN32
    /* Real update (2026-08-11): legacy tp_taskbar.c retired (archived,
     * originals deleted — see LEGACY-ARCHIVE-20260811.zip under the
     * livedesk-taskbar ops dir). khtpm_strip_parser.+x is the real taskbar now
     * — swept by name too, so a restart correctly clears a running khtpm
     * instance before relaunching (the exact class of "2 taskbars open"
     * bug this sweep exists to prevent, now for khtpm instead of
     * legacy). tp_taskbar names kept in the sweep as a harmless no-op
     * safety net in case a stray pre-retirement binary is still running
     * somewhere. */
    kill_by_name_win("tp_taskbar");
    kill_by_name_win("tp_taskbar.+x");
    kill_by_name_win("khtpm_strip_parser");
    kill_by_name_win("khtpm_strip_parser.+x");
    kill_by_name_win("khtpm_taskbar_manager_main");
    kill_by_name_win("khtpm_taskbar_manager_main.+x");
    kill_by_name_win("tp_desktop_window");
    kill_by_name_win("tp_desktop_window.+x");
    kill_by_name_win("tp_desktop_window_rgb");
    kill_by_name_win("tp_desktop_window_rgb.+x");
    Sleep(200);
#else
    {
        DIR *d = opendir("/proc");
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (!isdigit((unsigned char)ent->d_name[0])) continue;
                pid_t pid = (pid_t)atoi(ent->d_name);
                if (pid <= 1 || pid == getpid()) continue;
                char cpath[64];
                snprintf(cpath, sizeof(cpath), "/proc/%d/cmdline", (int)pid);
                FILE *cf = fopen(cpath, "r");
                if (!cf) continue;
                char cmd[PATH_BUF * 2];
                size_t n = fread(cmd, 1, sizeof(cmd) - 1, cf);
                fclose(cf);
                if (n == 0) continue;
                cmd[n] = '\0';
                for (size_t i = 0; i < n; i++) if (cmd[i] == '\0') cmd[i] = ' ';
                /* Real update (2026-08-11): legacy retired, khtpm_strip_
                 * parser.+x is the real taskbar now — matched here too so
                 * a restart correctly clears a running khtpm instance
                 * (tp_taskbar kept as a harmless safety net). */
                int is_tb = strstr(cmd, "tp_taskbar") != NULL
                         || strstr(cmd, "khtpm_strip_parser") != NULL
                         || strstr(cmd, "khtpm_taskbar_manager_main") != NULL;
                int is_dw = strstr(cmd, "tp_desktop_window") != NULL;
                if (!is_tb && !is_dw) continue;
                if (!strstr(cmd, house_root)) continue;
                if (pid_alive(pid)) {
                    kill(pid, SIGTERM);
                    printf("crypt_autostart: SIGTERM sweep pid=%d (%s)\n",
                           (int)pid, is_tb ? "taskbar" : "desktop_window");
                }
            }
            closedir(d);
        }
    }
    usleep(200000);
#endif

    /* 5) Clear registries */
    {
        FILE *wf = fopen(open_path, "w");
        if (wf) fclose(wf);
        wf = fopen(claims_path, "w");
        if (wf) fclose(wf);
#ifdef _WIN32
        _unlink(tbar_pid_path);
#else
        unlink(tbar_pid_path);
#endif
    }
    printf("crypt_autostart: livedesk quit complete\n");
}

static int launch_one(const char *house_root, const char *label, const char *cmd_raw) {
    char args[MAX_ARGS][PATH_BUF];
    int n = tokenize_cmd(cmd_raw, args, MAX_ARGS);
    if (n < 1) {
        fprintf(stderr, "crypt_autostart: empty LAUNCH for '%s'\n", label);
        return -1;
    }

    char exe[PATH_BUF];
    int found = resolve_binary(house_root, args[0], exe, sizeof(exe));
    if (!found) {
        fprintf(stderr, "crypt_autostart: binary missing for '%s': %s\n", label, exe);
        /* still try — CreateProcess may find it via PATH in odd cases */
    }

    /* Resolve remaining args against house_root */
    char resolved[MAX_ARGS][PATH_BUF];
    snprintf(resolved[0], sizeof(resolved[0]), "%s", exe);
    for (int i = 1; i < n; i++) {
        resolve_arg(house_root, args[i], resolved[i], sizeof(resolved[i]));
        path_norm_slashes(resolved[i]);
    }

    printf("crypt_autostart: launch '%s' -> %s", label, exe);
    for (int i = 1; i < n; i++) printf(" [%s]", resolved[i]);
    printf("\n");

#ifdef _WIN32
    return launch_detached_win(exe, resolved, n, house_root);
#else
    {
        /* Rebuild quoted command for setsid nohup */
        char cmd[MAX_LINE * 2];
        size_t pos = 0;
        for (int i = 0; i < n && pos + 8 < sizeof(cmd); i++) {
            if (i) cmd[pos++] = ' ';
            pos += (size_t)snprintf(cmd + pos, sizeof(cmd) - pos, "'%s'", resolved[i]);
        }
        char full[MAX_LINE * 2 + 64];
        snprintf(full, sizeof(full), "setsid nohup %s >/dev/null 2>&1 &", cmd);
        int rc = system(full);
        return rc;
    }
#endif
}

int main(int argc, char **argv) {
    char pdl_path[PATH_BUF];
    if (argc > 1) snprintf(pdl_path, sizeof(pdl_path), "%s", argv[1]);
    else resolve_default_pdl(pdl_path, sizeof(pdl_path));
    path_norm_slashes(pdl_path);

    /* When PDL is relative, caller (button.ps1) already chdir'd to house.
     * Keep house_root as "." so all joins stay relative (emoji-safe). */
    int pdl_was_relative = !is_abs_path(pdl_path);

    FILE *f = fopen(pdl_path, "r");
    if (!f) {
        fprintf(stderr, "crypt_autostart: cannot open %s\n", pdl_path);
        return 1;
    }

    int enabled = 1;
    char mount_uuid[128] = "";
    char launch_cmds[MAX_LAUNCH][MAX_LINE];
    char launch_names[MAX_LAUNCH][64];
    int n_launch = 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) == 0 && strstr(line, "enabled")) {
            char *v = strrchr(line, '|');
            if (v) enabled = atoi(v + 1);
        } else if (strncmp(line, "MOUNT", 5) == 0 && strstr(line, "uuid")) {
            char *p = strchr(line, '|');
            p = p ? strchr(p + 1, '|') : NULL;
            if (p) {
                p++;
                while (*p == ' ') p++;
                snprintf(mount_uuid, sizeof(mount_uuid), "%s", p);
                mount_uuid[strcspn(mount_uuid, "\r\n")] = '\0';
            }
        } else if (strncmp(line, "MOUNT", 5) == 0 && strstr(line, "mountpoint")) {
            char mp[PATH_BUF];
            char *p = strchr(line, '|');
            p = p ? strchr(p + 1, '|') : NULL;
            if (p && mount_uuid[0]) {
                p++;
                while (*p == ' ') p++;
                snprintf(mp, sizeof(mp), "%s", p);
                mp[strcspn(mp, "\r\n")] = '\0';
#ifdef _WIN32
                printf("crypt_autostart: MOUNT skipped on Windows (uuid=%s mp=%s)\n",
                       mount_uuid, mp);
#else
                if (!is_mounted(mp)) {
                    char cmd[PATH_BUF * 2];
                    snprintf(cmd, sizeof(cmd),
                             "udisksctl mount -b /dev/disk/by-uuid/'%s' >/dev/null 2>&1",
                             mount_uuid);
                    int rc = system(cmd);
                    printf("crypt_autostart: mount UUID=%s -> %s (rc=%d)\n",
                           mount_uuid, mp, rc);
                } else {
                    printf("crypt_autostart: %s already mounted, skipping\n", mp);
                }
#endif
                mount_uuid[0] = '\0';
            }
        } else if (strncmp(line, "LAUNCH", 6) == 0 && n_launch < MAX_LAUNCH) {
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = strchr(p, '|');
            if (!end) continue;
            char *label_end = end;
            while (label_end > p && label_end[-1] == ' ') label_end--;
            size_t nlen = (size_t)(label_end - p);
            if (nlen >= sizeof(launch_names[0])) nlen = sizeof(launch_names[0]) - 1;
            memcpy(launch_names[n_launch], p, nlen);
            launch_names[n_launch][nlen] = '\0';

            char *v = end + 1;
            while (*v == ' ') v++;
            v[strcspn(v, "\r\n")] = '\0';
            char *v_end = v + strlen(v);
            while (v_end > v && v_end[-1] == ' ') v_end--;
            *v_end = '\0';
            snprintf(launch_cmds[n_launch], sizeof(launch_cmds[0]), "%s", v);
            n_launch++;
        }
    }
    fclose(f);

    if (!enabled) {
        printf("crypt_autostart: disabled (STATE|enabled|0) - no-op\n");
        return 0;
    }

    char house_root[PATH_BUF];
    if (pdl_was_relative) {
        /* CWD is house_root (button.ps1 / button.sh convention) */
        snprintf(house_root, sizeof(house_root), ".");
    } else {
        resolve_house_root_from_pdl(pdl_path, house_root, sizeof(house_root));
        path_norm_slashes(house_root);
    }
    printf("crypt_autostart: house_root=%s pdl=%s\n", house_root, pdl_path);

    if (house_root[0]) quit_current_livedesk(house_root);

#ifdef _WIN32
    {
        wchar_t ps1[PATH_BUF], psexe[PATH_BUF], wcmd[PATH_BUF * 2], wcwd[PATH_BUF];
        MultiByteToWideChar(CP_UTF8, 0, house_root, -1, wcwd, PATH_BUF);
        _snwprintf(ps1, PATH_BUF, L"%s\\$.crypts\\win-start-livedesk.ps1", wcwd);
        if (ps1[0] == L'.' && (ps1[1] == L'\\' || ps1[1] == L'\0'))
            _snwprintf(ps1, PATH_BUF, L".\\$.crypts\\win-start-livedesk.ps1");
        wchar_t *root = _wgetenv(L"SystemRoot");
        _snwprintf(psexe, PATH_BUF, L"%s\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                   root ? root : L"C:\\Windows");
        _snwprintf(wcmd, PATH_BUF * 2 - 1,
                   L"\"%s\" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\"",
                   psexe, ps1);
        STARTUPINFOW si; PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        DWORD fl = CREATE_BREAKAWAY_FROM_JOB | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;
        if (CreateProcessW(psexe, wcmd, NULL, NULL, FALSE, fl, NULL, wcwd, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 20000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        return 0;
    }
#endif

    for (int i = 0; i < n_launch; i++) {
        int rc = launch_one(house_root, launch_names[i], launch_cmds[i]);
        printf("crypt_autostart: launch '%s' done (rc=%d)\n", launch_names[i], rc);
    }
#ifdef _WIN32
    /* Stay alive until rgb/strip have mapped windows (PS Start-Process
     * does not exit the parent console immediately in the same way). */
    Sleep(800);
#endif

    return 0;
}
