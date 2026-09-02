/* livedesk-start-button — clickable desktop starter for livedesk.
 *
 * Linux: walks up from /proc/self/exe to the house, optional strip rebuild,
 * then execs `sh $.crypts/button.sh run`. Unchanged POSIX path.
 *
 * Windows: walks up from this .exe (works when a shortcut points at the
 * house copy). If this file was copied to the Desktop, reads sibling
 * livedesk-house-root.txt (UTF-8 path written by button.ps1 install-desktop).
 * Then launches button.ps1 run with CREATE_BREAKAWAY_FROM_JOB so the
 * strip stays up after this starter and PowerShell exit.
 */
#ifndef _WIN32
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <glob.h>

int main(void) {
    /* Find the house root by walking up from this binary's own real
     * install dir until a directory holding BOTH #.desktop/ and
     * &.widgits/ is found - same marker-walk khtpm_vars.sh uses, so no
     * hardcoded NNEST-* / yz.muchiverse paths (the old button_launcher
     * globbed for them and broke on any relocation). */
    char self_path[PATH_MAX];
    ssize_t slen = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (slen <= 0) {
        fprintf(stderr, "Could not resolve own path\n");
        return 1;
    }
    self_path[slen] = '\0';

    char step[PATH_MAX];
    snprintf(step, sizeof(step), "%s", self_path);
    char house_root[PATH_MAX];
    house_root[0] = '\0';
    for (;;) {
        char *slash = strrchr(step, '/');
        if (!slash || slash == step) break; /* reached /, give up */
        *slash = '\0';
        char desk[PATH_MAX], widg[PATH_MAX];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
            snprintf(house_root, sizeof(house_root), "%s", step);
            break;
        }
    }
    if (house_root[0] == '\0') {
        fprintf(stderr, "Could not find house root\n");
        return 1;
    }

    char button_sh[PATH_MAX];
    snprintf(button_sh, sizeof(button_sh), "%s/$.crypts/button.sh", house_root);
    if (access(button_sh, F_OK) != 0) {
        fprintf(stderr, "Could not find button.sh\n");
        return 1;
    }

    char build_dir[PATH_MAX];
    snprintf(build_dir, sizeof(build_dir), "%s/*.monads/*.livedesk-taskbar/ops", house_root);

    char build_cmd[PATH_MAX * 2];
    snprintf(build_cmd, sizeof(build_cmd), "cd '%s' && bash build_khtpm_strip.sh >/dev/null 2>&1", build_dir);
    system(build_cmd);

    char *args[] = { "sh", button_sh, "run", NULL };
    execvp("sh", args);
    return 1;
}

#else /* _WIN32 */

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define SP 4096

static int is_house_dir(const wchar_t *dir) {
    wchar_t a[SP], b[SP];
    _snwprintf(a, SP, L"%s\\#.desktop", dir);
    _snwprintf(b, SP, L"%s\\&.widgits", dir);
    a[SP - 1] = 0; b[SP - 1] = 0;
    return GetFileAttributesW(a) != INVALID_FILE_ATTRIBUTES
        && GetFileAttributesW(b) != INVALID_FILE_ATTRIBUTES;
}

static int walk_up_to_house(wchar_t *start, wchar_t *out, int n) {
    wchar_t step[SP];
    wcsncpy(step, start, SP - 1); step[SP - 1] = 0;
    for (;;) {
        wchar_t *slash = wcsrchr(step, L'\\');
        if (!slash || slash == step) break;
        *slash = 0;
        if (is_house_dir(step)) {
            wcsncpy(out, step, n - 1); out[n - 1] = 0;
            return 1;
        }
    }
    return 0;
}

static int read_sidecar_house(const wchar_t *exe_path, wchar_t *out, int n) {
    wchar_t side[SP];
    wcsncpy(side, exe_path, SP - 1); side[SP - 1] = 0;
    wchar_t *slash = wcsrchr(side, L'\\');
    if (!slash) return 0;
    wcscpy(slash + 1, L"livedesk-house-root.txt");
    FILE *f = _wfopen(side, L"rb");
    if (!f) return 0;
    char line[SP];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);
    size_t L = strlen(line);
    while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
    if (L >= 3 && (unsigned char)line[0] == 0xEF &&
        (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
        memmove(line, line + 3, L - 2);
    }
    if (!MultiByteToWideChar(CP_UTF8, 0, line, -1, out, n)) return 0;
    return is_house_dir(out);
}

static int find_house(wchar_t *out, int n) {
    wchar_t self[SP];
    DWORD got = GetModuleFileNameW(NULL, self, SP);
    if (got == 0 || got >= SP) return 0;
    if (walk_up_to_house(self, out, n)) return 1;
    if (read_sidecar_house(self, out, n)) return 1;
    return 0;
}

static DWORD spawn_flags(void) {
    /* crypt_autostart is -mwindows. CREATE_NO_WINDOW+SW_HIDE here was
     * inherited by pal/strip children so a user double-click showed nothing. */
    return CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB;
}

int main(void) {
    wchar_t house[SP];
    if (!find_house(house, SP)) {
        MessageBoxW(NULL,
            L"Could not find the house (#.desktop + &.widgits).\n"
            L"Put livedesk-house-root.txt next to this exe, or run from the house tree.",
            L"livedesk-start-button", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!SetCurrentDirectoryW(house)) {
        MessageBoxW(NULL, L"Could not chdir to house root.", L"livedesk-start-button",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    /* Win: PowerShell Start-Process keeps pals alive. C CreateProcess did not. */
    wchar_t ps1[SP], psexe[SP], cmd[SP];
    _snwprintf(ps1, SP, L"%s\\$.crypts\\win-start-livedesk.ps1", house);
    ps1[SP - 1] = 0;
    if (GetFileAttributesW(ps1) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"Missing $.crypts\\win-start-livedesk.ps1",
                    L"livedesk-start-button", MB_OK | MB_ICONERROR);
        return 1;
    }
    {
        wchar_t *root = _wgetenv(L"SystemRoot");
        _snwprintf(psexe, SP, L"%s\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                   root ? root : L"C:\\Windows");
        psexe[SP - 1] = 0;
    }
    _snwprintf(cmd, SP,
               L"\"%s\" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\"",
               psexe, ps1);
    cmd[SP - 1] = 0;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    DWORD flags = CREATE_BREAKAWAY_FROM_JOB | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;
    BOOL ok = CreateProcessW(psexe, cmd, NULL, NULL, FALSE, flags, NULL, house, &si, &pi);
    if (!ok) {
        ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, house, &si, &pi);
    }
    if (!ok) {
        wchar_t msg[256];
        _snwprintf(msg, 256, L"CreateProcessW failed (%lu)", (unsigned long)GetLastError());
        MessageBoxW(NULL, msg, L"livedesk-start-button", MB_OK | MB_ICONERROR);
        return 1;
    }
    /* Wait for crypt_autostart to finish spawning pals (it sleeps ~2.5s). */
    WaitForSingleObject(pi.hProcess, 20000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

#endif
