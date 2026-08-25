/* LEGACY: do not add design logic here. Shared = khtpm_taskbar_core.c (+ plat_win/x11). See KHTPM-ARCH.txt */
/* tp_desktop_window_win.c â€” Win32 + WGL KHTPM entity window (L2).
 * Usage: tp_desktop_window <package_dir>
 *
 * Loads package sprite.csv (portraits / ranch extracts) as GL texture,
 * alpha-shaped via SetWindowRgn (egg_window pattern). RMB opens menu
 * from objects.pdl (main page) and meta.pdl METHOD rows.
 *
 * Build: gcc -O2 -o tp_desktop_window.exe tp_desktop_window_win.c -lopengl32 -lgdi32 -luser32
 */
#ifndef _WIN32
#error "Windows-only; Linux uses tp_desktop_window.c"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <io.h>
#include <direct.h>
#include <process.h>

#define PATH_BUF 4352
#define POLL_MS 300
#define DEFAULT_WIN_PX 64
#define GRID_CELL_PX 80
#define MAX_MENU 48

static char g_package[PATH_BUF];
static char g_house[PATH_BUF];
static char g_history[PATH_BUF];
static char g_relay[PATH_BUF];
static char g_glyph[256] = "?";
static char g_entity[128] = "entity";
static int g_win_px = DEFAULT_WIN_PX;
static int g_index = 0;
static int g_dragging = 0;
static int g_drag_dx = 0, g_drag_dy = 0;
static HWND g_hwnd = NULL;
static HDC g_hdc = NULL;
static HGLRC g_glrc = NULL;

static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;
static GLuint g_texture = 0;
static int g_has_texture = 0;

typedef struct { char label[128]; char action[PATH_BUF]; } MenuItem;
static MenuItem g_menu[MAX_MENU];
static int g_nmenu = 0;
static char g_menu_page[64] = "main"; /* objects.pdl multi-page (GOTO/BACK) */

/* ---- path / file helpers ---- */
static void join2(char *out, size_t n, const char *a, const char *b) {
    size_t al = strlen(a);
    if (strcmp(a, ".") == 0) snprintf(out, n, "%s", b);
    else if (al && (a[al - 1] == '/' || a[al - 1] == '\\'))
        snprintf(out, n, "%s%s", a, b);
    else
        snprintf(out, n, "%s\\%s", a, b);
    for (char *p = out; *p; p++) if (*p == '/') *p = '\\';
}

static int path_exists_u8(const char *utf8) {
    if (!utf8 || !utf8[0]) return 0;
    wchar_t w[PATH_BUF];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, utf8, -1, w, PATH_BUF))
        return _access(utf8, 0) == 0;
    return GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES;
}

static FILE *fopen_u8(const char *utf8, const char *mode) {
    wchar_t wp[PATH_BUF], wm[32];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wp, PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, utf8, -1, wp, PATH_BUF))
        return fopen(utf8, mode);
    MultiByteToWideChar(CP_ACP, 0, mode, -1, wm, 32);
    if (!wm[0]) MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 32);
    return _wfopen(wp, wm);
}

static void dirname_of(const char *in, char *out, size_t n) {
    snprintf(out, n, "%s", in);
    size_t L = strlen(out);
    while (L > 1 && (out[L - 1] == '\\' || out[L - 1] == '/')) out[--L] = 0;
    char *s = strrchr(out, '\\');
    char *s2 = strrchr(out, '/');
    if (s2 && (!s || s2 > s)) s = s2;
    if (!s) { snprintf(out, n, "."); return; }
    *s = 0;
}

static void append_history(const char *msg) {
    FILE *f = fopen_u8(g_history, "a");
    if (!f) return;
    fprintf(f, "%ld %s\n", (long)time(NULL), msg);
    fclose(f);
}

static void read_text_line(const char *path, char *out, size_t n) {
    out[0] = 0;
    FILE *f = fopen_u8(path, "r");
    if (!f) return;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = 0;
    fclose(f);
}

/* ---- sprite ---- */
static int load_sprite_csv(const char *csv_path) {
    FILE *f = fopen_u8(csv_path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }
    unsigned char *pixels = (unsigned char *)malloc((size_t)res * res * 4);
    if (!pixels) { fclose(f); return 0; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixels[count * 4 + 0] = (unsigned char)r;
            pixels[count * 4 + 1] = (unsigned char)g;
            pixels[count * 4 + 2] = (unsigned char)b;
            pixels[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return 0; }
    if (g_sprite_pixels) free(g_sprite_pixels);
    g_sprite_pixels = pixels;
    g_sprite_res = res;
    return 1;
}

static void upload_texture(void) {
    if (!g_sprite_pixels) return;
    if (g_texture) glDeleteTextures(1, &g_texture);
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_sprite_res, g_sprite_res, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_sprite_pixels);
    g_has_texture = 1;
}

static void build_shape_mask(HWND hwnd) {
    int W = g_win_px, H = g_win_px;
    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (g_sprite_pixels && g_sprite_res > 0) {
        for (int y = 0; y < H; y++) {
            int sy = (y * g_sprite_res) / H;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            int x = 0;
            while (x < W) {
                int sx = (x * g_sprite_res) / W;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] <= 127) { x++; continue; }
                int run = x;
                while (x < W) {
                    sx = (x * g_sprite_res) / W;
                    if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                    if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] <= 127) break;
                    x++;
                }
                HRGN r = CreateRectRgn(run, y, x, y + 1);
                CombineRgn(region, region, r, RGN_OR);
                DeleteObject(r);
            }
        }
    } else {
        HRGN box = CreateRectRgn(0, 0, W, H);
        CombineRgn(region, region, box, RGN_OR);
        DeleteObject(box);
    }
    if (!SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
}

static void render_frame(void) {
    if (!g_hdc) return;
    glViewport(0, 0, g_win_px, g_win_px);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (g_has_texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_texture);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, -1);
        glTexCoord2f(1, 0); glVertex2f(1, 1);
        glTexCoord2f(0, 0); glVertex2f(-1, 1);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        /* fallback blue square if no sprite */
        glBegin(GL_QUADS);
        glColor3f(0.15f, 0.35f, 0.55f);
        glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
        glEnd();
    }
    SwapBuffers(g_hdc);
}

/* ---- livedesk registry ---- */
static int next_index(void) {
    char path[PATH_BUF];
    join2(path, sizeof(path), g_house, "#.desktop\\livedesk_next_index.txt");
    int idx = 1;
    FILE *f = fopen_u8(path, "r");
    if (f) { if (fscanf(f, "%d", &idx) != 1) idx = 1; fclose(f); }
    f = fopen_u8(path, "w");
    if (f) { fprintf(f, "%d\n", idx + 1); fclose(f); }
    return idx;
}

static void register_open(void) {
    char path[PATH_BUF];
    join2(path, sizeof(path), g_house, "#.desktop\\livedesk_open.txt");
    FILE *f = fopen_u8(path, "a");
    if (f) {
        fprintf(f, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n",
                (int)GetCurrentProcessId(), g_index, g_entity, g_package);
        fclose(f);
    }
    join2(path, sizeof(path), g_house, "#.desktop\\livedesk_nav_claims.txt");
    f = fopen_u8(path, "a");
    if (f) {
        fprintf(f, "KIND=tab|PID=%d|NAV=%d|ENTITY=%s|PATH=%s\n",
                (int)GetCurrentProcessId(), g_index, g_entity, g_package);
        fclose(f);
    }
}

static void unregister_open(void) {
    char paths[2][PATH_BUF];
    join2(paths[0], sizeof(paths[0]), g_house, "#.desktop\\livedesk_open.txt");
    join2(paths[1], sizeof(paths[1]), g_house, "#.desktop\\livedesk_nav_claims.txt");
    char needle[64];
    snprintf(needle, sizeof(needle), "PID=%d", (int)GetCurrentProcessId());
    for (int pi = 0; pi < 2; pi++) {
        FILE *f = fopen_u8(paths[pi], "r");
        if (!f) continue;
        char tmp[PATH_BUF];
        snprintf(tmp, sizeof(tmp), "%s.tmp", paths[pi]);
        FILE *o = fopen_u8(tmp, "w");
        if (!o) { fclose(f); continue; }
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, needle)) continue;
            fputs(line, o);
        }
        fclose(f); fclose(o);
        remove(paths[pi]);
        rename(tmp, paths[pi]);
    }
}

static int pid_alive(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    DWORD code = 0;
    int ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return ok;
}

static void ensure_taskbar(void) {
    char pidpath[PATH_BUF];
    join2(pidpath, sizeof(pidpath), g_house, "#.desktop\\livedesk_taskbar.pid");
    FILE *f = fopen_u8(pidpath, "r");
    if (f) {
        int tpid = 0;
        if (fscanf(f, "%d", &tpid) == 1 && tpid > 1 && pid_alive((DWORD)tpid)) {
            fclose(f); return;
        }
        fclose(f);
    }
    char exe[PATH_BUF];
    /* Real fix, 2026-08-11, found while retiring legacy tp_taskbar.c
     * house-wide: TWO stale bugs on this one line — a pre-monad-
     * relocation path (&.widgits\livedesk-taskbar\... instead of
     * *.monads\*.livedesk-taskbar\...) AND a reference to tp_taskbar.exe
     * itself, which no longer exists at all (legacy retired, archived to
     * *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip).
     * Updated to khtpm's own binary name for consistency with the Linux
     * ensure_taskbar_running() fix — NOT independently build-tested on
     * Windows this session (no Windows build environment available
     * here), so verify this path once an actual khtpm Windows build
     * exists. */
    join2(exe, sizeof(exe), g_house, "*.monads\\*.livedesk-taskbar\\ops\\+x\\khtpm_strip_parser.exe");
    if (!path_exists_u8(exe)) return;
    char cmdline[PATH_BUF * 2];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" \".\"", exe);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                       CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        append_history("ensure_taskbar: launched");
    }
}

/* Clamp to primary work area â€” Linux sessions often leave y>1080 / x>1920
 * which is off-screen on this Win box (e.g. 1536x864). */
static void clamp_screen(int *x, int *y) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int max_x = sw - g_win_px - 8;
    int max_y = sh - g_win_px - 40; /* leave room for taskbar */
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x > max_x) *x = max_x;
    if (*y > max_y) *y = max_y;
}

static void write_pos(int x, int y) {
    if (x < 0) x = 0; if (y < 0) y = 0;
    x = (x / GRID_CELL_PX) * GRID_CELL_PX;
    y = (y / GRID_CELL_PX) * GRID_CELL_PX;
    clamp_screen(&x, &y);
    char path[PATH_BUF];
    join2(path, sizeof(path), g_package, "desktop_pos.txt");
    FILE *f = fopen_u8(path, "w");
    if (f) { fprintf(f, "x=%d\ny=%d\n", x, y); fclose(f); }
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

static int read_pos(int *x, int *y) {
    char path[PATH_BUF];
    join2(path, sizeof(path), g_package, "desktop_pos.txt");
    FILE *f = fopen_u8(path, "r");
    if (!f) return 0;
    char line[128];
    *x = 100; *y = 100;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) *x = atoi(line + 2);
        if (strncmp(line, "y=", 2) == 0) *y = atoi(line + 2);
    }
    fclose(f);
    clamp_screen(x, y);
    return 1;
}

/* ---- menu from meta.pdl / objects.pdl ---- */
static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == '\t'))
        s[--n] = 0;
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static void menu_add(const char *label, const char *action) {
    if (g_nmenu >= MAX_MENU || !label || !label[0]) return;
    snprintf(g_menu[g_nmenu].label, sizeof(g_menu[0].label), "%s", label);
    snprintf(g_menu[g_nmenu].action, sizeof(g_menu[0].action), "%s", action ? action : "void");
    g_nmenu++;
}

static void load_menu(void) {
    g_nmenu = 0;
    char path[PATH_BUF];
    int on_page = 0;
    int have_objects = 0;

    /* objects.pdl multi-page: load current g_menu_page */
    join2(path, sizeof(path), g_package, "objects.pdl");
    FILE *f = fopen_u8(path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PAGE", 4) == 0) {
                char *p = strchr(line, '|');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char pname[64];
                    snprintf(pname, sizeof(pname), "%s", p);
                    pname[strcspn(pname, "\r\n")] = 0;
                    trim(pname);
                    on_page = (strcmp(pname, g_menu_page) == 0);
                }
                continue;
            }
            if (!on_page) continue;
            if (strncmp(line, "OBJECT", 6) != 0) continue;
            char *lab = strstr(line, "label=");
            char *act = strstr(line, "action=");
            if (!lab) continue;
            lab += 6;
            char label[128], action[PATH_BUF];
            if (act) {
                size_t llen = (size_t)(act - lab);
                while (llen && (lab[llen - 1] == ' ' || lab[llen - 1] == '|')) llen--;
                if (llen >= sizeof(label)) llen = sizeof(label) - 1;
                memcpy(label, lab, llen); label[llen] = 0;
                trim(label);
                snprintf(action, sizeof(action), "%s", act + 7);
                trim(action);
            } else {
                snprintf(label, sizeof(label), "%s", lab);
                trim(label);
                snprintf(action, sizeof(action), "void");
            }
            menu_add(label, action);
            have_objects = 1;
        }
        fclose(f);
    }

    /* meta.pdl METHOD rows on main page only (or if no objects.pdl) */
    if (!have_objects || strcmp(g_menu_page, "main") == 0) {
        join2(path, sizeof(path), g_package, "meta.pdl");
        f = fopen_u8(path, "r");
        if (f) {
            char line[PATH_BUF];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "METHOD", 6) != 0) continue;
                char *p1 = strchr(line, '|');
                if (!p1) continue;
                p1++;
                while (*p1 == ' ') p1++;
                char *p2 = strchr(p1, '|');
                if (!p2) continue;
                char label[128];
                size_t llen = (size_t)(p2 - p1);
                while (llen && p1[llen - 1] == ' ') llen--;
                if (llen >= sizeof(label)) llen = sizeof(label) - 1;
                memcpy(label, p1, llen); label[llen] = 0;
                char *act = p2 + 1;
                while (*act == ' ') act++;
                act[strcspn(act, "\r\n")] = 0;
                trim(act);
                int dup = 0;
                for (int i = 0; i < g_nmenu; i++)
                    if (strcmp(g_menu[i].label, label) == 0) { dup = 1; break; }
                if (!dup) menu_add(label, act);
            }
            fclose(f);
        }
    }
    if (g_nmenu == 0) menu_add("Close", "CLOSE");
}

/* forward */
static void show_context_menu(void);

static void run_action(const char *action) {
    if (!action || !action[0] || strcmp(action, "void") == 0) return;
    if (strcmp(action, "CLOSE") == 0) {
        append_history("menu CLOSE");
        DestroyWindow(g_hwnd);
        return;
    }
    if (strncmp(action, "GOTO:", 5) == 0) {
        snprintf(g_menu_page, sizeof(g_menu_page), "%s", action + 5);
        trim(g_menu_page);
        append_history("menu GOTO");
        show_context_menu(); /* re-open page */
        return;
    }
    if (strncmp(action, "BACK", 4) == 0) {
        snprintf(g_menu_page, sizeof(g_menu_page), "main");
        append_history("menu BACK");
        show_context_menu();
        return;
    }
    if (strncmp(action, "STATE:", 6) == 0) {
        append_history("menu STATE deferred");
        return;
    }
    /* strip leading absolute linux house if present â€” keep relative tail */
    const char *cmd = action;
    const char *markers[] = { "/@.apps/", "/#.desktop/", "/&.widgits/", "/$.crypts/", NULL };
    for (int i = 0; markers[i]; i++) {
        const char *m = strstr(action, markers[i]);
        if (m) { cmd = m + 1; break; }
    }
    char full[PATH_BUF * 2];
    snprintf(full, sizeof(full), "%s", cmd);
    append_history(full);
    char cmdline[PATH_BUF * 2];
    if (strstr(full, ".ps1")) {
        snprintf(cmdline, sizeof(cmdline),
                 "powershell -ExecutionPolicy Bypass -File \"%s\"", full);
    } else if (strstr(full, ".sh")) {
        /* relative sh under house: bash from cwd */
        snprintf(cmdline, sizeof(cmdline), "bash \"%s\"", full);
    } else if (strcmp(full, "xdg-open") == 0) {
        snprintf(cmdline, sizeof(cmdline), "explorer \"%s\"", g_package);
    } else if (strstr(full, "gedit") || strstr(full, "gnome-terminal")) {
        append_history("linux-only action skipped on win");
        return;
    } else {
        snprintf(cmdline, sizeof(cmdline), "%s", full);
    }
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                   CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                   NULL, NULL, &si, &pi);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
}

static void show_context_menu(void) {
    load_menu();
    POINT pt; GetCursorPos(&pt);
    HMENU h = CreatePopupMenu();
    for (int i = 0; i < g_nmenu; i++)
        AppendMenuA(h, MF_STRING, (UINT)(1000 + i), g_menu[i].label);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(h, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(h);
}

static void poll_relay(void) {
    FILE *f = fopen_u8(g_relay, "r");
    if (!f) return;
    char line[512];
    int close_me = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "CLOSE", 5) == 0) close_me = 1;
        if (strncmp(line, "ACTIVATE", 8) == 0 && g_hwnd) {
            SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(g_hwnd);
        }
    }
    fclose(f);
    f = fopen_u8(g_relay, "w");
    if (f) fclose(f);
    if (close_me) { append_history("CLOSE relay"); DestroyWindow(g_hwnd); }
}

static void load_package_meta(void) {
    char path[PATH_BUF];
    join2(path, sizeof(path), g_package, "glyph.txt");
    read_text_line(path, g_glyph, sizeof(g_glyph));
    if (!g_glyph[0]) snprintf(g_glyph, sizeof(g_glyph), "?");
    {
        const char *base = g_package;
        const char *s = strrchr(g_package, '\\');
        const char *s2 = strrchr(g_package, '/');
        if (s2 && (!s || s2 > s)) s = s2;
        if (s && s[1]) base = s + 1;
        snprintf(g_entity, sizeof(g_entity), "%.120s", base);
    }
    join2(path, sizeof(path), g_package, "meta.pdl");
    FILE *f = fopen_u8(path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "footprint_tiles")) {
                char *v = strrchr(line, '|');
                if (v) {
                    int t = atoi(v + 1);
                    if (t >= 1 && t <= 8) g_win_px = DEFAULT_WIN_PX * t;
                }
            }
        }
        fclose(f);
    }
}

static int find_house_root(const char *pkg, char *out, size_t n) {
    if (pkg[0] != '/' && !(isalpha((unsigned char)pkg[0]) && pkg[1] == ':')) {
        if (path_exists_u8("#.desktop") || path_exists_u8("$.crypts")) {
            snprintf(out, n, ".");
            return 1;
        }
    }
    snprintf(out, n, ".");
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, POLL_MS, NULL);
        return 0;
    case WM_TIMER:
        if (wParam == 1) {
            if (!path_exists_u8(g_package)) { DestroyWindow(hwnd); return 0; }
            poll_relay();
            render_frame();
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        render_frame();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_dragging = 1;
        POINT pt; GetCursorPos(&pt);
        RECT rc; GetWindowRect(hwnd, &rc);
        g_drag_dx = pt.x - rc.left;
        g_drag_dy = pt.y - rc.top;
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_dragging && (wParam & MK_LBUTTON)) {
            POINT pt; GetCursorPos(&pt);
            SetWindowPos(hwnd, HWND_TOPMOST, pt.x - g_drag_dx, pt.y - g_drag_dy,
                         0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_dragging) {
            ReleaseCapture();
            g_dragging = 0;
            RECT rc; GetWindowRect(hwnd, &rc);
            write_pos(rc.left, rc.top);
        }
        return 0;
    case WM_RBUTTONUP:
        snprintf(g_menu_page, sizeof(g_menu_page), "main");
        show_context_menu();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 1000 && id < 1000 + g_nmenu)
            run_action(g_menu[id - 1000].action);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        unregister_open();
        if (g_glrc) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(g_glrc);
            g_glrc = NULL;
        }
        if (g_hdc) { ReleaseDC(hwnd, g_hdc); g_hdc = NULL; }
        if (g_sprite_pixels) { free(g_sprite_pixels); g_sprite_pixels = NULL; }
        append_history("window destroy");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_desktop_window <package_dir>\n");
        return 1;
    }
    snprintf(g_package, sizeof(g_package), "%s", argv[1]);
    for (char *p = g_package; *p; p++) if (*p == '/') *p = '\\';
    if (strncmp(g_package, ".\\", 2) == 0)
        memmove(g_package, g_package + 2, strlen(g_package + 2) + 1);

    if (!path_exists_u8(g_package)) {
        fprintf(stderr, "tp_desktop_window: package missing: %s\n", g_package);
        return 1;
    }

    find_house_root(g_package, g_house, sizeof(g_house));
    join2(g_history, sizeof(g_history), g_package, "history.txt");
    join2(g_relay, sizeof(g_relay), g_package, "interact_relay.txt");
    { FILE *f = fopen_u8(g_relay, "w"); if (f) fclose(f); }

    load_package_meta();
    g_index = next_index();

    char sprite_path[PATH_BUF];
    join2(sprite_path, sizeof(sprite_path), g_package, "sprite.csv");
    if (load_sprite_csv(sprite_path))
        append_history("sprite.csv loaded");
    else
        append_history("sprite.csv missing â€” blue fallback");

    int x = 80 + (g_index % 8) * GRID_CELL_PX;
    int y = 80 + (g_index % 6) * GRID_CELL_PX;
    if (!read_pos(&x, &y)) {
        /* stagger new windows inside visible primary screen */
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        x = 80 + ((g_index * GRID_CELL_PX) % (sw > 200 ? sw - 120 : 400));
        y = 80 + (((g_index * GRID_CELL_PX) / 2) % (sh > 200 ? sh - 160 : 300));
    }
    clamp_screen(&x, &y);

    HINSTANCE hi = GetModuleHandle(NULL);
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "KHTPM_DesktopWindow";
    RegisterClassA(&wc);

    char title[300];
    snprintf(title, sizeof(title), "%s %s", g_glyph, g_entity);

    g_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "KHTPM_DesktopWindow", title,
        WS_POPUP | WS_VISIBLE,
        x, y, g_win_px, g_win_px,
        NULL, NULL, hi, NULL);
    if (!g_hwnd) {
        fprintf(stderr, "tp_desktop_window: CreateWindow failed\n");
        return 1;
    }

    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) {
        fprintf(stderr, "tp_desktop_window: pixel format failed\n");
        return 1;
    }
    g_glrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_glrc);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (g_sprite_pixels) {
        upload_texture();
        build_shape_mask(g_hwnd);
    }

    register_open();
    ensure_taskbar();
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(g_hwnd, SW_SHOW);
    render_frame();
    append_history("win L2 open");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
